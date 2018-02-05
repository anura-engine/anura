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

#include <iostream>
#include <algorithm>
#include <string>
#include <iomanip>

#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/sha1.hpp>

#include "asserts.hpp"
#include "controls.hpp"
#include "difficulty.hpp"
#include "editor.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula_callable.hpp"
#include "game_registry.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "sys.hpp"
#include "string_utils.hpp"
#include "variant_utils.hpp"

#include <time.h>

#define SAVE_FILENAME					"save.cfg"
#define AUTOSAVE_FILENAME				"autosave.cfg"

#if defined(_MSC_VER)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>

std::string multi_byte_from_wide_string(LPCWSTR pwsz, UINT cp) 
{
    int cch = WideCharToMultiByte(cp, 0, pwsz, -1, 0, 0, nullptr, nullptr);
    char* psz = new char[cch];
    WideCharToMultiByte(cp, 0, pwsz, -1, psz, cch, nullptr, nullptr);
    std::string st(psz);
    delete[] psz;
	return st;
}

std::string get_windows_error_as_string()
{
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		nullptr 
	);

#if defined(_UNICODE)
	std::string res = multi_byte_from_wide_string((LPCTSTR)lpMsgBuf, CP_UTF8);
#else
	std::string res((LPCSTR)lpMsgBuf);
#endif
	// Free the buffer.
	::LocalFree(lpMsgBuf);
	return res;
}

class WindowsPrefs
{
public:
	std::string getPreferencePath() const   { return app_data_path_ + "/" + module::get_module_name() + "/";; }
	std::string getSaveFilePath() const     { return getPreferencePath() + SAVE_FILENAME; }
	std::string getAutoSaveFilePath() const { return getPreferencePath() + AUTOSAVE_FILENAME; }
	const std::string& getAppDataPath() const      { return app_data_path_; }
	static WindowsPrefs& getInstance() {
		static WindowsPrefs res;
		return res;
	}
private:
	WindowsPrefs() {
		char szPath[MAX_PATH];
		if(SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, szPath))) {
			app_data_path_ = std::string(szPath);
		} else {
			ASSERT_LOG(false, "Failed to read the application data path: " << get_windows_error_as_string());
		}
	}
	std::string app_data_path_;
};

#endif // _MSC_VER


namespace preferences 
{
	namespace 
	{
		struct RegisteredSetting 
		{
			RegisteredSetting() : persistent(false), has_been_set_from_persistent(false), int_value(nullptr), bool_value(nullptr), double_value(nullptr), string_value(nullptr), variant_value(nullptr), helpstring(nullptr)
			{}
			variant write() const {
				if(int_value) {
					return variant(*int_value);
				} else if(string_value) {
					return variant(*string_value);
				} else if(bool_value) {
					return variant::from_bool(*bool_value);
				} else if(double_value) {
					return variant(*double_value);
				} else if(variant_value) {
					return *variant_value;
				} else {
					return variant();
				}
			}

			void read(variant value) {
				if(int_value && value.is_int()) {
					*int_value = value.as_int();
				} else if(string_value && value.is_string()) {
					*string_value = value.as_string();
				} else if(bool_value && (value.is_bool() || value.is_int())) {
					*bool_value = value.as_bool();
				} else if(double_value && (value.is_decimal() || value.is_int())) {
					*double_value = value.as_decimal().as_float();
				} else if(variant_value) {
					*variant_value = value;
				}
			}
			bool persistent, has_been_set_from_persistent;
			int* int_value;
			bool* bool_value;
			double* double_value;
			std::string* string_value;
			variant* variant_value;
			const char* helpstring;
		};

		std::map<std::string, RegisteredSetting>& g_registered_settings() {
			static std::map<std::string, RegisteredSetting> instance;
			return instance;
		}

		class SettingsObject : public game_logic::FormulaCallable
		{
		private:
			variant getValue(const std::string& key) const override {
				if(key == "dir") {
					std::vector<variant> result;
					for(std::map<std::string, RegisteredSetting>::iterator itor = g_registered_settings().begin(); itor != g_registered_settings().end(); ++itor) {
						result.push_back(variant(itor->first));
					}

					return variant(&result);
				}

				std::map<std::string, RegisteredSetting>::const_iterator itor = g_registered_settings().find(key);
				if(itor == g_registered_settings().end()) {
					return variant();
				}

				if(itor->second.int_value) {
					return variant(*itor->second.int_value);
				} else if(itor->second.string_value) {
					return variant(*itor->second.string_value);
				} else if(itor->second.bool_value) {
					return variant::from_bool(*itor->second.bool_value);
				} else if(itor->second.double_value) {
					return variant(*itor->second.double_value);
				} else {
					return variant();
				}
			}

			void setValue(const std::string& key, const variant& value) override {
				std::map<std::string, RegisteredSetting>::iterator itor = g_registered_settings().find(key);
				if(itor == g_registered_settings().end()) {
					return;
				}

				if(itor->second.int_value) {
					*itor->second.int_value = value.as_int();
				} else if(itor->second.string_value) {
					*itor->second.string_value = value.as_string();
				} else if(itor->second.double_value) {
					*itor->second.double_value = value.as_decimal().as_float();
				}
			}

			void getInputs(std::vector<game_logic::FormulaInput>* inputs) const override {
				for(std::map<std::string, RegisteredSetting>::iterator itor = g_registered_settings().begin(); itor != g_registered_settings().end(); ++itor) {
					inputs->push_back(game_logic::FormulaInput(itor->first, game_logic::FORMULA_ACCESS_TYPE::READ_WRITE));
				}
			}
		};
	}

