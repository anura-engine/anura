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
#include "graphics.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <cstdio>
#if !defined(_MSC_VER)
#include <sys/wait.h>
#endif

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scoped_ptr.hpp>

#ifdef TARGET_OS_HARMATTAN
#include <glib-object.h>
#endif

#include "asserts.hpp"
#include "auto_update_window.hpp"
#include "background_task_pool.hpp"
#include "checksum.hpp"
#include "controls.hpp"
#include "custom_object.hpp"
#include "custom_object_functions.hpp"
#include "custom_object_type.hpp"
#include "draw_scene.hpp"
#ifndef NO_EDITOR
#include "editor.hpp"
#endif
#include "difficulty.hpp"
#include "external_text_editor.hpp"
#include "filesystem.hpp"
#include "font.hpp"
#include "foreach.hpp"
#include "formula_callable_definition.hpp"
#include "formula_object.hpp"
#include "formula_profiler.hpp"
#include "framed_gui_element.hpp"
#include "graphical_font.hpp"
#include "gui_section.hpp"
#include "i18n.hpp"
#include "input.hpp"
#include "ipc.hpp"
#include "iphone_device_info.h"
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
#include "raster.hpp"
#include "sound.hpp"
#include "stats.hpp"
#include "string_utils.hpp"
#include "surface_cache.hpp"
#include "tbs_internal_server.hpp"
#include "texture.hpp"
#include "texture_frame_buffer.hpp"
#include "tile_map.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"
#include "wm.hpp"

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

#if defined(TARGET_PANDORA) || defined(TARGET_TEGRA)
#include "eglport.h"
#elif defined(TARGET_BLACKBERRY)
#include <EGL/egl.h>
#endif

#define DEFAULT_MODULE	"frogatto"

variant g_auto_update_info;

namespace {
	PREF_BOOL(force_auto_update, false, "Will do a forced sync of auto-updates");
	PREF_BOOL(auto_update_module, false, "Auto updates the module from the module server on startup (number of milliseconds to spend attempting to update the module)");
	PREF_STRING(auto_update_anura, "", "Auto update Anura's binaries from the module server using the given name as the module ID (e.g. anura-windows might be the id for the windows binary)");
	PREF_INT(auto_update_timeout, 5000, "Timeout to use on auto updates (given in milliseconds)");

#if defined(_WINDOWS)
	const std::string anura_exe_name = "anura.exe";
#else
	const std::string anura_exe_name = "";
#endif

	std::vector<std::string> alternative_anura_exe_names() {
		std::vector<std::string> result;
#if defined(_WINDOWS)
		for(int i = 0; i != 10; ++i) {
			std::ostringstream s;
			s << "anura" << i << ".exe";
			result.push_back(s.str());
		}
#endif
		return result;
	}

	graphics::window_manager_ptr main_window;

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
"      --no-tests               skips the execution of unit tests on startup\n"
"      --utility=NAME           runs the specified UTILITY( NAME ) code block,\n" <<
"                                 such as compile_levels or compile_objects,\n" <<
"                                 with the specified arguments\n" <<
   preferences::get_registered_helpstring();
}

}

graphics::window_manager_ptr get_main_window()
{
	return main_window;
}

#if defined(UTILITY_IN_PROC)
boost::shared_ptr<char> child_args;

#if defined(_MSC_VER)
const std::string shared_sem_name = "Local\anura_local_process_semaphore";
HANDLE child_process;
HANDLE child_thread;
HANDLE child_stderr;
HANDLE child_stdout;
#else
const std::string shared_sem_name = "/anura_local_process_semaphore";
pid_t child_pid;
#endif

