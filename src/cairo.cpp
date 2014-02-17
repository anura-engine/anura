#include <map>

#include <string.h>

#include <cairo-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "asserts.hpp"
#include "fbo_scene.hpp"
#include "cairo.hpp"
#include "filesystem.hpp"
#include "module.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"

namespace graphics
{

namespace {

FT_Library& init_freetype_library()
{
	static FT_Library library;
	const int error = FT_Init_FreeType(&library);
	ASSERT_LOG(error == 0, "Could not initialize freetype: " << error);
	return library;
}

FT_Face get_ft_font(const std::string& ttf_file, int index=0)
{
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

bool initialize_gtype() {
	g_type_init();
	return true;
}
}

cairo_context::cairo_context(int w, int h)
  : width_(w), height_(h), temp_pattern_(NULL)
{
	static const bool init = initialize_gtype();

	surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_ = cairo_create(surface_);
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

	for(int i = 0; i != width_*height_; ++i) {
		dst[0] = data[2];
		dst[1] = data[1];
		dst[2] = data[0];
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

void cairo_context::render_svg(const std::string& fname)
{
		{
	cairo_status_t status = cairo_status(cairo_);
	ASSERT_LOG(status == 0, "SVG rendering error rendering " << fname << ": " << cairo_status_to_string(status));
		}

	GError *error = NULL;

	static std::map<std::string, RsvgHandle*> cache;

	RsvgHandle* handle = NULL;

	auto itor = cache.find(fname);
	if(itor == cache.end()) {
		std::string real_fname = module::map_file(fname);
		ASSERT_LOG(sys::file_exists(real_fname), "Could not find svg file: " << fname);

		handle = rsvg_handle_new_from_file(real_fname.c_str(), &error);
		ASSERT_LOG(error == NULL, "SVG rendering error: " << error->message);

		cache[fname] = handle;
	} else {
		handle = itor->second;
	}

	rsvg_handle_render_cairo(handle, cairo_);

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
private:
	DECLARE_CALLABLE(cairo_op);
	
	CairoOp fn_;
	std::vector<variant> args_;
};

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

BEGIN_CAIRO_FN(draw_svg, "(string)")
	context.render_svg(args[0].as_string());
END_CAIRO_FN

BEGIN_DEFINE_FN(render, "(int, int, [builtin cairo_op]) ->object")
	const int w = FN_ARG(0).as_int();
	const int h = FN_ARG(1).as_int();
	ASSERT_LOG(w > 0 && h > 0, "Invalid canvas render: " << w << "x" << h);
	cairo_context context(w, h);

	std::vector<variant> ops = FN_ARG(2).as_list();
	for(variant op : ops) {
		op.convert_to<cairo_op>()->execute(context);
	}

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
	FT_Face face = get_ft_font(module::map_file("data/fonts/" + args[0].as_string()));
	cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(face, 0);
	cairo_set_font_face(context.get(), cairo_face);
END_CAIRO_FN

BEGIN_CAIRO_FN(set_font_size, "(decimal)")
	cairo_set_font_size(context.get(), args[0].as_decimal().as_float());
END_CAIRO_FN

BEGIN_CAIRO_FN(show_text, "(string)")
	cairo_show_text(context.get(), args[0].as_string().c_str());
END_CAIRO_FN

BEGIN_CAIRO_FN(show_rich_text_multiline, "(string, decimal, decimal, {svg_scale: decimal, svg_width: decimal})")
	std::string all_text = args[0].as_string();
	float svg_width = args[3]["svg_width"].as_decimal().as_float();
	float svg_scale = args[3]["svg_scale"].as_decimal().as_float();

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

	float ypos = 0;
	for(const auto& line : lines) {
		cairo_save(context.get());
		cairo_translate(context.get(), 0, ypos);

		for(const auto& str : line) {
			if(str[0] == '&') {
				cairo_save(context.get());
				cairo_translate(context.get(), 0.0, -line_height);
				cairo_scale(context.get(), svg_scale, svg_scale);
				context.render_svg(std::string(str.begin()+1, str.end()-1));
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

END_CAIRO_FN

BEGIN_CAIRO_FN(text_path_in_bounds, "(string, decimal, [string])")
	std::string text = args[0].as_string();
	float size = args[1].as_decimal().as_float();

	bool right = false, center = false;
	bool shrink = true;

	if(args.size() > 2) {
		for(const auto& flag : args[2].as_list_string()) {
			if(flag == "left") {
				right = center = false;
			} else if(flag == "right") {
				right = true;
			} else if(flag == "center") {
				center = true;
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

	cairo_text_path(context.get(), text.c_str());

	cairo_restore(context.get());
END_CAIRO_FN

BEGIN_CAIRO_FN(text_path, "(string)")
	cairo_text_path(context.get(), args[0].as_string().c_str());
END_CAIRO_FN

BEGIN_CAIRO_FN(paint_image, "(string)")
	cairo_surface_t* surface = get_cairo_image(args[0].as_string());
	cairo_set_source_surface(context.get(), surface, 0, 0);
	cairo_paint(context.get());

	cairo_status_t status = cairo_status(context.get());
	ASSERT_LOG(status == 0, "SVG rendering error painting " << args[0].as_string() << ": " << cairo_status_to_string(status));
END_CAIRO_FN

BEGIN_DEFINE_FN(image_dim, "(string) ->[int,int]")
	cairo_surface_t* surface = get_cairo_image(FN_ARG(0).as_string());
	std::vector<variant> result;
	result.push_back(variant(cairo_image_surface_get_width(surface)));
	result.push_back(variant(cairo_image_surface_get_height(surface)));

	return variant(&result);
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

}
