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

//This file is designed to only work on Linux.
#ifdef __linux__

#include <algorithm>
#include <deque>
#include <boost/algorithm/string/replace.hpp>
#include <boost/bind.hpp>
#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "formula_object.hpp"
#include "http_server.hpp"
#include "ipc.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "string_utils.hpp"
#include "tbs_bot.hpp"
#include "tbs_server.hpp"
#include "tbs_web_server.hpp"
#include "unit_test.hpp"
#include "utils.hpp"
#include "variant.hpp"
#include "variant_utils.hpp"

using namespace tbs;

namespace {

const variant& get_server_info_file()
{
	static variant server_info;
	if(server_info.is_null()) {
		try {
			server_info = json::parse_from_file("data/server_info.cfg");
		} catch(...) {
			ASSERT_LOG(false, "Could not parse server info file data/server_info.cfg");
		}
		server_info.add_attr(variant("type"), variant("server_info"));
	}
	return server_info;
}

class matchmaking_server : public http::web_server
{
public:
	matchmaking_server(boost::asio::io_service& io_service, int port)
	  : http::web_server(io_service, port),
	    io_service_(io_service), port_(port), timer_(io_service),
		time_ms_(0), terminated_servers_(0)
	{
		timer_.expires_from_now(boost::posix_time::milliseconds(1000));
		timer_.async_wait(boost::bind(&matchmaking_server::heartbeat, this, boost::asio::placeholders::error));

		for(int i = 0; i != 256; ++i) {
			available_ports_.push_back(21156+i);
		}
	}

	~matchmaking_server()
	{
		timer_.cancel();
	}

	void heartbeat(const boost::system::error_code& error)
	{
		int pid_status = 0;
		const pid_t pid = waitpid(-1, &pid_status, WNOHANG);
		if(pid < 0) {
			if(errno != ECHILD) {
				switch(errno) {
				case EINVAL: fprintf(stderr, "waitpid() had invalid arguments\n"); break;
				default: fprintf(stderr, "waitpid() returns unknown error: %d\n", errno); break;
				}
			}
		} else if(pid > 0) {
			auto itor = servers_.find(pid);
			if(itor == servers_.end()) {
				fprintf(stderr, "ERROR: unknown pid exited: %d\n", (int)pid);
			} else {
				available_ports_.push_back(itor->second.port);
				servers_.erase(pid);

				++terminated_servers_;

				fprintf(stderr, "Child server exited. %d servers running\n", (int)servers_.size());
			}
		}

		time_ms_ += 1000;

		for(auto& p : sessions_) {
			if(p.second.current_socket && time_ms_ - p.second.last_contact >= 5000) {

				send_msg(p.second.current_socket, "text/json", "{ \"type\": \"heartbeat\" }", "");
				p.second.last_contact = time_ms_;
				p.second.current_socket = socket_ptr();
			} else if(!p.second.current_socket && time_ms_ - p.second.last_contact >= 10000) {
				p.second.session_id = 0;
			}
		}

		for(auto itor = sessions_.begin(); itor != sessions_.end(); ) {
			if(itor->second.session_id == 0) {
				sessions_.erase(itor++);
			} else {
				++itor;
			}
		}

		timer_.expires_from_now(boost::posix_time::milliseconds(1000));
		timer_.async_wait(boost::bind(&matchmaking_server::heartbeat, this, boost::asio::placeholders::error));
	}