bool create_utility_process(const std::string& app, const std::vector<std::string>& argv)
{
#if defined(_MSC_VER)
	char app_np[MAX_PATH];
	// Grab the full path name
	DWORD chararacters_copied = GetModuleFileNameA(NULL, app_np,  MAX_PATH);
	ASSERT_LOG(chararacters_copied > 0, "Failed to get module name: " << GetLastError());
	std::string app_name_and_path(app_np, chararacters_copied);

	// windows version
	std::string command_line_params;
	command_line_params += app_name_and_path + " ";
	for(size_t n = 0; n != argv.size(); ++n) {
		command_line_params += argv[n] + " ";
	}
	child_args = boost::shared_ptr<char>(new char[command_line_params.size()+1]);
	memset(child_args.get(), 0, command_line_params.size()+1);
	memcpy(child_args.get(), &command_line_params[0], command_line_params.size());

	STARTUPINFOA siStartupInfo; 
	PROCESS_INFORMATION piProcessInfo;
	SECURITY_ATTRIBUTES saFileSecurityAttributes;
	memset(&siStartupInfo, 0, sizeof(siStartupInfo));
	memset(&piProcessInfo, 0, sizeof(piProcessInfo));
	siStartupInfo.cb = sizeof(siStartupInfo);
	saFileSecurityAttributes.nLength = sizeof(saFileSecurityAttributes);
	saFileSecurityAttributes.lpSecurityDescriptor = NULL;
	saFileSecurityAttributes.bInheritHandle = true;
	child_stderr = siStartupInfo.hStdError = CreateFileA("stderr_server.txt", GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, &saFileSecurityAttributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	ASSERT_LOG(siStartupInfo.hStdError != INVALID_HANDLE_VALUE, 
		"Unable to open stderr_server.txt for child process.");
	child_stdout = siStartupInfo.hStdOutput = CreateFileA("stdout_server.txt", GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, &saFileSecurityAttributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	ASSERT_LOG(siStartupInfo.hStdOutput != INVALID_HANDLE_VALUE, 
		"Unable to open stdout_server.txt for child process.");
	siStartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	siStartupInfo.dwFlags = STARTF_USESTDHANDLES;
	std::cerr << "CREATE CHILD PROCESS: " << app_name_and_path << std::endl;
	ASSERT_LOG(CreateProcessA(app_name_and_path.c_str(), child_args.get(), NULL, NULL, true, CREATE_DEFAULT_ERROR_MODE, 0, 0, &siStartupInfo, &piProcessInfo),
		"Unable to create child process for utility: " << GetLastError());
	child_process = piProcessInfo.hProcess;
	child_thread = piProcessInfo.hThread;
#else
	// everyone else version using fork()
	//...
	child_pid = fork();
	if(child_pid == 0) {
		FILE* fout = std::freopen("stdout_server.txt","w", stdout);
		FILE* ferr = std::freopen("stderr_server.txt","w", stderr);
		std::cerr.sync_with_stdio(true);
	}
	ASSERT_LOG(child_pid >= 0, "Unable to fork process: " << errno);
#endif
	// Create a semaphore to signal termination.
	ASSERT_LOG(ipc::semaphore::create(shared_sem_name, 0), 
		"Unable to create shared semaphore");
#if defined(_MSC_VER)
	return false;
#else
	return child_pid == 0;
#endif
}

void terminate_utility_process()
{
	ipc::semaphore::post();
#if defined(_MSC_VER)
	WaitForSingleObject(child_process, INFINITE);
	CloseHandle(child_process);
	CloseHandle(child_thread);
	CloseHandle(child_stderr);
	CloseHandle(child_stdout);
#else
	// .. close child or whatever.
	int status;
	if(waitpid(child_pid, &status, 0) != child_pid) {
		std::cerr << "Error waiting for child process to finish: " << errno << std::endl;
	}
#endif
}
#endif


#if defined(__ANDROID__)
#include <jni.h>
#include <android/asset_manager_jni.h>
AAssetManager* static_assetManager = 0;
extern "C" void app_set_asset_manager(AAssetManager* assetMan)
{
	static_assetManager = assetMan;	
}
namespace sys {
AAssetManager* GetJavaAssetManager()
{
	return static_assetManager;
}
}
#endif

#if defined(__native_client__)
void register_file_and_data(const char* filename, const char* mode, char* buffer, int size)
{
}
#endif

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
		int insertion_point = argv->size();
		for(int i = 0; i != argv->size(); ++i) {
			const char* utility_arg = "--module=";
			if(std::equal(utility_arg, utility_arg+strlen(utility_arg), (*argv)[i].c_str())) {
				insertion_point = i+1;
				break;
			}
		}

		argv->insert(argv->begin() + insertion_point, arguments.begin(), arguments.end());

		std::cerr << "ARGS: ";
		for(int i = 0; i != argv->size(); ++i) {
			std::cerr << (*argv)[i] << " ";
		}

		std::cerr << "\n";

	}	
	return 0;
}

