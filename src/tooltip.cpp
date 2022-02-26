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


#include "Canvas.hpp"
#include "Font.hpp"
#include "WindowManager.hpp"

#include "input.hpp"
#include "tooltip.hpp"

namespace gui
{
	namespace
	{
		TooltipItemPtr cur_tooltip;

		KRE::TexturePtr& text() {
			static KRE::TexturePtr t;
			return t;
		}
	}

	void set_tooltip(const TooltipItemPtr& tip)
	{
		cur_tooltip = tip;
		text() = KRE::Font::getInstance()->renderText(cur_tooltip->text, cur_tooltip->font_color, cur_tooltip->font_size, true, cur_tooltip->font_name);
	}

	void remove_tooltip(const TooltipItemPtr& tip)
	{
		if(tip == cur_tooltip) {
			cur_tooltip.reset();
			text() = KRE::TexturePtr();
		}
	}

	void draw_tooltip()
	{
		if(!cur_tooltip || !text()) {
			return;
		}

		int mousex, mousey;
		input::sdl_get_mouse_state(&mousex,&mousey);

		const int pad = 10;
		const int width = text()->width() + pad*2;
		const int height = text()->height() + pad*2;
		int x = mousex - width/2;
		int y = mousey - height;
		if(x < 0) {
			x = 0;
		}

		if(x > KRE::WindowManager::getMainWindow()->width()-width) {
			x = KRE::WindowManager::getMainWindow()->width()-width;
		}

		if(y < 0) {
			y = 0;
		}

		if(y > KRE::WindowManager::getMainWindow()->height()-height) {
			y = KRE::WindowManager::getMainWindow()->height()-height;
		}

		auto canvas = KRE::Canvas::getInstance();
		canvas->drawSolidRect(rect(x,y,width,height), KRE::Color(0,0,0,160));
		canvas->blitTexture(text(), 0, x+pad, y+pad);
	}
}
