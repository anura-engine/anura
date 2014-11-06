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
#include <ctype.h>
#include <iostream>

#include "font.hpp"
#include "foreach.hpp"
#include "dialog.hpp"
#include "raster.hpp"
#include "options_dialog.hpp"

namespace {
	
	void draw_frame(const rect& r)
	{
		const SDL_Color border = { 0xa2, 0x64, 0x76, 0xff };
		const SDL_Color bg = { 0xbe, 0xa2, 0x8f, 0xff };
		
		const int Border = 4;
		const int Padding = 10;
		rect border_rect(r.x() - Padding - Border, r.y() - Padding - Border, r.w() + + Padding*2 + Border*2, r.h() + Padding*2 + Border*2);
		graphics::draw_rect(border_rect.sdl_rect(), border);
		rect back_rect(r.x() - Padding, r.y() - Padding, r.w() + Padding*2, r.h() + Padding*2);
		graphics::draw_rect(back_rect.sdl_rect(), bg);
	}
	
}


void options_dialog::draw() const
{
	draw_frame( rect(x(),y(),width(),height()) );
}

	
options_dialog::options_dialog(int x, int y, int w, int h)
: dialog(x,y,w,h)
{

}

void options_dialog::handle_draw() const
{
	/*if(clear_bg()) {
		SDL_Rect rect = {x(),y(),width(),height()};
		SDL_Color col = {0,0,0,0};
		graphics::draw_rect(rect,col,196);
		
		//fade effect for fullscreen dialogs
		if(bg_.valid()) {
			if(bg_alpha_ > 0.25) {
				bg_alpha_ -= 0.05;
			}
			glColor4f(1.0, 1.0, 1.0, bg_alpha_);
			graphics::blit_texture(bg_, x(), y(), width(), height(), 0.0, 0.0, 1.0, 1.0, 0.0);
			glColor4f(1.0, 1.0, 1.0, 1.0);
		}
	}*/
	draw_frame( rect(x(),y(),width(),height()) );
	//handle_draw_children();
}