#if defined(__native_client__)
extern "C" int game_main(int argcount, char* argvec[])
#else
extern "C" int main(int argcount, char* argvec[])
#endif
{
	{
		std::vector<std::string> args;
		for(int i = 0; i != argcount; ++i) {
			args.push_back(argvec[i]);
		}

		preferences::set_argv(args);
	}

#if defined(__native_client__)
	std::cerr << "Running game_main" << std::endl;

	chdir("/frogatto");
	{
		char buf[256];
		const char* const res = getcwd(buf,sizeof(buf));
		std::cerr << "Current working directory: " << res << std::endl;
	}
#endif 

#ifdef _MSC_VER
	freopen("CON", "w", stderr);
	freopen("CON", "w", stdout);
#endif

#if defined(__APPLE__) && TARGET_OS_MAC
    chdir([[[NSBundle mainBundle] resourcePath] fileSystemRepresentation]);
#endif

	#ifdef NO_STDERR
	std::freopen("/dev/null", "w", stderr);
	std::cerr.sync_with_stdio(true);
	#endif

	std::cerr << "Frogatto engine version " << preferences::version() << "\n";
	LOG( "After print engine version" );

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
	std::string server = "wesnoth.org";
#if defined(UTILITY_IN_PROC)
	bool create_utility_in_new_process = false;
	std::string utility_name;
#endif
	bool is_child_utility = false;

	const char* profile_output = NULL;
	std::string profile_output_buf;

#if defined(__ANDROID__)
	//monstartup("libapplication.so");
#endif

	std::string orig_level_cfg = level_cfg;
	std::string override_level_cfg = "";

	int modules_loaded = 0;

	std::vector<std::string> argv;
	for(int n = 1; n < argcount; ++n) {
#if defined(UTILITY_IN_PROC)
		std::string sarg(argvec[n]);
		if(sarg.compare(0, 15, "--utility-proc=") == 0) {
			create_utility_in_new_process = true;
			utility_name = "--utility-child=" + sarg.substr(15);
		} else {
			argv.push_back(argvec[n]);
		}
#else
		argv.push_back(argvec[n]);
#endif
        
        if(argv.size() >= 2 && argv[argv.size()-2] == "-NSDocumentRevisionsDebugMode" && argv.back() == "YES") {
            //XCode passes these arguments by default when debugging -- make sure they are ignored.
            argv.resize(argv.size()-2);
        }
	}

	std::cerr << "Build Options:";
	for(auto bo : preferences::get_build_options()) {
		std::cerr << " " << bo;
	}
	std::cerr << std::endl;

#if defined(UTILITY_IN_PROC)
	if(create_utility_in_new_process) {
		argv.push_back(utility_name);
#if defined(_MSC_VER)
		// app name is ignored for windows, we get windows to tell us.
		is_child_utility = create_utility_process("", argv);
#else 
		is_child_utility = create_utility_process(argvec[0], argv);
#endif
		if(!is_child_utility) {
			argv.pop_back();
		}
#if defined(_MSC_VER)
		atexit(terminate_utility_process);
#endif
	}
#endif

	if(sys::file_exists("./master-config.cfg")) {
		std::cerr << "LOADING CONFIGURATION FROM master-config.cfg" << std::endl;
		variant cfg = json::parse_from_file("./master-config.cfg");
		if(cfg.is_map()) {
			if( cfg["id"].is_null() == false) {
				std::cerr << "SETTING MODULE PATH FROM master-config.cfg: " << cfg["id"].as_string() << std::endl;
				preferences::set_preferences_path_from_module(cfg["id"].as_string());
				//XXX module::set_module_name(cfg["id"].as_string(), cfg["id"].as_string());
			}
			if(cfg["arguments"].is_null() == false) {
				std::vector<std::string> additional_args = cfg["arguments"].as_list_string();
				argv.insert(argv.begin(), additional_args.begin(), additional_args.end());
				std::cerr << "ADDING ARGUMENTS FROM master-config.cfg:";
				for(size_t n = 0; n < cfg["arguments"].num_elements(); ++n) {
					std::cerr << " " << cfg["arguments"][n].as_string();
				}
				std::cerr << std::endl;
			}
		}
	}

	stats::record_program_args(argv);

	for(size_t n = 0; n < argv.size(); ++n) {
		const int argc = argv.size();
		const std::string arg(argv[n]);
		std::string arg_name, arg_value;
		std::string::const_iterator equal = std::find(arg.begin(), arg.end(), '=');
		if(equal != arg.end()) {
			arg_name = std::string(arg.begin(), equal);
			arg_value = std::string(equal+1, arg.end());
		}
		if(arg_name == "--module") {
			if(load_module(arg_value, &argv) != 0) {
				std::cerr << "FAILED TO LOAD MODULE: " << arg_value << "\n";
				return -1;
			}
			++modules_loaded;
		} else if(arg == "--tests") {
			unit_tests_only = true;
		}
	}

	if(modules_loaded == 0 && !unit_tests_only) {
		if(load_module(DEFAULT_MODULE, &argv) != 0) {
			std::cerr << "FAILED TO LOAD MODULE: " << DEFAULT_MODULE << "\n";
			return -1;
		}
	} else if(unit_tests_only) {
		module::set_core_module_name(DEFAULT_MODULE);
	}

	preferences::load_preferences();
	LOG( "After load_preferences()" );

	// load difficulty settings after module, before rest of args.
	difficulty::manager();

	for(size_t n = 0; n < argv.size(); ++n) {
		const size_t argc = argv.size();
		const std::string arg(argv[n]);
		std::string arg_name, arg_value;
		std::string::const_iterator equal = std::find(arg.begin(), arg.end(), '=');
		if(equal != arg.end()) {
			arg_name = std::string(arg.begin(), equal);
			arg_value = std::string(equal+1, arg.end());
		}
		std::cerr << "ARGS: " << arg << std::endl;
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
		} else if(arg == "--tests") {
			// ignore as already processed.
		} else if(arg == "--no-tests") {
			skip_tests = true;
		} else if(arg_name == "--width") {
			std::string w(arg_value);
			preferences::set_actual_screen_width(boost::lexical_cast<int>(w));
		} else if(arg == "--width" && n+1 < argc) {
			std::string w(argv[++n]);
			preferences::set_actual_screen_width(boost::lexical_cast<int>(w));
		} else if(arg_name == "--height") {
			std::string h(arg_value);
			preferences::set_actual_screen_height(boost::lexical_cast<int>(h));
		} else if(arg == "--height" && n+1 < argc) {
			std::string h(argv[++n]);
			preferences::set_actual_screen_height(boost::lexical_cast<int>(h));
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
#if defined(TARGET_PANDORA)
		} else if(arg == "--no-fbo") {
			preferences::set_fbo(false);
		} else if(arg == "--no-bequ") {
			preferences::set_bequ(false);
#endif
		} else if(arg == "--help" || arg == "-h") {
			print_help(std::string(argvec[0]));
			return 0;
		} else {
			const bool res = preferences::parse_arg(argv[n].c_str());
			if(!res) {
				std::cerr << "unrecognized arg: '" << arg << "'\n";
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
						args.push_back(NULL);
				
						exe_name.resize(exe_name.size() - anura_exe_name.size());
						exe_name += match;
						args[0] = const_cast<char*>(exe_name.c_str());
						fprintf(stderr, "ZZZ: CALLING EXEC...\n");
						execv(args[0], &args[0]);
						fprintf(stderr, "Could not exec()\n");
					}
				}
			}
		}
	}

	background_task_pool::manager bg_task_pool_manager;
	LOG( "After expand_data_paths()" );

	std::cerr << "Preferences dir: " << preferences::user_data_path() << '\n';

	//make sure that the user data path exists.
	if(!preferences::setup_preferences_dir()) {
		std::cerr << "cannot create preferences dir!\n";
	}

	std::cerr << "\n";

	variant_builder update_info;
	if(g_auto_update_module || g_auto_update_anura != "") {

		//remove any .tmp files that may have been left from previous runs.
		std::vector<std::string> tmp_files;
		sys::get_files_in_dir(".", &tmp_files, NULL);
		for(auto f : tmp_files) {
			if(f.size() > 4 && std::equal(f.end()-4,f.end(),".tmp")) {
				try {
					sys::remove_file(f);
				} catch(...) {
				}
			}
		}

		boost::intrusive_ptr<module::client> cl, anura_cl;
		
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
		int start_time = SDL_GetTicks();
		int original_start_time = SDL_GetTicks();
		bool timeout = false;
		bool require_restart = false;
		fprintf(stderr, "Requesting update to module from server...\n");
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
						fprintf(stderr, "Transferred %d/%dKB\n", transferred/1024, cl->nbytes_total()/1024);
					}
					start_time = SDL_GetTicks();
					nbytes_transferred = transferred;
				}
			}

			if(anura_cl) {
				const int transferred = anura_cl->nbytes_transferred();
				nbytes_obtained += transferred;
				nbytes_needed += anura_cl->nbytes_total();
				if(transferred != nbytes_anura_transferred) {
					if(nupdate_cycle%10 == 0) {
						fprintf(stderr, "Transferred (anura) %d/%dKB\n", transferred/1024, anura_cl->nbytes_total()/1024);
					}
					start_time = SDL_GetTicks();
					nbytes_anura_transferred = transferred;
				}
			}

			const int time_taken = SDL_GetTicks() - start_time;
			if(time_taken > g_auto_update_timeout) {
				fprintf(stderr, "Timed out updating module. Canceling. %dms vs %dms\n", time_taken, g_auto_update_timeout);
				break;
			}

			char msg[1024];
			sprintf(msg, "Updating Game. Transferred %.02f/%.02fMB", float(nbytes_obtained/(1024.0*1024.0)), float(nbytes_needed/(1024.0*1024.0)));

			update_window.set_message(msg);

			const float ratio = nbytes_needed <= 0 ? 0.0 : float(nbytes_obtained)/float(nbytes_needed);
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

			const int target_end = SDL_GetTicks() + 50;
			while(SDL_GetTicks() < target_end && (cl || anura_cl)) {
				if(cl && !cl->process()) {
					if(cl->error().empty() == false) {
						fprintf(stderr, "Error while updating module: %s\n", cl->error().c_str());
						update_info.add("module_error", variant(cl->error()));
					} else {
						update_info.add("complete_module", true);
					}
					cl.reset();
				}

				if(anura_cl && !anura_cl->process()) {
					if(anura_cl->error().empty() == false) {
						fprintf(stderr, "Error while updating anura: %s\n", anura_cl->error().c_str());
						update_info.add("anura_error", variant(anura_cl->error()));
					} else {
						update_info.add("complete_anura", true);
						require_restart = anura_cl->nfiles_written() != 0;
					}
					anura_cl.reset();
				}
			}
		}

		} //dispose update_window

		if(require_restart) {
			std::vector<char*> args;
			for(char** a = argvec; *a; ++a) {
				std::string arg(*a);
				if(arg != "--force-auto-update" && arg != "--force_auto_update") {
					args.push_back(*a);
				}
			}
			args.push_back(NULL);
			fprintf(stderr, "ZZZ: CALLING EXEC...\n");
			execv(args[0], &args[0]);
			fprintf(stderr, "Could not exec()\n");
		}
	}

	g_auto_update_info = update_info.build();

	checksum::manager checksum_manager;
