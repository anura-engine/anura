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

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <cstdio>
#if !defined(_MSC_VER)
#include <sys/wait.h>
#endif

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#if defined(_DEBUG)
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#else
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")
//#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#endif
#endif

#include <boost/lexical_cast.hpp>

#include "FontDriver.hpp"

#include "asserts.hpp"
#include "auto_update_window.hpp"
#include "background_task_pool.hpp"
#include "breakpad.hpp"
#include "checksum.hpp"
#include "controls.hpp"
#include "custom_object.hpp"
#include "custom_object_functions.hpp"
#include "draw_scene.hpp"
#include "editor.hpp"
#include "difficulty.hpp"
#include "external_text_editor.hpp"
#include "filesystem.hpp"
#include "formula_callable_definition.hpp"
#include "formula_object.hpp"
#include "formula_profiler.hpp"
#include "framed_gui_element.hpp"
#include "graphical_font.hpp"
#include "gui_section.hpp"
#include "hex.hpp"
#include "i18n.hpp"
#include "input.hpp"
#include "joystick.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "level_object.hpp"
#include "level_runner.hpp"
#include "load_level.hpp"
#include "loading_screen.hpp"
#include "md5.hpp"
#include "message_dialog.hpp"
#include "module.hpp"
#include "multiplayer.hpp"
#include "player_info.hpp"
#include "preferences.hpp"
#include "preprocessor.hpp"
#include "profile_timer.hpp"
#include "screen_handling.hpp"
#include "shared_memory_pipe.hpp"
#include "sound.hpp"
#include "stats.hpp"
#include "string_utils.hpp"
#include "tbs_internal_server.hpp"
#include "tile_map.hpp"
#include "theme_imgui.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"

#include "CameraObject.hpp"
#include "Canvas.hpp"
#include "SDLWrapper.hpp"
#include "Font.hpp"
#include "SceneGraph.hpp"
#include "SceneNode.hpp"
#include "WindowManager.hpp"

#include "xhtml_render_ctx.hpp"

#if defined(__APPLE__)
    #include "TargetConditionals.h"
    #if TARGET_OS_MAC
    #define decimal decimal_carbon
        #import <Cocoa/Cocoa.h>
        #undef decimal
    #endif
#endif

#if defined(USE_BOX2D)
#include "b2d_ffl.hpp"
#endif

#define DEFAULT_MODULE	"frogatto"

variant g_auto_update_info;

extern int g_vsync;

PREF_BOOL(desktop_fullscreen, false, "Sets the game window to be a fullscreen window the size of the desktop");

namespace 
{
	PREF_BOOL(auto_update_module, false, "Auto updates the module from the module server on startup (number of milliseconds to spend attempting to update the module)");
	PREF_BOOL(force_auto_update, false, "Will do a forced sync of auto-updates");
	PREF_STRING(auto_update_anura, "", "Auto update Anura's binaries from the module server using the given name as the module ID (e.g. anura-windows might be the id for the windows binary)");
	PREF_INT(auto_update_timeout, 5000, "Timeout to use on auto updates (given in milliseconds)");

	PREF_BOOL(resizeable, false, "Window is dynamically resizeable.");
	PREF_INT(min_window_width, 1024, "Minimum window width when auto-determining window size");
	PREF_INT(min_window_height, 768, "Minimum window height when auto-determining window size");

	PREF_INT(max_window_width, 10240, "Minimum window width when auto-determining window size");
	PREF_INT(max_window_height, 7680, "Minimum window height when auto-determining window size");

	PREF_BOOL(disable_global_alpha_filter, false, "Disables using alpha-colors.png to denote some special colors as 'alpha colors'");

	PREF_INT(auto_size_ideal_width, 0, "");
	PREF_INT(auto_size_ideal_height, 0, "");
	PREF_BOOL(desktop_fullscreen_force, false, "(Windows) forces desktop fullscreen to actually use fullscreen rather than a borderless window the size of the desktop");
	PREF_BOOL(msaa, false, "Use msaa");


#if defined(_MSC_VER)
	const std::string anura_exe_name = "anura.exe";
#else
	const std::string anura_exe_name = "";
#endif

	std::vector<std::string> alternative_anura_exe_names() {
		std::vector<std::string> result;
#if defined(_MSC_VER)
		for(int i = 0; i != 10; ++i) {
			std::ostringstream s;
			s << "anura" << i << ".exe";
			result.push_back(s.str());
		}
#endif
		return result;
	}

	bool show_title_screen(std::string& level_cfg)
	{
		//currently the titlescreen is disabled.
		return false;
	}

