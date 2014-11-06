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
#include "graphics.hpp"

#include "font.hpp"
#include "input.hpp"
#include "raster.hpp"
#include "tooltip.hpp"

namespace gui {

namespace {
boost::shared_ptr<tooltip_item> cur_tooltip;

graphics::texture& text() {
	static graphics::texture t;
	return t;
}
}

void set_tooltip(const boost::shared_ptr<tooltip_item>& tip)
{
	cur_tooltip = tip;
	text() = font::render_text(cur_tooltip->text, cur_tooltip->font_color, cur_tooltip->font_size, cur_tooltip->font_name);
}

void remove_tooltip(const boost::shared_ptr<tooltip_item>& tip)
{
	if(tip == cur_tooltip) {
		cur_tooltip.reset();
		text() = graphics::texture();
	}
}

void draw_tooltip()
{
	if(!cur_tooltip) {
		return;
	}

	int mousex, mousey;
	input::sdl_get_mouse_state(&mousex,&mousey);

	const int pad = 10;
	const int width = text().width() + pad*2;
	const int height = text().height() + pad*2;
	int x = mousex - width/2;
	int y = mousey - height;
	if(x < 0) {
		x = 0;
	}

	if(x > graphics::screen_width()-width) {
		x = graphics::screen_width()-width;
	}

	if(y < 0) {
		y = 0;
	}

	if(y > graphics::screen_height()-height) {
		y = graphics::screen_height()-height;
	}

	SDL_Rect rect = {x,y,width,height};
	graphics::draw_rect(rect, graphics::color_black(), 160);

	graphics::blit_texture(text(),x+pad,y+pad);
}

}
