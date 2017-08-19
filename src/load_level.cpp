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

//use non-threaded loading unless/until we can fix threaded loading.

#if 0
#include <assert.h>

#include "asserts.hpp"
#include "concurrent_cache.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "load_level.hpp"
#include "module.hpp"
#include "package.hpp"
#include "preferences.hpp"
#include "preprocessor.hpp"
#include "string_utils.hpp"
#include "texture.hpp"
#include "thread.hpp"
#include "variant.hpp"

namespace {
typedef concurrent_cache<std::string, variant> level_wml_map;
level_wml_map& wml_cache() {
	static level_wml_map instance;
	return instance;
}

std::map<std::string, threading::thread*>& wml_threads()
{
	static std::map<std::string, threading::thread*> instance;
	return instance;
}

class wml_loader {
	std::string lvl_;
public:
	wml_loader(const std::string& lvl) : lvl_(lvl)
	{}
	void operator()() {
		static const std::string global_path = preferences::load_compiled() ? "data/compiled/level/" : preferences::level_path();

		std::string filename;

		std::vector<std::string> components = util::split(lvl_, '/');
		if(components.size() == 1) {
			filename = global_path + lvl_;
		} else if(components.size() == 2) {
			filename = std::string(preferences::user_data_path()) + "/packages/" + components.front() + "/" + components.back();
		} else {
			ASSERT_LOG(false, "UNRECOGNIZED LEVEL PATH FORMAT: " << lvl_);
		}

		try {
			variant node(json::parse_from_file(filename));
			wml_cache().put(lvl_, node);
		} catch(json::parse_error& e) {
			ASSERT_LOG(false, "ERROR PARSING LEVEL WML FOR '" << filename << "': " << e.error_message());
		}catch(...) {
			std::cerr << "FAILED TO LOAD " << filename << "\n";
			ASSERT_LOG(false, "FAILED TO LOAD");
		}
	}
};
}

void clear_level_wml()
{
	wml_cache().clear();
}

namespace {
bool is_save_file(const std::string& fname)
{
	static const std::string SaveFiles[] = {"save.cfg", "save2.cfg", "save3.cfg", "save4.cfg"};
	return std::count(SaveFiles, SaveFiles + sizeof(SaveFiles)/sizeof(SaveFiles[0]), fname) != 0;
}
}

void preload_level_wml(const std::string& lvl)
{
	if(is_save_file(lvl) || lvl == "autosave.cfg") {
		return;
	}

	if(wml_cache().count(lvl)) {
		return;
	}

	wml_cache().put(lvl, variant());
#if defined(__ANDROID__) && SDL_VERSION_ATLEAST(1, 3, 0)
	wml_threads()[lvl] = new threading::thread("load-"+lvl, wml_loader(lvl));
#else
	wml_threads()[lvl] = new threading::thread(wml_loader(lvl));
#endif
}

variant load_level_wml(const std::string& lvl)
{
	if(lvl == "tmp_state.cfg") {
		//special state for debugging.
		return json::parse_from_file("./tmp_state.cfg");
	}

	if(is_save_file(lvl) || lvl == "autosave.cfg") {
		std::string filename;
		if(is_save_file(lvl)) {
			filename = std::string(preferences::user_data_path()) + "/" + lvl;
		} else {
			filename = preferences::auto_save_file_path();
		}

		return json::parse_from_file(filename);
	}

	if(wml_cache().count(lvl)) {
		std::map<std::string, threading::thread*>::iterator t = wml_threads().find(lvl);
		if(t != wml_threads().end()) {
			delete t->second;
			wml_threads().erase(t);
		}

		return wml_cache().get(lvl);
	}

	wml_loader loader(lvl);
	loader();
	return load_level_wml_nowait(lvl);
}

variant load_level_wml_nowait(const std::string& lvl)
{
	return wml_cache().get(lvl);
}

namespace {
typedef std::map<std::string, std::pair<threading::thread*, level*> > level_map;
level_map levels_loading;

threading::mutex& levels_loading_mutex() {
	static threading::mutex m;
	return m; 
}

class level_loader {
	std::string lvl_;
public:
	level_loader(const std::string& lvl) : lvl_(lvl)
	{}
	void operator()() {
		level* lvl = NULL;
		try {
			lvl = new level(lvl_);
		} catch(const graphics::texture::worker_thread_error&) {
			//we can't load the level in here, we must do it in the main thread.
			std::cerr << "LOAD LEVEL FAILURE: " << lvl << " MUST LOAD IN "
			             "MAIN THREAD\n";
		}
		threading::lock lck(levels_loading_mutex());
		levels_loading[lvl_].second = lvl;
	}
};

}

load_level_manager::load_level_manager()
{}

load_level_manager::~load_level_manager()
{
	for(level_map::iterator i = levels_loading.begin(); i != levels_loading.end(); ++i) {
		i->second.first->join();
		delete i->second.first;
		delete i->second.second;
	}

	levels_loading.clear();
}

void preload_level(const std::string& lvl)
{
	//--TODO: Currently multi-threaded pre-loading causes weird crashes.
	//--      need to fix this!!
	assert(!lvl.empty());
	threading::lock lck(levels_loading_mutex());
	if(levels_loading.count(lvl) == 0) {
#if defined(__ANDROID__) && SDL_VERSION_ATLEAST(1, 3, 0)
		levels_loading[lvl].first = new threading::thread("load"+lvl,level_loader(lvl));
#else
		levels_loading[lvl].first = new threading::thread(level_loader(lvl));
#endif
	}
}

ffl::IntrusivePtr<level> load_level(const std::string& lvl)
{
	std::cerr << "START LOAD LEVEL\n";
	level_map::iterator itor;
	{
		threading::lock lck(levels_loading_mutex());
		itor = levels_loading.find(lvl);
		if(itor == levels_loading.end()) {
			ffl::IntrusivePtr<level> res(new level(lvl));
			res->finish_loading();
			fprintf(stderr, "LOADED LEVEL: %p\n", res);
			return res;
		}
	}

	itor->second.first->join();
	delete itor->second.first;
	ffl::IntrusivePtr<level> res;
	res.reset(itor->second.second);
	if(res.get() == NULL) {
		res.reset(new level(lvl));
	}
	res->finish_loading();
	levels_loading.erase(itor);
	std::cerr << "FINISH LOAD LEVEL\n";
	return res;
}

namespace {
bool hidden_file(const std::string& filename) {
	return !filename.empty() && filename[0] == '.';
}
}

std::vector<std::string> get_known_levels()
{
	std::vector<std::string> files;
	module::get_files_in_dir(preferences::level_path(), &files);
	files.erase(std::remove_if(files.begin(), files.end(), hidden_file), files.end());

	foreach(const std::string& pkg, package::all_packages()) {
		std::vector<std::string> v = package::package_levels(pkg);
		files.insert(files.end(), v.begin(), v.end());
	}

	std::sort(files.begin(), files.end());
	
	return files;
}
#endif
