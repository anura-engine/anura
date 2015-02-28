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
	const std::string& getPreferencePath() const   { return preferences_path_; }
	const std::string& getSaveFilePath() const     { return save_file_path_; }
	const std::string& getAutoSaveFilePath() const { return auto_save_file_path_; }
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

		preferences_path_ = app_data_path_ + "/" + module::get_module_name() + "/";
		save_file_path_ = preferences_path_ + SAVE_FILENAME;
		auto_save_file_path_ = preferences_path_ + AUTOSAVE_FILENAME;
	}
	std::string preferences_path_;
	std::string save_file_path_;
	std::string auto_save_file_path_;
	std::string app_data_path_;
};

#endif // _MSC_VER


namespace preferences 
{
	namespace 
	{
		struct RegisteredSetting 
		{
			RegisteredSetting() : persistent(false), int_value(nullptr), bool_value(nullptr), double_value(nullptr), string_value(nullptr), variant_value(nullptr), helpstring(nullptr)
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
			bool persistent;
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
			variant getValue(const std::string& key) const {
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

			void setValue(const std::string& key, const variant& value) {
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

			void getInputs(std::vector<game_logic::FormulaInput>* inputs) const {
				for(std::map<std::string, RegisteredSetting>::iterator itor = g_registered_settings().begin(); itor != g_registered_settings().end(); ++itor) {
					inputs->push_back(game_logic::FormulaInput(itor->first, game_logic::FORMULA_ACCESS_TYPE::READ_WRITE));
				}
			}
		};
	}

	game_logic::FormulaCallable* get_settings_obj()
	{
		static boost::intrusive_ptr<game_logic::FormulaCallable> obj(new SettingsObject);
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
		return g_registered_settings().size();
	}

	int register_int_setting(const std::string& id, bool persistent, int* value, const char* helpstring)
	{
		ASSERT_LOG(g_registered_settings().count(id) == 0, "Multiple definition of registered setting: " << id);
		RegisteredSetting& setting = g_registered_settings()[id];
		setting.int_value = value;
		setting.persistent = persistent;
		setting.helpstring = helpstring;
		return g_registered_settings().size();
	}

	int register_bool_setting(const std::string& id, bool persistent, bool* value, const char* helpstring)
	{
		ASSERT_LOG(g_registered_settings().count(id) == 0, "Multiple definition of registered setting: " << id);
		RegisteredSetting& setting = g_registered_settings()[id];
		setting.bool_value = value;
		setting.persistent = persistent;
		setting.helpstring = helpstring;
		return g_registered_settings().size();
	}