	game_logic::FormulaCallable* get_settings_obj()
	{
		static ffl::IntrusivePtr<game_logic::FormulaCallable> obj(new SettingsObject);
		return obj.get();
	}

	namespace {
	std::map<std::string, variant> g_module_settings;
	variant g_module_settings_variant;
	}

	void register_module_setting(const std::string& id, variant value)
	{
		if(g_module_settings.count(id) == 0) {
			ASSERT_LOG(g_registered_settings().count(id) == 0, "Multiple definition of module setting, mirrors built-in: " << id);
			g_module_settings_variant = variant();
			RegisteredSetting& setting = g_registered_settings()[id];
			setting.persistent = false;
			setting.variant_value = &g_module_settings[id];
			*setting.variant_value = value;
		}
	}

	variant get_module_settings()
	{
		if(g_module_settings_variant.is_null()) {
			std::map<variant,variant> result;
			for(auto p : g_module_settings) {
				result[variant(p.first)] = p.second;
			}

			g_module_settings_variant = variant(&result);
		}

		return g_module_settings_variant;
	}

	int register_string_setting(const std::string& id, bool persistent, std::string* value, const char* helpstring)
	{
		ASSERT_LOG(g_registered_settings().count(id) == 0, "Multiple definition of registered setting: " << id);
		RegisteredSetting& setting = g_registered_settings()[id];
		setting.string_value = value;
		setting.persistent = persistent;
		setting.helpstring = helpstring;
		return static_cast<int>(g_registered_settings().size());
	}

	int register_int_setting(const std::string& id, bool persistent, int* value, const char* helpstring)
	{
		ASSERT_LOG(g_registered_settings().count(id) == 0, "Multiple definition of registered setting: " << id);
		RegisteredSetting& setting = g_registered_settings()[id];
		setting.int_value = value;
		setting.persistent = persistent;
		setting.helpstring = helpstring;
		return static_cast<int>(g_registered_settings().size());
	}

	int register_bool_setting(const std::string& id, bool persistent, bool* value, const char* helpstring)
	{
		ASSERT_LOG(g_registered_settings().count(id) == 0, "Multiple definition of registered setting: " << id);
		RegisteredSetting& setting = g_registered_settings()[id];
		setting.bool_value = value;
		setting.persistent = persistent;
		setting.helpstring = helpstring;
		return static_cast<int>(g_registered_settings().size());
	}

	int register_float_setting(const std::string& id, bool persistent, double* value, const char* helpstring)
	{
		ASSERT_LOG(g_registered_settings().count(id) == 0, "Multiple definition of registered setting: " << id);
		RegisteredSetting& setting = g_registered_settings()[id];
		setting.double_value = value;
		setting.persistent = persistent;
		setting.helpstring = helpstring;
		return static_cast<int>(g_registered_settings().size());
	}

	std::string get_registered_helpstring()
	{
		std::string return_value;
		for(std::map<std::string, RegisteredSetting>::const_iterator i = g_registered_settings().begin(); i != g_registered_settings().end(); ++i) {
			std::ostringstream s;
			s << "        --";
			if(i->second.bool_value) {
				s << "[no-]";
			}

			s << i->first;
			if(i->second.int_value) {
				s << "=" << *i->second.int_value;
			} else if(i->second.string_value) {
				s << "=" << *i->second.string_value;
			} else if(i->second.bool_value) {
				s << " (default: " << (*i->second.bool_value ? "true" : "false") << ")";
			} else if(i->second.double_value) {
				s << "=" << *i->second.double_value;
			}

			std::string result = s.str();
			while(result.size() < 32) {
				result += " ";
			}

			if(i->second.helpstring) {
				result += i->second.helpstring;
			}
			result += "\n";
			return_value += result;
		}

		return return_value;
	}

	namespace {
		std::vector<std::string> g_program_argv;
	}

	const std::vector<std::string>& argv() {
		return g_program_argv;
	}

	void set_argv(const std::vector<std::string>& args) {
		g_program_argv = args;
	}

	const std::string& version() {
		static const std::string Version = "1.4";
		return Version;
	}
	
	const variant& version_decimal() {
		static const variant versiond = variant(decimal::from_string(version()));
		return versiond;
	}
	
	namespace 
	{
		int unique_user_id = 0;
		
		bool no_sound_ = false;
		bool no_music_ = false;
		bool show_debug_hitboxes_ = false;
		bool edit_and_continue_ = false;
		bool show_iphone_controls_ = false;
		bool use_pretty_scaling_ = false;
		ScreenMode fullscreen_ = ScreenMode::WINDOWED;
		bool fullscreen_disabled_ = false;
		bool resizable_ = false;
		bool proportional_resize_ = false;
		bool debug_ = true;
		bool reverse_ab_ = false;
		bool show_fps_ = false;
		int frame_time_millis_ = 20;
		int alt_frame_time_millis_ = -1;
		bool no_iphone_controls_ = false;
		bool allow_autopause_ = false;
		bool screen_dimensions_are_persistent = false;
		
		bool relay_through_server_ = false;
		
		std::string control_scheme_ = "iphone_2d";
		
		bool record_history_ = false;
		
		bool edit_on_start_ = false;
		
		variant external_code_editor_;
		
		int force_difficulty_ = std::numeric_limits<int>::min();
		
