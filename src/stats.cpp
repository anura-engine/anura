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

#include <map>
#include <sstream>
#include <stdio.h>
#include <vector>

#include <array>
#include <boost/asio.hpp>

#include "checksum.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "http_client.hpp"
#include "level.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "playable_custom_object.hpp"
#include "stats.hpp"

namespace
{
	std::string get_stats_dir()
	{
		return sys::get_dir(std::string(preferences::user_data_path()) + "stats/") + "/";
	}
}

namespace stats
{
	using std::placeholders::_1;
	using std::placeholders::_2;
	using std::placeholders::_3;

	namespace
	{
		PREF_BOOL(force_send_stats, false, "");
		PREF_STRING(stats_server, "theargentlark.com", "");
		PREF_STRING(stats_port, "5000", "");

		variant program_args;
		std::map<std::string, std::vector<variant>> write_queue;

		std::vector<std::pair<std::string, std::string>> upload_queue;

		threading::mutex& upload_queue_mutex()
		{
			static threading::mutex m;
			return m;
		}

		threading::condition& send_stats_signal()
		{
			static threading::condition c;
			return c;
		}

		bool send_stats_should_exit = false;

		void send_stats(std::map<std::string, std::vector<variant>>& queue)
		{
			if(queue.empty() || (!checksum::is_verified() && !g_force_send_stats)) {
				return;
			}

			std::map<variant, variant> attr;
			attr[variant("type")] = variant("stats");
			attr[variant("version")] = variant(preferences::version());
			attr[variant("module")] = variant(module::get_module_name());
			attr[variant("module_version")] = variant(module::get_module_version());
			attr[variant("user_id")] = variant(preferences::get_unique_user_id());
			attr[variant("program_args")] = program_args;

			if(checksum::is_verified()) {
				attr[variant("signature")] = variant(checksum::game_signature());
				attr[variant("build_description")] = variant(checksum::build_description());
			} else {
				attr[variant("signature")] = variant("UNSIGNED");
			}

			std::vector<variant> level_vec;

			for(std::map<std::string, std::vector<variant> >::iterator i = queue.begin(); i != queue.end(); ++i) {

				std::map<variant, variant> obj;
				obj[variant("level")] = variant(i->first);
				obj[variant("stats")] = variant(&i->second);
				level_vec.emplace_back(&obj);
			}

			attr[variant("levels")] = variant(&level_vec);

			std::string msg_str = variant(&attr).write_json();
			threading::lock lck(upload_queue_mutex());
			upload_queue.emplace_back("upload-frogatto", msg_str);
		}

		namespace
		{
			void finish_upload(std::string response, bool* flag)
			{
				LOG_INFO("UPLOAD COMPLETE: " << response);
				*flag = true;
			}

			void upload_progress(int sent, int total, bool uploaded)
			{
				LOG_INFO("SENT " << sent << "/" << total);
			}
		}

		void send_stats_thread()
		{
			if(preferences::send_stats() == false) {
				return;
			}

			for(;;) {
				std::vector<std::pair<std::string, std::string>> queue;
				{
					threading::lock lck(upload_queue_mutex());
					if(!send_stats_should_exit && upload_queue.empty()) {
						send_stats_signal().wait_timeout(upload_queue_mutex(), 600000);
					}

					if(send_stats_should_exit && upload_queue.empty()) {
						break;
					}

					queue.swap(upload_queue);
				}

				bool done = false;
				for(int n = 0; n != queue.size(); ++n) {
					try {
						http_client client(g_stats_server, g_stats_port);
						client.send_request("POST /cgi-bin/" + queue[n].first,
							queue[n].second,
							std::bind(finish_upload, _1, &done),
							std::bind(finish_upload, _1, &done),
							std::bind(upload_progress, _1, _2, _3));
						while(!done) {
							client.process();
						}
					} catch(const std::exception& e) {
						LOG_ERROR("Error sending stats: " << e.what() << "\n");
					}
				}
			}
		}
	}

	void download_finish(std::string stats_wml, bool* flag, const std::string& lvl)
	{
		sys::write_file(get_stats_dir() + lvl, stats_wml);
		LOG_INFO("DOWNLOAD COMPLETE");
		*flag = true;
	}

	void download_error(std::string response, bool* flag, bool* err)
	{
		LOG_INFO("DOWNLOAD ERROR: " << response);
		*flag = true;
		*err = true;
	}

	void download_progress(int sent, int total, bool uploaded)
	{
		LOG_INFO("SENT " << sent << "/" << total);
	}

	bool download(const std::string& lvl) {
		bool done = false;
		bool err = false;
		http_client client("www.wesnoth.org", "80");
		client.send_request("GET /files/dave/frogatto-stats/" + lvl,
			"",
			std::bind(download_finish, _1, &done, lvl),
			std::bind(download_error, _1, &done, &err),
			std::bind(download_progress, _1, _2, _3));
		while(!done) {
			client.process();
		}
		return !err;
	}

	namespace
	{
		threading::thread* background_thread = nullptr;
	}

	Manager::Manager()
	{
#if !TARGET_OS_IPHONE
		if(!background_thread) {
			background_thread = new threading::thread("stats-thread", send_stats_thread);
		}
#endif
	}

	Manager::~Manager()
	{
		flush_and_quit();
	}

	void flush_and_quit()
	{
		if(background_thread) {
			send_stats_should_exit = true;
			flush();

			delete background_thread;
			background_thread = nullptr;
		}
	}

	void flush()
	{
		send_stats(write_queue);
		threading::lock lck(upload_queue_mutex());
		send_stats_signal().notify_one();
	}

	Entry::Entry(const std::string& type)
		: level_id_(Level::current().id())
	{
		static const variant TypeStr("type");
		records_[TypeStr] = variant(type);
	}

	Entry::Entry(const std::string& type, const std::string& level_id)
		: level_id_(level_id)
	{
		static const variant TypeStr("type");
		records_[TypeStr] = variant(type);
	}

	Entry::~Entry()
	{
		record(variant(&records_), level_id_);
	}

	Entry& Entry::set(const std::string& name, const variant& value)
	{
		records_[variant(name)] = value;
		return *this;
	}

	Entry& Entry::addPlayerPos()
	{
		if(Level::current().player()) {
			set("x", variant(Level::current().player()->getEntity().getMidpoint().x));
			set("y", variant(Level::current().player()->getEntity().getMidpoint().y));
		}

		return *this;
	}

	void record_program_args(const std::vector<std::string>& args)
	{
		std::vector<variant> v;
		for(const std::string& s : args) {
			v.emplace_back(s);
		}

		program_args = variant(&v);
	}

	void record(const variant& value)
	{
		write_queue[Level::current().id()].push_back(value);
	}

	void record(const variant& value, const std::string& level_id)
	{
		write_queue[level_id].push_back(value);
	}
}
