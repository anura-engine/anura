/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <boost/algorithm/string.hpp>

#if defined(__linux__)
//Avoid link error on Linux when compiling with -std=c++0x and linking with
//a Boost lib not compiled with these flags.
#define BOOST_NO_SCOPED_ENUMS
#define BOOST_NO_CXX11_SCOPED_ENUMS
#endif

#include <boost/filesystem.hpp>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "preferences.hpp"
#include "thread.hpp"
#include "unit_test.hpp"

#ifdef __linux__
#include <sys/inotify.h>
#include <sys/select.h>
#endif

namespace sys
{
	using namespace boost::filesystem;

	namespace 
	{
#ifdef HAVE_CONFIG_H
		const std::string data_dir = DATADIR ;
		const bool have_datadir = true;
#else
		const std::string data_dir = "";
		const bool have_datadir = false;
#endif
	}

	bool is_directory(const std::string& dname)
	{
		return is_directory(path(dname));
	}


	void get_files_in_dir(const std::string& dir,
		std::vector<std::string>* files,
		std::vector<std::string>* dirs)
	{
		path p(dir);
		if(!is_directory(p) && !is_other(p)) {
			return;
		}
		for(directory_iterator it = directory_iterator(p); it != directory_iterator(); ++it) {
			if(is_directory(it->path()) || is_other(it->path())) {
				if(dirs != NULL) {
					dirs->push_back(it->path().filename().generic_string());
				}
			} else {
				if(files != NULL) {
					files->push_back(it->path().filename().generic_string());
				}
			}
		}
		if(files != NULL)
			std::sort(files->begin(), files->end());

		if (dirs != NULL)
			std::sort(dirs->begin(), dirs->end());
	}


	void get_unique_filenames_under_dir(const std::string& dir,
                                    std::map<std::string, std::string>* file_map,
									const std::string& prefix)
	{
		ASSERT_LOG(file_map != NULL, "get_unique_filenames_under_dir() passed a NULL file_map");
		path p(dir);
		if(!is_directory(p)) {
			return;
		}
		for(recursive_directory_iterator it = recursive_directory_iterator(p); it != recursive_directory_iterator(); ++it) {
			if(!is_directory(it->path())) {
				(*file_map)[prefix + it->path().filename().generic_string()] = it->path().generic_string();
			}
		}
	}

	std::string get_dir(const std::string& dir)
	{
		try {
			create_directory(path(dir));
		} catch(filesystem_error&) {
			return "";
		}
		return dir;
	}

	std::string read_file(const std::string& fname)
	{
		std::ifstream file(fname.c_str(), std::ios_base::binary);
		std::stringstream ss;
		ss << file.rdbuf();
		return ss.str();
	}

	void write_file(const std::string& fname, const std::string& data)
	{
		path p(fname);
		ASSERT_LOG(p.has_filename(), "No filename found in write_file path: " << fname);

		// Create any needed directories
		boost::system::error_code ec;
		create_directories(p.parent_path(), ec);

		// Write the file.
		std::ofstream file(fname.c_str(), std::ios_base::binary);
		file << data;
	}

	bool dir_exists(const std::string& fname)
	{
		path p(fname);
		return exists(p) && is_directory(p);
	}

	bool file_exists(const std::string& fname)
	{
		path p(fname);
		return exists(p) && is_regular_file(p);
	}

	std::string find_file(const std::string& fname)
	{
		if(file_exists(fname)) {
			return fname;
		}
		if(have_datadir) {
			std::string data_fname = data_dir + "/" + fname;
			if(file_exists(data_fname)) {
				return data_fname;
			}
		}
		return fname;
	}

	int64_t file_mod_time(const std::string& fname)
	{
		path p(fname);
		if(is_regular_file(p)) {
			return static_cast<int64_t>(last_write_time(p));
		} else {
			return 0;
		}
	}

	void move_file(const std::string& from, const std::string& to)
	{
		rename(path(from), path(to));
	}