		uri::uri tbs_uri_ = uri::uri::parse("http://localhost:23456");
		
		std::string username_;
		std::string password_;
		variant cookie_;

		bool internal_tbs_server_ = false;

		std::string locale_;
		
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
		
#ifndef PREFERENCES_PATH
#define PREFERENCES_PATH "../Documents/"
#endif
		bool send_stats_ = false;
		
		bool screen_rotated_ = true;
		
		bool use_joystick_ = false;
		
		bool load_compiled_ = true;
		
		bool use_16bpp_textures_ = false;
#elif defined(TARGET_OS_HARMATTAN)
		
		bool send_stats_ = false;
		
#ifndef PREFERENCES_PATH
#define PREFERENCES_PATH "~/.frogatto/"
#endif
		bool screen_rotated_ = false;
		
		bool use_joystick_ = true;
		
		bool load_compiled_ = true;
		
		bool use_fbo_ = true;
		bool use_bequ_ = true;
		
		bool use_16bpp_textures_ = true;
#elif defined(__ANDROID__)
		
#ifndef PREFERENCES_PATH
#define PREFERENCES_PATH ".frogatto/"
#endif
		
		bool send_stats_ = false;
		
		bool screen_rotated_ = false;
		
		bool use_joystick_ = true;
		
		bool load_compiled_ = true;
		
		bool use_fbo_ = true;
		bool use_bequ_ = true;
		
		bool use_16bpp_textures_ = true;
#elif defined(TARGET_BLACKBERRY)
		
#ifndef PREFERENCES_PATH
#define PREFERENCES_PATH "~/.frogatto/"
#endif
		bool send_stats_ = true;
		bool use_joystick_ = true;
		bool screen_rotated_ = false;
		bool load_compiled_ = true;
		bool use_fbo_ = true;
		bool use_bequ_ = true;
		bool use_16bpp_textures_ = false;
		
#else
		
#if defined(_MSC_VER)
#define PREFERENCES_PATH ""
#endif // _MSC_VER
		
#ifndef NO_UPLOAD_STATS
		bool send_stats_ = true;
#else
		bool send_stats_ = false;
#endif
		
#ifndef PREFERENCES_PATH
#define PREFERENCES_PATH "~/.frogatto/"
#endif
		bool screen_rotated_ = false;
		
		bool use_joystick_ = true;
		
#if defined(TARGET_TEGRA)
		bool load_compiled_ = true;
		bool use_fbo_ = true;
		bool use_bequ_ = true;
#else
		bool load_compiled_ = false;
#endif
		
#if defined(TARGET_PANDORA)
        bool use_fbo_ = true;
        bool use_bequ_ = true;
#endif
		
		bool use_16bpp_textures_ = false;
#endif
		
		std::string preferences_path_ = PREFERENCES_PATH;
		std::string save_file_path_ = PREFERENCES_PATH SAVE_FILENAME;
		std::string auto_save_file_path_ = PREFERENCES_PATH AUTOSAVE_FILENAME;

#ifdef __APPLE__
#include "TargetConditionals.h"
#if TARGET_OS_MAC
		bool editor_save_to_user_preferences_ = true;
#endif
#else
		bool editor_save_to_user_preferences_ = false;
#endif
		
		bool force_no_npot_textures_ = false;
		
		bool run_failing_unit_tests_ = false;
		bool serialize_bad_objects_ = true;
		bool die_on_assert_ = false;
		bool type_safety_checks_ = true;

		int requested_window_width_ = 0;
		int requested_window_height_ = 0;

		PREF_BOOL(auto_size_window, true, "If true, window is auto-sized");
		PREF_INT(virtual_window_width, 0, "Virtual width of the game window");
		PREF_INT(virtual_window_height, 0, "Virtual height of the game window");

		PREF_INT(virtual_window_width_max, 0, "If set, the virtual width of the game window can be adjusted up to this amount, to match the aspect ratio of the physical device");
	
	}
	
	int xypos_draw_mask = ~1;
	bool compiling_tiles = false;

	int get_unique_user_id() {
		if(unique_user_id == 0) {
			time_t t1;
			time(&t1);
			int tm = static_cast<int>(t1);
			unique_user_id = tm^rand();
		}
		
		return unique_user_id;
	}
	
	bool no_sound() {
		return no_sound_;
	}
	
	bool no_music() {
		return no_music_;
	}
	
	bool setup_preferences_dir()
	{
		return !sys::get_dir(user_data_path()).empty();
	}
	
	void set_preferences_path_from_module( const std::string& name)
	{
#ifdef _MSC_VER
		preferences::set_preferences_path(WindowsPrefs::getInstance().getAppDataPath() + "/" + name + "/"); 
#elif defined(__ANDROID__)
		preferences::set_preferences_path("." + name + "/");
#elif __APPLE__
#include "TargetConditionals.h"
#if TARGET_OS_MAC
		preferences::set_preferences_path("~/Library/Application Support/" + name + "/");
		//preferences::set_preferences_path(std::string(getenv("HOME")) + "/Library/Application Support/" + name + "/");
#endif
#else
		preferences::set_preferences_path("~/." + name + "/");
#endif
		save_file_path_ = preferences_path_ + SAVE_FILENAME;
		auto_save_file_path_ = preferences_path_ + AUTOSAVE_FILENAME;	
	}
	