	void print_help(const std::string& argv0)
	{
		std::cout << "Usage: " << argv0 << " [OPTIONS]\n" <<
			"\n" <<
			"User options:\n" <<
			//"      --bigscreen              FIXME\n" <<
			"      --config-path=PATH       sets the path to the user config dir\n" <<
			"      --fullscreen             starts in fullscreen mode\n" <<
			"      --height[=]NUM           sets the game window height to which contents\n" <<
			"                                 are scaled\n" <<
			"      --host                   set the game server host address\n" <<
			"      --[no-]joystick          enables/disables joystick support\n" <<
			"      --level[=]LEVEL_FILE     starts the game using the specified level file,\n" <<
			"                                 relative to the level path\n" <<
			"      --level-path=PATH        sets the path to the game level files\n" <<
			"      --[no-]music             enables/disables game music\n" <<
			"      --native                 one pixel in-game equals one pixel on monitor\n" <<
			"      --relay                  use the server as a relay in multiplayer rather\n" <<
			"                                 than trying to initiate direct connections\n" <<
			"      --[no-]resizable         allows/disallows to resize the game window\n" <<
			"      ----module-args=ARGS     map of arguments passed to the module\n" <<
			"      --scale                  enables an experimental pixel art interpolation\n" <<
			"                                 algorithm for scaling the game graphics (some\n" <<
			"                                 issues with this still have to be solved)\n" <<
			"      --[no-]send-stats        enables/disables sending game statistics over\n"
			"                                 the network\n" <<
			"      --server=URL             sets the server to use for the TBS client based\n"
			"                                 on the given url\n" <<
			"      --user=USERNAME          sets the username to use as part of the TBS\n"
			"                                 server and module system\n" <<
			"      --pass=PASSWORD          sets the password to use as part of the TBS\n"
			"                                 server and module system\n" <<
			"      --[no-]sound             enables/disables sound and music support\n" <<
			"      --widescreen             sets widescreen mode, increasing the game view\n" <<
			"                                 area for wide screen displays\n" <<
			"      --width[=]NUM            sets the game window width to which contents are\n" <<
			"                                 scaled\n" <<
			"      --windowed               starts in windowed mode\n" <<
			"      --wvga                   sets the display size to 800x480\n" <<
			"\n" <<
			"Diagnostic options:\n" <<
			"      --[no-]debug             enables/disables debug mode\n" <<
			"      --[no-]fps               enables/disables framerate display\n" <<
			"      --set-fps=FPS            sets the framerate to FPS\n" <<
			"      --potonly                use power of two-sized textures only\n" <<
			"      --textures16             use 16 bpp textures only (default on iPhone)\n" <<
			"      --textures32             use 32 bpp textures (default on PC/Mac)\n" <<
			"\n" <<
			"Developer options:\n" <<
			"      --benchmarks             runs all the engine's benchmarks (intended to\n" <<
			"                                 measure the speed of certain low-level\n" <<
			"                                 functions), only useful if you're actually\n" <<
			"                                 hacking on the engine to optimize the speed\n" <<
			"                                 of these\n" <<
			"      --benchmarks=NAME        runs a single named benchmark code\n" <<
			"      --[no-]compiled          enable or disable precompiled game data\n" <<
			"      --edit                   starts the game in edit mode.\n" <<
			//"      --profile                FIXME\n" <<
			//"      --profile=FILE           FIXME\n" <<
			"      --show-hitboxes          turns on the display of object hitboxes\n" <<
			"      --show-controls          turns on the display of iPhone control hitboxes\n" <<
			"      --simipad                changes various options to emulate an iPad\n" <<
			"                                 environment\n" <<
			"      --simiphone              changes various options to emulate an iPhone\n" <<
			"                                 environment\n" <<
			"      --no-autopause           Stops the game from pausing automatically\n" <<
			"                                 when it loses focus\n" <<
			"      --tests                  runs the game's unit tests and exits\n" <<
			"      --tests=\"foo,bar,baz\"  runs the named unit tests and exits\n" <<
			"      --no-tests               skips the execution of unit tests on startup\n"
			"      --utility=NAME           runs the specified UTILITY( NAME ) code block,\n" <<
			"                                 such as compile_levels or compile_objects,\n" <<
			"                                 with the specified arguments\n" <<
		   preferences::get_registered_helpstring();
	}

	int load_module(const std::string& mod, std::vector<std::string>* argv)
	{
		module::set_core_module_name(mod);

		variant mod_info = module::get(mod);
		if(mod_info.is_null()) {
			return -1;
		}
		module::reload(mod);
		if(mod_info["arguments"].is_list()) {
			const std::vector<std::string>& arguments = mod_info["arguments"].as_list_string();
			auto insertion_point = argv->size();
			for(std::vector<std::string>::size_type i = 0; i != argv->size(); ++i) {
				const char* module_arg = "--module=";
				if(std::equal(module_arg, module_arg+strlen(module_arg), (*argv)[i].c_str())) {
					insertion_point = i+1;
					break;
				}
			}

			if(insertion_point == argv->size()) {
				for(std::vector<std::string>::size_type i = 0; i != argv->size(); ++i) {
					const char* utility_arg = "--utility=";
					if(std::equal(utility_arg, utility_arg+strlen(utility_arg), (*argv)[i].c_str())) {
						insertion_point = i;
						break;
					}
				}
			}

			argv->insert(argv->begin() + insertion_point, arguments.begin(), arguments.end());
		}	
		return 0;
	}


	void set_alpha_masks()
	{
		LOG_INFO("Setting Alpha Masks:");
		using namespace KRE;
		std::vector<Color> alpha_colors;

		auto surf = Surface::create("alpha-colors.png");
		surf->iterateOverSurface([&alpha_colors](int x, int y, int r, int g, int b, int a) {
			alpha_colors.emplace_back(r, g, b);
			LOG_INFO("Added alpha color: (" << r << "," << g << "," << b << ")");	
		});

		Surface::setAlphaFilter([=](int r, int g, int b) {
			for(auto& c : alpha_colors) {
				if(c.ri() == r && c.gi() == g && c.bi() == b) {
					return true;
				}
			}
			return false;
		});
	}