	void remove_file(const std::string& fname)
	{
		remove(path(fname));
	}

	void copy_file(const std::string& from, const std::string& to)
	{
		copy_file(path(from), path(to), copy_option::fail_if_exists);
	}

	void rmdir_recursive(const std::string& fpath)
	{
		remove_all(path(fpath));
	}

	bool is_path_absolute(const std::string& fpath)
	{
		return path(fpath).is_absolute();
	}

	std::string make_conformal_path(const std::string& fpath)
	{
		return path(fpath).generic_string();
	}

	namespace
	{
		std::string del_substring_front(const std::string& target, const std::string& common)
		{
			if(boost::iequals(target.substr(0, common.length()), common)) {
			//if(target.find(common) == 0) {
				return target.substr(common.length());
			}
			return target;
		}

		std::string normalise_path(const std::string& path)
		{
			if(is_path_absolute(path)) { 
				return path;
			}
			std::vector<std::string> cur_path;
			std::string norm_path;
			boost::split(cur_path, path, std::bind2nd(std::equal_to<char>(), '/'));
			foreach(const std::string& s, cur_path) {
				if(s != ".") {
					norm_path += s + "/";
				}
			}
			return norm_path;
		}
	}

	// Calculates the path of target relative to source.
	std::string compute_relative_path(const std::string& source, const std::string& target)
	{
		//std::cerr << "compute_relative_path(a): " << source << " : " << target << std::endl;
		std::string common_part = normalise_path(source);
		std::string back;
		if(common_part.length() > 1 && common_part[common_part.length()-1] == '/') {
			common_part.erase(common_part.length()-1);
		}
		while(boost::iequals(del_substring_front(target, common_part), target)) {
			size_t offs = common_part.rfind('/');
			//std::cerr << "compute_relative_path(b2): " << back << " : " << common_part << std::endl;
			if(common_part.length() > 1 && offs != std::string::npos) {
				common_part.erase(offs);
				back = "../" + back;
			} else {
				break;
			}
		}
		common_part = del_substring_front(target, common_part);
		if(common_part.length() == 1) {
			common_part = common_part.substr(1);
			if(back.empty() == false) {
				back.erase(back.length()-1);
			}
		} else if(common_part.length() > 1 && common_part[0] == '/') {
			common_part = common_part.substr(1);
		} else {
			if(back.empty() == false) {
				back.erase(back.length()-1);
			}
		}
		//std::cerr << "compute_relative_path(b): " << back << " : " << common_part << std::endl;
		return back + common_part;
	}






	namespace 
	{
		typedef std::map<std::string, std::vector<boost::function<void()> > > file_mod_handler_map;
		file_mod_handler_map& get_mod_map() 
		{
			static file_mod_handler_map instance;
			return instance;
		}

	std::vector<std::string> new_files_listening;

	threading::mutex& get_mod_map_mutex() 
	{
		static threading::mutex instance;
		return instance;
	}

	std::vector<boost::function<void()> > file_mod_notification_queue;

	threading::mutex& get_mod_queue_mutex() {
		static threading::mutex instance;
		return instance;
	}

