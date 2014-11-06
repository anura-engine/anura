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
#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
#include <boost/bind.hpp>
#include <deque>
#include <iostream>

#if !defined(_WINDOWS)
#include <sys/time.h>
#endif

#include "asserts.hpp"
#include "base64.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "json_parser.hpp"
#include "md5.hpp"
#include "module_web_server.hpp"
#include "string_utils.hpp"
#include "utils.hpp"
#include "unit_test.hpp"
#include "variant.hpp"
#include "variant_utils.hpp"

using boost::asio::ip::tcp;

module_web_server::module_web_server(const std::string& data_path, boost::asio::io_service& io_service, int port)
	: http::web_server(io_service, port), timer_(io_service), 
	nheartbeat_(0), data_path_(data_path), next_lock_id_(1)
{
	if(data_path_.empty() || data_path_[data_path_.size()-1] != '/') {
		data_path_ += "/";
	}

	if(sys::file_exists(data_file_path())) {
		data_ = json::parse_from_file(data_file_path());
	} else {
		std::map<variant, variant> m;
		data_ = variant(&m);
	}

	heartbeat();
}

void module_web_server::heartbeat()
{
	timer_.expires_from_now(boost::posix_time::seconds(1));
	timer_.async_wait(boost::bind(&module_web_server::heartbeat, this));
}

