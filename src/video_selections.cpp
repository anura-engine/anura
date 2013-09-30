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
#include <boost/range/adaptor/reversed.hpp>

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

namespace 
{
	struct cmp
	{
		bool operator()(const std::pair<int,int> &left, const std::pair<int,int> &right) {
			return left.first < right.first ? true : left.first == right.first ? left.second < right.second : false;
		}
	};


	typedef std::set<std::pair<int,int>, cmp> wh_data;

	wh_data::const_iterator enumerate_video_modes(wh_data& display_modes)
	{
		//SDL_DisplayMode mode;
		const int display_index = SDL_GetWindowDisplayIndex(graphics::get_window());
		wh_data::const_iterator mode_index;
		//SDL_GetCurrentDisplayMode(display_index, &mode);
		//std::cerr << "Current display mode: " << mode.w << "," << mode.h << " : " << mode.format << std::endl;
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
				std::cerr << "Adding display mode: " << new_mode.w << "," << new_mode.h << " : " << new_mode.format << std::endl;
				auto wh = std::make_pair(new_mode.w, new_mode.h);
				auto it = display_modes.insert(wh);
				if(new_mode.w == preferences::actual_screen_width() && new_mode.h == preferences::actual_screen_height()) {
					mode_index = it.first;
				}
			}
		}
		return mode_index;
	}

	void map_modes_to_strings(const wh_data& display_modes, 
		std::vector<std::string>& display_strings,
		std::vector<std::pair<int,int> >& mode_data)
	{
		for(auto dm : boost::adaptors::reverse(display_modes)) {
			std::stringstream ss;
			ss << dm.first << " x " << dm.second;			
			//std::cerr << "XXX: " << ss.str() << std::endl;
			display_strings.push_back(ss.str());
			mode_data.push_back(std::make_pair(dm.first, dm.second));
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

	bool b_fullscreen = preferences::fullscreen();
	int selected_mode = -1;

	d.add_widget(widget_ptr(new graphical_font_label(_("Select video options:"), "door_label", 2)), padding, padding);
	wh_data display_modes;
	auto current_mode_index = enumerate_video_modes(display_modes);
	std::vector<std::string> display_strings;
	std::vector<std::pair<int,int> > mode_data;
	map_modes_to_strings(display_modes, display_strings, mode_data);
	auto it = std::find(mode_data.begin(), mode_data.end(), std::make_pair(current_mode_index->first,current_mode_index->second));
	auto index = it != mode_data.end() ? it - mode_data.begin() : -1;

	dropdown_widget* mode_list = new dropdown_widget(display_strings, 200, 20);
	mode_list->set_selection(index);
	mode_list->set_zorder(10);
	mode_list->set_on_select_handler([&selected_mode](int selection,const std::string& s){ 
		std::cerr << "XXX selected: " << s << std::cerr;
		selected_mode = selection;
	});
	d.add_widget(widget_ptr(mode_list));
	
	widget_ptr fullscreen_cb = new checkbox(new graphical_font_label(_("Fullscreen"), "door_label", 2), preferences::fullscreen(), [&b_fullscreen](bool checked){ 
		b_fullscreen = checked; 
	}, BUTTON_SIZE_DOUBLE_RESOLUTION);
	d.set_padding(20);
	d.add_widget(fullscreen_cb);

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
		preferences::set_actual_screen_dimensions_persistent(mode_data[selected_mode].first, mode_data[selected_mode].second);
		preferences::set_fullscreen(b_fullscreen);

		graphics::set_video_mode(preferences::actual_screen_width(), preferences::actual_screen_height(), preferences::fullscreen() ? SDL_WINDOW_FULLSCREEN : 0);
	}
}
