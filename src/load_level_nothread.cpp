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

#include "asserts.hpp"
#include "filesystem.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "load_level.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "preprocessor.hpp"
#include "string_utils.hpp"
#include "variant.hpp"

namespace 
{
	std::map<std::string,std::string>& get_level_paths() {
		static std::map<std::string,std::string> res;
		return res;
	}
}

void reload_level_paths() 
{
	get_level_paths().clear();
	load_level_paths();
}

void load_level_paths() 
{
	module::get_unique_filenames_under_dir(preferences::load_compiled() ? "data/compiled/level/" : "data/level/", &get_level_paths());
}

const std::string& get_level_path(const std::string& name) 
{
	if(get_level_paths().empty()) {
		load_level_paths();
	}
	std::map<std::string, std::string>::const_iterator itor = module::find(get_level_paths(), name);
	if(itor == get_level_paths().end()) {
		ASSERT_LOG(false, "FILE NOT FOUND: " << name);
	}
	return itor->second;
}

void clear_level_wml()
{
}

void preload_level_wml(const std::string& lvl)
{
}

variant load_level_wml(const std::string& lvl)
{
	return load_level_wml_nowait(lvl);
}

variant load_level_wml_nowait(const std::string& lvl)
{
	try {
		if(lvl == "autosave.cfg") {
			return json::parse_from_file(preferences::auto_save_file_path());
		} else if(lvl.size() >= 7 && lvl.substr(0,4) == "save" && lvl.substr(lvl.size()-4) == ".cfg") {
			preferences::set_save_slot(lvl);
			return json::parse_from_file(preferences::save_file_path());
		}
		return json::parse_from_file(get_level_path(lvl));
	} catch(json::ParseError& e) {
		ASSERT_LOG(false, e.errorMessage());
	}
}

load_level_manager::load_level_manager()
{
}

load_level_manager::~load_level_manager()
{
}

void preload_level(const std::string& lvl)
{
}

ffl::IntrusivePtr<Level> load_level(const std::string& lvl)
{
	ffl::IntrusivePtr<Level> res(new Level(lvl));
	res->finishLoading();
	return res;
}

namespace 
{
	bool not_cfg_file(const std::string& filename) 
	{
		return filename.size() < 4 || !std::equal(filename.end() - 4, filename.end(), ".cfg");
	}
}

std::vector<std::string> get_known_levels()
{
	std::vector<std::string> files;
	std::map<std::string, std::string> file_map;
	std::map<std::string, std::string>::iterator it;
	module::get_unique_filenames_under_dir("data/level/", &file_map);
	for(it = file_map.begin(); it != file_map.end(); ) {
		if(not_cfg_file(it->first)) {
			file_map.erase(it++);
		} else { 
			++it; 
		}
	}

	for(auto& file : file_map) {
		files.push_back(file.first);
	}
	std::sort(files.begin(), files.end());
	return files;
}