void module_web_server::handle_post(socket_ptr socket, variant doc, const http::environment& env)
{
	std::map<variant,variant> response;
	try {
		const std::string msg_type = doc["type"].as_string();
		if(msg_type == "download_module") {
			const std::string module_id = doc["module_id"].as_string();

			variant server_version = data_[module_id]["version"];

			if(doc.has_key("current_version")) {
				variant current_version = doc["current_version"];
				if(server_version <= current_version) {
					send_msg(socket, "text/json", "{ status: \"no_newer_module\" }", "");
					return;
				}
			}

			const std::string module_path = data_path_ + module_id + ".cfg";
			if(sys::file_exists(module_path)) {
				std::string response = "{\nstatus: \"ok\",\nversion: " + server_version.write_json() + ",\nmodule: ";
				{
					std::string contents = sys::read_file(module_path);
					fprintf(stderr, "MANIFEST: %d\n", (int)doc.has_key("manifest"));
					if(doc.has_key("manifest")) {
						variant their_manifest = doc["manifest"];
						variant module = json::parse(contents);
						variant our_manifest = module["manifest"];

						std::vector<variant> deletions;
						for(auto p : their_manifest.as_map()) {
							if(!our_manifest.has_key(p.first)) {
								deletions.push_back(p.first);
							}
						}

						if(!deletions.empty()) {
							module.add_attr_mutation(variant("delete"), variant(&deletions));
						}

						std::vector<variant> matches;

						for(auto p : our_manifest.as_map()) {
							if(!their_manifest.has_key(p.first)) {
								fprintf(stderr, "their manifest does not have key: %s\n", p.first.write_json().c_str());
								continue;
							}

							if(p.second["md5"] != their_manifest[p.first]["md5"]) {
								fprintf(stderr, "their manifest mismatch key: %s\n", p.first.write_json().c_str());
								continue;
							}

							matches.push_back(p.first);
						}

						for(variant match : matches) {
							our_manifest.remove_attr_mutation(match);
						}

						contents = module.write_json();
					}

					response += contents;
				}

				response += "\n}";
				send_msg(socket, "text/json", response, "");

				variant summary = data_[module_id];
				if(summary.is_map()) {
					summary.add_attr_mutation(variant("num_downloads"), variant(summary["num_downloads"].as_int() + 1));
				}
				return;

			} else {
				response[variant("message")] = variant("No such module");
			}

		} else if(msg_type == "prepare_upload_module") {
			const std::string module_id = doc["module_id"].as_string();
			ASSERT_LOG(std::count_if(module_id.begin(), module_id.end(), isalnum) + std::count(module_id.begin(), module_id.end(), '_') == module_id.size(), "ILLEGAL MODULE ID");

			const std::string module_path = data_path_ + module_id + ".cfg";
			if(sys::file_exists(module_path)) {
				std::string contents = sys::read_file(module_path);
				variant module = json::parse(contents);
				variant manifest = module["manifest"];
				for(auto p : manifest.as_map()) {
					p.second.remove_attr_mutation(variant("data"));
				}

				response[variant("manifest")] = manifest;
			}

			module_lock_ids_[module_id] = next_lock_id_;
			response[variant("status")] = variant("ok");
			response[variant("lock_id")] = variant(next_lock_id_);

			++next_lock_id_;

		} else if(msg_type == "upload_module") {
			variant module_node = doc["module"];
			const std::string module_id = module_node["id"].as_string();
			ASSERT_LOG(std::count_if(module_id.begin(), module_id.end(), isalnum) + std::count(module_id.begin(), module_id.end(), '_') == module_id.size(), "ILLEGAL MODULE ID");

			variant lock_id = doc["lock_id"];
			ASSERT_LOG(lock_id == variant(module_lock_ids_[module_id]), "Invalid lock on module: " << lock_id.write_json() << " vs " << module_lock_ids_[module_id]);

			variant current_data = data_[variant(module_id)];
			if(current_data.is_null() == false) {
				const variant new_version = module_node[variant("version")];
				const variant old_version = current_data[variant("version")];
				ASSERT_LOG(new_version > old_version, "VERSION " << new_version.write_json() << " IS NOT NEWER THAN EXISTING VERSION " << old_version.write_json());
			}

			const std::string module_path = data_path_ + module_id + ".cfg";

			if(sys::file_exists(module_path)) {
				std::vector<variant> deletions;
				if(doc.has_key("delete")) {
					deletions = doc["delete"].as_list();
				}

				variant current_module = json::parse(sys::read_file(module_path));
				variant new_manifest = module_node["manifest"];
				variant old_manifest = current_module["manifest"];
				for(auto p : old_manifest.as_map()) {
					if(!new_manifest.has_key(p.first) && !std::count(deletions.begin(), deletions.end(), p.first)) {
						new_manifest.add_attr_mutation(p.first, p.second);
					}
				}
			}


			const std::string module_path_tmp = module_path + ".tmp";
			const std::string contents = module_node.write_json();
			
			sys::write_file(module_path_tmp, contents);
			const int rename_result = rename(module_path_tmp.c_str(), module_path.c_str());
			ASSERT_LOG(rename_result == 0, "FAILED TO RENAME FILE: " << errno);

			response[variant("status")] = variant("ok");

			{
				std::map<variant, variant> summary;
				summary[variant("version")] = module_node[variant("version")];
				summary[variant("name")] = module_node[variant("name")];
				summary[variant("description")] = module_node[variant("description")];
				summary[variant("author")] = module_node[variant("author")];
				summary[variant("dependencies")] = module_node[variant("dependencies")];
				summary[variant("num_downloads")] = variant(0);
				summary[variant("num_ratings")] = variant(0);
				summary[variant("sum_ratings")] = variant(0);

				std::vector<variant> reviews_list;
				summary[variant("reviews")] = variant(&reviews_list);

				if(module_node.has_key("icon")) {
					std::string icon_str = base64::b64decode(module_node["icon"].as_string());

					const std::string hash = md5::sum(icon_str);
					sys::write_file(data_path_ + "/.glob/" + hash, icon_str);

					summary[variant("icon")] = variant(hash);
				}

				data_.add_attr_mutation(variant(module_id), variant(&summary));
				write_data();
			}

		} else if(msg_type == "replicate_module") {
			const std::string src_id = doc["src_id"].as_string();
			const std::string dst_id = doc["dst_id"].as_string();

			const std::string src_path = data_path_ + src_id + ".cfg";
			const std::string dst_path = data_path_ + dst_id + ".cfg";

			variant src_info = data_[src_id];
			ASSERT_LOG(src_info.is_map(), "Could not find source module " << src_id);
			ASSERT_LOG(sys::file_exists(src_path), "Source module " << src_id << " does not exist");

			std::vector<int> version_num = src_info["version"].as_list_int();

			variant dst_info = data_[dst_id];
			if(dst_info.is_map()) {
				std::vector<int> dst_version_num = dst_info["version"].as_list_int();
				ASSERT_LOG(!dst_version_num.empty(), "Illegal module version in " << dst_id);

				if(version_num <= dst_version_num) {
					version_num = dst_version_num;
					version_num[version_num.size()-1]++;
				}
			}

			std::string src_contents = sys::read_file(src_path);
			variant module_node = json::parse(src_contents);
			module_node.add_attr_mutation(variant("version"), vector_to_variant(version_num));


			const std::string module_path_tmp = dst_path + ".tmp";
			const std::string contents = module_node.write_json();
			
			sys::write_file(module_path_tmp, contents);
			const int rename_result = rename(module_path_tmp.c_str(), dst_path.c_str());
			ASSERT_LOG(rename_result == 0, "FAILED TO RENAME FILE: " << errno);

			response[variant("status")] = variant("ok");

			{
				std::map<variant, variant> summary;
				summary[variant("version")] = module_node[variant("version")];
				summary[variant("name")] = module_node[variant("name")];
				summary[variant("description")] = module_node[variant("description")];
				summary[variant("author")] = module_node[variant("author")];
				summary[variant("dependencies")] = module_node[variant("dependencies")];
				summary[variant("num_downloads")] = variant(0);
				summary[variant("num_ratings")] = variant(0);
				summary[variant("sum_ratings")] = variant(0);

				std::vector<variant> reviews_list;
				summary[variant("reviews")] = variant(&reviews_list);

				if(module_node.has_key("icon")) {
					std::string icon_str = base64::b64decode(module_node["icon"].as_string());

					const std::string hash = md5::sum(icon_str);
					sys::write_file(data_path_ + "/.glob/" + hash, icon_str);

					summary[variant("icon")] = variant(hash);
				}

				data_.add_attr_mutation(variant(dst_id), variant(&summary));
				write_data();
			}


		} else if(msg_type == "query_globs") {
			response[variant("status")] = variant("ok");
			foreach(const std::string& k, doc["keys"].as_list_string()) {
				const std::string data = sys::read_file(data_path_ + "/.glob/" + k);
				response[variant(k)] = variant(base64::b64encode(data));
			}
		} else if(msg_type == "rate") {
			const std::string module_id = doc["module_id"].as_string();
			variant summary = data_[module_id];
			ASSERT_LOG(summary.is_map(), "UNKNOWN MODULE ID: " << module_id);

			const int rating = doc["rating"].as_int();
			ASSERT_LOG(rating >= 1 && rating <= 5, "ILLEGAL RATING");
			summary.add_attr_mutation(variant("num_ratings"), variant(summary["num_ratings"].as_int() + 1));
			summary.add_attr_mutation(variant("sum_ratings"), variant(summary["sum_ratings"].as_int() + rating));

			if(doc["review"].is_null() == false) {
				std::vector<variant> v = summary["reviews"].as_list();
				v.push_back(doc);
				summary.add_attr_mutation(variant("reviews"), variant(&v));
			}

			response[variant("status")] = variant("ok");
		} else {
			ASSERT_LOG(false, "Unknown message type");
		}
	} catch(validation_failure_exception& e) {
		response[variant("status")] = variant("error");
		response[variant("message")] = variant(e.msg);
	}

	send_msg(socket, "text/json", variant(&response).write_json(), "");
}

