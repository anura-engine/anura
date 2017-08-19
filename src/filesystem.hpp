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

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace sys
{
	bool is_directory(const std::string& dname);

	//! Populates 'files' with all the files and
	//! 'dirs' with all the directories in dir.
	//! If files or dirs are nullptr they will not be used.
	//!
	//! Mode determines whether the entire path or just the filename is retrieved.
	void get_files_in_dir(const std::string& dir,
						  std::vector<std::string>* files,
						  std::vector<std::string>* dirs=nullptr);

	typedef std::map<std::string, std::string> file_path_map;
	//Function which given a directory, will recurse through all sub-directories,
	//and find each distinct filename. It will fill the files map such that the
	//keys are filenames and the values are the full path to the file.
	void get_unique_filenames_under_dir(const std::string& dir,
										file_path_map* file_map,
										const std::string& prefix);

	void get_all_filenames_under_dir(const std::string& dir,
										std::multimap<std::string, std::string>* file_map,
										const std::string& prefix);

	//creates a dir if it doesn't exist and returns the path
	std::string get_dir(const std::string& dir);
	std::string get_user_data_dir();
	std::string get_saves_dir();

	std::string read_file(const std::string& fname);
	void write_file(const std::string& fname, const std::string& data);

	bool dir_exists(const std::string& fname);
	bool file_exists(const std::string& fname);
	std::string find_file(const std::string& name);

	long long file_mod_time(const std::string& fname);

	void move_file(const std::string& from, const std::string& to);
	void remove_file(const std::string& fname);
	void copy_file(const std::string& from, const std::string& to);

	void rmdir_recursive(const std::string& path);

	bool is_path_absolute(const std::string& path);
	std::string make_conformal_path(const std::string& path);
	std::string compute_relative_path(const std::string& source, const std::string& target);

	struct FilesystemManager 
	{
		FilesystemManager();
		~FilesystemManager();
	};

	int notify_on_file_modification(const std::string& path, std::function<void()> handler);
	void remove_notify_on_file_modification(int handle);
	void pump_file_modifications();

	bool is_safe_write_path(const std::string& path, std::string* error=nullptr);

	bool is_file_writable(const std::string& path);
	void set_file_writable(const std::string& path);

	bool is_file_executable(const std::string& path);
	void set_file_executable(const std::string& path);

	std::string get_cwd();
}