	int register_float_setting(const std::string& id, bool persistent, double* value, const char* helpstring)
	{
		ASSERT_LOG(g_registered_settings().count(id) == 0, "Multiple definition of registered setting: " << id);
		RegisteredSetting& setting = g_registered_settings()[id];
		setting.double_value = value;
		setting.persistent = persistent;
		setting.helpstring = helpstring;
		return g_registered_settings().size();
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

			result += i->second.helpstring;
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
	
	namespace {
		int unique_user_id = 0;
		
		int screen_editor_mode = 0;
		
		bool no_sound_ = false;
		bool no_music_ = false;
		bool show_debug_hitboxes_ = false;
		bool edit_and_continue_ = false;
		bool show_iphone_controls_ = false;
		bool use_pretty_scaling_ = false;
		FullscreenMode fullscreen_ = FULLSCREEN_NONE;
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
		bool auto_size_window_ = false;
		bool screen_dimensions_are_persistent = false;
		
		std::string level_path_ = "data/level/";
		bool level_path_set_ = false;
		
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
		
		bool sim_iphone_ = true;
		
		int virtual_screen_width_ = 960;
		int virtual_screen_height_ = 640;
		
		int actual_screen_width_ = 320;
		int actual_screen_height_ = 480;
		
		bool screen_rotated_ = true;
		
		bool use_joystick_ = false;
		
		bool load_compiled_ = true;
		
		bool use_16bpp_textures_ = false;
#elif defined(TARGET_OS_HARMATTAN)
		
		bool send_stats_ = false;
		
		bool sim_iphone_ = false;
		
#ifndef PREFERENCES_PATH
#define PREFERENCES_PATH "~/.frogatto/"
#endif
		int virtual_screen_width_ = 854;
		int virtual_screen_height_ = 480;
		
		int actual_screen_width_ = 854;
		int actual_screen_height_ = 480;
		
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
		bool sim_iphone_ = false;
		int virtual_screen_width_ = 800;
		int virtual_screen_height_ = 480;
		
		int actual_screen_width_ = 800;
		int actual_screen_height_ = 480;
		
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
		bool sim_iphone_ = false;
		bool use_joystick_ = true;
		bool screen_rotated_ = false;
		int virtual_screen_width_ = 1024;
		int virtual_screen_height_ = 600;
		int actual_screen_width_ = 1024;
		int actual_screen_height_ = 600;
		bool load_compiled_ = true;
		bool use_fbo_ = true;
		bool use_bequ_ = true;
		bool use_16bpp_textures_ = false;
		
#else
		
#if defined(_WINDOWS)
#define PREFERENCES_PATH ""
#endif // _WINDOWS
		
#ifndef NO_UPLOAD_STATS
		bool send_stats_ = true;
#else
		bool send_stats_ = false;
#endif
		
		bool sim_iphone_ = false;
		
#ifndef PREFERENCES_PATH
#define PREFERENCES_PATH "~/.frogatto/"
#endif
		bool screen_rotated_ = false;
		
		bool use_joystick_ = true;
		
#if defined(TARGET_TEGRA)
		int virtual_screen_width_ = 1024;
		int virtual_screen_height_ = 600;
		
		int actual_screen_width_ = 1024;
		int actual_screen_height_ = 600;
		
		bool load_compiled_ = true;
		bool use_fbo_ = true;
		bool use_bequ_ = true;
#else
		int virtual_screen_width_ = 800;
		int virtual_screen_height_ = 600;
		
		int actual_screen_width_ = 800;
		int actual_screen_height_ = 600;
		
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
	}
	
	int get_unique_user_id() {
		if(unique_user_id == 0) {
			time_t t1;
			time(&t1);
			int tm = static_cast<int>(t1);
			unique_user_id = tm^rand();
		}
		
		return unique_user_id;
	}
	
	int xypos_draw_mask = actual_screen_width_ < virtual_screen_width_ ? ~1 : ~0;
	bool double_scale() {
		return xypos_draw_mask&1;
	}
	bool compiling_tiles = false;
	
	namespace {
		void recalculate_draw_mask() {
			xypos_draw_mask = actual_screen_width_ < virtual_screen_width_ ? ~1 : ~0;
		}
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
#ifdef _WINDOWS
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
	
	const std::string& level_path() {
		return level_path_;
	}
	
	bool is_level_path_set() {
		return level_path_set_;
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
		expand_path(level_path_);
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
		return edit_and_continue_ && !editor_resolution_manager::isActive();
	}

	void set_edit_and_continue(bool value) {
		edit_and_continue_ = value;
	}
	
	bool show_iphone_controls() {
		return show_iphone_controls_;
	}
	
	bool use_pretty_scaling() {
		return use_pretty_scaling_;
	}
	
	void set_use_pretty_scaling(bool value) {
		use_pretty_scaling_ = value;
	}
	
	FullscreenMode fullscreen() {
		return fullscreen_;
	}

	bool no_fullscreen_ever()
	{
		return fullscreen_disabled_;
	}
	
	void set_fullscreen(FullscreenMode value) {
		fullscreen_ = value;
	}
	
	bool resizable() {
		return resizable_;
	}
	
	bool no_iphone_controls() {
		return no_iphone_controls_;
	}
	
	bool proportional_resize() {
		return proportional_resize_;
	}
	
	bool reverse_ab() {
		return reverse_ab_;
	}
	
	void set_reverse_ab(bool value) {
		reverse_ab_ = value;
	}
	
	const std::string& control_scheme()
	{
		return control_scheme_;
	}
	
	void set_control_scheme(const std::string& scheme)
	{
		control_scheme_ = scheme;
	}
	
	void set_widescreen()
	{
		LOG_ERROR("Ignored: set_widescreen()");
		//virtual_screen_width_ = (virtual_screen_height_*16)/9;
		//actual_screen_width_ = (actual_screen_height_*16)/9;
		//recalculate_draw_mask();
	}
	
	int virtual_screen_width()
	{
		return KRE::WindowManager::getMainWindow()->logicalWidth();
		//return virtual_screen_width_;
	}
	
	int virtual_screen_height()
	{
		return KRE::WindowManager::getMainWindow()->logicalHeight();
		//return virtual_screen_height_;
	}
	
	void set_virtual_screen_width(int width)
	{
		LOG_ERROR("Ignored: set_virtual_screen_width()");
		//virtual_screen_width_ = width;
		//recalculate_draw_mask();
	}

	void tweak_virtual_screen(int awidth, int aheight) 
	{
		LOG_ERROR("Ignored: tweak_virtual_screen()");
		//virtual_screen_width_ = (virtual_screen_height_ * awidth)/aheight;
	}
	
	void set_virtual_screen_height (int height)
	{
		LOG_ERROR("Ignored: set_virtual_screen_height()");
		//virtual_screen_height_ = height;
	}
	
	int actual_screen_width()
	{
		//return actual_screen_width_;
		return KRE::WindowManager::getMainWindow()->width();
	}
	
	int actual_screen_height()
	{
		return KRE::WindowManager::getMainWindow()->height();
		//return actual_screen_height_;
	}
	
	void set_actual_screen_width(int width)
	{
		LOG_ERROR("Ignored: set_actual_screen_width()");
		//assert(width);
		//actual_screen_width_ = width;
		//if(screen_editor_mode) {
		//	virtual_screen_width_ = actual_screen_width_;
		//}
		//recalculate_draw_mask();
	}
	
	void set_actual_screen_height(int height)
	{
		LOG_ERROR("Ignored: set_actual_screen_width()");
		//assert(height);
		//actual_screen_height_ = height;
		//if(screen_editor_mode) {
		//	virtual_screen_height_ = actual_screen_height_;
		//}
	}
	
	void set_actual_screen_dimensions_persistent(int width, int height)
	{
		LOG_ERROR("Ignored: set_actual_screen_width()");
		//assert(width);
		//assert(height);
		//actual_screen_width_ = width;
		//actual_screen_height_ = height;
		//tweak_virtual_screen(width, height);
		//screen_dimensions_are_persistent = true;
		//if(screen_editor_mode) {
		//	virtual_screen_width_ = actual_screen_width_;
		//	virtual_screen_height_ = actual_screen_height_;
		//}
		//recalculate_draw_mask();
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
		return auto_size_window_;
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

#if defined(TARGET_OS_HARMATTAN) || defined(TARGET_PANDORA) || defined(TARGET_TEGRA) || defined(TARGET_BLACKBERRY)
	bool use_fbo()
	{
		return use_fbo_;
	}
	
	bool use_bequ()
	{
		return use_bequ_;
	}
	
    void set_fbo( bool value )
    {
		use_fbo_ = value;
    }
	
    void set_bequ( bool value )
    {
		use_bequ_ = value;
    }
#endif
	
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
			if(i->second.persistent && node.has_key(i->first)) {
				i->second.read(node[i->first]);
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

		if(node.has_key("width") && node.has_key("height")) {
			int w = node["width"].as_int();
			int h = node["height"].as_int();
			if(w > 0 && h > 0 && w < 4096 && h < 4096) {
				set_actual_screen_width(w);
				set_actual_screen_height(h);
				screen_dimensions_are_persistent = true;
				if(node.has_key("fullscreen")) {
					if(node["fullscreen"].is_bool()) {
						set_fullscreen(node["fullscreen"].as_bool() ? FULLSCREEN_NONE : FULLSCREEN_WINDOWED);
					} else {
						set_fullscreen(FullscreenMode(node["fullscreen"].as_int()));
					}
				}
			}
		}
		
#if !TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
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

        preferences::set_32bpp_textures_if_kb_memory_at_least( 512000 );
#endif
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
			
		if(screen_dimensions_are_persistent) {
			node.add("width", actual_screen_width());
			node.add("height", actual_screen_height());
			node.add("fullscreen", fullscreen());
		}

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
	
	editor_screen_size_scope::editor_screen_size_scope() : width_(virtual_screen_width_), height_(virtual_screen_height_) {
		++screen_editor_mode;
		virtual_screen_width_ = actual_screen_width_;
		virtual_screen_height_ = actual_screen_height_;
	}
	
	editor_screen_size_scope::~editor_screen_size_scope() {
		virtual_screen_width_ = width_;
		virtual_screen_height_ = height_;
		--screen_editor_mode;
	}
	
	bool parse_arg(const char* arg) {
		const std::string s(arg);
		
		std::string arg_name, arg_value;
		std::string::const_iterator equal = std::find(s.begin(), s.end(), '=');
		if(equal != s.end()) {
			arg_name = std::string(s.begin(), equal);
			arg_value = std::string(equal+1, s.end());
		}
		
		if(arg_name == "--level-path") {
			level_path_ = arg_value + "/";
			level_path_set_ = true;
		} else if(s == "--editor_save_to_user_preferences") {
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
			fullscreen_ = FULLSCREEN_WINDOWED;
		} else if(s == "--windowed") {
			fullscreen_ = FULLSCREEN_NONE;
		} else if(s == "--proportional-resize") {
			resizable_ = true;
			proportional_resize_ = true;
		} else if(s == "--resizable") {
			resizable_ = true;
		} else if(s == "--no-resizable") {
			resizable_ = false;
		} else if(s == "--widescreen") {
			set_widescreen();
		} else if(s == "--bigscreen") {
			virtual_screen_width_ = actual_screen_width_;
			virtual_screen_height_ = actual_screen_height_;
		} else if(s == "--potonly") {
			force_no_npot_textures_ = true;
		} else if(s == "--textures16") {
			use_16bpp_textures_ = true;
		} else if(s == "--textures32") {
			use_16bpp_textures_ = false;
		} else if(arg_name == "--textures32_if_kb_memory_at_least") {
            preferences::set_32bpp_textures_if_kb_memory_at_least( atoi(arg_value.c_str()) );
		} else if(s == "--debug") {
			debug_ = true;
		} else if(s == "--no-debug") {
			debug_ = false;
		} else if(s == "--simiphone") {
			sim_iphone_ = true;
			
			virtual_screen_width_ = 960;
			virtual_screen_height_ = 640;
			
			actual_screen_width_ = 480;
			actual_screen_height_ = 320;
			use_16bpp_textures_ = true;
			
			recalculate_draw_mask();
		} else if(s == "--simipad") {
			sim_iphone_ = true;
			control_scheme_ = "ipad_2d";
			
			virtual_screen_width_ = 1024;
			virtual_screen_height_ = 768;
			
			actual_screen_width_ = 1024;
			actual_screen_height_ = 768;
			
			recalculate_draw_mask();
		} else if(s == "--no-iphone-controls") {
			no_iphone_controls_ = true;
		} else if(s == "--wvga") {
			virtual_screen_width_ = 800;
			virtual_screen_height_ = 480;
			
			actual_screen_width_ = 800;
			actual_screen_height_ = 480;
			
			recalculate_draw_mask();
		} else if(s == "--native") {
			virtual_screen_width_ = (actual_screen_width_) * 2;
			virtual_screen_height_ = (actual_screen_height_) * 2;
			recalculate_draw_mask();
		} else if(s == "--fps") {
			show_fps_ = true;
		} else if(s == "--no-fps") {
			show_fps_ = false;
		} else if(arg_name == "--set-fps" && !arg_value.empty()) {
			frame_time_millis_ = 1000/boost::lexical_cast<int, std::string>(arg_value);
			LOG_INFO("FPS: " << arg_value << " = " << frame_time_millis_ << "ms/frame");
		} else if(arg_name == "--alt-fps" && !arg_value.empty()) {
			alt_frame_time_millis_ = 1000/boost::lexical_cast<int, std::string>(arg_value);
			LOG_INFO("FPS: " << arg_value << " = " << alt_frame_time_millis_ << "ms/frame");
		} else if(arg_name == "--config-path" && !arg_value.empty()) {
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
		} else if(arg_name == "--server") {
			tbs_uri_ = uri::uri::parse(arg_value);
		} else if(arg_name == "--user") {
			username_ = arg_value;
		} else if(arg_name == "--pass") {
			set_password(arg_value);
		} else if(arg_name == "--module-args") {
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
		} else if(s == "--auto-size-window") {
			auto_size_window_ = true;
		} else if(arg_name == "--difficulty" && !arg_value.empty()) {
			if(boost::regex_match(arg_value, boost::regex("-?[0-9]+"))) {
				force_difficulty_ = boost::lexical_cast<int>(arg_value);
			} else {
				force_difficulty_ = difficulty::from_string(arg_value);
			}
		} else if(s == "--edit-and-continue") {
			set_edit_and_continue(true);
		} else {
			if(s.size() > 2 && s[0] == '-' && s[1] == '-' && std::find(s.begin(), s.end(), '=') != s.end()) {
				std::string::const_iterator equal = std::find(s.begin(), s.end(), '=');
				std::string base_name(s.begin()+2,equal);
				std::replace(base_name.begin(), base_name.end(), '-', '_');
				if(g_registered_settings().count(base_name)) {
					RegisteredSetting& setting = g_registered_settings()[base_name];
					if(setting.string_value) {
						*setting.string_value = std::string(equal+1, s.end());
					} else if(setting.int_value) {
						*setting.int_value = atoi(std::string(equal+1, s.end()).c_str());
					} else if(setting.double_value) {
						*setting.double_value = boost::lexical_cast<double>(std::string(equal+1, s.end()));
					} else if(setting.bool_value) {
						std::string value(equal+1, s.end());
						if(value == "yes" || value == "true") {
							*setting.bool_value = true;
						} else if(value == "no" || value == "false") {
							*setting.bool_value = false;
						} else {
							ASSERT_LOG(false, "Invalid value for boolean parameter " << base_name << ". Must be true or false");
						}
					} else if(setting.variant_value) {
						std::string value(equal+1, s.end());
						*setting.variant_value = variant(value);
					} else {
						ASSERT_LOG(false, "Error making sense of preference type " << base_name);
					}

					return true;
				}
			} else if(s.size() > 2 && s[0] == '-' && s[1] == '-') {
				std::string::const_iterator begin = s.begin() + 2;
				bool value = true;
				if(s.size() > 5 && std::equal(begin, begin+3, "no-")) {
					value = false;
					begin += 3;
				}

				std::string base_name(begin, s.end());
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
    
	bool sim_iphone() {
		return sim_iphone_;
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

	ScreenDimensionOverrideScope::ScreenDimensionOverrideScope(int width, int height, int vwidth, int vheight) 
		: vold_width(virtual_screen_width_), 
		vold_height(virtual_screen_height_), 
		old_width(actual_screen_width_), 
		old_height(actual_screen_height_)
	{
		actual_screen_width_ = width;
		actual_screen_height_ = height;
		virtual_screen_width_ = vwidth;
		virtual_screen_height_ = vheight;
	}

	ScreenDimensionOverrideScope::~ScreenDimensionOverrideScope()
	{
		actual_screen_width_ = old_width;
		actual_screen_height_ = old_height;
		virtual_screen_width_ = vold_width;
		virtual_screen_height_ = vold_height;
	
	}
}
