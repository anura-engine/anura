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
#include <boost/algorithm/string/replace.hpp>
#include <iostream>

#if !defined(_MSC_VER)
#include <sys/time.h>
#endif

#include "asserts.hpp"
#include "base64.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "json_parser.hpp"
#include "stats_server.hpp"
#include "stats_web_server.hpp"
#include "string_utils.hpp"
#include "utils.hpp"
#include "variant.hpp"

std::string global_debug_str;

web_server::web_server(boost::asio::io_service& io_service, int port)
	: http::web_server(io_service, port), timer_(io_service), nheartbeat_(0)
{
	heartbeat();
}

void web_server::handlePost(socket_ptr socket, variant doc, const http::environment& env)
{
	static const variant TypeVariant("type");
	const std::string& type = doc[TypeVariant].as_string();
	if(type == "stats") {
		process_stats(doc);
	} else if(type == "upload_table_definitions") {
		//TODO: add authentication to get info about the user
		//and make sure they have permission to update this module.
		const std::string& module = doc[variant("module")].as_string();
		init_tables_for_module(module, doc[variant("definition")]);

		try {
			send_msg(socket, "text/json", "{ \"status\": \"ok\" }", "");
			sys::write_file("stats-definitions.json", get_tables_definition().write_json());
		} catch(validation_failure_exception& e) {
			std::map<variant,variant> msg;
			msg[variant("status")] = variant("error");
			msg[variant("message")] = variant(e.msg);
			send_msg(socket, "text/json", variant(&msg).write_json(true, variant::JSON_COMPLIANT), "");
		}

		return;
	}
	disconnect(socket);
}


void web_server::handleGet(socket_ptr socket, 
	const std::string& url, 
	const std::map<std::string, std::string>& args)
{
	std::map<std::string, std::string>::const_iterator raw_it = args.find("raw_stats");
	if(raw_it != args.end()) {
		variant msg = get_raw_stats(raw_it->second);
		send_msg(socket, "text/json", msg.write_json(true, variant::JSON_COMPLIANT), "");
		return;
	}
	
	std::map<std::string, std::string>::const_iterator it = args.find("type");
	if(it != args.end() && it->second == "status") {
		std::map<variant,variant> m;
		const std::map<std::string,std::string> errors = get_stats_errors();
		for(std::map<std::string,std::string>::const_iterator i = errors.begin(); i != errors.end(); ++i) {
			std::string msg = i->second;
			if(msg.empty()) {
				msg = "OK";
			}

			m[variant(i->first)] = variant(msg);
		}

		send_msg(socket, "text/json", variant(&m).write_json(true, variant::JSON_COMPLIANT), "");
		return;
	} else if(it != args.end() && it->second == "crashes") {
		variant crashes = get_crashes();
		send_msg(socket, "text/json", crashes.write_json(true, variant::JSON_COMPLIANT), "");
		return;

	}

	variant value = get_stats(args.count("version") ? args.find("version")->second : "", 
		args.count("module") ? args.find("module")->second : "",
		args.count("module_version") ? args.find("module_version")->second : "",
		args.count("level") ? args.find("level")->second : "");
	send_msg(socket, "text/json", value.write_json(true, variant::JSON_COMPLIANT), "");
}

void web_server::heartbeat()
{
	if(++nheartbeat_%3600 == 0) {
		LOG_INFO("WRITING DATA...");
		timeval start_time, end_time;
		gettimeofday(&start_time, nullptr);
		variant v = write_stats();
		std::string data = v.write_json();

		if(sys::file_exists("stats-5.json")) {
			sys::remove_file("stats-5.json");
		}

		for(int n = 4; n >= 1; --n) {
			if(sys::file_exists(formatter() << "stats-" << n << ".json")) {
				sys::move_file(formatter() << "stats-" << n << ".json",
				               formatter() << "stats-" << (n+1) << ".json");
			}
		}

		sys::write_file("stats-1.json", data);

		gettimeofday(&end_time, nullptr);

		const int time_us = (end_time.tv_sec - start_time.tv_sec)*1000000 + (end_time.tv_usec - start_time.tv_usec);
		LOG_INFO("WROTE STATS IN " << time_us << "us");
	}

	timer_.expires_from_now(boost::posix_time::seconds(1));
	timer_.async_wait(std::bind(&web_server::heartbeat, this));
}
