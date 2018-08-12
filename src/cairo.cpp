/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <atomic>
#include <future>
#include <map>
#include <mutex>

#include <string.h>
#include <sstream>

#include <cairo-ft.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "DisplayDevice.hpp"
#include "svg/svg_parse.hpp"
#include "svg/svg_paint.hpp"

#include "asserts.hpp"
#include "cairo.hpp"
#include "filesystem.hpp"
#include "formula_callable.hpp"
#include "formula_object.hpp"
#include "formula_profiler.hpp"
#include "graphical_font.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "string_utils.hpp"
#include "TextureObject.hpp"
#include "unit_test.hpp"
#include "utf8_to_codepoint.hpp"
#include "variant_utils.hpp"

namespace {
PREF_STRING(cairo_texture_filtering, "bilinear", "Filtering for cairo textures");
PREF_INT(cairo_texture_mipmaps, 2, "Mipmaps for cairo textures");

variant get_default_texture_settings()
{
	variant_builder b;
	b.add("filtering", g_cairo_texture_filtering);
	b.add("mipmaps", g_cairo_texture_mipmaps);
	return b.build();
}

}

namespace graphics
{
	namespace 
	{
		struct TextFragment 
		{
			float xpos, ypos, width, height, x_advance, y_bearing;
			std::string font;
			int font_size;
			variant tag;
			variant info;
			KRE::Color color;

			std::string text;
			variant valign;

			cairo_font_extents_t font_extents;

			std::string svg;
			std::string img;
			const rect* img_src_area;
		};

		struct LineOfText 
		{
			LineOfText() : fragment_width(0.0) {}
			float fragment_width;
			std::vector<TextFragment> fragments;
			variant align;
		};

		FT_Library& init_freetype_library()
		{
			static FT_Library library;
			const int error = FT_Init_FreeType(&library);
			ASSERT_LOG(error == 0, "Could not initialize freetype: " << error);
			return library;
		}

		const std::string& get_font_path_from_name(const std::string& name)
		{

			static std::map<std::string, std::string> paths;
			if(paths.empty()) {
				std::map<std::string, std::string> full_paths;
				module::get_unique_filenames_under_dir("data/fonts/", &full_paths);
				for(auto p : full_paths) {
					auto colon_itor = std::find(p.first.begin(), p.first.end(), ':');
					std::string key(colon_itor == p.first.end() ? p.first.begin() : colon_itor+1, p.first.end());
					if(key.size() > 4 &&
					   (std::equal(key.end()-4, key.end(), ".ttf") ||
						std::equal(key.end()-4, key.end(), ".otf"))) {
						paths[std::string(key.begin(), key.end()-4)] = p.second;
						LOG_DEBUG("Added Font: " << p.second << " at " << std::string(key.begin(), key.end()-4));
					}
				}
			}

			auto itor = paths.find(name);
			ASSERT_LOG(itor != paths.end(), "Could not find font: " << name);
			return itor->second;
		}

		FT_Face get_ft_font(const std::string& ttf_name, int index=0)
		{
			if (ttf_name.size() > 4
					&& (std::equal(ttf_name.end() - 4, ttf_name.end(), ".otf")
							|| std::equal(ttf_name.end() - 4, ttf_name.end(), ".ttf"))) {
				return get_ft_font(std::string(ttf_name.begin(), ttf_name.end()-4), index);
			}

			static std::mutex mutex;
			std::lock_guard<std::mutex> guard(mutex);

			const std::string& ttf_file = get_font_path_from_name(ttf_name.empty() ? module::get_default_font() == "bitmap" ? "FreeMono" : module::get_default_font() : ttf_name);
			static FT_Library& library = init_freetype_library();

			static std::map<std::string, FT_Face> cache;

			auto itor = cache.find(ttf_file);
			if(itor != cache.end()) {
				return itor->second;
			}

			FT_Face face;
			const int error = FT_New_Face(library, ttf_file.c_str(), index, &face);
			ASSERT_LOG(error == 0, "Could not load font face: " << ttf_file << " error: " << error);

			cache[ttf_file] = face;
			return face;
		}

namespace {
	class surface_cache_man {
	public:
		surface_cache_man() : usage_(0) {}
		~surface_cache_man() {
			for ( auto & v : cache_) {
				cairo_surface_destroy(v.second);
			}
		}

		bool has(const std::string& image) const {
			std::lock_guard<std::mutex> guard(mutex_);
			return cache_.count(image) != 0;
		}

		cairo_surface_t* get(const std::string& image) {
			mutex_.lock();
			auto itor = cache_.find(image);
			cairo_surface_t*& result = itor == cache_.end() ? cache_[image] : itor->second;
			mutex_.unlock();

			if(itor == cache_.end()) {
				result = cairo_image_surface_create_from_png(module::map_file(image).c_str());
				ASSERT_LOG(result, "Could not load cairo image: " << image);

				const int w = cairo_image_surface_get_width(result);
				const int h = cairo_image_surface_get_height(result);

				usage_ += w*h*4;
			}

			while(result == nullptr) {
				SDL_Delay(1);
			}

			return result;
		}

		size_t size() const {
			std::lock_guard<std::mutex> guard(mutex_);
			return cache_.size();
		}
		size_t usage() const {
			std::lock_guard<std::mutex> guard(mutex_);
			return usage_;
		}
	private:
		std::map<std::string, cairo_surface_t*> cache_;
		size_t usage_;

		mutable std::mutex mutex_;
	};

	static surface_cache_man g_surface_cache;
}

		cairo_surface_t* get_cairo_image(const std::string& image)
		{
			return g_surface_cache.get(image);
		}

		bool has_cairo_image(const std::string& image)
		{
			return g_surface_cache.has(image);
		}

		cairo_context& dummy_context() 
		{
			static cairo_context* res = new cairo_context(8, 8);
			return *res;
		}

		struct TextMarkupFragment
		{
			TextMarkupFragment() : nobr(false), color(255,255,255,255), img_w(-1), img_h(-1), img_src_area(nullptr) {}
			std::string text;
			std::string font;
			float font_size;
			int font_weight;
			bool font_italic;
			bool nobr;
			variant tag;
			variant info;
			variant align, valign;
			std::string svg;

			std::string img;
			int img_w, img_h;

			//used for bitmap fonts.
			const rect* img_src_area;

			KRE::Color color;
		};

	}

	cairo_context::cairo_context(int w, int h)
	  : width_((w+1) & ~1), 
	    height_((h+1) & ~1), 
		temp_pattern_(nullptr)
	{
		surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width_, height_);
		cairo_ = cairo_create(surface_);