	void set_preferences_path(const std::string& path)
	{
		LOG_INFO("SET PREFERENCES PATH: " << path);
		preferences_path_ = path;
		if(preferences_path_[preferences_path_.length()-1] != '/') {
			preferences_path_ += '/';
		}
		
		save_file_path_ = preferences_path_ + SAVE_FILENAME;
		auto_save_file_path_ = preferences_path_ + AUTOSAVE_FILENAME;	
	}
	
	const char *save_file_path() {
		LOG_INFO("GET SAVE FILE PATH: " << save_file_path_);
		return save_file_path_.c_str();
	}
	
	const char *auto_save_file_path() {
		LOG_INFO("GET AUTOSAVE FILE PATH: " << auto_save_file_path_);
		return auto_save_file_path_.c_str();
	}
	
	const char *user_data_path() {
		return preferences_path_.c_str();
	}

	bool editor_save_to_user_preferences() {
		return editor_save_to_user_preferences_;
	}

	namespace {
		void expand_path(std::string& str) {
			if(!str.empty() && str[0] == '~') {
#if defined(TARGET_PANDORA)
				str = std::string(getenv("PWD")) + std::string(str.begin()+1, str.end());
#else
				str = std::string(getenv("HOME")) + std::string(str.begin()+1, str.end());
#endif
			}
		}
	}
	
	std::string dlc_path() {
#if defined(_MSC_VER)
		std::string result(WindowsPrefs::getInstance().getAppDataPath() + "/" + module::get_module_name() + "/dlc");
#else
		std::string result(preferences_path_ + "/dlc");
#endif
		expand_path(result);
		return result;
	}
	
	void expand_data_paths() {
		expand_path(save_file_path_);
		expand_path(auto_save_file_path_);
		expand_path(preferences_path_);
		LOG_INFO("EXPAND DATA PATHS");
	}
	
	void set_save_slot(const std::string& fname) {
		save_file_path_ = std::string(user_data_path()) + fname;
		LOG_INFO("SET SAVE FILE PATH TO " << save_file_path_);
	}
	
	bool show_debug_hitboxes() {
		return show_debug_hitboxes_;
	}
	
	bool toogle_debug_hitboxes() {
		bool shown = show_debug_hitboxes_;
		show_debug_hitboxes_ = !show_debug_hitboxes_;
		return shown;
	}

	bool edit_and_continue() {
		return edit_and_continue_ && !EditorResolutionManager::isActive();
	}

	void set_edit_and_continue(bool value) {
		edit_and_continue_ = value;
	}
	
	bool use_pretty_scaling() {
		return use_pretty_scaling_;
	}
	
	void set_use_pretty_scaling(bool value) {
		use_pretty_scaling_ = value;
	}
	
	ScreenMode get_screen_mode() {
		return fullscreen_;
	}

	bool no_fullscreen_ever()
	{
		return fullscreen_disabled_;
	}
	
	void set_screen_mode(ScreenMode value) {
		fullscreen_ = value;
	}
	
	bool is_resizable() {
		return resizable_;
	}
	
	const std::string& control_scheme()
	{
		return control_scheme_;
	}
	
	void set_control_scheme(const std::string& scheme)
	{
		control_scheme_ = scheme;
	}
	
	bool load_compiled()
	{
		return load_compiled_;
	}
	
	void set_load_compiled(bool value)
	{
		load_compiled_ = value;
	}
	
	bool allow_autopause()
	{
		return allow_autopause_;
	}

	bool auto_size_window()
	{
		return g_auto_size_window;
	}

	int requested_window_width()
	{
		return requested_window_width_;
	}

	int requested_window_height()
	{
		return requested_window_height_;
	}

	int requested_virtual_window_width()
	{
		return g_virtual_window_width;
	}

	int requested_virtual_window_height()
	{
		return g_virtual_window_height;
	}

	void adjust_virtual_width_to_match_physical(int width, int height)
	{
		static int min_window_width = g_virtual_window_width;
		if(g_virtual_window_width_max > min_window_width) {
			const int ideal_width = (g_virtual_window_height * width) / height;
			if(ideal_width >= min_window_width) {
				g_virtual_window_width = std::min<int>(ideal_width, g_virtual_window_width_max);
			}
		}
	}

	bool edit_on_start()
	{
		return edit_on_start_;
	}
	
	void set_edit_on_start(bool value)
	{
		edit_on_start_ = value;
	}
	
	uri::uri get_tbs_uri()
	{
		return tbs_uri_;
	}
	
	std::string get_username()
	{
		return username_;
	}
	
	std::string get_password()
	{
		return password_;
	}

	void set_username(const std::string& uname)
	{
		username_ = uname;
	}

	variant get_cookie()
	{
		return cookie_;
	}

	void set_cookie(const variant &v)
	{
		cookie_ = v;
	}

	void set_password(const std::string& pword)
	{
		boost::uuids::detail::sha1 hash;
		hash.process_bytes(pword.c_str(), pword.length());
		unsigned int digest[5];
		hash.get_digest(digest);
		std::stringstream str;
		str << std::hex << std::setfill('0')  << std::setw(sizeof(unsigned int)*2) << digest[0] << digest[1] << digest[2] << digest[3] << digest[4];
		password_ = str.str();
	}

	bool force_no_npot_textures()
	{
		return force_no_npot_textures_;
	}
	
	bool screen_rotated()
	{
		return screen_rotated_;
	}
	
	bool debug()
	{
		return debug_;
	}
	
	bool show_fps()
	{
		return show_fps_;
	}

