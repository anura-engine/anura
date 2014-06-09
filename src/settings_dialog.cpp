/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "settingsDialog.hpp"

#include "gui_section.hpp"
#include "iphone_controls.hpp"
#include "preferences.hpp"
#include "raster.hpp"

#include <string>

namespace
{
	const int padding = 10;
}

void settingsDialog::draw (bool in_speech_dialog) const
{
	int sw = graphics::screen_width();
	int sh = graphics::screen_height();
	const ConstGuiSectionPtr button = GuiSection::get(std::string(in_speech_dialog ? "skip" : "menu") + "_button_" + std::string(menu_button_state_ ? "down" : "normal") + std::string(sw == 1024 ? "_ipad" : ""));
	if (sw != 1024) // not iPad
	{
		button->blit(sw - button->width() - padding, padding);
	} else {
		button->blit(sw - button->width()/2 - padding, padding, button->width()/2, button->height()/2);
	}
}

bool settingsDialog::handleEvent (const SDL_Event& event)
{
	int sw = graphics::screen_width();
	int sh = graphics::screen_height();
	// Not using _ipad for iPad here is a hack, which assumes the normal button is half the size of the iPad button
	const ConstGuiSectionPtr button = GuiSection::get("menu_button_normal");
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

void settingsDialog::reset ()
{
	show_window_ = false;
	menu_button_state_ = false;
}

settingsDialog::settingsDialog () : show_window_(false), menu_button_state_(false)
{
}

settingsDialog::~settingsDialog () {}
