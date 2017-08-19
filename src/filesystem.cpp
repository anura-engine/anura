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
#include <sstream>
#include <fstream>
#include <algorithm>
#include <boost/algorithm/string.hpp>

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/stat.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#if BOOST_VERSION <= 105600
//Avoid link error on Linux when compiling with -std=c++0x and linking with
//a Boost lib not compiled with these flags.
#define BOOST_NO_SCOPED_ENUMS
#define BOOST_NO_CXX11_SCOPED_ENUMS
#endif
#endif

#include <boost/filesystem.hpp>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
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
				if(dirs != nullptr) {
					dirs->push_back(it->path().filename().generic_string());
				}
			} else {
				if(files != nullptr) {
					files->push_back(it->path().filename().generic_string());
				}
			}
		}
		if(files != nullptr)
			std::sort(files->begin(), files->end());

		if (dirs != nullptr)
			std::sort(dirs->begin(), dirs->end());
	}


	void get_unique_filenames_under_dir(const std::string& dir,
                                    std::map<std::string, std::string>* file_map,
									const std::string& prefix)
	{
		ASSERT_LOG(file_map != nullptr, "get_unique_filenames_under_dir() passed a nullptr file_map");
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

	void get_all_filenames_under_dir(const std::string& dir,
                                    std::multimap<std::string, std::string>* file_map,
									const std::string& prefix)
	{
		ASSERT_LOG(file_map != nullptr, "get_unique_filenames_under_dir() passed a nullptr file_map");
		path p(dir);
		if(!is_directory(p)) {
			return;
		}
		for(recursive_directory_iterator it = recursive_directory_iterator(p); it != recursive_directory_iterator(); ++it) {
			if(!is_directory(it->path())) {
				file_map->insert(std::pair<std::string,std::string>(prefix + it->path().filename().generic_string(), it->path().generic_string()));
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

	long long file_mod_time(const std::string& fname)
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
		return rename(path(from), path(to));
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
			for(const std::string& s : cur_path) {
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
		std::string common_part = normalise_path(source);
		std::string back;
		if(common_part.length() > 1 && common_part[common_part.length()-1] == '/') {
			common_part.erase(common_part.length()-1);
		}
		while(boost::iequals(del_substring_front(target, common_part), target)) {
			size_t offs = common_part.rfind('/');
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
		return back + common_part;
	}

	namespace 
	{
		typedef std::map<std::string, std::vector<std::function<void()> > > file_mod_handler_map;
		file_mod_handler_map& get_mod_map() 
		{
			static file_mod_handler_map instance;
			return instance;
		}

		struct FileModHandle {
			std::string fname;
			int index;

			void removeHandle(const FileModHandle& other) {
				if(other.fname == fname && other.index < index) {
					--index;
				}
			}
		};

		std::map<int, FileModHandle> g_file_mod_handles;
		int g_next_file_mod_handle = 1;

	std::vector<std::string> new_files_listening;

	threading::mutex& get_mod_map_mutex() 
	{
		static threading::mutex instance;
		return instance;
	}

	std::vector<std::function<void()> > file_mod_notification_queue;

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
					LOG_WARN("COULD NOT LISTEN ON FILE " << new_files[n]);
				}
			}

			FD_ZERO(&read_set);
			FD_SET(inotify_fd, &read_set);
			timeval tv = {1, 0};
			const int select_res = select(inotify_fd+1, &read_set, nullptr, nullptr, &tv);
			if(select_res > 0) {
				inotify_event ev;
				const int nbytes = read(inotify_fd, &ev, sizeof(ev));
				if(nbytes == sizeof(ev)) {

					const std::string path = fd_to_path[ev.wd];
					LOG_INFO("LINUX FILE MOD: " << path);
					if(ev.mask&IN_IGNORED) {
						fd_to_path.erase(ev.wd);
						const int fd = inotify_add_watch(inotify_fd, path.c_str(), IN_MODIFY);
						if(fd > 0) {
							fd_to_path[fd] = path;
						}
					}
					std::vector<std::function<void()> >& handlers = m[path];
					LOG_INFO("FILE HANDLERS: " << handlers.size());

					threading::lock lck(get_mod_queue_mutex());
					file_mod_notification_queue.insert(file_mod_notification_queue.end(), handlers.begin(), handlers.end());
				} else {
					LOG_ERROR("READ FAILURE IN FILE NOTIFY");
				}
			}

#else
			for(file_mod_handler_map::iterator i = m.begin(); i != m.end(); ++i) {
				std::map<std::string, int64_t>::iterator mod_itor = mod_times.find(i->first);
				const int64_t mod_time = file_mod_time(i->first);
				if(mod_itor == mod_times.end()) {
					mod_times[i->first] = mod_time;
				} else if(mod_time != mod_itor->second) {
					LOG_INFO("MODIFY: " << mod_itor->first);
					mod_itor->second = mod_time;

					threading::lock lck(get_mod_queue_mutex());
					file_mod_notification_queue.insert(file_mod_notification_queue.end(), i->second.begin(), i->second.end());
				}
			}

			profile::delay(100);
#endif
		}
	}

	threading::thread* file_mod_worker_thread = nullptr;

	}

	FilesystemManager::FilesystemManager()
	{
	}

	FilesystemManager::~FilesystemManager()
	{
		{
			threading::lock lck(get_mod_map_mutex());
			get_mod_map().clear();
		}

		delete file_mod_worker_thread;
		file_mod_worker_thread = nullptr;
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

	int notify_on_file_modification(const std::string& path, std::function<void()> handler)
	{
		int handle = -1;
		{
			threading::lock lck(get_mod_map_mutex());
			handle = g_next_file_mod_handle++;
			std::vector<std::function<void()> >& handlers = get_mod_map()[path];
			if(handlers.empty()) {
				new_files_listening.push_back(path);
			}

			FileModHandle& handle_info = g_file_mod_handles[handle];
			handle_info.fname = path;
			handle_info.index = handlers.size();

			handlers.push_back(handler);
		}

		if(file_mod_worker_thread == nullptr) {
			file_mod_worker_thread = new threading::thread("file_change_notify", file_mod_worker_thread_fn);
		}

		return handle;
	}

	void remove_notify_on_file_modification(int handle)
	{
		auto itor = g_file_mod_handles.find(handle);
		if(itor != g_file_mod_handles.end()) {
			threading::lock lck(get_mod_map_mutex());
			std::vector<std::function<void()> >& handlers = get_mod_map()[itor->second.fname];
			handlers.erase(handlers.begin() + itor->second.index);
		}

		for(auto i = g_file_mod_handles.begin(); i != g_file_mod_handles.end(); ++i) {
			i->second.removeHandle(itor->second);
		}

		g_file_mod_handles.erase(itor);
	}

	void pump_file_modifications()
	{
		if(file_mod_worker_thread == nullptr) {
			return;
		}

		std::vector<std::function<void()> > v;
		{
			threading::lock lck(get_mod_queue_mutex());
			v.swap(file_mod_notification_queue);
		}

		for(std::function<void()> f : v) {
			LOG_INFO("CALLING FILE MOD HANDLER");
			f();
		}
	}

	bool consecutive_periods(char a, char b) {
		return a == '.' && b == '.';
	}

	bool is_safe_write_path(const std::string& path, std::string* error)
	{
		if(path.empty()) {
			if(error) {
				*error = "DOCUMENT NAME IS EMPTY";
			}

			return false;
		}
		if(sys::is_path_absolute(path)) {
			if(error) {
				*error = "DOCUMENT NAME IS ABSOLUTE PATH";
			}
			return false;
		}
		if(std::adjacent_find(path.begin(), path.end(), consecutive_periods) != path.end()) {
			if(error) {
				*error = "ILLEGAL RELATIVE FILE PATH";
			}

			return false;
		}

		return true;
	}

	bool is_file_executable(const std::string& path)
	{
#ifdef __APPLE__
		struct stat buf;
		stat(path.c_str(), &buf);
		return (buf.st_mode&S_IXUSR) != 0;
#else
		return (boost::filesystem::status(path).permissions()&boost::filesystem::owner_exe) != 0;
#endif
	}

	void set_file_executable(const std::string& path)
	{
#ifdef __APPLE__
		struct stat buf;
		stat(path.c_str(), &buf);
		chmod(path.c_str(), buf.st_mode|S_IXUSR);
#else
		boost::filesystem::permissions(path, boost::filesystem::status(path).permissions() | boost::filesystem::owner_exe);
#endif
	}

	bool is_file_writable(const std::string& path)
	{
#ifdef __APPLE__
		struct stat buf;
		stat(path.c_str(), &buf);
		return (buf.st_mode&S_IWUSR) != 0;
#else
		return (boost::filesystem::status(path).permissions()&boost::filesystem::owner_write) != 0;
#endif
	}

	void set_file_writable(const std::string& path)
	{
#ifdef __APPLE__
		struct stat buf;
		stat(path.c_str(), &buf);
		chmod(path.c_str(), buf.st_mode|S_IWUSR);
#else
		boost::filesystem::permissions(path, boost::filesystem::status(path).permissions() | boost::filesystem::owner_write);
#endif
	}

	std::string get_cwd()
	{
		return boost::filesystem::current_path().generic_string();
	}
}
