#include <map>

#include <string.h>
#include <sstream>

#include <cairo-ft.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "svg/svg_parse.hpp"

#include "IMG_savepng.h"
#include "asserts.hpp"
#include "fbo_scene.hpp"
#include "cairo.hpp"
#include "color_utils.hpp"
#include "filesystem.hpp"
#include "formula_object.hpp"
#include "module.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"

namespace graphics
{

namespace {

struct TextFragment {
	float xpos, ypos, width, height;
	std::string font;
	int font_size;
	variant tag;
	graphics::color color;

	std::string text;
	variant valign;

	cairo_font_extents_t font_extents;

	std::string svg;
};

struct LineOfText {
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
			}
		}
	}

	auto itor = paths.find(name);
	ASSERT_LOG(itor != paths.end(), "Could not find font: " << name);
	return itor->second;
}

FT_Face get_ft_font(const std::string& ttf_name, int index=0)
{
	if(ttf_name.size() > 4 && std::equal(ttf_name.end()-4, ttf_name.end(), ".otf")) {
		return get_ft_font(std::string(ttf_name.begin(), ttf_name.end()-4), index);
	}
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

cairo_surface_t* get_cairo_image(const std::string& image)
{
	static std::map<std::string, cairo_surface_t*> cache;

	cairo_surface_t*& result = cache[image];
	if(result == NULL) {
		result = cairo_image_surface_create_from_png(module::map_file(image).c_str());
		ASSERT_LOG(result, "Could not load cairo image: " << image);
	}

	return result;
}

cairo_context& dummy_context() {
	static cairo_context* res = new cairo_context(8, 8);
	return *res;
}

struct TextMarkupFragment
{
	TextMarkupFragment() : color(255,255,255,255) {}
	std::string text;
	std::string font;
	int font_size;
	int font_weight;
	bool font_italic;
	variant tag;
	variant align, valign;
	std::string svg;

	graphics::color color;
};

}

cairo_context::cairo_context(int w, int h)
  : width_(w), height_(h), temp_pattern_(NULL)
{
	surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_ = cairo_create(surface_);

	cairo_font_options_t* options = cairo_font_options_create();
	cairo_get_font_options(cairo_, options);
	cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);
	cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_OFF);
	cairo_set_font_options(cairo_, options);
	cairo_font_options_destroy(options);
}

cairo_context::~cairo_context()
{
	if(temp_pattern_) {
		cairo_pattern_destroy(temp_pattern_);
		temp_pattern_ = NULL;
	}

	cairo_destroy(cairo_);
	cairo_surface_destroy(surface_);
}

cairo_t* cairo_context::get() const
{
	return cairo_;
}

surface cairo_context::get_surface() const
{
	unsigned char* data = cairo_image_surface_get_data(surface_);
	surface result(SDL_CreateRGBSurface(0, width_, height_, 32, SURFACE_MASK));
	memcpy(result->pixels, data, width_*height_*4);
	unsigned char* dst = reinterpret_cast<unsigned char*>(result->pixels);

	//cairo uses pre-multiplied alpha while GL uses non-multiplied, so
	//correct that here. The channels also have to be reordered.
	for(int i = 0; i != width_*height_; ++i) {
		if(data[3] != 0 && data[3] != 255) {
			dst[0] = std::min<int>(255, (data[2]*255)/data[3]);
			dst[1] = std::min<int>(255, (data[1]*255)/data[3]);
			dst[2] = std::min<int>(255, (data[0]*255)/data[3]);
		} else {
			dst[0] = data[2];
			dst[1] = data[1];
			dst[2] = data[0];
		}
		dst[3] = data[3];
		data += 4;
		dst += 4;
	}

	return result;
}

graphics::texture cairo_context::write() const
{
	std::vector<surface> s;
	s.push_back(get_surface());
	return graphics::texture(s);
}

