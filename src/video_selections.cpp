/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>

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

#include "WindowManager.hpp"

#include "dialog.hpp"
#include "checkbox.hpp"
#include "draw_scene.hpp"
#include "dropdown_widget.hpp"
#include "grid_widget.hpp"
#include "graphical_font_label.hpp"
#include "i18n.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "video_selections.hpp"

using namespace gui;

PREF_INT_PERSISTENT(vsync, 0, "Vertical synchronization setting. 0 = none. 1 = snc, -1 = late synchronizaiton.");

namespace
{
	typedef std::vector<KRE::WindowMode> WindowModeList;

	int enumerate_video_modes(WindowModeList* mode_list)
	{
		auto wnd = KRE::WindowManager::getMainWindow();
		*mode_list = wnd->getWindowModes([](const KRE::WindowMode& mode){
			//return mode.pf->bitsPerPixel() == 24 ? true : false;
			return true;
		});
		std::sort(mode_list->begin(), mode_list->end(), [](const KRE::WindowMode& lhs, const KRE::WindowMode& rhs){
			return lhs.width == rhs.width ? lhs.height < rhs.height : lhs.width < rhs.width;
		});
		mode_list->erase(std::unique(mode_list->begin(), mode_list->end()), mode_list->end());
		std::reverse(mode_list->begin(), mode_list->end());

		int mode_index = -1;
		int n = 0;
		for(auto dm : *mode_list) {
			if(dm.width == wnd->width() && dm.height == wnd->height()) {
				mode_index = n;
			}
			++n;
		}
		return mode_index;
	}

	void map_modes_to_strings(const WindowModeList& mode_list, std::vector<std::string>* display_strings)
	{
		for(auto dm : mode_list) {
			std::stringstream ss;
			ss << dm.width << " x " << dm.height;
			display_strings->emplace_back(ss.str());
		}
	}

	std::vector<std::string>& common_screen_sizes()
	{
		static std::vector<std::string> res;
		if(res.empty()) {
			common_screen_sizes().emplace_back("3840 x 2160");
		}
		return res;
	}
}

void show_video_selection_dialog()
{
	// XX This code needs fixing.
	// Windowed mode should be any size up to maximum desktop resolution (we should provide a cut-down list of standard values).
	// The display modes are only for fullscreen.
	// we should have a flag of resizeable.
	// Font chosen should be based on the module configuration
	// There should be an option to save the screen dimensions for future use
	// There should be an option
	const int x = static_cast<int>(KRE::WindowManager::getMainWindow()->width()*0.1);
	const int y = static_cast<int>(KRE::WindowManager::getMainWindow()->height()*0.1);
	const int w = static_cast<int>(KRE::WindowManager::getMainWindow()->width()*0.8);
	const int h = static_cast<int>(KRE::WindowManager::getMainWindow()->height()*0.8);

	Dialog d(x,y,w,h);
	d.setBackgroundFrame("empty_window");
	d.setDrawBackgroundFn(draw_last_scene);

	const int button_width = 150;
	const int button_height = 40;
	const int padding = 20;

	int selected_mode = -1;

	std::function<WidgetPtr(const std::string&)> make_font_label;
	if(module::get_default_font() == "bitmap") {
		make_font_label = [](const std::string& label){
			return WidgetPtr(new GraphicalFontLabel(label, "door_label", 2));
		};
	} else {
		make_font_label = [](const std::string& label){
			return WidgetPtr(new Label(label, 16, module::get_default_font()));
		};
	}

	d.addWidget(make_font_label(_("Select video options:")), padding, padding);
	WindowModeList display_modes;
	int current_mode_index = enumerate_video_modes(&display_modes);
	if(!display_modes.empty()) {
		if(current_mode_index < 0 || static_cast<unsigned>(current_mode_index) >= display_modes.size()) {
			current_mode_index = 0;
		}
		std::vector<std::string> display_strings;
		map_modes_to_strings(display_modes, &display_strings);

		// Video mode list.
		DropdownWidget* mode_list = new DropdownWidget(display_strings, 260, 20);
		mode_list->setDropdownHeight(420);
		mode_list->setSelection(current_mode_index);
		mode_list->setZOrder(10);
		mode_list->setOnSelectHandler([&selected_mode](int selection,const std::string& s){
			selected_mode = selection;
		});
		d.addWidget(WidgetPtr(mode_list));
	} else {
		d.addWidget(make_font_label(_("Unable to enumerate video modes")));
	}

	
	preferences::ScreenMode fs_mode = preferences::get_screen_mode();
	if(!preferences::no_fullscreen_ever()) {
		// Fullscreen selection
		std::vector<std::string> fs_options;
		fs_options.emplace_back(_("Windowed Mode"));
		fs_options.emplace_back(_("Fullscreen Mode")); //Windowed-type fullscreen.
		DropdownWidget* fs_list = new DropdownWidget(fs_options, 260, 20);
		fs_list->setSelection(static_cast<int>(preferences::get_screen_mode()));
		fs_list->setZOrder(9);
		fs_list->setOnSelectHandler([&fs_mode](int selection,const std::string& s){
			switch(selection) {
				case 0:	fs_mode = preferences::ScreenMode::WINDOWED; break;
				case 1:	fs_mode = preferences::ScreenMode::FULLSCREEN_WINDOWED; break;
			}
		});
		d.addWidget(WidgetPtr(fs_list));
	}

	// Vertical sync options
	std::vector<std::string> vsync_options;
	vsync_options.push_back(_("No synchronisation"));
	vsync_options.push_back(_("Synchronised to retrace"));
	vsync_options.push_back(_("Late synchronisation"));
	DropdownWidget* synch_list = new DropdownWidget(vsync_options, 260, 20);
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

	WidgetPtr b_okay = new Button(make_font_label(_("OK")), [&d](){
		d.close();
	});
	WidgetPtr b_cancel = new Button(make_font_label(_("Cancel")), [&d](){
		d.cancel();
	});
	b_okay->setDim(button_width, button_height);
	b_cancel->setDim(button_width, button_height);
	d.addWidget(b_okay, 20, d.height() - button_height - 20);
	d.addWidget(b_cancel, d.width() - button_width - 20, d.height() - button_height - 20);

	d.showModal();
	if(d.cancelled() == false) {
		// set selected video mode here
		if(selected_mode >= 0 && static_cast<unsigned>(selected_mode) < display_modes.size()) {
			KRE::WindowManager::getMainWindow()->setWindowSize(display_modes[selected_mode].width, display_modes[selected_mode].height);
		}
		preferences::set_screen_mode(fs_mode);
	}
}
