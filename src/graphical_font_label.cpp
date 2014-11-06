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
#include "asserts.hpp"
#include "graphical_font_label.hpp"

namespace gui {

graphical_font_label::graphical_font_label(
       const std::string& text, const std::string& font, int size)
  : text_(text), font_(graphical_font::get(font)), size_(size)
{
	set_environment();
	ASSERT_LOG(font_.get(), "UNKNOWN FONT: " << font);
	reset_text_dimensions();
}

graphical_font_label::graphical_font_label(const variant& v, game_logic::formula_callable* e)
	: widget(v,e)
{
	text_ = v["text"].as_string_default("TEXT");
	font_ = graphical_font::get(v.has_key("font") ? v["font"].as_string() : "door_label");
	ASSERT_LOG(font_.get(), "UNKNOWN FONT: " << v["font"].as_string());
	size_ = v["size"].as_int(12);
	reset_text_dimensions();
}

void graphical_font_label::handle_draw() const
{
	font_->draw(x(), y(), text_, size_);
}

void graphical_font_label::reset_text_dimensions()
{
	rect dim = font_->dimensions(text_, size_);
	widget::set_dim(dim.w(), dim.h());
}

void graphical_font_label::set_text(const std::string& text)
{
	text_ = text;
	reset_text_dimensions();
}

void graphical_font_label::set_value(const std::string& key, const variant& v)
{
	if(key == "text") {
		set_text(v.as_string());
	} else if(key == "font") {
		font_ = graphical_font::get(v.as_string());
		ASSERT_LOG(font_.get(), "UNKNOWN FONT: " << v.as_string());
		reset_text_dimensions();
	} else if(key == "size") {
		size_ = v.as_int();
		reset_text_dimensions();
	}
	//widget::set_value(key);
}

variant graphical_font_label::get_value(const std::string& key) const
{
	if(key == "text") {
		return variant(text_);
	} else if(key == "font") {
		return variant(font_->id());
	} else if(key == "size") {
		return variant(size_);
	}
	return widget::get_value(key);
}

}
