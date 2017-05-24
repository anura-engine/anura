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

#pragma once

#include <set>
#include <string>

#include "uri.hpp"
#include "variant.hpp"

namespace game_logic 
{
	class FormulaCallable;
}

namespace preferences 
{
	enum class ScreenMode {
		WINDOWED,
		FULLSCREEN_WINDOWED,
	};

	game_logic::FormulaCallable* get_settings_obj();

	void register_module_setting(const std::string& id, variant value);
	variant get_module_settings();

	int register_string_setting(const std::string& id, bool persistent, std::string* value, const char* helpstring);
	int register_int_setting(const std::string& id, bool persistent, int* value, const char* helpstring);
	int register_bool_setting(const std::string& id, bool persistent, bool* value, const char* helpstring);
	int register_float_setting(const std::string& id, bool persistent, double* value, const char* helpstring);

	std::string get_registered_helpstring();

#define PREF_BOOL(id, default_value, helpstring) \
	bool g_##id = default_value; \
	int g_##id##_dummy = preferences::register_bool_setting(#id, false, &g_##id, helpstring)

#define PREF_FLOAT(id, default_value, helpstring) \
	double g_##id = default_value; \
	int g_##id##_dummy = preferences::register_float_setting(#id, false, &g_##id, helpstring)

#define PREF_INT(id, default_value, helpstring) \
	int g_##id = default_value; \
	int g_##id##_dummy = preferences::register_int_setting(#id, false, &g_##id, helpstring)

#define PREF_STRING(id, default_value, helpstring) \
	std::string g_##id = default_value; \
	int g_##id##_dummy = preferences::register_string_setting(#id, false, &g_##id, helpstring)

#define PREF_BOOL_PERSISTENT(id, default_value, helpstring) \
	bool g_##id = default_value; \
	int g_##id##_dummy = preferences::register_bool_setting(#id, true, &g_##id, helpstring)

#define PREF_FLOAT_PERSISTENT(id, default_value, helpstring) \
	double g_##id = default_value; \
	int g_##id##_dummy = preferences::register_float_setting(#id, true, &g_##id, helpstring)

#define PREF_INT_PERSISTENT(id, default_value, helpstring) \
	int g_##id = default_value; \
	int g_##id##_dummy = preferences::register_int_setting(#id, true, &g_##id, helpstring)

#define PREF_STRING_PERSISTENT(id, default_value, helpstring) \
	std::string g_##id = default_value; \
	int g_##id##_dummy = preferences::register_string_setting(#id, true, &g_##id, helpstring)

	const std::vector<std::string>& argv();
	void set_argv(const std::vector<std::string>& args);

	const std::string& version();
	const variant& version_decimal();
	int get_unique_user_id();

	bool parse_arg(const std::string& arg, const std::string& next_arg);
	bool no_sound();
	bool no_music();

	void set_preferences_path(const std::string& path);
	void set_preferences_path_from_module(const std::string& name);

	bool setup_preferences_dir();

	const char* user_data_path();
	const char* save_file_path();
	const char* auto_save_file_path();
	bool editor_save_to_user_preferences();

	std::string dlc_path();
	void expand_data_paths();
	void set_save_slot(const std::string& fname);
	bool show_debug_hitboxes();
	bool toogle_debug_hitboxes();
	bool edit_and_continue();
	void set_edit_and_continue(bool value);

	// Control scheme to use on iOS or other touch systems
	const std::string& control_scheme();
	void set_control_scheme(const std::string& scheme);
	
	//whether we are debugging
	bool debug();
	
	int frame_time_millis();
	bool has_alt_frame_time();

	struct alt_frame_time_scope {
		explicit alt_frame_time_scope(bool value);
		~alt_frame_time_scope();
		bool active() const { return active_; }
	private:
		int old_value_;
		bool active_;
	};

	//whether to show the fps display at the top
	bool show_fps();
	void set_show_fps(bool show);

	bool use_joystick();

	//load compiled data from data/compiled/
	bool load_compiled();

	void set_load_compiled(bool value);
	
	void set_edit_on_start(bool value);
	bool edit_on_start();

	// Configured language
	const std::string& locale();
	void setLocale(const std::string& value);

	//this is the mask which we apply to all x,y values before drawing, to
	//avoid drawing things at "half pixels" when the actual screen dimensions
	//are lower than the virtual screen dimensions.
	extern int xypos_draw_mask;

	//this is a flag set to true iff we are in a mode where we write
	//'compiled' tile output.
	extern bool compiling_tiles;
	
	bool auto_size_window();
	int requested_window_width();
	int requested_window_height();
	int requested_virtual_window_width();
	int requested_virtual_window_height();
	void adjust_virtual_width_to_match_physical(int width, int height);
	bool is_resizeable();
	ScreenMode get_screen_mode();
	void set_screen_mode(ScreenMode mode);
	bool no_fullscreen_ever();
	
	bool allow_autopause();

	bool screen_rotated();
	
	bool force_no_npot_textures();

	bool use_16bpp_textures();

	void set_32bpp_textures_if_kb_memory_at_least(int memory_required );
    
	bool send_stats();

	int force_difficulty();

	bool record_history();
	void set_record_history(bool value);

	bool relay_through_server();
	void set_relay_through_server(bool value);

	variant external_code_editor();

	bool run_failing_unit_tests();

	bool serialize_bad_objects();
	
	bool die_on_assert();

	bool type_safety_checks();

	game_logic::FormulaCallable* registry();

	void load_preferences();
	void save_preferences();

	uri::uri get_tbs_uri();
	std::string get_username();
	std::string get_password();
	void set_username(const std::string& uname);
	void set_password(const std::string& pword);
	variant get_cookie();
	void set_cookie(const variant &v);

	bool internal_tbs_server();
	const std::set<std::string>& get_build_options();

	variant ffl_interface();
}