#ifndef NO_EDITOR
	sys::filesystem_manager fs_manager;
#endif // NO_EDITOR

	const tbs::internal_server_manager internal_server_manager_scope(preferences::internal_tbs_server());

	if(utility_program.empty() == false 
		&& test::utility_needs_video(utility_program) == false) {
#if defined(UTILITY_IN_PROC)
		if(is_child_utility) {
			ASSERT_LOG(ipc::semaphore::create(shared_sem_name, 1) != false, 
				"Unable to create shared semaphore: " << errno);
			std::cerr.sync_with_stdio(true);
		}
#endif
		test::run_utility(utility_program, util_args);
		return 0;
	}

#if defined(TARGET_PANDORA)
    EGL_Open();
#endif

#if defined(__ANDROID__)
	std::freopen("stdout.txt","w",stdout);
	std::freopen("stderr.txt","w",stderr);
	std::cerr.sync_with_stdio(true);
#endif

	LOG( "Start of main" );

	if(!skip_tests && !test::run_tests()) {
		return -1;
	}

	if(unit_tests_only) {
		return 0;
	}

	// Create the main window.
	// Initalise SDL and Open GL.
	main_window = graphics::window_manager_ptr(new graphics::window_manager());
	main_window->create_window(preferences::actual_screen_width(), preferences::actual_screen_height());