void module_web_server::handle_get(socket_ptr socket, const std::string& url, const std::map<std::string, std::string>& args)
{
	std::map<variant,variant> response;
	try {
		std::cerr << "URL: (" << url << ")\n";
		response[variant("status")] = variant("error");
		 if(url == "/get_summary") {
			response[variant("status")] = variant("ok");
			response[variant("summary")] = data_;
		} else if(url == "/package") {
			ASSERT_LOG(args.count("id"), "Must specify module id");
			const std::string id = args.find("id")->second;
			const std::string module_path = data_path_ + id + ".cfg";
			ASSERT_LOG(sys::file_exists(module_path), "No such module");

			std::string contents = sys::read_file(module_path);
			variant module = json::parse(contents);
			variant manifest = module["manifest"];
			for(auto p : manifest.as_map()) {
				p.second.remove_attr_mutation(variant("data"));
			}

			response[variant("manifest")] = manifest;
			response[variant("status")] = variant("ok");
		} else {
			response[variant("message")] = variant("Unknown path");
		}
	} catch(validation_failure_exception& e) {
		response[variant("status")] = variant("error");
		response[variant("message")] = variant(e.msg);
	}

	send_msg(socket, "text/json", variant(&response).write_json(), "");
}

std::string module_web_server::data_file_path() const
{
	return data_path_ + "/module-data.json";
}

void module_web_server::write_data()
{
	const std::string tmp_path = data_file_path() + ".tmp";
	sys::write_file(tmp_path, data_.write_json());
	const int rename_result = rename(tmp_path.c_str(), data_file_path().c_str());
		ASSERT_LOG(rename_result == 0, "FAILED TO RENAME FILE: " << errno);
}

COMMAND_LINE_UTILITY(module_server)
{
	std::string path = ".";
	int port = 23456;

	std::deque<std::string> arguments(args.begin(), args.end());
	while(!arguments.empty()) {
		const std::string arg = arguments.front();
		arguments.pop_front();
		if(arg == "--path") {
			ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
			path = arguments.front();
			arguments.pop_front();
		} else if(arg == "-p" || arg == "--port") {
			ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
			port = atoi(arguments.front().c_str());
			arguments.pop_front();
		} else {
			ASSERT_LOG(false, "UNRECOGNIZED ARGUMENT: " << arg);
		}
	}

	const assert_recover_scope recovery;
	boost::asio::io_service io_service;
	module_web_server server(path, io_service, port);
	io_service.run();
}