	void set_show_fps(bool show)
	{
		show_fps_ = show;
	}
	
	int frame_time_millis()
	{
		return frame_time_millis_;
	}

	bool has_alt_frame_time()
	{
		return alt_frame_time_millis_ != -1;
	}

	alt_frame_time_scope::alt_frame_time_scope(bool value) : old_value_(frame_time_millis_), active_(false)
	{
		if(value && has_alt_frame_time()) {
			frame_time_millis_ = alt_frame_time_millis_;
			active_ = true;
		}
	}

	alt_frame_time_scope::~alt_frame_time_scope()
	{
		frame_time_millis_ = old_value_;
	}
	
	bool use_joystick()
	{
		return use_joystick_;
	}
	
	game_logic::FormulaCallable* registry()
	{
		return &GameRegistry::getInstance();
	}
	
	void load_preferences()
	{
		std::string path;
		if(preferences_path_.empty()) {
#if defined(_MSC_VER)
			
			preferences_path_ = WindowsPrefs::getInstance().getPreferencePath();
			save_file_path_ = WindowsPrefs::getInstance().getSaveFilePath();
			auto_save_file_path_ = WindowsPrefs::getInstance().getAutoSaveFilePath();
			path = preferences_path_;
#else
			path = PREFERENCES_PATH;
#endif
		} else {
			path = preferences_path_;
		}
		expand_path(path);

		variant node;
		
		if(!sys::file_exists(path + "preferences.cfg")) {
			if(module::get_default_preferences().is_map()) {
				sys::write_file(path + "preferences.cfg", module::get_default_preferences().write_json());
				node = module::get_default_preferences();
			} else {
				return;
			}
		}
		
		if(node.is_null()) {
			try {
				node = json::parse_from_file(path + "preferences.cfg");
			} catch(json::ParseError&) {
				return;
			}
		}

		for(std::map<std::string, RegisteredSetting>::iterator i = g_registered_settings().begin(); i != g_registered_settings().end(); ++i) {
			if(node.has_key(i->first)) {
				i->second.read(node[i->first]);
				i->second.has_been_set_from_persistent = true;
				i->second.persistent = true;
			}
		}
		
		unique_user_id = node["user_id"].as_int(0);
		
		use_joystick_ = node["joystick"].as_bool(use_joystick_);
		variant show_control_rects = node["show_iphone_controls"];
		if(show_control_rects.is_null() == false) {
			show_iphone_controls_ = show_control_rects.as_bool(show_iphone_controls_);
		}
		
		no_sound_ = node["no_sound"].as_bool(no_sound_);
		no_music_ = node["no_music"].as_bool(no_music_);
		reverse_ab_ = node["reverse_ab"].as_bool(reverse_ab_);
		allow_autopause_ = node["allow_autopause"].as_bool(allow_autopause_);
		
		sound::set_music_volume(node["music_volume"].as_int(1000)/1000.0f);
		sound::set_sound_volume(node["sound_volume"].as_int(1000)/1000.0f);

		locale_ = node["locale"].as_string_default("system");
		
		const variant registry_node = node["registry"];
		if(registry_node.is_null() == false) {
			GameRegistry::getInstance().setContents(registry_node);
		}
		
		if(node["code_editor"].is_map()) {
			external_code_editor_ = node["code_editor"];
		}

		username_ = node["username"].as_string_default("");
		password_ = node["passhash"].as_string_default("");
		cookie_ = node.has_key("cookie") ? node["cookie"] : variant();

		controls::set_keycode(controls::CONTROL_UP, static_cast<key_type>(node["key_up"].as_int(SDLK_UP)));
		controls::set_keycode(controls::CONTROL_DOWN, static_cast<key_type>(node["key_down"].as_int(SDLK_DOWN)));
		controls::set_keycode(controls::CONTROL_LEFT, static_cast<key_type>(node["key_left"].as_int(SDLK_LEFT)));
		controls::set_keycode(controls::CONTROL_RIGHT, static_cast<key_type>(node["key_right"].as_int(SDLK_RIGHT)));
		controls::set_keycode(controls::CONTROL_ATTACK, static_cast<key_type>(node["key_attack"].as_int(SDLK_d)));
		controls::set_keycode(controls::CONTROL_JUMP, static_cast<key_type>(node["key_jump"].as_int(SDLK_a)));
		controls::set_keycode(controls::CONTROL_TONGUE, static_cast<key_type>(node["key_tongue"].as_int(SDLK_s)));

		int ctrl_item = 0;
		for(const char** control_name = controls::control_names(); *control_name && ctrl_item != controls::NUM_CONTROLS; ++control_name, ++ctrl_item) {
			std::string key = "mouse_";
			key += *control_name;
			if(node.has_key(key)) {
				controls::set_mouse_to_keycode(static_cast<controls::CONTROL_ITEM>(ctrl_item), node[key].as_int());
			}
		}

        preferences::set_32bpp_textures_if_kb_memory_at_least(512000);
	}
	
