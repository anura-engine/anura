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

#include <set>
#include <sstream>

#include "dialog.hpp"
#include "checkbox.hpp"
#include "draw_scene.hpp"
#include "dropdown_widget.hpp"
#include "grid_widget.hpp"
#include "graphics.hpp"
#include "graphical_font_label.hpp"
#include "i18n.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "video_selections.hpp"

using namespace gui;

PREF_INT_PERSISTENT(vsync, 0, "Vertical synchronization setting. 0 = none. 1 = snc, -1 = late synchronizaiton.");

namespace 
{
	struct cmp
	{
		bool operator()(const std::pair<int,int> &left, const std::pair<int,int> &right) {
			return left.first < right.first ? true : left.first == right.first ? left.second < right.second : false;
		}
	};


	typedef std::vector<std::pair<int,int>> wh_data;

	int enumerate_video_modes(wh_data& display_modes)
	{
		const int display_index = SDL_GetWindowDisplayIndex(get_main_window()->sdl_window());
		int mode_index = -1;
		const int nmodes = SDL_GetNumDisplayModes(display_index);
		for(int n = 0; n != nmodes; ++n) {
			SDL_DisplayMode new_mode;
			const int nvalue = SDL_GetDisplayMode(display_index, n, &new_mode);
			if(nvalue != 0) {
				std::cerr << "ERROR QUERYING DISPLAY INFO: " << SDL_GetError() << "\n";
				continue;
			}
			// filter modes based on pixel format here
			if(SDL_BITSPERPIXEL(new_mode.format) == 24) {
				std::cerr << "Adding display mode: " << new_mode.w << "," << new_mode.h << std::endl;
				display_modes.push_back(std::make_pair(new_mode.w, new_mode.h));
			}
		}
		std::sort(display_modes.begin(), display_modes.end(), cmp());
		display_modes.erase(std::unique(display_modes.begin(), display_modes.end()), display_modes.end());
		std::reverse(display_modes.begin(), display_modes.end());
		int n = 0;
		for(auto dm : display_modes) {
			if(dm.first == preferences::actual_screen_width() && dm.second == preferences::actual_screen_height()) {
				mode_index = n;
			}
			++n;
		}
		return mode_index;
	}

	void map_modes_to_strings(const wh_data& display_modes, std::vector<std::string>& display_strings)
	{
		for(auto dm : display_modes) {
			std::stringstream ss;
			ss << dm.first << " x " << dm.second;			
			display_strings.push_back(ss.str());
		}
	}
}

void show_video_selection_dialog()
{
	dialog d(int(preferences::virtual_screen_width()*0.1), 
		int(preferences::virtual_screen_height()*0.1), 
		int(preferences::virtual_screen_width()*0.8), 
		int(preferences::virtual_screen_height()*0.8));
	d.setBackgroundFrame("empty_window");
	d.setDrawBackgroundFn([](){ draw_scene(level::current(), last_draw_position()); });

	const int button_width = 150;
	const int button_height = 40;
	const int padding = 20;

	int selected_mode = -1;

	d.addWidget(WidgetPtr(new GraphicalFontLabel(_("Select video options:"), "door_label", 2)), padding, padding);
	wh_data display_modes;
	int current_mode_index = enumerate_video_modes(display_modes);
	if(!display_modes.empty()) {
		if(current_mode_index < 0 || current_mode_index >= display_modes.size()) {
			current_mode_index = 0;
		}
		std::vector<std::string> display_strings;
		map_modes_to_strings(display_modes, display_strings);

		// Video mode list.
		dropdown_widget* mode_list = new dropdown_widget(display_strings, 220, 20);
		mode_list->setSelection(current_mode_index);
		mode_list->setZOrder(10);
		mode_list->setOnSelectHandler([&selected_mode](int selection,const std::string& s){ 
			selected_mode = selection;
		});
		d.addWidget(WidgetPtr(mode_list));
	} else {
		d.addWidget(WidgetPtr(new GraphicalFontLabel(_("Unable to enumerate video modes"), "door_label", 2)), padding, padding);
	}

	// Fullscreen selection
	preferences::FullscreenMode fs_mode = preferences::fullscreen();
	std::vector<std::string> fs_options;
	fs_options.push_back("Windowed mode");
	fs_options.push_back("Fullscreen Windowed");
	fs_options.push_back("Fullscreen");
	dropdown_widget* fs_list = new dropdown_widget(fs_options, 220, 20);
	fs_list->setSelection(int(preferences::fullscreen()));
	fs_list->setZOrder(9);
	fs_list->setOnSelectHandler([&fs_mode](int selection,const std::string& s){ 
		switch(selection) {
			case 0:	fs_mode = preferences::FULLSCREEN_NONE; break;
			case 1:	fs_mode = preferences::FULLSCREEN_WINDOWED; break;
			case 2:	fs_mode = preferences::FULLSCREEN; break;
		}
	});
	d.addWidget(WidgetPtr(fs_list));

	// Vertical sync options
	std::vector<std::string> vsync_options;
	vsync_options.push_back("No synchronisation");
	vsync_options.push_back("Synchronised to retrace");
	vsync_options.push_back("Late synchronisation");
	dropdown_widget* synch_list = new dropdown_widget(vsync_options, 220, 20);
	synch_list->setSelection(g_vsync);
	synch_list->setZOrder(8);
	synch_list->setOnSelectHandler([&selected_mode](int selection,const std::string& s){ 
		switch(selection) {
			case 0:	g_vsync = 0; break;
			case 1:	g_vsync = 1; break;
			case 2:	g_vsync = -1; break;
		}
	});
	d.addWidget(WidgetPtr(synch_list));

	WidgetPtr b_okay = new button(new GraphicalFontLabel(_("OK"), "door_label", 2), [&d](){ 
		d.close();
	});
	WidgetPtr b_cancel = new button(new GraphicalFontLabel(_("Cancel"), "door_label", 2), [&d](){ 
		d.cancel();
	});
	b_okay->setDim(button_width, button_height);
	b_cancel->setDim(button_width, button_height);
	d.addWidget(b_okay, 20, d.height() - button_height - 20);
	d.addWidget(b_cancel, d.width() - button_width - 20, d.height() - button_height - 20);

	d.showModal();
	if(d.cancelled() == false) {
		// set selected video mode here
		if(selected_mode >= 0 && selected_mode < display_modes.size()) {
			preferences::set_actual_screen_dimensions_persistent(display_modes[selected_mode].first, display_modes[selected_mode].second);
		}
		preferences::set_fullscreen(fs_mode);

		get_main_window()->set_window_size(preferences::actual_screen_width(), preferences::actual_screen_height());
	}
}