	void handle_post(socket_ptr socket, variant doc, const http::environment& env)
	{
		try {
			static int session_id_gen = 8000000;
			fprintf(stderr, "HANDLE POST: %s\n", doc.write_json().c_str());

			assert_recover_scope recover_scope;
			std::string request_type = doc["type"].as_string();

			if(request_type == "get_server_info") {
				static const std::string server_info = get_server_info_file().write_json();
				send_msg(socket, "text/json", server_info, "");
			} else if(request_type == "matchmake") {
				const int session_id = session_id_gen++;

				std::map<variant,variant> response;
				response[variant("type")] = variant("matchmaking_queued");
				response[variant("session_id")] = variant(session_id);
				send_msg(socket, "text/json", variant(&response).write_json(), "");

				SessionInfo& info = sessions_[session_id];
				info.session_id = session_id;
				info.user_id = doc["user"].as_string();
				info.last_contact = time_ms_;
				
				std::vector<int> match_sessions;
				for(auto p : sessions_) {
					if(p.second.game_details == "" && !session_timed_out(p.second.last_contact) && !p.second.game_pending) {
						match_sessions.push_back(p.first);
						if(match_sessions.size() == 2) {
							break;
						}
					}
				}

				if(match_sessions.size() == 2 && !available_ports_.empty()) {
					//spawn off a server to play this game.
					std::string fname = formatter() << "/tmp/anura_tbs_server." << match_sessions.front();
					std::string fname_out = formatter() << "/tmp/anura.out." << match_sessions.front();

					variant_builder game;
					game.add("game_type", "citadel");
					
					std::vector<variant> users;
					for(int i : match_sessions) {
						SessionInfo& session_info = sessions_[i];
						session_info.game_pending = time_ms_;

						variant_builder user;
						user.add("user", session_info.user_id);
						user.add("session_id", session_info.session_id);
						users.push_back(user.build());
					}

					variant users_info(&users);

					game.add("users", users_info);

					variant_builder server_config;
					server_config.add("game", game.build());

					server_config.add("matchmaking_host", "localhost");
					server_config.add("matchmaking_port", port_);

					sys::write_file(fname, server_config.build().write_json());

					int new_port = available_ports_.front();
					available_ports_.pop_front();

					assert(!preferences::argv().empty());

					const char* cmd = preferences::argv().front().c_str();

					std::vector<std::string> args;
					args.push_back(cmd);
					args.push_back("--module=" + module::get_module_name());
					args.push_back("--utility=tbs_server");
					args.push_back("--port");
					args.push_back(formatter() << new_port);
					args.push_back("--config");
					args.push_back(fname);

					fprintf(stderr, "EXECUTING:");

					std::vector<char*> cstr_argv;
					for(std::string& s : args) {
						s.push_back('\0');
						cstr_argv.push_back(&s[0]);
						fprintf(stderr, " %s", cstr_argv.back());
					}

					fprintf(stderr, "\n");

					cstr_argv.push_back(NULL);

					const pid_t pid = fork();
					if(pid < 0) {
						fprintf(stderr, "FATAL ERROR: FAILED TO FORK\n");
						assert(!"failed to fork");
					} else if(pid == 0) {

						int fd = open(fname_out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
						dup2(fd, STDERR_FILENO);
						fprintf(stderr, "Execing server...\n");

						//child
						execv(cmd, &cstr_argv[0]);
						fprintf(stderr, "EXEC FAILED!\n");
						_exit(0);
					} else {
						//parent
						fprintf(stderr, "Forked process %d\n", static_cast<int>(pid));

						ProcessInfo& info = servers_[pid];
						info.port = new_port;
						info.sessions = match_sessions;
						info.users = users_info;
					}


				} else if(match_sessions.size() == 2 && available_ports_.empty()) {
					fprintf(stderr, "ERROR: AVAILABLE PORTS EXHAUSTED\n");
				}

			} else if(request_type == "request_updates") {

				int session_id = doc["session_id"].as_int();

				auto itor = sessions_.find(session_id);
				if(itor == sessions_.end()) {
					send_msg(socket, "text/json", "{ type: \"error\", message: \"unknown session\" }", "");
				} else {
					itor->second.last_contact = time_ms_;
					if(itor->second.game_details != "" && !itor->second.game_pending) {
						send_msg(socket, "text/json", itor->second.game_details, "");
					} else {
						if(itor->second.current_socket) {
							disconnect(itor->second.current_socket);
						}

						itor->second.current_socket = socket;
					}
				}
			} else if(request_type == "server_created_game") {
				fprintf(stderr, "Notified of game up on server\n");

				variant_builder msg;
				msg.add("type", "match_made");
				msg.add("game_id", doc["game_id"].as_int());
				msg.add("port", doc["port"].as_int());

				variant msg_variant = msg.build();

				for(variant user : doc["game"]["users"].as_list()) {
					int session_id = user["session_id"].as_int();
					auto itor = sessions_.find(session_id);
					if(itor == sessions_.end()) {
						fprintf(stderr, "ERROR: Session not found: %d\n", session_id);
					} else {
						itor->second.game_pending = 0;
						itor->second.game_port = doc["port"].as_int();
						itor->second.game_details = msg_variant.write_json();
						fprintf(stderr, "Queued game message for session %d\n", session_id);

						if(itor->second.current_socket) {
							send_msg(itor->second.current_socket, "text/json", itor->second.game_details, "");
							itor->second.current_socket.reset();
						}
					}
				}

				send_msg(socket, "text/json", "{ \"type\": \"ok\" }", "");
			} else if(request_type == "query_status") {
				variant response = build_status();
				if(doc.has_key("session_id") == false) {
					const int session_id = session_id_gen++;
					response.add_attr(variant("session_id"), variant(session_id));
				}
				send_msg(socket, "text/json", response.write_json(), "");
			} else {
				fprintf(stderr, "UNKNOWN REQUEST TYPE: '%s'\n", request_type.c_str());
				disconnect(socket);
			}

		} catch(validation_failure_exception& error) {
			fprintf(stderr, "ERROR HANDLING POST: %s\n", error.msg.c_str());
			disconnect(socket);
		}
	}

	void handle_get(socket_ptr socket, const std::string& url, const std::map<std::string, std::string>& args)
	{
		fprintf(stderr, "HANDLE GET: %s\n", url.c_str());

		if(url == "/tbs_monitor") {
			send_msg(socket, "text/json", build_status().write_json(), "");
		}
	}

private:
	variant build_status() const {

		variant_builder doc;

		doc.add("type", "server_status");
		doc.add("uptime", time_ms_/1000);
		doc.add("port", port_);
		doc.add("terminated_servers", terminated_servers_);

		std::vector<variant> servers;
		for(auto p : servers_) {
			variant_builder server;
			server.add("pid", p.first);
			server.add("port", p.second.port);
			server.add("sessions", vector_to_variant(p.second.sessions));
			server.add("users", p.second.users);

			servers.push_back(server.build());
		}

		doc.add("servers", variant(&servers));

		std::map<variant,variant> sessions;
		for(auto p : sessions_) {
			variant_builder s;
			s.add("user", p.second.user_id);
			s.add("last_connection", (time_ms_ - p.second.last_contact)/1000);
			s.add("game_port", p.second.game_port);
			s.add("active_connection", p.second.current_socket ? true : false);

			if(p.second.game_pending) {
				s.add("game_pending", true);
			}

			sessions[variant(formatter() << p.first)] = s.build();
		}

		doc.add("sessions", variant(&sessions));

		return doc.build();
	}

	boost::asio::io_service& io_service_;
	int port_;
	boost::asio::deadline_timer timer_;

	struct SessionInfo {
		SessionInfo() : game_pending(0), game_port(0) {}
		int session_id;
		std::string user_id;
		std::string game_details;
		int last_contact;
		int game_pending;
		int game_port;
		socket_ptr current_socket;
	};

	std::map<int, SessionInfo> sessions_;

	int time_ms_;

	int session_timed_out(int last_contact) const {
		return time_ms_ - last_contact > 10000;
	}


	struct ProcessInfo {
		int port;
		std::vector<int> sessions;
		variant users;
	};

	std::deque<int> available_ports_;
	std::map<int, ProcessInfo> servers_;

	//stats
	int terminated_servers_;
};

}

COMMAND_LINE_UTILITY(tbs_matchmaking_server) {
	int port = 23456;

	std::deque<std::string> arguments(args.begin(), args.end());
	while(arguments.empty() == false) {
		std::string arg = arguments.front();
		arguments.pop_front();

		if(arg == "--port") {
			ASSERT_LOG(!arguments.empty(), "Need another argument after --port");
			port = atoi(arguments.front().c_str());
			arguments.pop_front();
		} else {
			ASSERT_LOG(false, "Unrecognized argument: " << arg);
		}
	}

	boost::asio::io_service io_service;
	matchmaking_server server(io_service, port);
	io_service.run();
}

#endif // __linux__
