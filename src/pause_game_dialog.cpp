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

#include "WindowManager.hpp"

#include "button.hpp"
#include "controls_dialog.hpp"
#include "slider.hpp"
#include "checkbox.hpp"
#include "dialog.hpp"
#include "draw_scene.hpp"
#include "graphical_font_label.hpp"
#include "i18n.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "pause_game_dialog.hpp"
#include "preferences.hpp"
#include "screen_handling.hpp"
#include "sound.hpp"
#include "language_dialog.hpp"
#include "video_selections.hpp"
#include "widget_factory.hpp"

namespace 
{
	void end_dialog(gui::Dialog* d, PAUSE_GAME_RESULT* result, PAUSE_GAME_RESULT value)
	{
		*result = value;
		d->close();
	}
}

PAUSE_GAME_RESULT show_pause_game_dialog()
{
	PAUSE_GAME_RESULT result = PAUSE_GAME_RESULT::QUIT;
	
	int button_width = 220;//232;
	int button_height = 45;//50;
	int padding = 12;//16;
	int slider_width = 175;//200;
	bool show_exit = true;
	bool show_controls = true;
	bool show_video_mode_select = true;
	bool show_of = false;
	bool show_language = true;
	gui::BUTTON_RESOLUTION buttonResolution = gui::BUTTON_SIZE_DOUBLE_RESOLUTION;
	bool upscale_dialog_frame = true;
	
	using namespace gui;
	using std::placeholders::_1;

	WidgetPtr t1;
	WidgetPtr t2;
	WidgetPtr resume_label;
	WidgetPtr controls_label;
	WidgetPtr language_label;
	WidgetPtr video_select_label;
	WidgetPtr return_label;
	WidgetPtr exit_label;
	WidgetPtr button_swap_label;

	std::shared_ptr<Button::SetColorSchemeScope> color_scheme_scope;

	variant v;
	try {
		v = json::parse_from_file("data/pause-menu.cfg");
		ASSERT_LOG(v.is_map(), "\"data/pause-menu.cfg\" found but isn't a map.");
		variant button_color_scheme = v["button_color_scheme"];
		if(!button_color_scheme.is_null()) {
			color_scheme_scope.reset(new Button::SetColorSchemeScope(button_color_scheme));
		}

		show_exit = v["show_exit"].as_bool(true);
		show_controls = v["show_controls"].as_bool(true);
		show_of = v["show_openfeint"].as_bool(false);
		show_video_mode_select = v["show_video_mode_select"].as_bool(true);
		show_language = v["show_language"].as_bool(true);
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
		if(v.has_key("buttonResolution")) {
			if(v["buttonResolution"].is_string()) {
				const std::string& res = v["buttonResolution"].as_string();
				if(res == "double") {
					buttonResolution = gui::BUTTON_SIZE_DOUBLE_RESOLUTION;
				} else {
					buttonResolution = gui::BUTTON_SIZE_NORMAL_RESOLUTION;
				}
			} else if(v["buttonResolution"].is_int()) {
				if(v["buttonResolution"].as_int() == 0) {
					buttonResolution = gui::BUTTON_SIZE_NORMAL_RESOLUTION;
				} else {
					buttonResolution = gui::BUTTON_SIZE_DOUBLE_RESOLUTION;
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
		t1 = widget_factory::create(v["music_label"], nullptr);
	} else {
		t1 = WidgetPtr(new GraphicalFontLabel(_("Music Volume:"), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("sound_label")) {
		t2 = widget_factory::create(v["sound_label"], nullptr);
	} else {
		t2 = WidgetPtr(new GraphicalFontLabel(_("Sound Volume:"), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("resume_label")) {
		resume_label = widget_factory::create(v["resume_label"], nullptr);
	} else {
		resume_label = WidgetPtr(new GraphicalFontLabel(_("Resume"), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("controls_label")) {
		controls_label = widget_factory::create(v["controls_label"], nullptr);
	} else {
		controls_label = WidgetPtr(new GraphicalFontLabel(_("Controls..."), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("language_label")) {
		language_label = widget_factory::create(v["language_label"], nullptr);
	} else {
		language_label = WidgetPtr(new GraphicalFontLabel(_("Language..."), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("video_select_label")) {
		video_select_label = widget_factory::create(v["video_select_label"], nullptr);
	} else {
		video_select_label = WidgetPtr(new GraphicalFontLabel(_("Video Options..."), "door_label", 2));
	}
	if(v.is_null() == false && v.has_key("return_label")) {
		return_label = widget_factory::create(v["return_label"], nullptr);
	} else {
		return_label = WidgetPtr(new GraphicalFontLabel(_("Return to Titlescreen"), "door_label", 2));
	}
	if(module::get_module_args() != nullptr) {
		variant mod_args = module::get_module_args()->queryValue("from_lobby");
		if(mod_args.is_bool() && mod_args.as_bool() == true && module::get_module_name() != "lobby") {
			return_label->setValue("text", variant(_("Return to Lobby")));
		}
	}
	if(v.is_null() == false && v.has_key("exit_label")) {
		exit_label = widget_factory::create(v["exit_label"], nullptr);
	} else {
		exit_label = WidgetPtr(new GraphicalFontLabel(_("Exit Game"), "door_label", 2));
	}
	ASSERT_LOG(t1 != nullptr, "Couldn't create music label widget.");
	ASSERT_LOG(t2 != nullptr, "Couldn't create sound label widget.");
	ASSERT_LOG(resume_label != nullptr, "Couldn't create resume label widget.");
	ASSERT_LOG(controls_label != nullptr, "Couldn't create controls label widget.");
	ASSERT_LOG(!show_language || language_label != nullptr, "Couldn't create language label widget.");
	ASSERT_LOG(video_select_label != nullptr, "Couldn't create video select label widget.");
	ASSERT_LOG(return_label != nullptr, "Couldn't create return label widget.");
	ASSERT_LOG(exit_label != nullptr, "Couldn't create exit label widget.");

	using namespace std::placeholders;
	WidgetPtr s1(new Slider(slider_width, std::bind(sound::set_music_volume, _1), sound::get_music_volume()));
	WidgetPtr s2(new Slider(slider_width, std::bind(sound::set_sound_volume, _1), sound::get_sound_volume()));

	// Prevents them from being selectable as tab items when using a controller, keys.
	t1->setTabStop(-1);
	t2->setTabStop(-1);

	const int num_buttons = 2 + show_exit + show_controls + show_of + show_video_mode_select + show_language;
	int window_w, window_h;
	if(graphics::GameScreen::get().getHeight() >= 600) {
		window_w = button_width + padding*4;
		window_h = button_height * num_buttons + t1->height()*2 + s1->height()*2 + padding*(3+4+num_buttons);
	} else {
		window_w = button_width*2 + padding*5;
		window_h = button_height * num_buttons/2 + t1->height() + s1->height() + padding*(3+2+num_buttons/2);
	}

	const int screen_w = graphics::GameScreen::get().getWidth();
	const int screen_h = graphics::GameScreen::get().getHeight();
	Dialog dd((screen_w/2 - window_w/2) & ~1, (screen_h/2 - window_h/2) & ~1, window_w, window_h);
	dd.setPadding(padding);
	dd.setBackgroundFrame("empty_window");
	dd.setUpscaleFrame(upscale_dialog_frame);
	dd.setDrawBackgroundFn(draw_last_scene);
							
	ButtonPtr b1(new Button(resume_label, std::bind(end_dialog, &dd, &result, PAUSE_GAME_RESULT::CONTINUE), BUTTON_STYLE_NORMAL, buttonResolution));
	ButtonPtr b2(new Button(controls_label, show_controls_dialog, BUTTON_STYLE_NORMAL, buttonResolution));
	ButtonPtr b3(new Button(return_label, std::bind(end_dialog, &dd, &result, PAUSE_GAME_RESULT::GO_TO_TITLESCREEN), BUTTON_STYLE_NORMAL, buttonResolution));
	ButtonPtr b4(new Button(exit_label, std::bind(end_dialog, &dd, &result, PAUSE_GAME_RESULT::QUIT), BUTTON_STYLE_DEFAULT, buttonResolution));
	ButtonPtr b_video(new Button(video_select_label, show_video_selection_dialog, BUTTON_STYLE_NORMAL, buttonResolution));
	
	b1->setDim(button_width, button_height);
	b2->setDim(button_width, button_height);
	b3->setDim(button_width, button_height);
	b4->setDim(button_width, button_height);
	b_video->setDim(button_width, button_height);
	
	ButtonPtr language_button = nullptr;
	if(show_language) {
		language_button = new Button(language_label, show_language_dialog, BUTTON_STYLE_NORMAL, buttonResolution);
		language_button->setDim(button_width, button_height);
	}

	dd.setPadding(padding-12);
	dd.addWidget(t1, padding*2, padding*2);
	dd.setPadding(padding+12);
	dd.addWidget(s1);

	if(screen_h >= 600) {
		dd.setPadding(padding-12);
		dd.addWidget(t2);
		dd.setPadding(padding+12);
		dd.addWidget(s2);
		dd.setPadding(padding);
		dd.addWidget(b1);
		if(show_controls) { dd.addWidget(b2); }
		if(show_video_mode_select) { dd.addWidget(b_video); }
		if(show_language) { dd.addWidget(language_button); }
		dd.addWidget(b3);
		if(show_exit) { dd.addWidget(b4); }
	} else {
		dd.setPadding(padding);
		dd.addWidget(b1);
		if(show_controls) { dd.addWidget(b2); }
		if(show_video_mode_select) { dd.addWidget(b_video); }
		dd.setPadding(padding-12);
		dd.addWidget(t2, padding*3 + button_width, padding*2);
		dd.setPadding(padding+12);
		dd.addWidget(s2);
		dd.setPadding(padding);
		if(show_language) { dd.addWidget(language_button); }
		dd.addWidget(b3);
		if(show_exit) { dd.addWidget(b4); }
	}

	//d->setOnQuit(std::bind(end_dialog, d, &result, PAUSE_GAME_RESULT::QUIT));
	dd.setOnQuit([&dd, &result](){ end_dialog(&dd, &result, PAUSE_GAME_RESULT::QUIT); });
	dd.showModal();
	if(dd.cancelled() && result == PAUSE_GAME_RESULT::QUIT) {
		result = PAUSE_GAME_RESULT::CONTINUE;
	}
	return result;
}