	void save_preferences()
	{
		variant_builder node;
		node.add("user_id", get_unique_user_id());
		node.add("no_sound", variant::from_bool(no_sound_));
		node.add("no_music", variant::from_bool(no_music_));
		node.add("allow_autopause", variant::from_bool(allow_autopause_));
		node.add("reverse_ab", variant::from_bool(reverse_ab_));
		node.add("joystick", variant::from_bool(use_joystick_));
		node.add("sound_volume", static_cast<int>(sound::get_sound_volume()*1000));
		node.add("music_volume", static_cast<int>(sound::get_music_volume()*1000));
		node.add("key_up", controls::get_keycode(controls::CONTROL_UP));
		node.add("key_down", controls::get_keycode(controls::CONTROL_DOWN));
		node.add("key_left", controls::get_keycode(controls::CONTROL_LEFT));
		node.add("key_right", controls::get_keycode(controls::CONTROL_RIGHT));
		node.add("key_attack", controls::get_keycode(controls::CONTROL_ATTACK));
		node.add("key_jump", controls::get_keycode(controls::CONTROL_JUMP));
		node.add("key_tongue", controls::get_keycode(controls::CONTROL_TONGUE));
		node.add("show_iphone_controls", variant::from_bool(show_iphone_controls_));

		for(int n = 1; n <= 3; ++n) {
			controls::CONTROL_ITEM ctrl = controls::get_mouse_keycode(n);
			if(ctrl != controls::NUM_CONTROLS) {
				node.add(std::string("mouse_") + controls::control_names()[ctrl], variant(n));
			}
		}

		node.add("locale", locale_);
		node.add("username", variant(get_username()));
		node.add("passhash", variant(get_password()));
		node.add("cookie", get_cookie());
			
		node.add("sdl_version", SDL_COMPILEDVERSION);
		if(external_code_editor_.is_null() == false) {
			node.add("code_editor", external_code_editor_);
		}
		
		node.add("registry", GameRegistry::getInstance().writeContents());

		for(std::map<std::string, RegisteredSetting>::const_iterator i = g_registered_settings().begin(); i != g_registered_settings().end(); ++i) {
			if(i->second.persistent) {
				node.add(i->first, i->second.write());
			}
		}
		
		LOG_INFO("WRITE PREFS: " << (preferences_path_ + "preferences.cfg"));
		sys::write_file(preferences_path_ + "preferences.cfg", node.build().write_json());
	}
	
