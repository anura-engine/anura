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
#include "settings_dialog.hpp"

#include "gui_section.hpp"
#include "iphone_controls.hpp"
#include "preferences.hpp"
#include "raster.hpp"

#include <string>

namespace
{
	const int padding = 10;
}

void settings_dialog::draw (bool in_speech_dialog) const
{
	int sw = graphics::screen_width();
	int sh = graphics::screen_height();
	const const_gui_section_ptr button = gui_section::get(std::string(in_speech_dialog ? "skip" : "menu") + "_button_" + std::string(menu_button_state_ ? "down" : "normal") + std::string(sw == 1024 ? "_ipad" : ""));
	if (sw != 1024) // not iPad
	{
		button->blit(sw - button->width() - padding, padding);
	} else {
		button->blit(sw - button->width()/2 - padding, padding, button->width()/2, button->height()/2);
	}
}

bool settings_dialog::handle_event (const SDL_Event& event)
{
	int sw = graphics::screen_width();
	int sh = graphics::screen_height();
	// Not using _ipad for iPad here is a hack, which assumes the normal button is half the size of the iPad button
	const const_gui_section_ptr button = gui_section::get("menu_button_normal");
	if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
	{
		const int menu_button_x = sw - button->width() - padding;
		const int menu_button_y = padding;
		int x = event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x;
		int y = event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y;
		translate_mouse_coords(&x, &y);
		bool hittest = (x > (menu_button_x-padding*2) && y < menu_button_y+button->height()+padding*2);
		if (hittest && (event.type == SDL_MOUSEBUTTONDOWN || (event.type == SDL_MOUSEMOTION && event.motion.state)))
		{
			menu_button_state_ = true;
		} else {
			menu_button_state_ = false;
		}
		if (hittest && event.type == SDL_MOUSEBUTTONUP)
		{
			//show_window_ = true;
			return true;
		}
	}
	return false;
	//return show_window_;
}

void settings_dialog::reset ()
{
	show_window_ = false;
	menu_button_state_ = false;
}

settings_dialog::settings_dialog () : show_window_(false), menu_button_state_(false)
{
}

settings_dialog::~settings_dialog () {}