	void process_log_level(const std::string& argstr)
	{
		SDL_LogPriority log_priority = SDL_LOG_PRIORITY_INFO;
		std::string::size_type pos = argstr.find('=');
		if(pos != std::string::npos) {
			std::string level = argstr.substr(pos+1);
			if(level == "verbose") {
				log_priority = SDL_LOG_PRIORITY_INFO;
			} else if(level == "debug") {
				log_priority = SDL_LOG_PRIORITY_DEBUG;
			} else if(level == "info") {
				log_priority = SDL_LOG_PRIORITY_INFO;
			} else if(level == "warn") {
				log_priority = SDL_LOG_PRIORITY_WARN;
			} else if(level == "error") {
				log_priority = SDL_LOG_PRIORITY_ERROR;
			} else if(level == "critical") {
				log_priority = SDL_LOG_PRIORITY_CRITICAL;
			}
		}
		SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, log_priority);
	}

	std::string g_log_filename;
	FILE* g_log_file;

	void log_data(void* userdata, int category, SDL_LogPriority priority, const char* message)
	{
		if(g_log_file) {
			fprintf(g_log_file, "%s\n", message);
			fflush(g_log_file);
		}
	}

	void set_log_file(const std::string& fname)
	{
		if(g_log_file) {
			fclose(g_log_file);
		}

		g_log_file = fopen(fname.c_str(), "w");

		SDL_LogSetOutputFunction(log_data, nullptr);
	}

} //namespace

//   Meant to apply to only-online or mainly-online games while not
// impacting standalone and mainly-offline games. The `citadel` module
// would use this, and the default value is taylored to the pre existing
// behavior of the module. The `frogatto` module does not use this.
PREF_BOOL(remember_me, true, "Remember me (my gamer account) when connecting to the server");

void auto_select_resolution(const KRE::WindowPtr& wm, int *width, int *height, bool reduce)
{
	ASSERT_LOG(width != nullptr, "width is null.");
	ASSERT_LOG(height != nullptr, "height is null.");

	auto mode = wm->getDisplaySize();
	auto best_mode = mode;
	bool found = false;
	
	const float MinReduction = reduce ? 0.9f : 2.0f;
	for(auto& candidate_mode : wm->getWindowModes([](const KRE::WindowMode&){ return true; })) {
		if(g_auto_size_ideal_width && g_auto_size_ideal_height) {
			if(found && candidate_mode.width < best_mode.width) {
				continue;
			}

			if(candidate_mode.width > mode.width * MinReduction) {
				LOG_INFO("REJECTED MODE IS " << candidate_mode.width << "x" << candidate_mode.height);
				continue;
			}

			int h = (candidate_mode.width * g_auto_size_ideal_height) / g_auto_size_ideal_width;
			if(h > mode.height * MinReduction) {
				continue;
			}

			best_mode = candidate_mode;
			best_mode.height = h;
			found = true;

			LOG_INFO("BETTER MODE IS " << best_mode.width << "x" << best_mode.height);

		} else
		if(    candidate_mode.width < mode.width * MinReduction
			&& candidate_mode.height < mode.height * MinReduction
			&& ((candidate_mode.width >= best_mode.width
			&& candidate_mode.height >= best_mode.height) || !found)
		) {
			found = true;
			LOG_INFO("BETTER MODE IS " << candidate_mode.width << "x" << candidate_mode.height << " vs " << best_mode.width << "x" << best_mode.height);
			best_mode = candidate_mode;
		} else {
			LOG_INFO("REJECTED MODE IS " << candidate_mode.width << "x" << candidate_mode.height);
		}
	}
	
	LOG_INFO("CHOSEN MODE IS " << best_mode.width << "x" << best_mode.height);

	*width = best_mode.width;
	*height = best_mode.height;
}

extern int g_tile_scale;
extern int g_tile_size;

std::string g_anura_exe_name;