void cairo_context::render_svg(const std::string& fname, int w, int h)
{
	{
		cairo_status_t status = cairo_status(cairo_);
		ASSERT_LOG(status == 0, "SVG rendering error rendering " << fname << ": " << cairo_status_to_string(status));
	}

	std::shared_ptr<KRE::SVG::parse> handle;
	static std::map<std::string, std::shared_ptr<KRE::SVG::parse>> cache;

	auto itor = cache.find(fname);
	if(itor == cache.end()) {
		std::string real_fname = module::map_file(fname);
		ASSERT_LOG(sys::file_exists(real_fname), "Could not find svg file: " << fname);

		handle = std::shared_ptr<KRE::SVG::parse>(new KRE::SVG::parse(real_fname));		
		cache[fname] = handle;
	} else {
		handle = itor->second;
	}

	KRE::SVG::render_context ctx(cairo_, w, h);
	handle->render(ctx);

	cairo_status_t status = cairo_status(cairo_);
	ASSERT_LOG(status == 0, "SVG rendering error rendering " << fname << ": " << cairo_status_to_string(status));
}

void cairo_context::write_png(const std::string& fname) 
{
	cairo_surface_write_to_png(surface_, fname.c_str());
}

void cairo_context::set_pattern(cairo_pattern_t* pattern, bool take_ownership)
{
	if(temp_pattern_) {
		cairo_pattern_destroy(temp_pattern_);
		temp_pattern_ = NULL;
	}

	if(take_ownership) {
		temp_pattern_ = pattern;
	}

	cairo_set_source(cairo_, pattern);
}

cairo_matrix_saver::cairo_matrix_saver(cairo_context& ctx) : ctx_(ctx)
{
	cairo_save(ctx_.get());
}

cairo_matrix_saver::~cairo_matrix_saver()
{
	cairo_restore(ctx_.get());
}

namespace
{

typedef std::function<void(cairo_context&, const std::vector<variant>&)> CairoOp;
class cairo_op : public game_logic::formula_callable
{
public:
	cairo_op(CairoOp fn, const std::vector<variant>& args) : fn_(fn), args_(args)
	{}

	void execute(cairo_context& context) {
		fn_(context, args_);
	}

	bool is_cairo_op() const { return true; }
private:
	DECLARE_CALLABLE(cairo_op);
	
	CairoOp fn_;
	std::vector<variant> args_;
};

class cairo_text_fragment : public game_logic::formula_callable
{
public:
	cairo_text_fragment() : x(0), y(0), width(0), height(0), ascent(0), descent(0) {
	}
	variant path, tag;
	float x, y, width, height, ascent, descent;
	graphics::color color;
private:
	DECLARE_CALLABLE(cairo_text_fragment);

};

BEGIN_DEFINE_CALLABLE_NOBASE(cairo_text_fragment)
DEFINE_FIELD(path, "builtin cairo_op")
	return obj.path;
DEFINE_FIELD(tag, "any")
	return obj.tag;
DEFINE_FIELD(x, "decimal")
	return variant(obj.x);
DEFINE_FIELD(y, "decimal")
	return variant(obj.y);
DEFINE_FIELD(width, "decimal")
	return variant(obj.width);
DEFINE_FIELD(height, "decimal")
	return variant(obj.height);
DEFINE_FIELD(ascent, "decimal")
	return variant(obj.ascent);
DEFINE_FIELD(descent, "decimal")
	return variant(obj.descent);
DEFINE_FIELD(color, "[decimal,decimal,decimal,decimal]")
	std::vector<variant> result;
	result.resize(4);
	result[0] = variant(obj.color.r()/255.0);
	result[1] = variant(obj.color.g()/255.0);
	result[2] = variant(obj.color.b()/255.0);
	result[3] = variant(obj.color.a()/255.0);
	return variant(&result);
END_DEFINE_CALLABLE(cairo_text_fragment)

namespace {
struct MarkupEntry {
	const char* tag;
	char values[4];
};

MarkupEntry MarkupMap[] = {
	{ "ldquo", {0xE2, 0x80, 0x9C, 0x00} },
	{ "rdquo", {0xE2, 0x80, 0x9D, 0x00} },
	{ "emdash", { 0xE2, 0x80, 0x94, 0x00 } },
	{ "amp", { '&', 0x00, 0x00, 0x00 } },
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
			break;
		}