#ifdef TARGET_OS_HARMATTAN
	g_type_init();
#endif
	i18n::init ();
	LOG( "After i18n::init()" );

#if TARGET_OS_IPHONE || defined(TARGET_BLACKBERRY) || defined(__ANDROID__)
	//on the iPhone and PlayBook, try to restore the auto-save if it exists
	if(sys::file_exists(preferences::auto_save_file_path()) && sys::read_file(std::string(preferences::auto_save_file_path()) + ".stat") == "1") {
		level_cfg = "autosave.cfg";
		sys::write_file(std::string(preferences::auto_save_file_path()) + ".stat", "0");
	}
#endif

	if(override_level_cfg.empty() != true) {
		level_cfg = override_level_cfg;
		orig_level_cfg = level_cfg;
	}

	const stats::manager stats_manager;
#ifndef NO_EDITOR
	const external_text_editor::manager editor_manager;
#endif // NO_EDITOR

#if defined(USE_BOX2D)
	box2d::manager b2d_manager;
#endif

	const load_level_manager load_manager;

	{ //manager scope
	const font::manager font_manager;
	const sound::manager sound_manager;
#if !defined(__native_client__)
	const joystick::manager joystick_manager;
#endif 
	
	graphics::texture::manager texture_manager;

#ifndef NO_EDITOR
	editor::manager editor_manager;
#endif

	variant preloads;
	loading_screen loader;
	try {
		variant gui_node = json::parse_from_file(preferences::load_compiled() ? "data/compiled/gui.cfg" : "data/gui.cfg");
		gui_section::init(gui_node);
		loader.draw_and_increment(_("Initializing GUI"));
		framed_gui_element::init(gui_node);

		sound::init_music(json::parse_from_file("data/music.cfg"));
		graphical_font::init_for_locale(i18n::get_locale());
		preloads = json::parse_from_file("data/preload.cfg");
		int preload_items = preloads["preload"].num_elements();
		loader.set_number_of_items(preload_items+7); // 7 is the number of items that will be loaded below
		custom_object::init();
		loader.draw_and_increment(_("Initializing custom object functions"));
		loader.draw_and_increment(_("Initializing textures"));
		loader.load(preloads);
		loader.draw_and_increment(_("Initializing tiles"));
		tile_map::init(json::parse_from_file("data/tiles.cfg"));


		game_logic::formula_object::load_all_classes();

	} catch(const json::parse_error& e) {
		std::cerr << "ERROR PARSING: " << e.error_message() << "\n";
		return 0;
	}
	loader.draw(_("Loading level"));

#if defined(__native_client__)
	while(1) {
	}
#endif

#if defined(__APPLE__) && !(TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE) && !defined(USE_SHADERS)
	GLint swapInterval = 1;
	CGLSetParameter(CGLGetCurrentContext(), kCGLCPSwapInterval, &swapInterval);
#endif

	loader.finish_loading();
	//look to see if we got any quit events while loading.
	{
	SDL_Event event;
	while(input::sdl_poll_event(&event)) {
		if(event.type == SDL_QUIT) {
			return 0;
		}
	}
	}

	formula_profiler::manager profiler(profile_output);

#ifdef USE_SHADERS
	texture_frame_buffer::init(preferences::actual_screen_width(), preferences::actual_screen_height());
#else
	texture_frame_buffer::init();
#endif

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

	while(!quit && !show_title_screen(level_cfg)) {
		boost::intrusive_ptr<level> lvl(load_level(level_cfg));
		

#if !defined(__native_client__)
		//see if we're loading a multiplayer level, in which case we
		//connect to the server.
		multiplayer::manager mp_manager(lvl->is_multiplayer());
		if(lvl->is_multiplayer()) {
			multiplayer::setup_networked_game(server);
		}

		if(lvl->is_multiplayer()) {
			last_draw_position() = screen_position();
			std::string level_cfg = "waiting-room.cfg";
			boost::intrusive_ptr<level> wait_lvl(load_level(level_cfg));
			wait_lvl->finish_loading();
			wait_lvl->set_multiplayer_slot(0);
			if(wait_lvl->player()) {
				wait_lvl->player()->set_current_level(level_cfg);
			}
			wait_lvl->set_as_current_level();

			level_runner runner(wait_lvl, level_cfg, orig_level_cfg);

			multiplayer::sync_start_time(*lvl, boost::bind(&level_runner::play_cycle, &runner));

			lvl->set_multiplayer_slot(multiplayer::slot());
		}
#endif

		last_draw_position() = screen_position();

		assert(lvl.get());
		if(!lvl->music().empty()) {
			sound::play_music(lvl->music());
		}

		if(lvl->player() && level_cfg != "autosave.cfg") {
			lvl->player()->set_current_level(level_cfg);
			lvl->player()->get_entity().save_game();
		}

		set_scene_title(lvl->title());

		try {
			quit = level_runner(lvl, level_cfg, orig_level_cfg).play_level();
			level_cfg = orig_level_cfg;
		} catch(multiplayer_exception&) {
		}
	}

	level::clear_current_level();

	} //end manager scope, make managers destruct before calling SDL_Quit
//	controls::debug_dump_controls();
#if defined(TARGET_PANDORA) || defined(TARGET_TEGRA)
    EGL_Destroy();
#endif

	preferences::save_preferences();

#if !defined(_MSC_VER) && defined(UTILITY_IN_PROC)
	if(create_utility_in_new_process) {
		terminate_utility_process();
	}
#endif

	std::set<variant*> loading;
	swap_variants_loading(loading);
	if(loading.empty() == false) {
		fprintf(stderr, "Illegal object: %p\n", (void*)(*loading.begin())->as_callable_loading());
		ASSERT_LOG(false, "Unresolved unserialized objects: " << loading.size());
	}

//#ifdef _MSC_VER
//	ExitProcess(0);
//#endif

	return 0;
}