	bool parse_arg(const std::string& arg, const std::string& next_arg) 
	{
		std::string s, arg_value;
		std::string::const_iterator equal = std::find(arg.begin(), arg.end(), '=');
		if(equal != arg.end()) {
			s = std::string(arg.begin(), equal);
			arg_value = std::string(equal+1, arg.end());
		} else {
			s = arg;
			arg_value = next_arg;
		}
		
		if(s == "--editor_save_to_user_preferences") {
			editor_save_to_user_preferences_ = true;
		} else if(s == "--show-hitboxes") {
			show_debug_hitboxes_ = true;
		} else if(s == "--show-controls") {
			show_iphone_controls_ = true;
		} else if(s == "--scale") {
			set_use_pretty_scaling(true);
		} else if(s == "--no-sound") {
			no_sound_ = true;
		} else if(s == "--no-music") {
			no_music_ = true;
		} else if(s == "--sound") {
			no_sound_ = false;
		} else if(s == "--music") {
			no_music_ = false;
		} else if(s == "--disable-fullscreen") {
			fullscreen_disabled_ = true;
		} else if(s == "--fullscreen") {
			fullscreen_ = ScreenMode::FULLSCREEN_WINDOWED;
		} else if(s == "--windowed") {
			fullscreen_ = ScreenMode::WINDOWED;
		} else if(s == "--resizable") {
			resizable_ = true;
        } else if(s == "--width") {
			auto widths = util::split_into_vector_int(arg_value, ':');
			if(widths.size() > 0) {
				requested_window_width_ = widths[0];
			}
			if(widths.size() > 1) {
				g_virtual_window_width = widths[1];
				//if(g_virtual_window_width > requested_window_width_) {
				//	xypos_draw_mask = 0;
				//}
			} else if(!g_virtual_window_width) {
				g_virtual_window_width = requested_window_width_;
			}
        } else if(s == "--height") {
			auto heights = util::split_into_vector_int(arg_value, ':');
			if(heights.size() > 0) {
				requested_window_height_ = heights[0];
			}
			if(heights.size() > 1) {
				g_virtual_window_height = heights[1];
			} else if(!g_virtual_window_height) {
				g_virtual_window_height = requested_window_height_;
			}
		} else if(s == "--no-resizable") {
			resizable_ = false;
		} else if(s == "--potonly") {
			force_no_npot_textures_ = true;
		} else if(s == "--textures16") {
			use_16bpp_textures_ = true;
		} else if(s == "--textures32") {
			use_16bpp_textures_ = false;
		} else if(s == "--textures32_if_kb_memory_at_least") {
            preferences::set_32bpp_textures_if_kb_memory_at_least( atoi(arg_value.c_str()) );
		} else if(s == "--debug") {
			debug_ = true;
		} else if(s == "--no-debug") {
			debug_ = false;
		} else if(s == "--fps") {
			show_fps_ = true;
		} else if(s == "--no-fps") {
			show_fps_ = false;
		} else if(s == "--set-fps" && !arg_value.empty()) {
			frame_time_millis_ = 1000/boost::lexical_cast<int, std::string>(arg_value);
			LOG_INFO("FPS: " << arg_value << " = " << frame_time_millis_ << "ms/frame");
		} else if(s == "--alt-fps" && !arg_value.empty()) {
			alt_frame_time_millis_ = 1000/boost::lexical_cast<int, std::string>(arg_value);
			LOG_INFO("FPS: " << arg_value << " = " << alt_frame_time_millis_ << "ms/frame");
		} else if(s == "--config-path" && !arg_value.empty()) {
			set_preferences_path(arg_value);
		} else if(s == "--send-stats") {
			send_stats_ = true;
		} else if(s == "--no-send-stats") {
			send_stats_ = false;
		} else if(s == "--time-travel") {
			record_history_ = true;
		} else if(s == "--joystick") {
			use_joystick_ = true;
		} else if(s == "--no-joystick") {
			use_joystick_ = false;
		} else if(s == "--server") {
			tbs_uri_ = uri::uri::parse(arg_value);
		} else if(s == "--user") {
			username_ = arg_value;
		} else if(s == "--pass") {
			set_password(arg_value);
		} else if(s == "--module-args") {
			game_logic::ConstFormulaCallablePtr callable = map_into_callable(json::parse(arg_value));
			module::set_module_args(callable);
		} else if(s == "--relay") {
			relay_through_server_ = true;
		} else if(s == "--failing-tests") {
			run_failing_unit_tests_ = true;
		} else if(s == "--no-serialize-bad-objects") {
			serialize_bad_objects_ = false;
		} else if(s == "--serialize-bad-objects") {
			serialize_bad_objects_ = true;
		} else if(s == "--die-on-assert") {
			die_on_assert_ = true;
		} else if(s == "--no-type-safety") {
			type_safety_checks_ = false;
		} else if(s == "--tbs-server") {
			internal_tbs_server_ = true;
			LOG_INFO("TURN ON internal server");
		} else if(s == "--no-tbs-server") {
			internal_tbs_server_ = false;
			LOG_INFO("TURN OFF internal server");
		} else if(s == "--no-autopause") {
			allow_autopause_ = false;
		} else if(s == "--autopause") {
			allow_autopause_ = true;
		} else if(s == "--difficulty" && !arg_value.empty()) {
			if(boost::regex_match(arg_value, boost::regex("-?[0-9]+"))) {
				force_difficulty_ = boost::lexical_cast<int>(arg_value);
			} else {
				force_difficulty_ = difficulty::from_string(arg_value);
			}
		} else if(s == "--edit-and-continue") {
			set_edit_and_continue(true);
		} else {
			if(arg.size() > 2 && arg[0] == '-' && arg[1] == '-' && std::find(arg.begin(), arg.end(), '=') != s.end()) {
				std::string::const_iterator equal = std::find(arg.begin(), arg.end(), '=');
				std::string base_name(arg.begin()+2,equal);
				std::replace(base_name.begin(), base_name.end(), '-', '_');

				static const std::string NoOverridePrefix("defer_archive_");
				bool do_override = true;
				if(base_name.size() > NoOverridePrefix.size() && std::equal(NoOverridePrefix.begin(), NoOverridePrefix.end(), base_name.begin())) {
					do_override = false;
					base_name.erase(base_name.begin(), base_name.begin() + NoOverridePrefix.size());
				}

				if(g_registered_settings().count(base_name)) {
					RegisteredSetting& setting = g_registered_settings()[base_name];
					if(!do_override && setting.has_been_set_from_persistent) {
						//do nothing. This was set from the archive and we don't override.
					} else if(setting.string_value) {
						*setting.string_value = std::string(equal+1, arg.end());
					} else if(setting.int_value) {
						*setting.int_value = atoi(std::string(equal+1, arg.end()).c_str());
					} else if(setting.double_value) {
						*setting.double_value = boost::lexical_cast<double>(std::string(equal+1, arg.end()));
					} else if(setting.bool_value) {
						if(equal != arg.end()) {
							std::string value(equal+1, arg.end());
							if(value == "yes" || value == "true") {
								*setting.bool_value = true;
							} else if(value == "no" || value == "false") {
								*setting.bool_value = false;
							} else {
								ASSERT_LOG(false, "Invalid value for boolean parameter " << base_name << ". Must be true or false");
							}
						} else {
							*setting.bool_value = true;
						}
					} else if(setting.variant_value) {
						if(equal != arg.end()) {
							std::string value(equal+1, arg.end());
							if(value == "yes" || value == "true") {
								*setting.variant_value = variant::from_bool(true);
							} else if(value == "no" || value == "false") {
								*setting.variant_value = variant::from_bool(false);
							} else {
								*setting.variant_value = variant(value);
							}
						} else {
							*setting.variant_value = variant::from_bool(true);
						}
					} else {
						ASSERT_LOG(false, "Error making sense of preference type " << base_name);
					}

					return true;
				}
			} else if(arg.size() > 2 && arg[0] == '-' && arg[1] == '-') {
				std::string::const_iterator begin = arg.begin() + 2;
				bool value = true;
				if(arg.size() > 5 && std::equal(begin, begin+3, "no-")) {
					value = false;
					begin += 3;
				}

				std::string base_name(begin, arg.end());
				std::replace(base_name.begin(), base_name.end(), '-', '_');
				if(g_registered_settings().count(base_name)) {
					RegisteredSetting& setting = g_registered_settings()[base_name];
					ASSERT_LOG(setting.bool_value, "Must provide value for option: " << base_name);

					*setting.bool_value = value;
					return true;
				}
			}

			return false;
		}
		
		return true;
	}
	
	bool use_16bpp_textures() {
		return use_16bpp_textures_;
	}