		cairo_font_options_t* options = cairo_font_options_create();
		cairo_get_font_options(cairo_, options);
		cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_SLIGHT);
		cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_OFF);
		cairo_set_font_options(cairo_, options);
		cairo_font_options_destroy(options);

		cairo_status_t status = cairo_status(cairo_);
		ASSERT_LOG(status == 0, "SVG rendering error creating cairo context " << w << "x" << h << ": " << cairo_status_to_string(status));
	}

	cairo_context::~cairo_context()
	{
		if(temp_pattern_) {
			cairo_pattern_destroy(temp_pattern_);
			temp_pattern_ = nullptr;
		}

		cairo_destroy(cairo_);
		cairo_surface_destroy(surface_);
	}

	cairo_t* cairo_context::get() const
	{
		return cairo_;
	}

	KRE::TexturePtr cairo_context::write(const variant& node) const
	{
		formula_profiler::Instrument instrument("CAIRO_CONTEXT_WRITE");
		auto surf = KRE::Surface::create(width_, height_, KRE::PixelFormat::PF::PIXELFORMAT_ARGB8888);
		std::vector<unsigned char> bytes;
		bytes.resize(cairo_image_surface_get_stride(surface_) * height_);
		memcpy(&bytes[0], cairo_image_surface_get_data(surface_), bytes.size());

		for(size_t n = 0; n < bytes.size(); n += 4) {
			int alpha = bytes[n+3];
			if(alpha > 0 && alpha < 255) {
				bytes[n+0] = (int(bytes[n+0])*255)/alpha;
				bytes[n+1] = (int(bytes[n+1])*255)/alpha;
				bytes[n+2] = (int(bytes[n+2])*255)/alpha;
			}
		}

		if(bytes.empty() == false) {
			surf->writePixels(&bytes[0], bytes.size());
		}

		surf->createAlphaMap();

		static variant settings = get_default_texture_settings();
		if(node.is_map()) {
			settings = settings + node;
		}

		formula_profiler::Instrument instrument2("CAIRO_CONTEXT_CREATE_TEXTURE");
		KRE::TexturePtr t = KRE::Texture::createTexture(surf, settings);
		return t;
		//tex->update2D(0, 0, 0, width_, height_, cairo_image_surface_get_stride(surface_), cairo_image_surface_get_data(surface_));
		// Use the blend mode below to give correct for pre-multiplied alpha.
		// If that doesn't work satisfactorily, then creating a texture with PIXELFORMAT_XRGB8888
		// might be a better option. 
		// Naturally if you had the luxury a shader that just ignored the texture alpha would work
		// just as well.
		//return tex;
	}

	void cairo_context::render_svg(const std::string& fname, int w, int h)
	{
		w = (w+1) & ~1;
		h = (h+1) & ~1;
		cairo_status_t status = cairo_status(cairo_);
		ASSERT_LOG(status == 0, "SVG rendering error rendering " << w << "x" << h << ": " << fname << ": " << cairo_status_to_string(status));

		std::shared_ptr<KRE::SVG::parse> handle;

		{
		static std::map<std::string, std::shared_ptr<KRE::SVG::parse>> cache;

		static std::mutex mutex;
		std::lock_guard<std::mutex> guard(mutex);

		auto itor = cache.find(fname);
		if(itor == cache.end()) {
			std::string real_fname = module::map_file(fname);
			ASSERT_LOG(sys::file_exists(real_fname), "Could not find svg file: " << fname);

			handle = std::shared_ptr<KRE::SVG::parse>(new KRE::SVG::parse(real_fname));		
			cache[fname] = handle;
		} else {
			handle = itor->second;
		}
		}

		KRE::SVG::render_context ctx(cairo_, w, h);

		
		double red = 0, green = 0, blue = 0, alpha = 0;
		status = cairo_pattern_get_rgba(cairo_get_source(cairo_), &red, &green, &blue, &alpha);
		if(status == CAIRO_STATUS_SUCCESS) {
			ctx.set_color_multiply(KRE::Color(static_cast<float>(red), static_cast<float>(green), static_cast<float>(blue), static_cast<float>(alpha)));
		}

		cairo_save(cairo_);
		handle->render(ctx);
		cairo_restore(cairo_);

		status = cairo_status(cairo_);
		ASSERT_LOG(status == 0, "SVG rendering error rendering " << w << "x" << h << ": " << fname << ": " << cairo_status_to_string(status));
	}

	void cairo_context::write_png(const std::string& fname) 
	{
		cairo_surface_write_to_png(surface_, fname.c_str());
	}

	void cairo_context::set_pattern(cairo_pattern_t* pattern, bool take_ownership)
	{
		if(temp_pattern_) {
			cairo_pattern_destroy(temp_pattern_);
			temp_pattern_ = nullptr;
		}

		if(take_ownership) {
			temp_pattern_ = pattern;
		}

		cairo_set_source(cairo_, pattern);
	}

	cairo_pattern_t* cairo_context::get_pattern_ownership(bool* get_ownership)
	{
		if(temp_pattern_) {
			temp_pattern_ = nullptr;
			*get_ownership = true;
		} else {
			*get_ownership = false;
		}

		return cairo_get_source(cairo_);
	}

	cairo_matrix_saver::cairo_matrix_saver(cairo_context& ctx) : ctx_(ctx)
	{
		cairo_save(ctx_.get());
	}

	cairo_matrix_saver::~cairo_matrix_saver()
	{
		cairo_restore(ctx_.get());
	}

	typedef std::function<void(cairo_context&, const std::vector<variant>&)> CairoOp;
	class cairo_op : public game_logic::FormulaCallable
	{
	public:
		cairo_op(CairoOp fn, const std::vector<variant>& args) : fn_(fn), args_(args)
		{}

		void execute(cairo_context& context) {
			fn_(context, args_);
		}

		bool isCairoOp() const override { return true; }
	private:
		DECLARE_CALLABLE(cairo_op);
	
		CairoOp fn_;
		std::vector<variant> args_;
	};

	class cairo_text_fragment : public game_logic::FormulaCallable
	{
	public:
		cairo_text_fragment() : x(0), y(0), width(0), height(0), ascent(0), descent(0), x_advance(0), has_image(false), has_text(false), img_src_area(nullptr), font_size(12), translate_x(0), translate_y(0) {
		}
		variant tag, info;
		float x, y, width, height, ascent, descent, x_advance;
		KRE::Color color;
		bool has_image, has_text;
		std::string text;

		std::string img, svg;
		const rect* img_src_area;

		std::string font;
		int font_size;

		float translate_x, translate_y;

		size_t num_chars() const {
			utils::utf8_to_codepoint s(text);
			return std::distance(s.begin(), s.end());
		}

		variant get_slice(int begin, int end) const {
			const size_t nchars = num_chars();
			if(begin == 0 && end == nchars) {
				return variant(this);
			}

			ASSERT_LOG(begin >= 0 && begin <= nchars && end >= begin && end <= nchars, "Illegal slice of text fragment: " << begin << ", " << end << " / " << text << " " << nchars);

			cairo_text_fragment* res = new cairo_text_fragment(*this);

			utils::utf8_to_codepoint utf8_str(text);

			auto utf8_begin = utf8_str.begin();
			auto utf8_i1 = utf8_str.begin();
			std::advance(utf8_i1, begin);
			auto utf8_i2 = utf8_i1;
			std::advance(utf8_i2, end - begin);

			res->text = std::string(utf8_i1.str_itor(), utf8_i2.str_itor());

			FT_Face face = get_ft_font(font);
			cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
			cairo_set_font_face(dummy_context().get(), cairo_face);
			cairo_set_font_size(dummy_context().get(), font_size);

			std::string s(utf8_begin.str_itor(), utf8_i1.str_itor());
			cairo_text_extents_t extents;
			cairo_text_extents(dummy_context().get(), s.c_str(), &extents);
			res->translate_x += extents.x_advance;

			return variant(res);
		}

		variant calculate_path(bool do_translate=true) const {
			if(img_src_area != nullptr) {
				std::vector<variant> fn_args;
				fn_args.push_back(variant(img));
				fn_args.push_back(variant(translate_x));
				fn_args.push_back(variant(translate_y));
				fn_args.push_back(variant(img_src_area->x()));
				fn_args.push_back(variant(img_src_area->y()));
				fn_args.push_back(variant(img_src_area->w()));
				fn_args.push_back(variant(img_src_area->h()));

				return variant(new cairo_op([=](cairo_context& context, const std::vector<variant>& args) {
					cairo_save(context.get());


					//fprintf(stderr, "DRAW GLYPH: %s, %f, %f -> %f, %f, %f, %f\n", args[0].as_string().c_str(), args[1].as_decimal().as_float(), args[2].as_decimal().as_float(), args[3].as_decimal().as_float(), args[4].as_decimal().as_float(), args[5].as_decimal().as_float(), args[6].as_decimal().as_float());


					cairo_translate(context.get(), args[1].as_decimal().as_float(), args[2].as_decimal().as_float());

					cairo_surface_t* surface = get_cairo_image(args[0].as_string());
					cairo_set_source_surface(context.get(), surface, -args[3].as_float(), -args[4].as_float());

					cairo_rectangle(context.get(), 0.0f, 0.0f, args[5].as_decimal().as_float(), args[6].as_decimal().as_float());

					cairo_clip(context.get());
					cairo_paint(context.get());
					cairo_reset_clip(context.get());

					cairo_restore(context.get());
				}, fn_args));

			} else if(img.empty() == false) {
				std::vector<variant> fn_args;
				fn_args.push_back(variant(img));
				fn_args.push_back(variant(translate_x));
				fn_args.push_back(variant(translate_y));
				fn_args.push_back(variant(width));
				fn_args.push_back(variant(height));
				return variant(new cairo_op([=](cairo_context& context, const std::vector<variant>& args) {
					cairo_save(context.get());

					if(do_translate) {
						cairo_translate(context.get(), args[1].as_decimal().as_float(), args[2].as_decimal().as_float());
					}

					cairo_surface_t* surface = get_cairo_image(args[0].as_string());
					cairo_status_t status = cairo_status(context.get());
					ASSERT_LOG(status == 0, "rendering error painting " << args[0].as_string() << ": " << cairo_status_to_string(status));

					float img_w = cairo_image_surface_get_width(surface);
					float img_h = cairo_image_surface_get_height(surface);
					cairo_scale(context.get(), args[3].as_decimal().as_float()/img_w, args[4].as_decimal().as_float()/img_h);

					cairo_set_source_surface(context.get(), surface, 0.0, 0.0);

					status = cairo_status(context.get());
					ASSERT_LOG(status == 0, "SVG rendering error painting " << args[0].as_string() << ": " << cairo_status_to_string(status));
					cairo_paint(context.get());

					status = cairo_status(context.get());
					ASSERT_LOG(status == 0, "SVG rendering error painting " << args[0].as_string() << ": " << cairo_status_to_string(status));

					cairo_restore(context.get());

				}, fn_args));

			} else if(svg.empty() == false) {

				std::vector<variant> fn_args;
				fn_args.push_back(variant(svg));
				fn_args.push_back(variant(translate_x));
				fn_args.push_back(variant(translate_y));
				fn_args.push_back(variant(width));
				fn_args.push_back(variant(height));

				return variant(new cairo_op([=](cairo_context& context, const std::vector<variant>& args) {
					if(do_translate) {
						cairo_translate(context.get(), args[1].as_decimal().as_float(), args[2].as_decimal().as_float());
					}

					variant svg = args[0];
					int w = args[3].as_decimal().as_int();
					int h = args[4].as_decimal().as_int();
					const cairo_matrix_saver saver(context);

					context.render_svg(svg.as_string(), w, h);

				}, fn_args));

			} else {

				std::vector<variant> fn_args;
				fn_args.push_back(variant(translate_x));
				fn_args.push_back(variant(translate_y));
				fn_args.push_back(variant(text));
				fn_args.push_back(variant(font));
				fn_args.push_back(variant(font_size));

				return variant(new cairo_op([=](cairo_context& context, const std::vector<variant>& args) {

					if(do_translate) {
						cairo_translate(context.get(), args[0].as_decimal().as_float(), args[1].as_decimal().as_float());
					}

					FT_Face face = get_ft_font(args[3].as_string());
					cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
					cairo_set_font_face(context.get(), cairo_face);
					cairo_set_font_size(context.get(), args[4].as_int());

					cairo_text_path(context.get(), args[2].as_string().c_str());
				}, fn_args));
			}
		}

	private:
		DECLARE_CALLABLE(cairo_text_fragment);

	};

	BEGIN_DEFINE_CALLABLE_NOBASE(cairo_text_fragment)
		DEFINE_FIELD(path, "builtin cairo_op")
			return obj.calculate_path();
		DEFINE_FIELD(path_no_translate, "builtin cairo_op")
			return obj.calculate_path(false);
		DEFINE_FIELD(translate_x, "decimal")
			return variant(obj.translate_x);
		DEFINE_FIELD(translate_y, "decimal")
			return variant(obj.translate_y);

		DEFINE_FIELD(tag, "[string]")
			if(obj.tag.is_list()) {
				return obj.tag;
			} else {
				static std::vector<variant> v;
				static variant empty_list(&v);
				return empty_list;
			}
		DEFINE_FIELD(info, "any")
			return obj.info;
		DEFINE_FIELD(x, "decimal")
			return variant(obj.x);
		DEFINE_FIELD(y, "decimal")
			return variant(obj.y);
		DEFINE_FIELD(width, "decimal")
			return variant(obj.width);
		DEFINE_FIELD(height, "decimal")
			return variant(obj.height);
		DEFINE_FIELD(x_advance, "decimal")
			return variant(obj.x_advance);
		DEFINE_FIELD(ascent, "decimal")
			return variant(obj.ascent);
		DEFINE_FIELD(descent, "decimal")
			return variant(obj.descent);
		DEFINE_FIELD(color, "[decimal,decimal,decimal,decimal]")
			std::vector<variant> result;
			result.resize(4);
			result[0] = variant(obj.color.r());
			result[1] = variant(obj.color.g());
			result[2] = variant(obj.color.b());
			result[3] = variant(obj.color.a());
			return variant(&result);
		DEFINE_FIELD(has_image, "bool")
			return variant::from_bool(obj.has_image);
		DEFINE_FIELD(has_text, "bool")
			return variant::from_bool(obj.has_text);
		DEFINE_FIELD(text, "string")
			return variant(obj.text);
		DEFINE_FIELD(num_chars, "int")
			return variant(static_cast<int>(obj.num_chars()));
		BEGIN_DEFINE_FN(get_slice, "(int, int) ->builtin cairo_text_fragment")
			int begin = FN_ARG(0).as_int();
			int end = FN_ARG(1).as_int();
			return obj.get_slice(begin, end);
		END_DEFINE_FN
	END_DEFINE_CALLABLE(cairo_text_fragment)

	struct MarkupEntry 
	{
		const char* tag;
		char32_t codepoint; 
	};

	MarkupEntry MarkupMap[] = {
		{ "ldquo", 8220 },
		{ "rdquo", 8221 },
		{ "emdash", 8212 },
		{ "amp", 38 },
		{ "lt", 60 },
		{ "gt", 62 },
		{ "aelig", 230 },
		{ "AElig", 198 },
	};

	std::string parse_special_chars_internal(const std::string& s)
	{
		std::string result;
		auto start = s.begin();
		auto itor = std::find(s.begin(), s.end(), '&');
		if(itor == s.end()) {
			return s;
		}

		while(itor != s.end()) {
			result += std::string(start, itor);

			auto semi = std::find(itor, s.end(), ';');
			if(semi == s.end()) {
				start = itor;
				break;
			}

			bool found = false;
			std::string tag(itor+1, semi);
			for(const MarkupEntry& e : MarkupMap) {
				if(tag == e.tag) {
					result += utils::codepoint_to_utf8(e.codepoint);
					found = true;
					break;
				}
			}

			ASSERT_LOG(found, "Could not find markup: '" << tag << "'");

			start = semi + 1;
			itor = std::find(semi, s.end(), '&');
		}

		result += std::string(start, s.end());
		return result;
	}

	std::string utf8_to_markup(const std::string& text)
	{
		std::string result;

		utils::utf8_to_codepoint s(text);
		auto i1 = s.begin();
		auto i2 = s.end();
		while(i1 != i2) {
			auto codepoint = *i1;
			if(codepoint < 128) {
				result.push_back(static_cast<char>(codepoint));
			} else {
				const char* symbol = nullptr;
				for(const MarkupEntry& entry : MarkupMap) {
					if(entry.codepoint == codepoint) {
						symbol = entry.tag;
						break;
					}
				}
				std::ostringstream stream;

				if(symbol == nullptr) {
					stream << "&#" << static_cast<unsigned int>(codepoint) << ";";
				} else {
					stream << "&" << symbol << ";";
				}
				result += stream.str();
			}

			++i1;
		}

		return result;
	}

	void execute_cairo_ops(cairo_context& context, variant v)
	{
		if(v.is_null()) {
			return;
		}

		if(v.is_list()) {
			for(int n = 0; n != v.num_elements(); ++n) {
				execute_cairo_ops(context, v[n]);
			}

			return;
		}

		v.convert_to<cairo_op>()->execute(context);
	}

	class TextureObjectFuture : public game_logic::FormulaCallable
	{
	public:
		TextureObjectFuture(int w, int h, variant ops, variant texture_args) : context_(w, h), w_(w), h_(h), ops_(ops), texture_args_(texture_args)
		{
			finished_ = false; //init here becaues for some reason MSVC++ doesn't
			                   //have a constructor version of atomic_bool
			TextureObjectFuture* f = this;
			future_ = std::async(std::launch::async, [=]() {
				execute_cairo_ops(f->context_, f->ops_);
				f->finished_ = true;
				return true;
			});
		}

		~TextureObjectFuture() {
			future_.wait();
		}

		bool finished() const { return finished_; }

		const ffl::IntrusivePtr<TextureObject>& result() const {
			if(!result_ && finished_) {
				result_.reset(new TextureObject(context_.write(texture_args_)));
			}

			return result_;
		}

	private:
		DECLARE_CALLABLE(TextureObjectFuture);
		cairo_context context_;
		int w_, h_;
		variant ops_;
		std::atomic_bool finished_;

		variant texture_args_;

		mutable ffl::IntrusivePtr<TextureObject> result_;

		std::future<bool> future_;
	};

	BEGIN_DEFINE_CALLABLE_NOBASE(TextureObjectFuture)
	DEFINE_FIELD(finished, "bool")
		return variant::from_bool(obj.finished());
	DEFINE_FIELD(texture, "builtin texture_object")
		
		return variant(obj.result().get());
	END_DEFINE_CALLABLE(TextureObjectFuture)

	variant layout_text_impl(const TextMarkupFragment* f1, const TextMarkupFragment* f2, float width, float width_delta=0.0, float scale_line_heights=1.0)
	{

		cairo_context context(8, 8);

		float original_width = width;

		float xpos = 0;
		float ypos = 0;
		float line_height = 0;

		std::vector<LineOfText> output;
		output.push_back(LineOfText());

		const TextMarkupFragment* first_item = f1;

		for(; f1 != f2; ++f1) {
			const TextMarkupFragment& item = *f1;

			const std::string& text = item.text;
			const std::string& font = item.font;
			const int font_size = item.font_size;
			const variant& tag = item.tag;
			const variant& info = item.info;
			const variant& align = item.align;
			const variant& valign = item.valign;
			std::string svg = item.svg;
			std::string img = item.img;

			float min_line_height = 0.0f;

			cairo_font_extents_t font_extents;
			if(item.img_src_area == nullptr) {

				FT_Face face = get_ft_font(font);
				cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
				cairo_set_font_face(context.get(), cairo_face);
				cairo_set_font_size(context.get(), font_size);

				cairo_font_extents(context.get(), &font_extents);
	
				if(font_extents.height > line_height) {
					line_height = static_cast<float>(font_extents.height);
				}

				min_line_height = static_cast<float>(font_extents.height);
			} else {
				font_extents.max_y_advance = font_extents.height = static_cast<float>(item.img_src_area->h());
				font_extents.max_x_advance = static_cast<float>(item.img_src_area->w());
				font_extents.ascent = font_extents.descent = font_extents.height/2.0f;
			}

			std::vector<std::string> lines = util::split(text, '\n', 0);

			if(!output.empty() && output.back().fragments.empty()) {
				output.back().align = align;
			}

			bool first_line = true;

			if(item.nobr && (first_item == f1 || !(f1-1)->nobr) && lines.size() == 1) {
				//the start of a nobr sequence. See if we need to split
				//to a new line.
				int nobr_x_advance = 0;
				for(const TextMarkupFragment* f = f1; f != f2 && f->nobr; ++f) {
					cairo_text_extents_t extents;
					cairo_text_extents(context.get(), f->text.c_str(), &extents);
					nobr_x_advance += extents.x_advance;
				}

				if(xpos + nobr_x_advance > width) {
					first_line = false;
				}
			}

			for(const std::string& line : lines) {
				if(!first_line) {
					xpos = 0;
					ypos += line_height*scale_line_heights;
					width += width_delta*line_height*scale_line_heights;
					line_height = min_line_height;

					output.push_back(LineOfText());
					output.back().align = align;
				}

				first_line = false;

				std::string::const_iterator i1 = line.begin();
				while(i1 != line.end() || svg.empty() == false || img.empty() == false) {
					std::string::const_iterator i2 = std::find(i1, line.end(), ' ');
					if(item.nobr) {
						i2 = line.end();
					}

					if(i2 != line.end()) {
						++i2;
					}

					std::string word(i1, i2);

					cairo_text_extents_t extents;
					cairo_text_extents(context.get(), word.c_str(), &extents);

					bool break_line = false;

					if(xpos + extents.x_advance > width) {
						if(item.nobr == false) {
							break_line = true;
							i2 = i1+1;
						}

						xpos = 0;
						ypos += line_height*scale_line_heights;
						width += width_delta*line_height*scale_line_heights;
						line_height = min_line_height;

						output.push_back(LineOfText());
						output.back().align = align;
					} else {
						while(i2 != line.end()) {
							auto anchor = i2;
							i2 = std::find(i2, line.end(), ' ');
							if(i2 != line.end()) {
								++i2;
							}

							cairo_text_extents_t new_extents;
							cairo_text_extents(context.get(), std::string(i1,i2).c_str(), &new_extents);

							if(xpos + new_extents.x_advance > width) {
								i2 = anchor;
								break;
							}

							extents = new_extents;
							word = std::string(i1,i2);
						}
					}

					if(break_line) {
						auto anchor = i2;
						while(i2 != line.end()) {
							i2 = std::find(i2, line.end(), ' ');
							if(i2 != line.end()) {
								++i2;
							}

							word = std::string(i1, i2);
							cairo_text_extents_t new_extents;
							cairo_text_extents(context.get(), word.c_str(), &new_extents);
							if(xpos + new_extents.x_advance > width) {
								i2 = anchor;
								word = std::string(i1, i2);
								break;
							}

							anchor = i2;
							extents = new_extents;
						}
					}

					int img_w = item.img_w, img_h = item.img_h;

					if(item.img_src_area != nullptr) {
						img_w = item.img_src_area->w();
						img_h = item.img_src_area->h();
					}

					if(svg.empty() == false) {
						//advance the text position along to account
						//for the svg icon, wrapping to next line if necessary
						float advance = static_cast<float>(font_extents.height);

						if(xpos > 0 && xpos + advance > width) {
							xpos = 0;
							ypos += line_height*scale_line_heights;
							width += width_delta*line_height*scale_line_heights;
							line_height = min_line_height;

							output.push_back(LineOfText());
							output.back().align = align;
						}
					} else if(img.empty() == false) {

						if(img_w == -1 || img_h == -1) {
							cairo_surface_t* surface = get_cairo_image(item.img);
							cairo_status_t status = cairo_status(context.get());
							ASSERT_LOG(status == 0, "rendering error painting " << item.img << ": " << cairo_status_to_string(status));

							float w = cairo_image_surface_get_width(surface);
							float h = cairo_image_surface_get_height(surface);

							float ratio = 1.0;

							if(img_w != -1) {
								ratio = img_w/w;
							} else if(img_h != -2) {
								ratio = img_h/h;
							}

							if(img_w == -1) {
								img_w = static_cast<int>(w*ratio);
							}

							if(img_h == -1) {
								img_h = static_cast<int>(h*ratio);
							}

						}

						extents.width = img_w;
						extents.height = img_h;
						extents.y_bearing = img_h;

						float advance = static_cast<float>(img_w);

						if(xpos > 0 && xpos + advance > width) {
							xpos = 0;
							ypos += line_height*scale_line_heights;
							width += width_delta*line_height*scale_line_heights;
							line_height = min_line_height;

							output.push_back(LineOfText());
							output.back().align = align;
						}

						if(img_h > line_height) {
							line_height = static_cast<float>(img_h);
						}
					}

					TextFragment fragment = { xpos, ypos, static_cast<float>(extents.width), static_cast<float>(extents.height), static_cast<float>(extents.x_advance), extents.y_bearing, font, font_size, tag, info, item.color, std::string(i1, i2), valign, font_extents, svg, img, item.img_src_area };
					output.back().fragments.push_back(fragment);
					output.back().fragment_width += static_cast<float>(extents.width);

					if(svg.empty() == false) {
						xpos += static_cast<float>(font_extents.height);
					} else if(img.empty() == false) {
						xpos += static_cast<float>(img_w);
					}
				
					svg = "";
					img = "";

					xpos += static_cast<float>(extents.x_advance);

					i1 = i2;
				}
			}
		}

		if(output.empty() == false && output.back().fragments.empty()) {
			output.pop_back();
		}

		width = original_width;

		std::vector<variant> result;

		for(const LineOfText& line : output) {

			//calculate the baseline. 
			float line_height = 0.0;
			float max_ascent = 0.0;
			float max_descent = 0.0;
			for(const TextFragment& fragment : line.fragments) {
				if(fragment.font_extents.height > line_height) {
					line_height = static_cast<float>(fragment.font_extents.height);
				}

				if(fragment.height > line_height) {
					line_height = fragment.height;
				}

				if(fragment.font_extents.ascent > max_ascent) {
					max_ascent = static_cast<float>(fragment.font_extents.ascent);
				}

				if(fragment.font_extents.descent > max_descent) {
					max_descent = static_cast<float>(fragment.font_extents.descent);
				}
			}

			const float baseline = max_ascent; // + (line_height-(max_ascent+max_descent))/2;
			const float line_top = (line_height - (max_ascent+max_descent))/2;
			const float line_bottom = line_height - line_top;

			float xpos_align_adjust = 0;
			if(line.align.is_null() == false) {
				const std::string& align = line.align.as_string();
				if(align == "right") {
					xpos_align_adjust = width - line.fragment_width;
				} else if(align == "center") {
					xpos_align_adjust = (width - line.fragment_width)/2;
				} else {
					ASSERT_LOG(align == "left", "Unrecognized alignment: " << align);
				}
			}

			for(const TextFragment& fragment : line.fragments) {
				cairo_text_fragment* res = new cairo_text_fragment;
				static const variant CmdStr("cmd");
				static const variant AreaStr("area");

				float fragment_baseline = baseline;
				if(fragment.valign.is_null() == false) {
					const std::string& valign = fragment.valign.as_string();
					if(valign == "top") {
						fragment_baseline = line_top + static_cast<float>(fragment.font_extents.ascent);
					} else if(valign == "bottom") {
						fragment_baseline = line_bottom - static_cast<float>(fragment.font_extents.descent);
					} else if(valign == "middle") {
						//average of top and bottom.
						fragment_baseline = (line_top + static_cast<float>(fragment.font_extents.ascent) + line_bottom - static_cast<float>(fragment.font_extents.descent))/2.0f;
					} else {
						ASSERT_LOG(valign == "baseline", "Unrecognized valignment: " << valign);
					}
				}

				res->tag = fragment.tag;
				res->info = fragment.info;
				res->color = fragment.color;
				res->has_image = fragment.svg.empty() == false || fragment.img.empty() == false;
				res->has_text = fragment.text.empty() == false;
				res->text = fragment.text;

				if(fragment.img.empty() == false) {
					res->translate_x = fragment.xpos;
					res->translate_y = fragment.ypos;
					res->width = fragment.width;
					res->height = fragment.height;
				} else if(fragment.svg.empty() == false) {
					res->translate_x = fragment.xpos;
					res->translate_y = fragment.ypos;
					res->width = fragment.font_extents.ascent + fragment.font_extents.descent;
					res->height = fragment.font_extents.ascent + fragment.font_extents.descent;
				} else {
					res->translate_x = fragment.xpos + xpos_align_adjust;
					res->translate_y = fragment.ypos + fragment_baseline;
				}

				res->img = fragment.img;
				res->svg = fragment.svg;
				res->font = fragment.font;
				res->font_size = fragment.font_size;
				res->img_src_area = fragment.img_src_area;

				res->x = fragment.xpos + xpos_align_adjust;
				res->y = fragment.ypos + fragment_baseline - static_cast<float>(fragment.font_extents.ascent);

				res->width = fragment.width;
				res->height = line_height*scale_line_heights;
				if(fragment.svg.empty() == false) {
					const float dim = fragment.font_extents.ascent + fragment.font_extents.descent;
					if(dim > res->width) {
						res->width = dim;
					}
					if(dim > res->height) {
						res->height = dim;
					}
				}
				res->x_advance = fragment.x_advance;
				res->ascent = static_cast<float>(fragment.font_extents.ascent);
				res->descent = static_cast<float>(fragment.font_extents.descent);

				result.push_back(variant(res));
			}

			width += width_delta*line_height*scale_line_heights;
		}

		return variant(&result);
	}

	std::string query_font_from_class(const variant& get_font_fn, const TextMarkupFragment& fragment)
	{
		static const variant WeightStr("weight");
		static const variant ItalicStr("italic");
		std::map<variant,variant> args;
		args[WeightStr] = variant(fragment.font_weight);
		if(fragment.font_italic) {
			args[ItalicStr] = variant::from_bool(true);
		}
		std::vector<variant> arg_list;
		arg_list.push_back(variant(&args));

		return get_font_fn(arg_list).as_string();
	}

	void parse_text_markup_impl(std::vector<TextMarkupFragment>& stack, std::vector<TextMarkupFragment>& output, const boost::property_tree::ptree& ptree, const variant& get_font_fn, const std::string& element, double scale, bool escape_special_chars)
	{
		static const std::string XMLText = "<xmltext>";
		static const std::string XMLAttr = "<xmlattr>";

		for(auto itor = ptree.begin(); itor != ptree.end(); ++itor) {

			const size_t start_size = stack.size();

			if(itor->first == XMLText) {
				if(itor->second.data().empty() == false) {

					if(stack.back().font.size() > 7 && std::equal(stack.back().font.begin(), stack.back().font.begin()+7, "bitmap:")) {
						std::string actual_font(stack.back().font.begin()+7, stack.back().font.end());
						auto font_info = GraphicalFont::get(actual_font);
						ASSERT_LOG(font_info, "Unknown graphical font: " << actual_font);

						stack.back().img = "images/" + font_info->texture_file();

						for(unsigned int cp : utils::utf8_to_codepoint(itor->second.data())) {
							const rect& area = font_info->get_codepoint_area(cp);
							if(area.w() > 0) {
								stack.back().img_src_area = &area;
								output.push_back(stack.back());
								output.back().text = "";
							}
						}

					} else {

						output.push_back(stack.back());
						if(escape_special_chars) {
							output.back().text = parse_special_chars_internal(itor->second.data());
						} else {
							output.back().text = itor->second.data();
						}
					}
				}

				continue;
			} else if(itor->first == XMLAttr) {
				if(element == "img") {
					output.push_back(stack.back());
				}

				for(auto a = itor->second.begin(); a != itor->second.end(); ++a) {
					const std::string& attr = a->first;
					const std::string& value = a->second.data();
					if(element == "font") {
						if(attr == "size") {
							ASSERT_LOG(value.empty() == false, "Invalid font size");

							bool is_percent = false;
							std::string valstr = value;
							if(valstr.size() > 2 && valstr[valstr.size()-1] == '%') {
								is_percent = true;
								valstr.resize(valstr.size()-1);
							}

							const int num_value = atoi(valstr.c_str());
							if(valstr[0] == '+' || valstr[0] == '-') {
								stack.back().font_size += is_percent ? (stack.back().font_size*num_value)/100 : num_value*scale;
							} else {
								stack.back().font_size = is_percent ? (stack.back().font_size*num_value)/100 : num_value*scale;
							}

						} else if(attr == "weight") {
							ASSERT_LOG(value.empty() == false, "Invalid fontweight");
							const int num_value = atoi(value.c_str());
							if(value[0] == '+' || value[0] == '-') {
								stack.back().font_weight += num_value;
							} else {
								stack.back().font_weight = num_value;
							}
						} else if(attr == "color") {
							stack.back().color = KRE::Color(value);

						} else if(attr == "tag") {
							std::vector<variant> v;
							if(stack.back().tag.is_list()) {
								v = stack.back().tag.as_list();
							}

							v.push_back(variant(value));
							stack.back().tag = variant(&v);
						} else if(attr == "info") {
							stack.back().info = game_logic::Formula(variant(value)).execute();
						} else if(attr == "align") {
							stack.back().align = variant(value);
						} else if(attr == "valign") {
							stack.back().valign = variant(value);
						}

					} else if(element == "img") {
						if(attr == "src" && value.size() > 4 && std::equal(value.end()-4,value.end(),".png")) {
							output.back().text = "";
							output.back().img = value;
						} else if(attr == "src") {
							output.back().text = "";
							output.back().svg = value;
						} else if(attr == "width") {
							output.back().img_w = static_cast<int>(atoi(value.c_str())*scale);
						} else if(attr == "height") {
							output.back().img_h = static_cast<int>(atoi(value.c_str())*scale);
						} else {
							ASSERT_LOG(false, "Unrecognized attribute in text markup " << element << "." << attr);
						}
					} else {
						ASSERT_LOG(false, "Unrecognized attribute in text markup " << element << "." << attr);
					}
				}

				if(element == "font") {
					stack.back().font = query_font_from_class(get_font_fn, stack.back());
				}

				continue;
			} else if(itor->first == "nobr") {
				stack.push_back(stack.back());
				stack.back().nobr = true;
			} else if(itor->first == "font") {
				stack.push_back(stack.back());
			} else if(itor->first == "root") {
			} else if(itor->first == "b") {
				stack.push_back(stack.back());
				stack.back().font_weight = 55;
				stack.back().font = query_font_from_class(get_font_fn, stack.back());
			} else if(itor->first == "i") {
				stack.push_back(stack.back());
				stack.back().font_italic = true;
				stack.back().font = query_font_from_class(get_font_fn, stack.back());
			} else if(itor->first == "img") {
			} else {
				ASSERT_LOG(false, "Unrecognized markup element: " << itor->first);
			}

			parse_text_markup_impl(stack, output, itor->second, get_font_fn, itor->first, scale, escape_special_chars);

			stack.resize(start_size);
		}
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(cairo_op)
		DEFINE_FIELD(dummy, "null")
			return variant();
	END_DEFINE_CALLABLE(cairo_op)

#define BEGIN_CAIRO_FN(a,b) BEGIN_DEFINE_FN(a,b "->builtin cairo_op")	\
	std::vector<variant> fn_args;										\
	for(int i = 0; i < NUM_FN_ARGS; ++i) {								\
		fn_args.push_back(FN_ARG(i));									\
	}																	\
	return variant(new cairo_op([](cairo_context& context, const std::vector<variant>& args) {

#define END_CAIRO_FN }, fn_args)); END_DEFINE_FN

	cairo_callable::cairo_callable()
	{}

	BEGIN_DEFINE_CALLABLE_NOBASE(cairo_callable)
	BEGIN_CAIRO_FN(save, "()")
		cairo_save(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(restore, "()")
		cairo_restore(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(translate, "(decimal,decimal)")
		cairo_translate(context.get(), args[0].as_decimal().as_float(), args[1].as_decimal().as_float());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(scale, "(decimal,decimal|null=null)")
		float x = args[0].as_float();
		float y = x;
		if(args.size() > 1 && args[1].is_null() == false) {
			y = args[1].as_float();
		}
		cairo_scale(context.get(), x, y);
	END_CAIRO_FN

	BEGIN_CAIRO_FN(rotate, "(decimal)")
		cairo_rotate(context.get(), args[0].as_decimal().as_float());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(draw_svg, "(string, decimal, decimal)")
		context.render_svg(args[0].as_string(), args[1].as_int(), args[2].as_int());
	END_CAIRO_FN

	BEGIN_DEFINE_FN(render, "(int, int, cairo_commands, map|null=null) ->builtin texture_object")
		game_logic::Formula::failIfStaticContext();
		formula_profiler::Instrument instrument("CAIRO_CALC");

		int w = FN_ARG(0).as_int();
		int h = FN_ARG(1).as_int();
		ASSERT_LOG(w >= 0 && h >= 0 && w <= 8192 && h <= 8192, "Invalid canvas render: " << w << "x" << h);
		if(w&1) {
			++w;
		}

		if(h&1) {
			++h;
		}

		if(w < 2) {
			w = 2;
		}

		if(h < 2) {
			h = 2;
		}
		cairo_context context(w, h);

		variant ops = FN_ARG(2);

		formula_profiler::Instrument instrument2("CAIRO_RENDER");
		execute_cairo_ops(context, ops);

		formula_profiler::Instrument instrument3("CAIRO_CREATE_TEX");

		return variant(new TextureObject(context.write(NUM_FN_ARGS > 3 ? FN_ARG(3) : variant())));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(render_async, "(int, int, cairo_commands, map|null=null) ->builtin texture_object_future")
		formula_profiler::Instrument instrument("CAIRO_RENDER_ASYNC_SETUP");
		game_logic::Formula::failIfStaticContext();
		int w = FN_ARG(0).as_int();
		int h = FN_ARG(1).as_int();
		ASSERT_LOG(w >= 0 && h >= 0 && w <= 8192 && h <= 8192, "Invalid canvas render: " << w << "x" << h);
		if(w&1) {
			++w;
		}

		if(h&1) {
			++h;
		}

		if(w < 2) {
			w = 2;
		}

		if(h < 2) {
			h = 2;
		}

		return variant(new TextureObjectFuture(w, h, FN_ARG(2), NUM_FN_ARGS > 3 ? FN_ARG(3) : variant())); 

	END_DEFINE_FN

	BEGIN_CAIRO_FN(new_path, "()")
		cairo_new_path(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(new_sub_path, "()")
		cairo_new_sub_path(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(close_path, "()")
		cairo_close_path(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(clip, "()")
		cairo_clip(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(reset_clip, "()")
		cairo_reset_clip(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(rectangle, "(decimal, decimal, decimal, decimal)")
		cairo_rectangle(context.get(),
						args[0].as_decimal().as_float(),
						args[1].as_decimal().as_float(),
						args[2].as_decimal().as_float(),
						args[3].as_decimal().as_float());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(arc, "(decimal, decimal, decimal, decimal, decimal)")
		cairo_arc(context.get(),
						args[0].as_decimal().as_float(),
						args[1].as_decimal().as_float(),
						args[2].as_decimal().as_float(),
						args[3].as_decimal().as_float(),
						args[4].as_decimal().as_float());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(arc_negative, "(decimal, decimal, decimal, decimal, decimal)")
		cairo_arc_negative(context.get(),
						args[0].as_decimal().as_float(),
						args[1].as_decimal().as_float(),
						args[2].as_decimal().as_float(),
						args[3].as_decimal().as_float(),
						args[4].as_decimal().as_float());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(curve_to, "(decimal, decimal, decimal, decimal, decimal, decimal)")
		cairo_curve_to(context.get(),
						args[0].as_decimal().as_float(),
						args[1].as_decimal().as_float(),
						args[2].as_decimal().as_float(),
						args[3].as_decimal().as_float(),
						args[4].as_decimal().as_float(),
						args[5].as_decimal().as_float());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(line_to, "(decimal, decimal)")
		cairo_line_to(context.get(),
					  args[0].as_decimal().as_float(),
					  args[1].as_decimal().as_float());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(move_to, "(decimal, decimal)")
		cairo_move_to(context.get(),
					  args[0].as_decimal().as_float(),
					  args[1].as_decimal().as_float());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(set_source_color, "(" + KRE::Color::getSetFieldType() + ")")
   		using namespace KRE;
    	Color color(args[0], DecodingHint::DECIMAL);
    	cairo_set_source_rgba(context.get(), color.r(), color.g(), color.b(), color.a());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(set_source_rgba, "(decimal, decimal, decimal, decimal=1.0)")
		cairo_set_source_rgba(context.get(),
						args[0].as_decimal().as_float(),
						args[1].as_decimal().as_float(),
						args[2].as_decimal().as_float(),
						args.size() > 3 ? args[3].as_decimal().as_float() : 1.0);
	END_CAIRO_FN

	BEGIN_CAIRO_FN(set_line_width, "(decimal)")
		cairo_set_line_width(context.get(), args[0].as_decimal().as_float());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(set_line_dashes, "([decimal], decimal=0.0)")
		std::vector<decimal> dashes = args[0].as_list_decimal();
		double offset = args.size() > 1 ? args[1].as_decimal().as_float() : 0.0;

		std::vector<double> dashes_double;
		dashes_double.reserve(dashes.size());
		for(decimal d : dashes) {
			dashes_double.push_back(d.as_float());
		}

		if(dashes_double.empty()) {
			cairo_set_dash(context.get(), nullptr, 0, 0.0);
		} else {
			cairo_set_dash(context.get(), &dashes_double[0], static_cast<int>(dashes_double.size()), offset);
		}

	END_CAIRO_FN

	BEGIN_CAIRO_FN(set_line_cap, "(enum{butt,round,square})")
		auto cap = CAIRO_LINE_CAP_BUTT;

		std::string s = args[0].as_enum();

		if(s == "butt") {
			cap = CAIRO_LINE_CAP_BUTT;
		} else if(s == "round") {
			cap = CAIRO_LINE_CAP_ROUND;
		} else if(s == "square") {
			cap = CAIRO_LINE_CAP_SQUARE;
		}
		
		cairo_set_line_cap(context.get(), cap);

	END_CAIRO_FN

	BEGIN_CAIRO_FN(fill, "()")
		cairo_fill(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(stroke, "()")
		cairo_stroke(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(set_linear_pattern, "(decimal, decimal, decimal, decimal, [{offset: decimal, red: decimal, green: decimal, blue: decimal, alpha: decimal|null}])")
		cairo_pattern_t* pattern = cairo_pattern_create_linear(
			args[0].as_decimal().as_float(),
			args[1].as_decimal().as_float(),
			args[2].as_decimal().as_float(),
			args[3].as_decimal().as_float());

		for(variant v : args[4].as_list()) {

			float alpha = 1.0;
			variant a = v["alpha"];
			if(a.is_numeric()) {
				alpha = a.as_float();
			}

			cairo_pattern_add_color_stop_rgba(pattern,
			  v["offset"].as_decimal().as_float(),
			  v["red"].as_decimal().as_float(),
			  v["green"].as_decimal().as_float(),
			  v["blue"].as_decimal().as_float(),
			  alpha);
		}

		context.set_pattern(pattern);
	END_CAIRO_FN

	BEGIN_CAIRO_FN(set_radial_pattern, "(decimal, decimal, decimal, decimal, decimal, decimal, [{offset: decimal, red: decimal, green: decimal, blue: decimal, alpha: decimal|null}])")
		cairo_pattern_t* pattern = cairo_pattern_create_radial(
			args[0].as_decimal().as_float(),
			args[1].as_decimal().as_float(),
			args[2].as_decimal().as_float(),
			args[3].as_decimal().as_float(),
			args[4].as_decimal().as_float(),
			args[5].as_decimal().as_float());

		for(variant v : args[6].as_list()) {

			float alpha = 1.0;
			variant a = v["alpha"];
			if(a.is_decimal()) {
				alpha = a.as_float();
			}

			cairo_pattern_add_color_stop_rgba(pattern,
			  v["offset"].as_decimal().as_float(),
			  v["red"].as_decimal().as_float(),
			  v["green"].as_decimal().as_float(),
			  v["blue"].as_decimal().as_float(),
			  alpha);
		}

		context.set_pattern(pattern);
	END_CAIRO_FN

	BEGIN_CAIRO_FN(set_font, "(string)")
		FT_Face face = get_ft_font(args[0].as_string());
		cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
		cairo_set_font_face(context.get(), cairo_face);
	END_CAIRO_FN

	BEGIN_CAIRO_FN(set_font_size, "(decimal)")
		cairo_set_font_size(context.get(), args[0].as_decimal().as_float());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(show_text, "(string)")
		cairo_show_text(context.get(), args[0].as_string().c_str());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(show_rich_text_multiline, "(string, decimal, decimal, {scaling: decimal|null, svg_scale: decimal, svg_width: decimal})")
		std::string all_text = args[0].as_string();
		float svg_width = args[3]["svg_width"].as_float();
		float svg_scale = args[3]["svg_scale"].as_float();
		float scaling = 1.0f;
		if(args[3]["scaling"].is_decimal()) {
			scaling = args[3]["scaling"].as_float();
		}

		float line_height = 0;

		std::vector<std::string> split_text = util::split(all_text, '\n');

		std::vector<std::vector<std::string>> lines;

		for(std::string text : split_text) {
			lines.push_back(std::vector<std::string>());

			float width  = args[1].as_float();
			float height = args[2].as_float();

			for(auto itor = std::find(text.begin(), text.end(), '&'); itor != text.end();
					 itor = std::find(text.begin(), text.end(), '&')) {
				auto end_itor = std::find(itor, text.end(), ';');
				ASSERT_LOG(end_itor != text.end(), "Could not find end of & in ((" << args[0].as_string() << ")) -- & at ((" << std::string(itor, text.end()) << "))");
	
				if(text.begin() != itor) {
					lines.back().push_back(std::string(text.begin(), itor));
				}
				lines.back().push_back(std::string(itor, end_itor + 1));

				text.erase(text.begin(), end_itor + 1);
			}

			if(!text.empty()) {
				lines.back().push_back(text);
			}
	
			for(;;) {
				std::vector<float> lengths;
				float total_length = 0.0;
				float length_at_breaking_point = 0.0;
				int breaking_point = -1;
				int index = 0;
				for(auto s : lines.back()) {
					if(s[0] == '&') {
						lengths.push_back(svg_width);
					} else {
						cairo_text_extents_t extents;
						cairo_text_extents(context.get(), s.c_str(), &extents);
						lengths.push_back(static_cast<float>(extents.x_advance));
						if(extents.height > line_height) {
							line_height = static_cast<float>(extents.height);
						}
					}

					total_length += lengths.back();
					if(breaking_point == -1 && total_length > width) {
						breaking_point = index;
						length_at_breaking_point = total_length - lengths.back();
					}

					++index;
				}

				if(breaking_point == -1) {
					break;
				}

				std::string extra_text;
				const std::string& line = lines.back()[breaking_point];
				if(line[0] != '&') {
					for(;;) {
						auto itor = std::find(line.begin() + extra_text.size(), line.end(), ' ');
						if(itor == line.end()) {
							break;
						}

						std::string new_text(line.begin(), itor);

						cairo_text_extents_t extents;
						cairo_text_extents(context.get(), new_text.c_str(), &extents);
						if(length_at_breaking_point + extents.x_advance > width) {
							break;
						}

						extra_text = new_text + " ";
					}
				}

				ASSERT_LOG(!extra_text.empty() || breaking_point > 0, "Could not render text due to it being too large for the area: " << args[0].as_string());

				std::vector<std::string> new_line(lines.back().begin() + breaking_point, lines.back().end());
				lines.back().erase(lines.back().begin() + breaking_point, lines.back().end());
				if(extra_text.empty() == false) {
					new_line.front().erase(new_line.front().begin(), new_line.front().begin() + extra_text.size());
					lines.back().push_back(extra_text);
				}

				lines.push_back(new_line);
			}
		}

		cairo_save(context.get()),

		cairo_scale(context.get(), scaling, scaling);

		float ypos = 0;
		for(const auto& line : lines) {
			cairo_save(context.get());
			cairo_translate(context.get(), 0, ypos);

			for(const auto& str : line) {
				if(str[0] == '&') {
					cairo_save(context.get());
					cairo_translate(context.get(), 0.0, -line_height);
					cairo_scale(context.get(), svg_scale, svg_scale);
				std::string svg_name(str.begin()+1, str.end()-1);
				ASSERT_LOG(svg_name.size() > 4 && std::equal(svg_name.end()-4, svg_name.end(), ".svg"), "Unknown markup found: " << all_text);

				context.render_svg(svg_name, static_cast<int>(context.width()), static_cast<int>(context.height()));
					cairo_restore(context.get());
					cairo_translate(context.get(), svg_width, 0.0);
				} else {
					cairo_text_extents_t extents;
					cairo_text_extents(context.get(), str.c_str(), &extents);
					cairo_new_path(context.get());
					cairo_show_text(context.get(), str.c_str());

					cairo_translate(context.get(), extents.x_advance, 0.0);
				}
			}

			cairo_restore(context.get());
			ypos += line_height*1.1f;
		}

		cairo_restore(context.get());

	END_CAIRO_FN

	BEGIN_CAIRO_FN(text_path_in_bounds, "(string, decimal, [string])")
		std::string text = args[0].as_string();
		float size = args[1].as_float();

		bool right = false, center = false, top = false;
		bool shrink = true;

		if(args.size() > 2) {
			for(const auto& flag : args[2].as_list_string()) {
				if(flag == "left") {
					right = center = false;
				} else if(flag == "right") {
					right = true;
				} else if(flag == "center") {
					center = true;
				} else if(flag == "top") {
					top = true;
				} else if(flag == "shrink") {
					shrink = true;
				} else if(flag == "truncate") {
					shrink = false;
				}
			}
		}

		cairo_text_extents_t extents;
		cairo_text_extents(context.get(), text.c_str(), &extents);

		if(extents.width > size) {
			if(shrink) {
				size *= static_cast<float>(size)/static_cast<float>(extents.width);
			} else {
				const int forced_len = static_cast<int>(text.size()*static_cast<float>(size)/static_cast<float>(extents.width));
				if(static_cast<unsigned>(forced_len) < text.size()) {
					text.resize(forced_len);
					for(int i = 0; i < 3 && i < forced_len; ++i) {
						text[text.size()-i-1] = '.';
					}
				}
			}

			cairo_text_extents(context.get(), text.c_str(), &extents);
		}


		cairo_save(context.get());

		if(right) {
			cairo_translate(context.get(), size - extents.width, 0.0);
		} else if(center) {
			cairo_translate(context.get(), (size - extents.width)/2.0, 0.0);
		}

		if(top) {
			cairo_translate(context.get(), 0.0, extents.height);
		}

		cairo_text_path(context.get(), text.c_str());

		cairo_restore(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(text_path, "(string)")
		std::string text = args[0].as_string();
		std::vector<std::string> lines = util::split(text, '\n');

	cairo_text_extents_t basic_extents;
	cairo_text_extents(context.get(), "a", &basic_extents);

	double xpos = 0.0;
	cairo_get_current_point(context.get(), &xpos, nullptr);
	for(int n = 0; n != lines.size(); ++n) {
		cairo_text_path(context.get(), lines[n].c_str());
		if(n+1 != lines.size()) {
			double ypos = 0.0;
			cairo_get_current_point(context.get(), nullptr, &ypos);

			ypos += basic_extents.height*2;
			cairo_move_to(context.get(), xpos, ypos);
		}
	}
	END_CAIRO_FN

	BEGIN_CAIRO_FN(paint_image, "(string, [decimal,decimal]|null=null, [enum { mask, filter_best }]|null=null)")

		bool use_mask = false;
		bool use_best_filter = false;
		if(args.size() > 2) {
			variant flags = args[2];
			if(flags.is_list()) {
				for(variant f : flags.as_list()) {
					const std::string& str = f.as_enum();
					if(str == "mask") {
						use_mask = true;
					} else if(str == "filter_best") {
						use_best_filter = true;
					} else {
						ASSERT_LOG(false, "Unknown flag");
					}
				}
			}
		}

		cairo_status_t status_before = cairo_status(context.get());
		ASSERT_LOG(status_before == 0, "rendering error before painting " << args[0].as_string() << ": " << cairo_status_to_string(status_before));
		cairo_surface_t* surface = get_cairo_image(args[0].as_string());
		cairo_status_t status = cairo_status(context.get());
		ASSERT_LOG(status == 0, "rendering error painting " << args[0].as_string() << ": " << cairo_status_to_string(status));
		double translate_x = 0, translate_y = 0;
		if(args.size() > 1) {
			variant pos = args[1];
			if(pos.is_list()) {
				translate_x = pos[0].as_decimal().as_float();
				translate_y = pos[1].as_decimal().as_float();
			}
		}

		cairo_pattern_t* old_pattern = nullptr;
		bool own_old_pattern = false;
		if(use_mask) {
			old_pattern = context.get_pattern_ownership(&own_old_pattern);
		}

		cairo_set_source_surface(context.get(), surface, translate_x, translate_y);
		cairo_pattern_set_filter(cairo_get_source(context.get()), use_best_filter ? CAIRO_FILTER_BEST : CAIRO_FILTER_BILINEAR);

		status = cairo_status(context.get());
		ASSERT_LOG(status == 0, "rendering error painting " << args[0].as_string() << ": " << cairo_status_to_string(status));

		if(old_pattern != nullptr) {
			cairo_mask(context.get(), old_pattern);
		} else {
			cairo_paint(context.get());
		}

		if(own_old_pattern) {
			cairo_pattern_destroy(old_pattern);
		}

		status = cairo_status(context.get());
		ASSERT_LOG(status == 0, "rendering error painting " << args[0].as_string() << ": " << cairo_status_to_string(status));

	END_CAIRO_FN

	BEGIN_CAIRO_FN(push_group, "()")
		cairo_push_group(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(pop_group_to_source, "()")
		cairo_pop_group_to_source(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(paint, "()")
		cairo_paint(context.get());
	END_CAIRO_FN

	BEGIN_CAIRO_FN(paint_with_alpha, "(decimal)")
		double alpha = args[0].as_decimal().as_float();
		cairo_paint_with_alpha(context.get(), alpha);
	END_CAIRO_FN

	BEGIN_CAIRO_FN(set_operator, "(string)")
		std::string arg = args[0].as_string();
		cairo_operator_t op = CAIRO_OPERATOR_CLEAR;
		bool found = false;

#define OP_STR(s) } else if(arg == #s) { op = CAIRO_OPERATOR_##s; found = true;

		if(false) {
			OP_STR(CLEAR)
			OP_STR(SOURCE)
			OP_STR(OVER)
			OP_STR(IN)
			OP_STR(OUT)
			OP_STR(ATOP)
			OP_STR(DEST)
			OP_STR(DEST_OVER)
			OP_STR(DEST_IN)
			OP_STR(DEST_OUT)
			OP_STR(DEST_ATOP)
			OP_STR(XOR)
			OP_STR(ADD)
			OP_STR(SATURATE)
			OP_STR(MULTIPLY)
			OP_STR(SCREEN)
			OP_STR(OVERLAY)
			OP_STR(DARKEN)
			OP_STR(DARKEN)
			OP_STR(LIGHTEN)
			OP_STR(COLOR_DODGE)
			OP_STR(COLOR_BURN)
			OP_STR(HARD_LIGHT)
			OP_STR(SOFT_LIGHT)
			OP_STR(DIFFERENCE)
			OP_STR(EXCLUSION)
			OP_STR(HSL_HUE)
			OP_STR(HSL_SATURATION)
			OP_STR(HSL_COLOR)
			OP_STR(HSL_LUMINOSITY)
		}

		ASSERT_LOG(found, "Illegal draw operator: " << arg);

		cairo_set_operator(context.get(), op);

	END_CAIRO_FN

	BEGIN_DEFINE_FN(escape_text, "(string) ->string")
		std::string text = FN_ARG(0).as_string();

		std::string out;

		for(char c : text) {
			if(c == '&') {
				out += "&amp;";
			} else if(c == '<') {
				out += "&lt;";
			} else if(c == '>') {
				out += "&gt;";
			} else {
				out.push_back(c);
			}
		}

		return variant(out);
	END_DEFINE_FN

	BEGIN_DEFINE_FN(is_valid_markup_text, "(string) ->bool")
		const std::string markup = "<root>" + FN_ARG(0).as_string() + "</root>";
		std::istringstream s(markup);
		boost::property_tree::ptree ptree;
		try {
			boost::property_tree::xml_parser::read_xml(s, ptree, boost::property_tree::xml_parser::no_concat_text);
		} catch(...) {
			return variant::from_bool(false);
		}
		return variant::from_bool(true);

	END_DEFINE_FN

	BEGIN_DEFINE_FN(markup_text, "(string, decimal|{width: decimal, width_delta: null|decimal, scale: null|decimal, scale_line_heights: null|decimal, no_escaping: null|bool}, decimal=0.0) ->[builtin cairo_text_fragment]")
		const std::string markup = "<root>" + FN_ARG(0).as_string() + "</root>";

		variant params = FN_ARG(1);


		float scale_line_heights = 1.0;
		double scale = 1.0;
		float width = params.is_map() ? params["width"].as_float() : params.as_float();
		float width_delta = 0.0;
		bool escape_special_chars = true;
		if(NUM_FN_ARGS > 2) {
			width_delta = FN_ARG(2).as_float();
		}

		if(params.is_map()) {
			if(params["width_delta"].is_decimal()) {
				width_delta = params["width_delta"].as_float();
			}

			if(params["scale"].is_decimal()) {
				scale = params["scale"].as_float();
			}
			
			if(params["scale_line_heights"].is_decimal()) {
				scale_line_heights = params["scale_line_heights"].as_float();
			}

			if(params["no_escaping"].as_bool(false)) {
				escape_special_chars = false;
			}
		}

		std::istringstream s(markup);
		boost::property_tree::ptree ptree;
		try {
			boost::property_tree::xml_parser::read_xml(s, ptree, boost::property_tree::xml_parser::no_concat_text);
		} catch(...) {
			ASSERT_LOG(false, "Error parsing XML: " << markup);
		}

		game_logic::FormulaCallablePtr lib_obj = game_logic::get_library_object();
		static game_logic::ConstFormulaCallablePtr font_lib(lib_obj->queryValue("font").as_callable());
		ASSERT_LOG(font_lib.get(), "Could not find font class needed to call markup_text");

		static const variant get_font_fn = font_lib->queryValue("get_font");
		ASSERT_LOG(get_font_fn.is_null() == false, "Could not find get() function in font lib");

		std::vector<TextMarkupFragment> stack(1);
		stack.back().font_size = 14*scale;
		stack.back().font_weight = 35;
		stack.back().font_italic = false;

		static std::string starting_font;
		if(starting_font.empty()) {
			std::map<variant,variant> args;
			args[variant("weight")] = variant(stack.back().font_weight);
			std::vector<variant> arg_list;
			arg_list.push_back(variant(&args));

			starting_font = get_font_fn(arg_list).as_string();
		}

		stack.back().font = starting_font;

		std::vector<TextMarkupFragment> fragments;

		parse_text_markup_impl(stack, fragments, ptree, get_font_fn, "", scale, escape_special_chars);

		if(fragments.empty() == false) {
			return layout_text_impl(&fragments[0], &fragments[0] + fragments.size(), width, width_delta, scale_line_heights);
		} else {
			std::vector<variant> result;
			return variant(&result);
		}
	END_DEFINE_FN

	BEGIN_DEFINE_FN(layout_text, "([{text: string, font: string, size: int, tag: null|any, align: string|null, valign: string|null, svg: string|null}], decimal) ->[builtin cairo_text_fragment]")
		variant info = FN_ARG(0);
		float width = FN_ARG(1).as_float();

		const std::vector<variant>& items = info.as_list();

		std::vector<TextMarkupFragment> fragments;
		fragments.resize(items.size());
		for(int i = 0; i != items.size(); ++i) {
			const variant& item = items[i];
			TextMarkupFragment& fragment = fragments[i];

			static const variant TagStr("tag");
			static const variant TextStr("text");
			static const variant FontStr("font");
			static const variant SizeStr("size");
			static const variant AlignStr("align");
			static const variant VAlignStr("valign");
			static const variant SvgStr("svg");

			const std::map<variant,variant>& items = item.as_map();
			fragment.text = items.find(TextStr)->second.as_string();
			fragment.font = items.find(FontStr)->second.as_string();
			fragment.font_size = items.find(SizeStr)->second.as_int();

			if(items.count(TagStr)) {
				fragment.tag = items.find(TagStr)->second;
			}

			if(items.count(AlignStr)) {
				fragment.align = items.find(AlignStr)->second;
			}
		
			if(items.count(VAlignStr)) {
				fragment.valign = items.find(VAlignStr)->second;
			}

			if(items.count(SvgStr)) {
				fragment.svg = items.find(SvgStr)->second.as_string();
			}
		}

		if(fragments.empty()) {
			std::vector<variant> result;
			return variant(&result);
		}

		return layout_text_impl(&fragments[0], &fragments[0] + fragments.size(), width);
	END_DEFINE_FN

	BEGIN_DEFINE_FN(image_dim, "(string) ->[int,int]")
		cairo_surface_t* surface = get_cairo_image(FN_ARG(0).as_string());
		std::vector<variant> result;
		result.push_back(variant(cairo_image_surface_get_width(surface)));
		result.push_back(variant(cairo_image_surface_get_height(surface)));

		return variant(&result);
	END_DEFINE_FN

	BEGIN_DEFINE_FN(precache_image, "(string) ->commands")
		const std::string img = FN_ARG(0).as_string();
		if(has_cairo_image(img)) {
			return variant();
		}

		return variant(new game_logic::FnCommandCallable("precache_image", [=]() {
			std::async(std::launch::async, [=]() {
				get_cairo_image(img);
			});
		}));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(font_extents, "(string, decimal) -> {ascent: decimal, descent: decimal, height: decimal, max_x_advance: decimal, max_y_advance: decimal}")
		static cairo_context& context = *new cairo_context(8,8);
		FT_Face face = get_ft_font(FN_ARG(0).as_string());
		cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
		cairo_set_font_face(context.get(), cairo_face);
		cairo_set_font_size(context.get(), FN_ARG(1).as_decimal().as_float());

		cairo_font_extents_t extents;
		cairo_font_extents(context.get(), &extents);

		std::map<variant,variant> result;
		result[variant("ascent")] = variant(extents.ascent);
		result[variant("descent")] = variant(extents.descent);
		result[variant("height")] = variant(extents.height);
		result[variant("max_x_advance")] = variant(extents.max_x_advance);
		result[variant("max_y_advance")] = variant(extents.max_y_advance);

		return variant(&result);

	END_DEFINE_FN

	BEGIN_DEFINE_FN(text_extents, "(string, decimal, string) -> { width: decimal, height: decimal, x_bearing: decimal, y_bearing: decimal }")
		static cairo_context& context = *new cairo_context(8,8);

		FT_Face face = get_ft_font(FN_ARG(0).as_string());
		cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
		cairo_set_font_face(context.get(), cairo_face);

		cairo_set_font_size(context.get(), FN_ARG(1).as_decimal().as_float());

		std::string text = FN_ARG(2).as_string();
		std::vector<std::string> lines = util::split(text, '\n');

		double width = 0.0, height = 0.0, x_bearing = 0.0, y_bearing = 0.0;

		for(const std::string& line : lines) {
			cairo_text_extents_t extents;
			cairo_text_extents(context.get(), line.c_str(), &extents);
			height += extents.height;
			x_bearing = extents.x_bearing;
			y_bearing = extents.y_bearing;
			if(extents.x_advance > width) {
				width = extents.x_advance;
			}
		}

		std::map<variant,variant> result;
		result[variant("width")] = variant(width);
		result[variant("height")] = variant(height);
		result[variant("x_bearing")] = variant(x_bearing);
		result[variant("y_bearing")] = variant(y_bearing);
		return variant(&result);
	END_DEFINE_FN

	BEGIN_DEFINE_FN(parse_special_chars, "(string) -> string")
		std::string s = FN_ARG(0).as_string();
		return variant(parse_special_chars_internal(s));
	END_DEFINE_FN

	BEGIN_DEFINE_FN(utf8_to_markup, "(string) -> string")
		std::string s = FN_ARG(0).as_string();
		return variant(utf8_to_markup(s));

	END_DEFINE_FN

	//a string representing an emdash in utf-8.
	DEFINE_FIELD(emdash, "string")
		const std::string str = utils::codepoint_to_utf8(8212);
		return variant(str);
	END_DEFINE_CALLABLE(cairo_callable)

	namespace 
	{
		std::string fix_svg_path(const std::string& path)
		{
			std::vector<std::string> tokens;
			const char* i1 = path.c_str();
			const char* i2 = i1 + path.size();
			while(i1 != i2) {
				if(util::c_isalpha(*i1)) {
					tokens.push_back(std::string(i1, i1+1));
					++i1;
				} else if(util::c_isspace(*i1) || *i1 == ',') {
					++i1;
				} else {
					char* end = nullptr;
					double d = strtod(i1, &end);
					ASSERT_LOG(end != i1, "Could not parse svg path: " << i1);

					std::ostringstream s;
					s << d;
					tokens.push_back(s.str());

					//tokens.push_back(std::string(i1, static_cast<const char*>(end)));
					i1 = end;
				}
			}

			std::string out;
			for(const std::string& tok : tokens) {
				if(!out.empty()) {
					out += " ";
				}

				out += tok;
			}

			return out;
		}
	}

	COMMAND_LINE_UTILITY(fix_svg)
	{
		for(auto fname : args) {
			std::string contents = sys::read_file(fname);
			std::string output;

			const char* begin = contents.c_str();
			const char* ptr = strstr(begin, "<path d=\"");
			while(ptr != nullptr) {
				ptr += 9;
				output += std::string(begin, ptr);

				const char* end = strstr(ptr, "\"");
				ASSERT_LOG(end != nullptr, "Unexpected end of file: " << ptr);

				output += fix_svg_path(std::string(ptr, end));

				begin = end;
				ptr = strstr(begin, "<path d=\"");
			}

			output += std::string(begin, contents.c_str() + contents.size());
			sys::write_file(fname, output);
		}
	}

	namespace cairo_font
	{
		KRE::TexturePtr render_text_uncached(const std::string& text, const KRE::Color& color, int size, const std::string& font_name)
		{
			FT_Face face = get_ft_font(font_name);
			cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
			cairo_set_font_face(dummy_context().get(), cairo_face);
			cairo_set_font_size(dummy_context().get(), size);

			std::vector<float> lengths, heights;
			float width = 0.0, height = 0.0;
			std::vector<std::string> lines = util::split(text, '\n');
			for(auto s : lines) {
				cairo_text_extents_t extents;
				cairo_text_extents(dummy_context().get(), s.c_str(), &extents);
				lengths.push_back(static_cast<float>(extents.width));
				heights.push_back(static_cast<float>(extents.height));
				if(static_cast<float>(extents.width) > width) {
					width = static_cast<float>(extents.width);
				}

				height += static_cast<float>(extents.height);
			}

			cairo_context context(int(width+1), int(height+1));

			{
				cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
				cairo_set_font_face(context.get(), cairo_face);
				cairo_set_font_size(context.get(), size);
			}

			for(int n = 0; n != lines.size(); ++n) {
				const std::string& line = lines[n];
				cairo_translate(context.get(), /*lengths[n]/2*/ 0, heights[n]);

				cairo_new_path(context.get());
				cairo_show_text(context.get(), line.c_str());

				cairo_translate(context.get(), 0.0, heights[n]);
			}


			return context.write(variant());
		}
	}

	CairoCacheStatus get_cairo_image_cache_status()
	{
		CairoCacheStatus status;

		status.num_items = static_cast<int>(g_surface_cache.size());
		status.memory_usage = static_cast<int>(g_surface_cache.usage());
		return status;
	}
}

namespace 
{
	void handle_node(const boost::property_tree::ptree& ptree, int depth)
	{
		for(auto itor = ptree.begin(); itor != ptree.end(); ++itor) {
			for(int n = 0; n != depth; ++n) { std::cerr << "  "; }
			std::cerr << "XML: " << itor->first << " -> " << itor->second.data() << "\n";
			handle_node(itor->second, depth+1);
			for(int n = 0; n != depth; ++n) { std::cerr << "  "; }
			std::cerr << "DONE: " << itor->first << "\n";
		}
	}
}

COMMAND_LINE_UTILITY(test_xml)
{
	std::string test_doc = "<root>Some text<bold attr='blah' attr2='bleh'>more text</bold>yet more text</root>";
	std::istringstream s(test_doc);
	boost::property_tree::ptree ptree;
	boost::property_tree::xml_parser::read_xml(s, ptree, boost::property_tree::xml_parser::no_concat_text);

	handle_node(ptree, 0);
}

COMMAND_LINE_UTILITY(cairo_version)
{
	LOG_INFO ("Cairo version: " << cairo_version_string());
}
