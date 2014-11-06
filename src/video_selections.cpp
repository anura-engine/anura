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
	d.set_background_frame("empty_window");
	d.set_draw_background_fn([](){ draw_scene(level::current(), last_draw_position()); });

	const int button_width = 150;
	const int button_height = 40;
	const int padding = 20;

	int selected_mode = -1;

	d.add_widget(widget_ptr(new graphical_font_label(_("Select video options:"), "door_label", 2)), padding, padding);
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
		mode_list->set_selection(current_mode_index);
		mode_list->set_zorder(10);
		mode_list->set_on_select_handler([&selected_mode](int selection,const std::string& s){ 
			selected_mode = selection;
		});
		d.add_widget(widget_ptr(mode_list));
	} else {
		d.add_widget(widget_ptr(new graphical_font_label(_("Unable to enumerate video modes"), "door_label", 2)), padding, padding);
	}

	// Fullscreen selection
	preferences::FullscreenMode fs_mode = preferences::fullscreen();
	std::vector<std::string> fs_options;
	fs_options.push_back("Windowed mode");
	fs_options.push_back("Fullscreen Windowed");
	fs_options.push_back("Fullscreen");
	dropdown_widget* fs_list = new dropdown_widget(fs_options, 220, 20);
	fs_list->set_selection(int(preferences::fullscreen()));
	fs_list->set_zorder(9);
	fs_list->set_on_select_handler([&fs_mode](int selection,const std::string& s){ 
		switch(selection) {
			case 0:	fs_mode = preferences::FULLSCREEN_NONE; break;
			case 1:	fs_mode = preferences::FULLSCREEN_WINDOWED; break;
			case 2:	fs_mode = preferences::FULLSCREEN; break;
		}
	});
	d.add_widget(widget_ptr(fs_list));

	// Vertical sync options
	std::vector<std::string> vsync_options;
	vsync_options.push_back("No synchronisation");
	vsync_options.push_back("Synchronised to retrace");
	vsync_options.push_back("Late synchronisation");
	dropdown_widget* synch_list = new dropdown_widget(vsync_options, 220, 20);
	synch_list->set_selection(g_vsync);
	synch_list->set_zorder(8);
	synch_list->set_on_select_handler([&selected_mode](int selection,const std::string& s){ 
		switch(selection) {
			case 0:	g_vsync = 0; break;
			case 1:	g_vsync = 1; break;
			case 2:	g_vsync = -1; break;
		}
	});
	d.add_widget(widget_ptr(synch_list));

	widget_ptr b_okay = new button(new graphical_font_label(_("OK"), "door_label", 2), [&d](){ 
		d.close();
	});
	widget_ptr b_cancel = new button(new graphical_font_label(_("Cancel"), "door_label", 2), [&d](){ 
		d.cancel();
	});
	b_okay->set_dim(button_width, button_height);
	b_cancel->set_dim(button_width, button_height);
	d.add_widget(b_okay, 20, d.height() - button_height - 20);
	d.add_widget(b_cancel, d.width() - button_width - 20, d.height() - button_height - 20);

	d.show_modal();
	if(d.cancelled() == false) {
		// set selected video mode here
		if(selected_mode >= 0 && selected_mode < display_modes.size()) {
			preferences::set_actual_screen_dimensions_persistent(display_modes[selected_mode].first, display_modes[selected_mode].second);
		}
		preferences::set_fullscreen(fs_mode);

		get_main_window()->set_window_size(preferences::actual_screen_width(), preferences::actual_screen_height());
	}
}