	void set_32bpp_textures_if_kb_memory_at_least( int memory_required ) {
        sys::AvailableMemoryInfo mem_info;
        const bool result = sys::get_available_memory(&mem_info);
        if(result) {
            use_16bpp_textures_ = mem_info.mem_total_kb < memory_required;
            LOG_INFO("USING " << (use_16bpp_textures_ ? 16 : 32) << "bpp TEXTURES BECAUSE SYSTEM HAS " << mem_info.mem_total_kb << "KB AND " << memory_required << "KB REQUIRED FOR 32bpp TEXTURES");
        }
    }
    
	bool send_stats() {
		return send_stats_;
	}
	
	int force_difficulty() {
		return force_difficulty_;
	}
	
	bool record_history() {
		return record_history_;
	}
	
	void set_record_history(bool value) {
		record_history_ = value;
	}
	
	bool relay_through_server() {
		return relay_through_server_;
	}
	
	variant external_code_editor() {
		return external_code_editor_;
	}
	
	void set_relay_through_server(bool value) {
		relay_through_server_ = value;
	}
	
	bool run_failing_unit_tests() {
		return run_failing_unit_tests_;
	}
	
	bool serialize_bad_objects() {
		return serialize_bad_objects_;
	}

	bool die_on_assert() {
		return die_on_assert_;
	}

	bool type_safety_checks() {
		return type_safety_checks_;
	}
	
	const std::string& locale() {
		return locale_;
	}

	bool internal_tbs_server() {
		return internal_tbs_server_;
	}

	const std::set<std::string>& get_build_options()
	{
		static std::set<std::string> res;
		if(res.empty()) {
			res.insert("isomap");
			res.insert("sdl2");
			res.insert("save_png");
			res.insert("svg");
#if defined(USE_BOX2D)
			res.insert("box2d");
#endif
#if defined(USE_BULLET)
			res.insert("bullet");
#endif
#if defined(USE_LUA)
			res.insert("lua");
#endif
		}
		return res;
	}

	void setLocale(const std::string& value) {
		locale_ = value;
	}

	class GamePreferences : public game_logic::FormulaCallable
	{
	public:
	private:
		DECLARE_CALLABLE(GamePreferences);
	};

	BEGIN_DEFINE_CALLABLE_NOBASE(GamePreferences)
		DEFINE_FIELD(sound_volume, "decimal")
			return variant(sound::get_sound_volume());
		DEFINE_SET_FIELD
			sound::set_sound_volume(value.as_float());

		DEFINE_FIELD(music_volume, "decimal")
			return variant(sound::get_music_volume());
		DEFINE_SET_FIELD
			sound::set_music_volume(value.as_float());

		BEGIN_DEFINE_FN(get_bool_preference_value, "(string)->bool")
			const std::string key = FN_ARG(0).as_string();
			auto it = g_registered_settings().find(key);
			ASSERT_LOG(it != g_registered_settings().end(), "Unknown preference setting: " << key);
			ASSERT_LOG(it->second.bool_value, "Preference is not a bool: " << key);
			return variant::from_bool(*it->second.bool_value);
		END_DEFINE_FN

		BEGIN_DEFINE_FN(get_int_preference_value, "(string)->int")
			const std::string key = FN_ARG(0).as_string();
			auto it = g_registered_settings().find(key);
			ASSERT_LOG(it != g_registered_settings().end(), "Unknown preference setting: " << key);
			ASSERT_LOG(it->second.int_value, "Preference is not an int: " << key);
			return variant(*it->second.int_value);
		END_DEFINE_FN

		BEGIN_DEFINE_FN(get_decimal_preference_value, "(string)->decimal")
			const std::string key = FN_ARG(0).as_string();
			auto it = g_registered_settings().find(key);
			ASSERT_LOG(it != g_registered_settings().end(), "Unknown preference setting: " << key);
			ASSERT_LOG(it->second.double_value, "Preference is not a decimal: " << key);
			return variant(*it->second.double_value);
		END_DEFINE_FN

		BEGIN_DEFINE_FN(set_preference_value, "(string, any, null|[enum {persistent}]=null)->commands")
			const std::string key = FN_ARG(0).as_string();
			variant val = FN_ARG(1);
			auto it = g_registered_settings().find(key);
			ASSERT_LOG(it != g_registered_settings().end(), "Unknown preference setting: " << key);

			bool force_persistent = false;

			if(NUM_FN_ARGS > 2 && FN_ARG(2).is_list()) {
				variant flags = FN_ARG(2);
				for(variant flag : flags.as_list()) {
					if(flag.as_enum() == "persistent") {
						force_persistent = true;
					}
				}
			}

			return variant(new game_logic::FnCommandCallable("set_preference_value", [=]() {
				if(force_persistent) {
					it->second.persistent = true;
				}

				if(it->second.int_value) {
					*it->second.int_value = val.as_int();
				} else if(it->second.bool_value) {
					*it->second.bool_value = val.as_bool();
				} else if(it->second.double_value) {
					*it->second.double_value = val.as_double();
				} else if(it->second.string_value) {
					*it->second.string_value = val.as_string();
				} else if(it->second.variant_value) {
					*it->second.variant_value = val;
				}
			}));

		END_DEFINE_FN

		BEGIN_DEFINE_FN(save_preferences, "()->commands")
			return variant(new game_logic::FnCommandCallable("save_preferences", [=]() {
				save_preferences();
			}));
		END_DEFINE_FN

	END_DEFINE_CALLABLE(GamePreferences)

	variant ffl_interface()
	{
		static variant result(new GamePreferences);
		return result;
	}
}
