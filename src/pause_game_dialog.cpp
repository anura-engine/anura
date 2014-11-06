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
#include <boost/bind.hpp>

#include "button.hpp"
#include "controls_dialog.hpp"
#include "slider.hpp"
#include "checkbox.hpp"
#include "dialog.hpp"
#include "draw_scene.hpp"
#include "graphical_font_label.hpp"
#include "i18n.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "module.hpp"
#include "pause_game_dialog.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "language_dialog.hpp"
#include "video_selections.hpp"
#include "widget_factory.hpp"

namespace {
void end_dialog(gui::dialog* d, PAUSE_GAME_RESULT* result, PAUSE_GAME_RESULT value)
{
	*result = value;
	d->close();
}

void do_draw_scene() {
	draw_scene(level::current(), last_draw_position());
}

}

PAUSE_GAME_RESULT show_pause_game_dialog()
{
	PAUSE_GAME_RESULT result = PAUSE_GAME_QUIT;
	
	int button_width = 220;//232;
	int button_height = 45;//50;
	int padding = 12;//16;
	int slider_width = 175;//200;
	bool show_exit = true;
	bool show_controls = true;
	bool show_button_swap = false;
	bool show_video_mode_select = true;
	bool show_of = false;
	gui::BUTTON_RESOLUTION button_resolution = gui::BUTTON_SIZE_DOUBLE_RESOLUTION;
	bool upscale_dialog_frame = true;
	
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
	show_exit = false;
	show_controls = false;
	show_button_swap = false;
	show_of = true;
#elif TARGET_BLACKBERRY
	show_exit = false;
	show_controls = false;
#endif

	using namespace gui;
	widget_ptr t1;
	widget_ptr t2;
	widget_ptr resume_label;
	widget_ptr controls_label;
	widget_ptr language_label;
	widget_ptr video_select_label;
	widget_ptr return_label;
	widget_ptr exit_label;
	widget_ptr button_swap_label;

	boost::shared_ptr<button::SetColorSchemeScope> color_scheme_scope;

	variant v;
	try {
		v = json::parse_from_file("data/pause-menu.cfg");
		ASSERT_LOG(v.is_map(), "\"data/pause-menu.cfg\" found but isn't a map.");
		variant button_color_scheme = v["button_color_scheme"];
		if(!button_color_scheme.is_null()) {
			color_scheme_scope.reset(new button::SetColorSchemeScope(button_color_scheme));
		}

		show_exit = v["show_exit"].as_bool(true);
		show_controls = v["show_controls"].as_bool(true);
		show_button_swap = v["show_button_swap"].as_bool(false);
		show_of = v["show_openfeint"].as_bool(false);
		if(v.has_key("button_width")) {
			button_width = v["button_width"].as_int();
		}
		if(v.has_key("button_height")) {
			button_height = v["button_height"].as_int();
		}
		if(v.has_key("button_padding")) {
			padding = v["button_padding"].as_int();
		}
		if(v.has_key("slider_width")) {
			slider_width = v["slider_width"].as_int();
		}
		if(v.has_key("button_resolution")) {
			if(v["button_resolution"].is_string()) {
				const std::string& res = v["button_resolution"].as_string();
				if(res == "double") {
					button_resolution = gui::BUTTON_SIZE_DOUBLE_RESOLUTION;
				} else {
					button_resolution = gui::BUTTON_SIZE_NORMAL_RESOLUTION;
				}
			} else if(v["button_resolution"].is_int()) {
				if(v["button_resolution"].as_int() == 0) {
					button_resolution = gui::BUTTON_SIZE_NORMAL_RESOLUTION;
				} else {
					button_resolution = gui::BUTTON_SIZE_DOUBLE_RESOLUTION;
				}
			} else {
				ASSERT_LOG(false, "Unrecognised button resolution, either string or int");
			}
		}
		if(v.has_key("dialog_upscale")) {
			upscale_dialog_frame = v["dialog_upscale"].as_bool();
		}
	} catch(...) {
		// skipping errors
	}
	if(v.is_null() == false && v.has_key("music_label")) {
		t1 = widget_factory::create(v["music_label"], NULL);
	} else {
		t1 = widget_ptr(new graphical_font_label(_("Music Volume:"), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("sound_label")) {
		t2 = widget_factory::create(v["sound_label"], NULL);
	} else {
		t2 = widget_ptr(new graphical_font_label(_("Sound Volume:"), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("resume_label")) {
		resume_label = widget_factory::create(v["resume_label"], NULL);
	} else {
		resume_label = widget_ptr(new graphical_font_label(_("Resume"), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("controls_label")) {
		controls_label = widget_factory::create(v["controls_label"], NULL);
	} else {
		controls_label = widget_ptr(new graphical_font_label(_("Controls..."), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("language_label")) {
		language_label = widget_factory::create(v["language_label"], NULL);
	} else {
		language_label = widget_ptr(new graphical_font_label(_("Language..."), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("video_select_label")) {
		video_select_label = widget_factory::create(v["video_select_label"], NULL);
	} else {
		video_select_label = widget_ptr(new graphical_font_label(_("Video Options..."), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("return_label")) {
		return_label = widget_factory::create(v["return_label"], NULL);
	} else {
		return_label = widget_ptr(new graphical_font_label(_("Return to Titlescreen"), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("button_swap_label")) {
		button_swap_label = widget_factory::create(v["button_swap_label"], NULL);
	} else {
		button_swap_label = widget_ptr(new graphical_font_label(_("Reverse A and B"), "door_label", 2));
	}
	if(module::get_module_args() != NULL) {
		variant mod_args = module::get_module_args()->query_value("from_lobby");
		if(mod_args.is_bool() && mod_args.as_bool() == true && module::get_module_name() != "lobby") {
			return_label->set_value("text", variant(_("Return to Lobby")));
		}
	}
	if(v.is_null() == false && v.has_key("exit_label")) {
		exit_label = widget_factory::create(v["exit_label"], NULL);
	} else {
		exit_label = widget_ptr(new graphical_font_label(_("Exit Game"), "door_label", 2));
	}
	ASSERT_LOG(t1 != NULL, "Couldn't create music label widget.");
	ASSERT_LOG(t2 != NULL, "Couldn't create sound label widget.");
	ASSERT_LOG(resume_label != NULL, "Couldn't create resume label widget.");
	ASSERT_LOG(controls_label != NULL, "Couldn't create controls label widget.");
	ASSERT_LOG(language_label != NULL, "Couldn't create language label widget.");
	ASSERT_LOG(video_select_label != NULL, "Couldn't create video select label widget.");
	ASSERT_LOG(return_label != NULL, "Couldn't create return label widget.");
	ASSERT_LOG(exit_label != NULL, "Couldn't create exit label widget.");
	ASSERT_LOG(button_swap_label != NULL, "Couldn't create button swap label widget.");

	widget_ptr s1(new slider(slider_width, boost::bind(sound::set_music_volume, _1), sound::get_music_volume()));
	widget_ptr s2(new slider(slider_width, boost::bind(sound::set_sound_volume, _1), sound::get_sound_volume()));

	// Prevents them from being selectable as tab items when using a controller, keys.
	t1->set_tab_stop(-1);
	t2->set_tab_stop(-1);

	const int num_buttons = 3 + show_exit + show_controls + show_button_swap + show_of + show_video_mode_select;
	int window_w, window_h;
	if(preferences::virtual_screen_height() >= 600) {
		window_w = button_width + padding*4;
		window_h = button_height * num_buttons + t1->height()*2 + s1->height()*2 + padding*(3+4+num_buttons);
	} else {
		window_w = button_width*2 + padding*5;
		window_h = button_height * num_buttons/2 + t1->height() + s1->height() + padding*(3+2+num_buttons/2);
	}
	dialog d((preferences::virtual_screen_width()/2 - window_w/2) & ~1, (preferences::virtual_screen_height()/2 - window_h/2) & ~1, window_w, window_h);
	d.set_padding(padding);
	d.set_background_frame("empty_window");
	d.set_upscale_frame(upscale_dialog_frame);

	d.set_draw_background_fn(do_draw_scene);

	button_ptr b1(new button(resume_label, boost::bind(end_dialog, &d, &result, PAUSE_GAME_CONTINUE), BUTTON_STYLE_NORMAL, button_resolution));
	button_ptr b2(new button(controls_label, show_controls_dialog, BUTTON_STYLE_NORMAL, button_resolution));
	button_ptr language_button(new button(language_label, show_language_dialog, BUTTON_STYLE_NORMAL, button_resolution));
	button_ptr b3(new button(return_label, boost::bind(end_dialog, &d, &result, PAUSE_GAME_GO_TO_TITLESCREEN), BUTTON_STYLE_NORMAL, button_resolution));
	button_ptr b4(new button(exit_label, boost::bind(end_dialog, &d, &result, PAUSE_GAME_QUIT), BUTTON_STYLE_DEFAULT, button_resolution));
	button_ptr b5(new checkbox(button_swap_label, preferences::reverse_ab(), boost::bind(preferences::set_reverse_ab, _1), button_resolution));
	button_ptr b_video(new button(video_select_label, show_video_selection_dialog, BUTTON_STYLE_NORMAL, button_resolution));

	
	b1->set_dim(button_width, button_height);
	b2->set_dim(button_width, button_height);
	b3->set_dim(button_width, button_height);
	b4->set_dim(button_width, button_height);
	b5->set_dim(button_width, button_height);
	language_button->set_dim(button_width, button_height);
	b_video->set_dim(button_width, button_height);
	
	d.set_padding(padding-12);
	d.add_widget(t1, padding*2, padding*2);
	d.set_padding(padding+12);
	d.add_widget(s1);

	if(preferences::virtual_screen_height() >= 600) {
		d.set_padding(padding-12);
		d.add_widget(t2);
		d.set_padding(padding+12);
		d.add_widget(s2);
		d.set_padding(padding);
		if(show_button_swap) { d.add_widget(b5); }
		d.add_widget(b1);
		if(show_controls) { d.add_widget(b2); }
		if(show_video_mode_select) { d.add_widget(b_video); }
		d.add_widget(language_button);
		d.add_widget(b3);
		if(show_exit) { d.add_widget(b4); }
	} else {
		d.set_padding(padding);
		d.add_widget(b1);
		if(show_controls) { d.add_widget(b2); }
		if(show_video_mode_select) { d.add_widget(b_video); }
		d.set_padding(padding-12);
		d.add_widget(t2, padding*3 + button_width, padding*2);
		d.set_padding(padding+12);
		d.add_widget(s2);
		d.set_padding(padding);
		d.add_widget(language_button);
		d.add_widget(b3);
		if(show_exit) { d.add_widget(b4); }
	}

	d.set_on_quit(boost::bind(end_dialog, &d, &result, PAUSE_GAME_QUIT));
	d.show_modal();
	if(d.cancelled() && result == PAUSE_GAME_QUIT) {
		result = PAUSE_GAME_CONTINUE;
	}

	return result;
}