int main(int argcount, char* argvec[])
{
	g_anura_exe_name = argvec[0];

	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

#ifdef USE_BREAKPAD
	breakpad::install();
#endif

	variant::registerThread();

	{
		std::vector<std::string> args;
		for(int i = 0; i != argcount; ++i) {
			std::string argstr(argvec[i]);
			if(argstr.substr(0,12) == "--log-level=") {
				process_log_level(argstr);
			} else if(argstr.substr(0,11) == "--log-file=") {
				set_log_file(argstr.substr(11));
			} else {
				args.emplace_back(argstr);
			}
		}

		preferences::set_argv(args);
	}

#ifdef _MSC_VER
#if(_WIN32_WINNT >= 0x0600)
	SetProcessDPIAware();
#endif
#if defined(_DEBUG)
	std::freopen("CON", "w", stderr);
	std::freopen("CON", "w", stdout);
#else
//	std::freopen("CON", "w", stderr);
//	std::freopen("CON", "w", stdout);
//	std::freopen("stdout.txt","w",stdout);
//	std::freopen("stderr.txt","w",stderr);
#endif
#endif
#if defined(__ANDROID__)
	std::freopen("stdout.txt","w",stdout);
	std::freopen("stderr.txt","w",stderr);
	std::cerr.sync_with_stdio(true);
#endif

#if defined(__APPLE__) && TARGET_OS_MAC
    chdir([[[NSBundle mainBundle] resourcePath] fileSystemRepresentation]);
#endif

#ifdef NO_STDERR
	std::freopen("/dev/null", "w", stderr);
	std::cerr.sync_with_stdio(true);
#endif

	LOG_INFO("Anura engine version " << preferences::version());

#if defined(TARGET_BLACKBERRY)
	chdir("app/native");
	std::cout<< "Changed working directory to: " << getcwd(0, 0) << std::endl;
#endif

	game_logic::init_callable_definitions();

	std::string level_cfg = "titlescreen.cfg";
	bool unit_tests_only = false, skip_tests = false;
	bool run_benchmarks = false;
	std::vector<std::string> benchmarks_list;
	std::string utility_program;
	std::vector<std::string> util_args;
	boost::scoped_ptr<std::vector<std::string> > test_names;
	std::string server = "wesnoth.org";
	bool is_child_utility = false;

	const char* profile_output = nullptr;
	std::string profile_output_buf;

	std::string orig_level_cfg = level_cfg;
	std::string override_level_cfg = "";

	int modules_loaded = 0;

	int requested_width = 0;
	int requested_height = 0;

	std::vector<std::string> argv;
	for(int n = 1; n < argcount; ++n) {
		argv.push_back(argvec[n]);        
        if(argv.size() >= 2 && argv[argv.size()-2] == "-NSDocumentRevisionsDebugMode" && argv.back() == "YES") {
            //XCode passes these arguments by default when debugging -- make sure they are ignored.
            argv.resize(argv.size()-2);
        }
	}

	LOG_INFO("Default Tile Size: " << g_tile_size);
	LOG_INFO("Default Tile Scale: " << g_tile_scale);

	LOG_INFO("Build Options:");
	for(auto bo : preferences::get_build_options()) {
		LOG_INFO("    " << bo);
	}

	if(sys::file_exists("./master-config.cfg")) {
		LOG_INFO("LOADING CONFIGURATION FROM master-config.cfg");
		variant cfg;
		
		try {
			cfg = json::parse_from_file("./master-config.cfg");
		} catch(json::ParseError& error) {
			ASSERT_LOG(false, "" << error.errorMessage());
		}

		if(cfg.is_map()) {
			if(cfg["arguments"].is_null() == false) {
				std::vector<std::string> additional_args = cfg["arguments"].as_list_string();
				argv.insert(argv.begin(), additional_args.begin(), additional_args.end());
				LOG_INFO("ADDING ARGUMENTS FROM master-config.cfg:");
				for(int n = 0; n < cfg["arguments"].num_elements(); ++n) {
					LOG_INFO("   " << cfg["arguments"][n].as_string());
				}
			}
		}
	}

	stats::record_program_args(argv);

	{
	//copy argv since the loop can modify it
	std::vector<std::string> argv_copy = argv;
	for(const std::string& arg : argv_copy) {
		std::string arg_name, arg_value;
		std::string::const_iterator equal = std::find(arg.begin(), arg.end(), '=');
		if(equal != arg.end()) {
			arg_name = std::string(arg.begin(), equal);
			arg_value = std::string(equal+1, arg.end());
		}
		if(arg_name == "--module") {
			preferences::set_preferences_path_from_module(arg_value);

			bool update_launcher = false;
			for(size_t n = 0; n < argv.size(); ++n) {
				if(argv[n] == "--utility=update_launcher") {
					module::set_core_module_name(arg_value);
					update_launcher = true;
					break;
				}
			}

			//don't load the actual module if we're in the update launcher.
			if(!update_launcher && load_module(arg_value, &argv) != 0) {
				bool auto_update = false;

				for(size_t n = 0; n < argv.size(); ++n) {
					if(argv[n] == "--auto-update-module") {
						auto_update = true;
						break;
					}
				}

				if(!auto_update) {
					LOG_ERROR("FAILED TO LOAD MODULE: " << arg_value);
					return -1;
				}
			}
			++modules_loaded;
		} else if(arg == "--tests" || arg_name == "--tests") {
			unit_tests_only = true;
		}
	}
	}

	if(modules_loaded == 0 && !unit_tests_only) {
		if(load_module(DEFAULT_MODULE, &argv) != 0) {
			LOG_INFO("FAILED TO LOAD MODULE: " << DEFAULT_MODULE);
			return -1;
		}
	} else if(unit_tests_only) {
		module::set_core_module_name(DEFAULT_MODULE);
	}

	preferences::load_preferences();

	// load difficulty settings after module, before rest of args.
	try {
		difficulty::manager();
	} catch (json::ParseError & e) {
		if (!unit_tests_only) {
			ASSERT_LOG(false, "JSON parse error: " << e.errorMessage());
		}
	}

	for(size_t n = 0; n < argv.size(); ++n) {
		const size_t argc = argv.size();
		const std::string arg(argv[n]);
		std::string arg_name, arg_value;
		std::string::const_iterator equal = std::find(arg.begin(), arg.end(), '=');
		if(equal != arg.end()) {
			arg_name = std::string(arg.begin(), equal);
			arg_value = std::string(equal+1, arg.end());
		}
		LOG_INFO("ARGS: " << arg);
		if(arg.substr(0,4) == "-psn") {
			// ignore.
		} else if(arg_name == "--module") {
			// ignore already processed.
		} else if(arg_name == "--profile" || arg == "--profile") {
			profile_output_buf = arg_value;
			profile_output = profile_output_buf.c_str();
		} else if(arg_name == "--utility" || arg_name == "--utility-child") {
			if(arg_name == "--utility-child") {
				is_child_utility = true;
			}
			utility_program = arg_value;
			for(++n; n < argc; ++n) {
				const std::string arg(argv[n]);
				util_args.push_back(arg);
			}

			break;
		} else if(arg == "--benchmarks") {
			run_benchmarks = true;
		} else if(arg_name == "--benchmarks") {
			run_benchmarks = true;
			benchmarks_list = util::split(arg_value);
		} else if(arg == "--no-tests") {
			skip_tests = true;
		} else if(arg == "--tests" || arg_name == "--tests") {
			// If there is a value, split it as the list of tests
			if (arg_value.size()) {
				// If the list is quoted, remove the quotes
				if (arg_value.at(0) == '\"' && arg_value.at(arg_value.size()-1) == '\"') {
					test_names.reset(new std::vector<std::string> (util::split(arg_value.substr(1, arg_value.size()-2))));
				} else {
					test_names.reset(new std::vector<std::string> (util::split(arg_value)));
				}
			}
		} else if(arg.substr(0,12) == "--log-level=") {
			//respect these log arguments here too so they can be set in
			//master-config.cfg or module.cfg
			process_log_level(arg);
		} else if(arg.substr(0,11) == "--log-file=") {
			set_log_file(arg.substr(11));
		} else if(arg_name == "--level") {
			override_level_cfg = arg_value;
		} else if(arg == "--level" && n+1 < argc) {
			override_level_cfg = argv[++n];
		} else if(arg_name == "--host") {
			server = arg_value;
		} else if(arg == "--host" && n+1 < argc) {
			server = argv[++n];
		} else if(arg == "--compiled") {
			preferences::set_load_compiled(true);
#ifndef NO_EDITOR
		} else if(arg == "--edit") {
			preferences::set_edit_on_start(true);
#endif
		} else if(arg == "--no-compiled") {
			preferences::set_load_compiled(false);
		} else if(arg == "--help" || arg == "-h") {
			print_help(std::string(argvec[0]));
			return 0;
		} else {
			bool require = true;
			std::string a(argv[n]);
			if(a.empty() == false && a[0] == '?') {
				//putting a ? in front of an argument indicates to only use the
				//argument if the option is known, and to ignore it silently if
				//it's unknown to the engine.
				a.erase(a.begin());
				require = false;
			}

			const bool res = preferences::parse_arg(a, n+1 < argc ? argv[n+1] : "");
			if(!res && require) {
				print_help(std::string(argvec[0]));
				LOG_ERROR("unrecognized arg: '" << arg);
				return -1;
			}
		}
	}

	preferences::expand_data_paths();

	if(g_auto_update_anura != "" && anura_exe_name != "" && sys::file_exists("manifest.cfg")) {
		std::string exe_name = argvec[0];
		if(exe_name.size() >= anura_exe_name.size() && std::equal(exe_name.end()-anura_exe_name.size(), exe_name.end(), anura_exe_name.begin())) {
			variant manifest = json::parse(sys::read_file("manifest.cfg"));
			if(manifest.is_map()) {
				variant anura_entry = manifest[anura_exe_name];
				if(anura_entry.is_map()) {
					std::string expected_md5 = anura_entry["md5"].as_string();
					std::string match;
					if(expected_md5 != md5::sum(sys::read_file(exe_name))) {
						for(auto fname : alternative_anura_exe_names()) {
							if(sys::file_exists(fname) && md5::sum(sys::read_file(fname)) == expected_md5) {
								match = fname;
								break;
							}
						}


						ASSERT_LOG(match != "", "anura.exe does not match md5 in manifest and no alternative anura.exe found");

						try {
							sys::move_file(exe_name, "anura.exe.tmp");
							sys::move_file(match, exe_name);
							match = exe_name;
						} catch(...) {
						}


						std::vector<char*> args;
						for(char** a = argvec; *a; ++a) {
							args.push_back(*a);
						}
						args.push_back(nullptr);
				
						exe_name.resize(exe_name.size() - anura_exe_name.size());
						exe_name += match;
						args[0] = const_cast<char*>(exe_name.c_str());
#if defined(_MSC_VER)
						_execv(args[0], &args[0]);
#else
						execv(args[0], &args[0]);
#endif
						LOG_ERROR("Could not exec()");
					}
				}
			}
		}
	}

	background_task_pool::manager bg_task_pool_manager;

	LOG_INFO("Preferences dir: " << preferences::user_data_path());

	//make sure that the user data path exists.
	if(!preferences::setup_preferences_dir()) {
		LOG_ERROR("cannot create preferences dir!");
	}

	bool update_require_restart = false;
	variant_builder update_info;
	if(g_auto_update_module || g_auto_update_anura != "") {

		//remove any .tmp files that may have been left from previous runs.
		std::vector<std::string> tmp_files;
		sys::get_files_in_dir(".", &tmp_files, nullptr);
		for(auto f : tmp_files) {
			if(f.size() > 4 && std::equal(f.end()-4,f.end(),".tmp")) {
				try {
					sys::remove_file(f);
				} catch(...) {
				}
			}
		}

		ffl::IntrusivePtr<module::client> cl, anura_cl;
		
		if(g_auto_update_module) {
			cl.reset(new module::client);
			cl->install_module(module::get_module_name(), g_force_auto_update);
			update_info.add("attempt_module", true);
		}

		if(g_auto_update_anura != "") {
			anura_cl.reset(new module::client);
			anura_cl->set_install_image(true);
			anura_cl->install_module(g_auto_update_anura, g_force_auto_update);
			update_info.add("attempt_anura", true);
		}


		int nbytes_transferred = 0, nbytes_anura_transferred = 0;
		int start_time = profile::get_tick_time();
		int original_start_time = profile::get_tick_time();
		bool timeout = false;
		LOG_INFO("Requesting update to module from server...");
		int nupdate_cycle = 0;

		{
		auto_update_window update_window;
		while(cl || anura_cl) {
			update_window.process();

			int nbytes_obtained = 0;
			int nbytes_needed = 0;

			++nupdate_cycle;

			if(cl) {
				const int transferred = cl->nbytes_transferred();
				nbytes_obtained += transferred;
				nbytes_needed += cl->nbytes_total();
				if(transferred != nbytes_transferred) {
					if(nupdate_cycle%10 == 0) {
						LOG_INFO("Transferred " << (transferred/1024) << "/" << (cl->nbytes_total()/1024) << "KB");
					}
					start_time = profile::get_tick_time();
					nbytes_transferred = transferred;
				}
			}

			if(anura_cl) {
				const int transferred = anura_cl->nbytes_transferred();
				nbytes_obtained += transferred;
				nbytes_needed += anura_cl->nbytes_total();
				if(transferred != nbytes_anura_transferred) {
					if(nupdate_cycle%10 == 0) {
						LOG_INFO("Transferred " << (transferred/1024) << "/" << (anura_cl->nbytes_total()/1024) << "KB");
					}
					start_time = profile::get_tick_time();
					nbytes_anura_transferred = transferred;
				}
			}

			const int time_taken = profile::get_tick_time() - start_time;
			if(time_taken > g_auto_update_timeout) {
				LOG_ERROR("Timed out updating module. Canceling. " << time_taken << "ms vs " << g_auto_update_timeout << "ms");
				break;
			}

			char msg[1024];
			sprintf(msg, "Updating Game. Transferred %.02f/%.02fMB", float(nbytes_obtained/(1024.0*1024.0)), float(nbytes_needed/(1024.0*1024.0)));

			update_window.set_message(msg);

			const float ratio = nbytes_needed <= 0 ? 0 : static_cast<float>(nbytes_obtained)/static_cast<float>(nbytes_needed);
			update_window.set_progress(ratio);
			update_window.draw();

			SDL_Event event;
			while(SDL_PollEvent(&event)) {
				if(event.type == SDL_QUIT) {
					cl.reset();
					anura_cl.reset();
					break;
				}
			}

			const int target_end = profile::get_tick_time() + 50;
			while(static_cast<int>(profile::get_tick_time()) < target_end && (cl || anura_cl)) {
				if(cl && !cl->process()) {
					if(cl->error().empty() == false) {
						LOG_ERROR("Error while updating module: " << cl->error().c_str());
						update_info.add("module_error", variant(cl->error()));
					} else {
						update_info.add("complete_module", true);
						update_require_restart = cl->nfiles_written() != 0;
					}
					cl.reset();
				}

				if(anura_cl && !anura_cl->process()) {
					if(anura_cl->error().empty() == false) {
						LOG_ERROR("Error while updating anura: " << anura_cl->error().c_str());
						update_info.add("anura_error", variant(anura_cl->error()));
					} else {
						update_info.add("complete_anura", true);
						update_require_restart = anura_cl->nfiles_written() != 0;
					}
					anura_cl.reset();
				}
			}
		}

		} //dispose update_window

		if(update_require_restart) {
			std::vector<char*> args;
			for(char** a = argvec; *a; ++a) {
				std::string arg(*a);
				if(arg != "--force-auto-update" && arg != "--force_auto_update") {
					args.push_back(*a);
				}
			}
			args.push_back(nullptr);
#if defined(_MSC_VER)
			_execv(args[0], &args[0]);
#else
			execv(args[0], &args[0]);
#endif
			LOG_ERROR("Could not exec()");
		}
	}

	g_auto_update_info = update_info.build();

	checksum::manager checksum_manager;
#ifndef NO_EDITOR
	sys::FilesystemManager fs_manager;
#endif // NO_EDITOR

	const stats::Manager stats_manager;

	const SharedMemoryPipeManager ipc_manager;

	const tbs::internal_server_manager internal_server_manager_scope(preferences::internal_tbs_server());

	if(utility_program.empty() == false 
		&& test::utility_needs_video(utility_program) == false) {
#ifdef _MSC_VER
		std::freopen("CON", "w", stderr);
		std::freopen("CON", "w", stdout);
#endif
		test::run_utility(utility_program, util_args);
		return 0;
	}

	try {
		if(!skip_tests) {
			if (test_names) {
				if (!test::run_tests(test_names.get())) {
					return -1;
				}
			} else {
				if (!test::run_tests()) {
					return -1;
				}
			}
		}
	} catch(json::ParseError& error) {
		ASSERT_LOG(false, "Error parsing JSON when running starting validation: " << error.errorMessage());
	}

	if(unit_tests_only) {
		return 0;
	}

	// Create the main window.
	// Initalise SDL and Open GL.
	using namespace KRE;

	SDL::SDL_ptr manager(new SDL::SDL());

	WindowManager wm("SDL");

	variant_builder hints;
	hints.add("renderer", "opengl");
	hints.add("use_vsync", g_vsync != 0 ? true : false);
	hints.add("width", preferences::requested_window_width() > 0 ? preferences::requested_window_width() : 800);
	hints.add("height", preferences::requested_window_height() > 0 ? preferences::requested_window_height() : 600);
	hints.add("resizeable", g_resizeable);
	hints.add("fullscreen", preferences::get_screen_mode() == preferences::ScreenMode::FULLSCREEN_WINDOWED ? true : false);
	if(g_msaa) {
		hints.add("use_multisampling", true);
	}


	if(g_desktop_fullscreen) {
		//KRE::WindowMode mode = main_wnd->getDisplaySize();

		SDL_DisplayMode dm;
		int res = SDL_GetDesktopDisplayMode(0, &dm);
		ASSERT_LOG(res == 0, "Could not get desktop display mode: " << SDL_GetError());

		preferences::adjust_virtual_width_to_match_physical(dm.w, dm.h);

		hints.set("width", dm.w);
		hints.set("height", dm.h);
#if defined(_MSC_VER)
		if(g_desktop_fullscreen_force) {
			hints.set("fullscreen", true);
		} else {
			hints.set("fullscreen", false);
			hints.set("borderless", true);
		}
#else
		hints.set("fullscreen", true);
#endif
	}

	variant built_hints = hints.build();
	LOG_INFO("Initializing fullscreen window: " << built_hints.write_json());

    KRE::WindowPtr main_wnd = wm.allocateWindow(built_hints);
	main_wnd->setWindowTitle(module::get_module_pretty_name());
	
	if(!g_desktop_fullscreen &&
	   preferences::auto_size_window() 
		&& preferences::requested_window_width() == 0 
		&& preferences::requested_window_height() == 0) {
		int width = 0;
		int height = 0;
		auto_select_resolution(main_wnd, &width, &height, true);

		preferences::adjust_virtual_width_to_match_physical(width, height);

		main_wnd->setWindowSize(width, height);
	}

	int vw = preferences::requested_virtual_window_width() > 0 
		? preferences::requested_virtual_window_width() 
		: main_wnd->width();
	int vh = preferences::requested_virtual_window_height() > 0 
		? preferences::requested_virtual_window_height() 
		: main_wnd->height();

	wm.createWindow(main_wnd);
	
	auto canvas = Canvas::getInstance();
	LOG_INFO("canvas size: " << canvas->width() << "x" << canvas->height());
	
	//WindowManager::getMainWindow()->setWindowSize(main_wnd->width(), main_wnd->height());

	graphics::GameScreen::get().setDimensions(main_wnd->width(), main_wnd->height());
	graphics::GameScreen::get().setVirtualDimensions(vw, vh);
	//main_wnd->setWindowIcon(module::map_file("images/window-icon.png"));

	//we prefer late swap tearing so as to minimize frame loss when possible
	int swap_result = SDL_GL_SetSwapInterval(g_vsync != 0 ? -1 : 0);
	if(swap_result != 0 && g_vsync != 0) {
		swap_result = SDL_GL_SetSwapInterval(1);
	}
	
	if(swap_result != 0) {
		LOG_ERROR("Could not set swap interval with SDL_GL_SetSwapInterval: " << SDL_GetError());
	}

	try {
		std::map<std::string, std::string> shader_files;
		module::get_unique_filenames_under_dir("data/shaders/", &shader_files);
		for(auto p : shader_files) {
			if(p.second.size() >= 4 && std::equal(p.second.end()-4, p.second.end(), ".cfg")) {
				ShaderProgram::loadFromVariant(json::parse_from_file(p.second));
			}
		}
		ShaderProgram::loadFromVariant(json::parse_from_file("data/shaders.cfg"));
	} catch(const json::ParseError& e) {
		LOG_ERROR("ERROR PARSING: " << e.errorMessage());
		return 1;
	}

	// Set the image loading filter function, so that files are found in the correct place.
	Surface::setFileFilter(FileFilterType::LOAD, [](const std::string& s){ return module::map_file("images/" + s); });
	Surface::setFileFilter(FileFilterType::SAVE, [](const std::string& s){ return std::string(preferences::user_data_path()) + s; });

	if(g_disable_global_alpha_filter == false) {
		set_alpha_masks();
	}

	//SceneGraphPtr scene = SceneGraph::create("root");
	//SceneNodePtr root = scene->getRootNode();
	//root->setNodeName("root_node");
	auto orthocam = std::make_shared<Camera>("orthocam", 0, main_wnd->width(), 0, main_wnd->height());
	//root->attachCamera(orthocam);

	// Set a default camera in case no other is specified.
	DisplayDevice::getCurrent()->setDefaultCamera(orthocam);

	// Set the default font to use for rendering. This can of course be overridden when rendering the
	// text to a texture.
	Font::setDefaultFont(module::get_default_font() == "bitmap" 
		? "FreeMono" 
		: module::get_default_font());
	std::map<std::string,std::string> font_paths;
	std::map<std::string,std::string> font_paths2;
	module::get_unique_filenames_under_dir("data/fonts/", &font_paths);
	for(auto& fp : font_paths) {
		font_paths2[module::get_id(fp.first)] = fp.second;
	}
	KRE::FontDriver::setFontProvider("freetype");
	KRE::Font::setAvailableFonts(font_paths2);
	KRE::FontDriver::setAvailableFonts(font_paths2);
	font_paths.clear();
	font_paths2.clear();

	xhtml::RenderContextManager rcm;

	i18n::init ();
	LOG_DEBUG("After i18n::init()");

	// Read auto-save file if it exists.
	if(sys::file_exists(preferences::auto_save_file_path()) 
		&& sys::read_file(std::string(preferences::auto_save_file_path()) + ".stat") == "1") {
		level_cfg = "autosave.cfg";
		sys::write_file(std::string(preferences::auto_save_file_path()) + ".stat", "0");
	}

	if(override_level_cfg.empty() != true) {
		level_cfg = override_level_cfg;
		orig_level_cfg = level_cfg;
	}

#ifndef NO_EDITOR
	const ExternalTextEditor::Manager editor_manager;
#endif // NO_EDITOR

#if defined(USE_BOX2D)
	box2d::manager b2d_manager;
#endif

	const load_level_manager load_manager;

	{ //manager scope
	const sound::Manager sound_manager;
	const joystick::Manager joystick_manager;
	
#ifndef NO_EDITOR
	editor::manager editor_manager;
#endif

	variant preloads;
	LoadingScreen loader;
	try {
		variant gui_node = json::parse_from_file(preferences::load_compiled() ? "data/compiled/gui.cfg" : "data/gui.cfg");
		GuiSection::init(gui_node);
		loader.drawAndIncrement(_("Initializing GUI"));
		FramedGuiElement::init(gui_node);

		try {
			hex::load("data/");
		} catch(KRE::ImageLoadError& ile) {
			ASSERT_LOG(false, ile.what());
		}

		GraphicalFont::initForLocale(i18n::get_locale());
		preloads = json::parse_from_file("data/preload.cfg");
		int preload_items = preloads["preload"].num_elements();
		loader.setNumberOfItems(preload_items+7); // 7 is the number of items that will be loaded below
		CustomObject::init();
		loader.drawAndIncrement(_("Initializing custom object functions"));
		loader.drawAndIncrement(_("Initializing textures"));
		loader.load(preloads);
		loader.drawAndIncrement(_("Initializing tiles"));
		TileMap::init(json::parse_from_file("data/tiles.cfg"));

		game_logic::FormulaObject::loadAllClasses();

	} catch(const json::ParseError& e) {
		LOG_ERROR("ERROR PARSING: " << e.errorMessage());
		return 0;
	}
	loader.draw(_("Loading level"));

	loader.finishLoading();
	//look to see if we got any quit events while loading.
	{
		SDL_Event event;
		while(input::sdl_poll_event(&event)) {
			if(event.type == SDL_QUIT) {
				return 0;
			}
		}
	}
	LOG_INFO("finishloading()");

	formula_profiler::Manager profiler(profile_output);

	if(run_benchmarks) {
		if(benchmarks_list.empty() == false) {
			test::run_benchmarks(&benchmarks_list);
		} else {
			test::run_benchmarks();
		}
		return 0;
	} else if(utility_program.empty() == false && test::utility_needs_video(utility_program) == true) {
		test::run_utility(utility_program, util_args);
		return 0;
	}

	bool quit = false;

	// Apply a new default theme to ImGui.
	theme_imgui_default();

	{
	LevelPtr lvl;
	while(!quit && !show_title_screen(level_cfg)) {
		if(!lvl) {
			lvl = load_level(level_cfg);
		}
		
		//see if we're loading a multiplayer level, in which case we
		//connect to the server.
		multiplayer::Manager mp_manager(lvl->is_multiplayer());
		if(lvl->is_multiplayer()) {
			multiplayer::setup_networked_game(server);
		}

		if(lvl->is_multiplayer()) {
			last_draw_position() = screen_position();
			std::string level_cfg = "waiting-room.cfg";
			LevelPtr wait_lvl(load_level(level_cfg));
			wait_lvl->finishLoading();
			wait_lvl->setMultiplayerSlot(0);
			if(wait_lvl->player()) {
				wait_lvl->player()->setCurrentLevel(level_cfg);
			}
			wait_lvl->setAsCurrentLevel();

			LevelRunner runner(wait_lvl, level_cfg, orig_level_cfg);

			multiplayer::sync_start_time(*lvl, std::bind(&LevelRunner::play_cycle, &runner));

			lvl->setMultiplayerSlot(multiplayer::slot());
		}

		last_draw_position() = screen_position();

		assert(lvl.get());

		if(lvl->player() && level_cfg != "autosave.cfg") {
			lvl->player()->setCurrentLevel(level_cfg);
			lvl->player()->getEntity().saveGame();
		}

		set_scene_title(lvl->title());

		try {
			quit = LevelRunner(lvl, level_cfg, orig_level_cfg).play_level();
			level_cfg = orig_level_cfg;
			lvl.reset();
		} catch(multiplayer_exception&) {
		}
	}
	}

	Level::clearCurrentLevel();

	} //end manager scope, make managers destruct before calling SDL_Quit

	preferences::save_preferences();

	std::set<variant*> loading;
	swap_variants_loading(loading);
	if(loading.empty() == false) {
		LOG_ERROR("Illegal object: " << write_uuid((*loading.begin())->as_callable_loading()));
		ASSERT_LOG(false, "Unresolved unserialized objects: " << loading.size());
	}

	return 0;
}
