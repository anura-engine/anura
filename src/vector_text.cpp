/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#include <boost/algorithm/string.hpp>

#include "asserts.hpp"
#include "font.hpp"
#include "i18n.hpp"
#include "raster.hpp"
#include "string_utils.hpp"
#include "vector_text.hpp"

namespace gui {

vector_text::vector_text(const variant& node)
	: text_(i18n::tr(node["text"].as_string())),
	visible_(node["visible"].as_bool(true)),
	size_(node["size"].as_int(12)),
	font_(node["font"].as_string_default()),
	align_(ALIGN_LEFT)
{
	std::vector<int> r = node["rect"].as_list_int();
	draw_area_ = rect(r[0], r[1], r[2], r[3]);
	if(node.has_key("color")) {
		set_color(node["color"]);
	} else {
		color_ = graphics::color(255,255,255,255).as_sdl_color();
	}
	if(node.has_key("align")) {
		std::string align = node["align"].as_string();
		if(align == "left") {
			align_ = ALIGN_LEFT;
		} else if(align == "center" || align == "centre") {
			align_ = ALIGN_CENTER;
		} else if(align == "right") {
			align_ = ALIGN_RIGHT;
		} else {
			ASSERT_LOG(false, "Invalid value for \"align\" attribute: " << align);
		}
	} else {
		align_ = ALIGN_LEFT;
	}

	recalculate_texture();
}

void vector_text::handle_draw() const
{
	foreach(const offset_texture& tex, textures_) {
		graphics::blit_texture(tex.first, x() + tex.second.x, y() + tex.second.y);
	}
}

void vector_text::recalculate_texture()
{
	textures_.clear();

	size_t tex_y = 0;
	int letter_size = font::char_width(size(), font_);
	std::vector<std::string> lines;

	foreach(const std::string& paragraph, util::split(text_, '\n')) {
		std::vector<std::string> words;
		boost::split(words, paragraph, std::bind2nd(std::equal_to<char>(), ' '));
		size_t current_line_length = 0;
		std::string current_line;
		foreach(const std::string& word, words) {
			if(current_line_length + (word.length() + 1) * letter_size < width()) {
				current_line_length += (word.length() + 1) * letter_size;
				current_line += (current_line.empty() ? "" : " ") + word;
			} else {
				lines.push_back(current_line);
				current_line = word;
				current_line_length = word.length() * letter_size;
			}
		}
		if(current_line.empty() == false) {
			lines.push_back(current_line);
		}
	}

	foreach(const std::string line, lines) {
		if(tex_y < height()) {
			graphics::texture tex = font::render_text(line, color_, size_, font_);
			if(align_ == ALIGN_LEFT) {
				textures_.push_back(offset_texture(tex, point(0,tex_y)));
			} else if(align_ == ALIGN_CENTER) {
				textures_.push_back(offset_texture(tex, point((width() - tex.width())/2,tex_y)));
			} else {
				textures_.push_back(offset_texture(tex, point(width() - tex.width(),tex_y)));
			}
			tex_y += tex.height();
		} else {
			std::cerr << "vector_text::recalculate_texture(): Ignored line: \"" 
				<< line << "\" line is outside the maximum area" << std::endl;
		}
	}
}

void vector_text::set_text(const std::string& txt)
{
	text_ = i18n::tr(txt);
	recalculate_texture();
}

void vector_text::set_font(const std::string& fnt)
{
	font_ = fnt;
	recalculate_texture();
}

void vector_text::set_size(int size)
{
	size_ = size;
	recalculate_texture();
}

void vector_text::set_color(const variant& node)
{
	if(node.is_string()) {
		color_ = graphics::get_color_from_name(node.as_string());
	} else {
		color_ = graphics::color(node).as_sdl_color();
	}
	recalculate_texture();
}

void vector_text::set_align(const std::string& align)
{
	TEXT_ALIGNMENT new_align;
	if(align == "left") {
		new_align = ALIGN_LEFT;
	} else if(align == "center" || align == "centre") {
		new_align = ALIGN_CENTER;
	} else if(align == "right") {
		new_align = ALIGN_RIGHT;
	} else {
		ASSERT_LOG(false, "Invalid value for \"align\" attribute: " << align);
	}
	set_align(new_align);
}

void vector_text::set_align(TEXT_ALIGNMENT align)
{
	align_ = align;
	recalculate_texture();
}

variant vector_text::get_value(const std::string& key) const
{
	if(key == "text") {
		return variant(text_);
	} else if(key == "color") {
		return graphics::color(color_.r, color_.g, color_.b, color_.a).write();
	} else if(key == "size") {
		return variant(size_);
	} else if(key == "font") {
		return variant(font_);
	} else if(key == "align") {
		if(align_ < 0) {
			return variant("left");
		} else if(align_ == 0) {
			return variant("center");
		} else {
			return variant("right");
		}
	} else if(key == "x") {
		return variant(x());
	} else if(key == "y") {
		return variant(y());
	} else if(key == "width") {
		return variant(width());
	} else if(key == "height") {
		return variant(height());
	}
	return variant();
}

void vector_text::set_value(const std::string& key, const variant& value)
{
	if(key == "text") {
		set_text(value.as_string());
	} else if(key == "color") {
		set_color(value);
	} else if(key == "size") {
		set_size(value.as_int());
	} else if(key == "font") {
		set_font(value.as_string());
	} else if(key == "align") {
		set_align(value.as_string());
	} else if(key == "visible") {
		set_visible(value.as_bool());
	} else if(key == "rect") {
		std::vector<int> r = value.as_list_int();
		draw_area_ = rect(r[0], r[1], r[2], r[3]);
	} else if(key == "x") {
		draw_area_ = rect(value.as_int(), draw_area_.y(), draw_area_.w(), draw_area_.h());
	} else if(key == "y") {
		draw_area_ = rect(draw_area_.x(), value.as_int(), draw_area_.w(), draw_area_.h());
	} else if(key == "width") {
		draw_area_ = rect(draw_area_.x(), draw_area_.y(), value.as_int(), draw_area_.h());
	} else if(key == "height") {
		draw_area_ = rect(draw_area_.x(), draw_area_.y(), draw_area_.w(), value.as_int());
	}
}

}