	void file_mod_worker_thread_fn()
	{
#ifdef __linux__
		const int inotify_fd = inotify_init();
		std::map<int, std::string> fd_to_path;
		fd_set read_set;
#endif

		std::map<std::string, int64_t> mod_times;
		for(;;) {
			file_mod_handler_map m;
			std::vector<std::string> new_files;

			{
				threading::lock lck(get_mod_map_mutex());
				m = get_mod_map();
				new_files = new_files_listening;
				new_files_listening.clear();
			}

			if(m.empty()) {
				break;
			}

#ifdef __linux__
			for(int n = 0; n != new_files.size(); ++n) {
				const int fd = inotify_add_watch(inotify_fd, new_files[n].c_str(), IN_CLOSE_WRITE);
				if(fd > 0) {
					fd_to_path[fd] = new_files[n];
				} else {
					std::cerr << "COULD NOT LISTEN ON FILE " << new_files[n] << "\n";
				}
			}

			FD_ZERO(&read_set);
			FD_SET(inotify_fd, &read_set);
			timeval tv = {1, 0};
			const int select_res = select(inotify_fd+1, &read_set, NULL, NULL, &tv);
			if(select_res > 0) {
				inotify_event ev;
				const int nbytes = read(inotify_fd, &ev, sizeof(ev));
				if(nbytes == sizeof(ev)) {

					const std::string path = fd_to_path[ev.wd];
					std::cerr << "LINUX FILE MOD: " << path << "\n";
					if(ev.mask&IN_IGNORED) {
						fd_to_path.erase(ev.wd);
						const int fd = inotify_add_watch(inotify_fd, path.c_str(), IN_MODIFY);
						if(fd > 0) {
							fd_to_path[fd] = path;
						}
					}
					std::vector<boost::function<void()> >& handlers = m[path];
					std::cerr << "FILE HANDLERS: " << handlers.size() << "\n";

					threading::lock lck(get_mod_queue_mutex());
					file_mod_notification_queue.insert(file_mod_notification_queue.end(), handlers.begin(), handlers.end());
				} else {
					std::cerr << "READ FAILURE IN FILE NOTIFY\n";
				}
			}

#else

			const int begin = SDL_GetTicks();

			for(file_mod_handler_map::iterator i = m.begin(); i != m.end(); ++i) {
				std::map<std::string, int64_t>::iterator mod_itor = mod_times.find(i->first);
				const int64_t mod_time = file_mod_time(i->first);
				if(mod_itor == mod_times.end()) {
					mod_times[i->first] = mod_time;
				} else if(mod_time != mod_itor->second) {
					std::cerr << "MODIFY: " << mod_itor->first << "\n";
					mod_itor->second = mod_time;

					threading::lock lck(get_mod_queue_mutex());
					file_mod_notification_queue.insert(file_mod_notification_queue.end(), i->second.begin(), i->second.end());
				}
			}

			//std::cerr << "CHECKED " << m.size() << " FILES IN " << (SDL_GetTicks() - begin) << "\n";

			SDL_Delay(100);
#endif
		}
	}

	threading::thread* file_mod_worker_thread = NULL;

	}

	filesystem_manager::filesystem_manager()
	{
	}

	filesystem_manager::~filesystem_manager()
	{
		{
			threading::lock lck(get_mod_map_mutex());
			get_mod_map().clear();
		}

		delete file_mod_worker_thread;
		file_mod_worker_thread = NULL;
	}

	std::string get_user_data_dir()
	{
		static bool inited_dirs = false;
		const std::string dir_path = preferences::user_data_path();

		if(!inited_dirs) {
			create_directory("userdata");
			create_directory("userdata/saves");
			create_directory("dlc");
			inited_dirs = true;
		}
		path p = current_path() / "userdata";
		return p.generic_string();
	}

	std::string get_saves_dir()
	{
		const std::string dir_path = get_user_data_dir() + "/saves";
		return get_dir(dir_path);
	}

	void notify_on_file_modification(const std::string& path, boost::function<void()> handler)
	{
		{
			threading::lock lck(get_mod_map_mutex());
			std::vector<boost::function<void()> >& handlers = get_mod_map()[path];
			if(handlers.empty()) {
				new_files_listening.push_back(path);
			}
			handlers.push_back(handler);
		}

		if(file_mod_worker_thread == NULL) {
			file_mod_worker_thread = new threading::thread("file_change_notify", file_mod_worker_thread_fn);
		}
	}

	void pump_file_modifications()
	{
		if(file_mod_worker_thread == NULL) {
			return;
		}

		std::vector<boost::function<void()> > v;
		{
			threading::lock lck(get_mod_queue_mutex());
			v.swap(file_mod_notification_queue);
		}

		foreach(boost::function<void()> f, v) {
			std::cerr << "CALLING FILE MOD HANDLER\n";
			f();
		}
	}
}