		bool found = false;
		std::string tag(itor+1, semi);
		for(const MarkupEntry& e : MarkupMap) {
			if(tag == e.tag) {
				result += e.values;
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

variant layout_text_impl(const TextMarkupFragment* f1, const TextMarkupFragment* f2, float width)
{

	cairo_context context(8, 8);

	float xpos = 0;
	float ypos = 0;
	float line_height = 0;

	std::vector<LineOfText> output;
	output.push_back(LineOfText());

	for(; f1 != f2; ++f1) {
		const TextMarkupFragment& item = *f1;

		const std::string& text = item.text;
		const std::string& font = item.font;
		const int font_size = item.font_size;
		const variant& tag = item.tag;
		const variant& align = item.align;
		const variant& valign = item.valign;
		std::string svg = item.svg;


		FT_Face face = get_ft_font(font);
		cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
		cairo_set_font_face(context.get(), cairo_face);
		cairo_set_font_size(context.get(), font_size);

		cairo_font_extents_t font_extents;
		cairo_font_extents(context.get(), &font_extents);

		if(font_extents.height > line_height) {
			line_height = font_extents.height;
		}

		float min_line_height = font_extents.height;

		std::vector<std::string> lines = util::split(text, '\n', 0);

		if(!output.empty() && output.back().fragments.empty()) {
			output.back().align = align;
		}

		bool first_line = true;
		for(const std::string& line : lines) {
			if(!first_line) {
				xpos = 0;
				ypos += line_height;
				line_height = min_line_height;

				output.push_back(LineOfText());
				output.back().align = align;
			}

			first_line = false;

			std::string::const_iterator i1 = line.begin();
			while(i1 != line.end() || svg.empty() == false) {
				std::string::const_iterator i2 = std::find(i1, line.end(), ' ');
				if(i2 != line.end()) {
					++i2;
				}

				std::string word(i1, i2);

				cairo_text_extents_t extents;
				cairo_text_extents(context.get(), word.c_str(), &extents);

				if(xpos + extents.x_advance > width) {
					while(extents.x_advance > width && i2 > i1+1) {
						--i2;
						cairo_text_extents(context.get(), word.c_str(), &extents);
					}

					xpos = 0;
					ypos += line_height;
					line_height = min_line_height;

					output.push_back(LineOfText());
					output.back().align = align;
				}

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

				TextFragment fragment = { xpos, ypos, extents.width, extents.height, font, font_size, tag, item.color, std::string(i1, i2), valign, font_extents, svg };
				output.back().fragments.push_back(fragment);
				output.back().fragment_width += extents.width;

				if(svg.empty() == false) {
					//advance the text position along to account
					//for the svg icon, wrapping to next line if necessary
					float advance = font_extents.height;

					if(xpos > 0 && xpos + advance > width) {
						xpos = 0;
						ypos += line_height;
						line_height = min_line_height;

						output.push_back(LineOfText());
						output.back().align = align;
					}

					xpos += advance;
				}
				
				svg = "";

				xpos += extents.x_advance;

				i1 = i2;
			}
		}
	}

	if(output.empty() == false && output.back().fragments.empty()) {
		output.pop_back();
	}

	std::vector<variant> result;

	for(const LineOfText& line : output) {

		//calculate the baseline. 
		float line_height = 0.0;
		float max_ascent = 0.0;
		float max_descent = 0.0;
		for(const TextFragment& fragment : line.fragments) {
			if(fragment.font_extents.height > line_height) {
				line_height = fragment.font_extents.height;
			}

			if(fragment.font_extents.ascent > max_ascent) {
				max_ascent = fragment.font_extents.ascent;
			}

			if(fragment.font_extents.descent > max_descent) {
				max_descent = fragment.font_extents.descent;
			}
		}

		const float baseline = max_ascent + (line_height-(max_ascent+max_descent))/2;
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
					fragment_baseline = line_top + fragment.font_extents.ascent;
				} else if(valign == "bottom") {
					fragment_baseline = line_bottom - fragment.font_extents.descent;
				} else if(valign == "middle") {
					//average of top and bottom.
					fragment_baseline = (line_top + fragment.font_extents.ascent + line_bottom - fragment.font_extents.descent)/2.0;
				} else {
					ASSERT_LOG(valign == "baseline", "Unrecognized valignment: " << valign);
				}
			}

			res->tag = fragment.tag;
			res->color = fragment.color;

			if(fragment.svg.empty() == false) {

				std::vector<variant> fn_args;
				fn_args.push_back(variant(fragment.svg));
				fn_args.push_back(variant(fragment.xpos));
				fn_args.push_back(variant(fragment.ypos));
				fn_args.push_back(variant(fragment.font_extents.height));
				fn_args.push_back(variant(fragment.font_extents.height));

				res->path = variant(new cairo_op([](cairo_context& context, const std::vector<variant>& args) {
					cairo_translate(context.get(), args[1].as_decimal().as_float(), args[2].as_decimal().as_float());

					variant svg = args[0];
					int w = args[3].as_decimal().as_int();
					int h = args[4].as_decimal().as_int();
					const cairo_matrix_saver saver(context);

					context.render_svg(svg.as_string(), w, h);

				}, fn_args));
			} else {

				std::vector<variant> fn_args;
				fn_args.push_back(variant(fragment.xpos + xpos_align_adjust));
				fn_args.push_back(variant(fragment.ypos + fragment_baseline));
				fn_args.push_back(variant(fragment.text));
				fn_args.push_back(variant(fragment.font));
				fn_args.push_back(variant(fragment.font_size));

				res->path = variant(new cairo_op([](cairo_context& context, const std::vector<variant>& args) {
	
					cairo_translate(context.get(), args[0].as_decimal().as_float(), args[1].as_decimal().as_float());

					FT_Face face = get_ft_font(args[3].as_string());
					cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
					cairo_set_font_face(context.get(), cairo_face);
					cairo_set_font_size(context.get(), args[4].as_int());

					cairo_text_path(context.get(), args[2].as_string().c_str());
				}, fn_args));

			}

			res->x = fragment.xpos + xpos_align_adjust;
			res->y = fragment.ypos + fragment_baseline - fragment.font_extents.ascent;
			res->width = fragment.width;
			res->height = fragment.font_extents.ascent + fragment.font_extents.descent;
			res->ascent = fragment.font_extents.ascent;
			res->descent = fragment.font_extents.descent;

			result.push_back(variant(res));
		}
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

void parse_text_markup_impl(std::vector<TextMarkupFragment>& stack, std::vector<TextMarkupFragment>& output, const boost::property_tree::ptree& ptree, const variant& get_font_fn, const std::string& element)
{
	static const std::string XMLText = "<xmltext>";
	static const std::string XMLAttr = "<xmlattr>";

	for(auto itor = ptree.begin(); itor != ptree.end(); ++itor) {
		fprintf(stderr, "XML: %s -> %s\n", itor->first.c_str(), itor->second.data().c_str());

		const size_t start_size = stack.size();

		if(itor->first == XMLText) {
			if(itor->second.data().empty() == false) {
				output.push_back(stack.back());
				output.back().text = parse_special_chars_internal(itor->second.data());
			}

			continue;
		} else if(itor->first == XMLAttr) {
			for(auto a = itor->second.begin(); a != itor->second.end(); ++a) {
				const std::string& attr = a->first;
				const std::string& value = a->second.data();
				if(element == "font") {
					if(attr == "size") {
						ASSERT_LOG(value.empty() == false, "Invalid font size");
						const int num_value = atoi(value.c_str());
						if(value[0] == '+' || value[0] == '-') {
							stack.back().font_size += num_value;
						} else {
							stack.back().font_size = num_value;
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
						stack.back().color = graphics::color(value);

					} else if(attr == "tag") {
						std::vector<variant> v;
						if(stack.back().tag.is_list()) {
							v = stack.back().tag.as_list();
						}

						v.push_back(variant(value));
						stack.back().tag = variant(&v);
					} else if(attr == "align") {
						stack.back().align = variant(value);
					} else if(attr == "valign") {
						stack.back().valign = variant(value);
					}

				} else if(element == "img") {
					if(attr == "src") {
						output.push_back(stack.back());
						output.back().text = "";
						output.back().svg = value;
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

		parse_text_markup_impl(stack, output, itor->second, get_font_fn, itor->first);

		stack.resize(start_size);
		fprintf(stderr, "DONE: %s\n", itor->first.c_str());
	}
}

}

BEGIN_DEFINE_CALLABLE_NOBASE(cairo_op)
END_DEFINE_CALLABLE(cairo_op)

#define BEGIN_CAIRO_FN(a,b) BEGIN_DEFINE_FN(a,b "->builtin cairo_op") std::vector<variant> fn_args; for(int i = 0; i < NUM_FN_ARGS; ++i) { fn_args.push_back(FN_ARG(i)); } return variant(new cairo_op([](cairo_context& context, const std::vector<variant>& args) {

#define END_CAIRO_FN }, fn_args)); END_DEFINE_FN

}

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
	float x = args[0].as_decimal().as_float();
	float y = x;
	if(args.size() > 1 && args[1].is_null() == false) {
		y = args[1].as_decimal().as_float();
	}
	cairo_scale(context.get(), x, y);
END_CAIRO_FN

BEGIN_CAIRO_FN(rotate, "(decimal)")
	cairo_rotate(context.get(), args[0].as_decimal().as_float());
END_CAIRO_FN

BEGIN_CAIRO_FN(draw_svg, "(string, decimal, decimal)")
	context.render_svg(args[0].as_string(), args[1].as_int(), args[2].as_int());
END_CAIRO_FN

BEGIN_DEFINE_FN(render, "(int, int, cairo_commands) ->builtin texture_object")
	const int w = FN_ARG(0).as_int();
	const int h = FN_ARG(1).as_int();
	ASSERT_LOG(w > 0 && h > 0, "Invalid canvas render: " << w << "x" << h);
	cairo_context context(w, h);

	variant ops = FN_ARG(2);
	execute_cairo_ops(context, ops);

	return variant(new texture_object(context.write()));
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

BEGIN_CAIRO_FN(set_source_color, "([decimal, decimal, decimal]|[decimal,decimal,decimal,decimal])")
	const std::vector<variant>& col = args[0].as_list();
	float alpha = 1.0;
	if(col.size() > 3) {
		alpha = col[3].as_decimal().as_float();
	}

	cairo_set_source_rgba(context.get(),
	                      col[0].as_decimal().as_float(),
	                      col[1].as_decimal().as_float(),
	                      col[2].as_decimal().as_float(),
						  alpha);
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
			alpha = a.as_decimal().as_float();
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
			alpha = a.as_decimal().as_float();
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
	float svg_width = args[3]["svg_width"].as_decimal().as_float();
	float svg_scale = args[3]["svg_scale"].as_decimal().as_float();
	float scaling = 1.0;
	if(args[3]["scaling"].is_decimal()) {
		scaling = args[3]["scaling"].as_decimal().as_float();
	}

	float line_height = 0;

	std::vector<std::string> split_text = util::split(all_text, '\n');

	std::vector<std::vector<std::string>> lines;

	for(std::string text : split_text) {
		lines.push_back(std::vector<std::string>());

		float width = args[1].as_decimal().as_float();
		float height = args[2].as_decimal().as_float();

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
					lengths.push_back(extents.x_advance);
					if(extents.height > line_height) {
						line_height = extents.height;
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

				context.render_svg(svg_name, context.width(), context.height());
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
		ypos += line_height*1.1;
	}

	cairo_restore(context.get());

END_CAIRO_FN

BEGIN_CAIRO_FN(text_path_in_bounds, "(string, decimal, [string])")
	std::string text = args[0].as_string();
	float size = args[1].as_decimal().as_float();

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
			size *= float(size)/float(extents.width);
		} else {
			const int forced_len = text.size()*float(size)/float(extents.width);
			if(forced_len < text.size()) {
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
	cairo_get_current_point(context.get(), &xpos, NULL);
	for(int n = 0; n != lines.size(); ++n) {
		cairo_text_path(context.get(), lines[n].c_str());
		if(n+1 != lines.size()) {

			double ypos = 0.0;
			cairo_get_current_point(context.get(), NULL, &ypos);

			ypos += basic_extents.height*2;
			cairo_move_to(context.get(), xpos, ypos);
		}
	}
END_CAIRO_FN

BEGIN_CAIRO_FN(paint_image, "(string, [decimal,decimal]|null=null)")
	cairo_surface_t* surface = get_cairo_image(args[0].as_string());
	double translate_x = 0, translate_y = 0;
	if(args.size() > 1) {
		variant pos = args[1];
		if(pos.is_list()) {
			translate_x = pos[0].as_decimal().as_float();
			translate_y = pos[1].as_decimal().as_float();
		}
	}
	cairo_set_source_surface(context.get(), surface, translate_x, translate_y);
	cairo_paint(context.get());

	cairo_status_t status = cairo_status(context.get());
	ASSERT_LOG(status == 0, "SVG rendering error painting " << args[0].as_string() << ": " << cairo_status_to_string(status));
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

BEGIN_DEFINE_FN(markup_text, "(string, decimal) ->[builtin cairo_text_fragment]")
	const std::string markup = "<root>" + FN_ARG(0).as_string() + "</root>";
	float width = FN_ARG(1).as_decimal().as_float();

	std::istringstream s(markup);
	boost::property_tree::ptree ptree;
	try {
		boost::property_tree::xml_parser::read_xml(s, ptree, boost::property_tree::xml_parser::no_concat_text);
	} catch(...) {
		ASSERT_LOG(false, "Error parsing XML: " << markup);
	}

	static game_logic::formula_callable_ptr lib_obj = game_logic::get_library_object();
	static game_logic::const_formula_callable_ptr font_lib(lib_obj->query_value("font").as_callable());
	ASSERT_LOG(font_lib.get(), "Could not find font class needed to call markup_text");

	static const variant get_font_fn = font_lib->query_value("get_font");
	ASSERT_LOG(get_font_fn.is_null() == false, "Could not find get() function in font lib");

	std::vector<TextMarkupFragment> stack(1);
	stack.back().font_size = 14;
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

	parse_text_markup_impl(stack, fragments, ptree, get_font_fn, "");

	if(fragments.empty() == false) {
		return layout_text_impl(&fragments[0], &fragments[0] + fragments.size(), width);
	} else {
		std::vector<variant> result;
		return variant(&result);
	}


END_DEFINE_FN

BEGIN_DEFINE_FN(layout_text, "([{text: string, font: string, size: int, tag: null|any, align: string|null, valign: string|null, svg: string|null}], decimal) ->[builtin cairo_text_fragment]")

	variant info = FN_ARG(0);
	float width = FN_ARG(1).as_decimal().as_float();

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

BEGIN_DEFINE_FN(text_extents, "(string, decimal, string) -> { width: decimal, height: decimal }")
	static cairo_context& context = *new cairo_context(8,8);

	FT_Face face = get_ft_font(FN_ARG(0).as_string());
	cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
	cairo_set_font_face(context.get(), cairo_face);

	cairo_set_font_size(context.get(), FN_ARG(1).as_decimal().as_float());

	std::string text = FN_ARG(2).as_string();
	std::vector<std::string> lines = util::split(text, '\n');

	double width = 0.0, height = 0.0;

	for(const std::string& line : lines) {
		cairo_text_extents_t extents;
		cairo_text_extents(context.get(), line.c_str(), &extents);
		height += extents.height;
		if(extents.x_advance > width) {
			width = extents.x_advance;
		}
	}

	std::map<variant,variant> result;
	result[variant("width")] = variant(width);
	result[variant("height")] = variant(height);
	return variant(&result);

END_DEFINE_FN

BEGIN_DEFINE_FN(parse_special_chars, "(string) -> string")
	std::string s = FN_ARG(0).as_string();
	return variant(parse_special_chars_internal(s));
END_DEFINE_FN

//a string representing an emdash in utf-8.
DEFINE_FIELD(emdash, "string")
	const char em[] = { 0xE2, 0x80, 0x94, 0x00 };
	const std::string str = em;
	return variant(str);

END_DEFINE_CALLABLE(cairo_callable)

namespace {
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
			char* end = NULL;
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
		while(ptr != NULL) {
			ptr += 9;
			output += std::string(begin, ptr);

			const char* end = strstr(ptr, "\"");
			ASSERT_LOG(end != NULL, "Unexpected end of file: " << ptr);

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

graphics::texture render_text_uncached(const std::string& text,
                                       const SDL_Color& color, int size, const std::string& font_name)
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
		lengths.push_back(extents.width);
		heights.push_back(extents.height);
		if(extents.width > width) {
			width = extents.width;
		}

		height += extents.height;
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


	return context.write();
}

}

}

namespace {
void handle_node(const boost::property_tree::ptree& ptree, int depth)
{
	for(auto itor = ptree.begin(); itor != ptree.end(); ++itor) {
		for(int n = 0; n != depth; ++n) { fprintf(stderr, "  "); }
		fprintf(stderr, "XML: %s -> %s\n", itor->first.c_str(), itor->second.data().c_str());
		handle_node(itor->second, depth+1);
		for(int n = 0; n != depth; ++n) { fprintf(stderr, "  "); }
		fprintf(stderr, "DONE: %s\n", itor->first.c_str());
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
