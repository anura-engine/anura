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
#ifndef TOOLTIP_HPP_INCLUDED
#define TOOLTIP_HPP_INCLUDED

#include <boost/shared_ptr.hpp>
#include <string>

#include "color_utils.hpp"
#include "color_chart.hpp"
#include "font.hpp"

namespace gui {

struct tooltip_item
{
	explicit tooltip_item(const std::string& s, int fs=18, const SDL_Color& color=graphics::color_yellow(), const std::string& font="") 
		: font_size(fs), text(s), font_color(color), font_name(font)
	{}
	std::string text;
	int font_size;
	SDL_Color font_color;
	std::string font_name;
};

void set_tooltip(const boost::shared_ptr<tooltip_item>& str);
void remove_tooltip(const boost::shared_ptr<tooltip_item>& str);
void draw_tooltip();

}

#endif
