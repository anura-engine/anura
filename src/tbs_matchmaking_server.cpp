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

#include <algorithm>
#include <ctype.h>
#include <deque>
#include <boost/algorithm/string/replace.hpp>
#include <boost/bind.hpp>
#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "asserts.hpp"
#include "db_client.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "formula.hpp"
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
#include "uuid.hpp"
#include "variant.hpp"
#include "variant_utils.hpp"

#ifdef __linux__

using namespace tbs;

namespace {
static int g_session_id_gen = 8000000;

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

std::string normalize_username(std::string username)
{
	util::strip(username);
	for(char& c : username) {
		c = tolower(c);
	}

	return username;
}

bool username_valid(const std::string& username)
{
	if(username.empty()) {
		return false;
	}

	for(char c : username) {
		if(!isalnum(c) && c != '_' && c != ' ' && c != '^') {
			return false;
		}
	}

	return true;
}

class matchmaking_server : public game_logic::formula_callable, public http::web_server
{
public:
	matchmaking_server(boost::asio::io_service& io_service, int port)
	  : http::web_server(io_service, port),
	    io_service_(io_service), port_(port),
		timer_(io_service), db_timer_(io_service),
		time_ms_(0), terminated_servers_(0),
		controller_(game_logic::formula_object::create("matchmaking_server"))
	{

		create_account_fn_ = controller_->query_value("create_account");
		ASSERT_LOG(create_account_fn_.is_function(), "Could not find create_account in matchmaking_server class");

		handle_request_fn_ = controller_->query_value("handle_request");
		ASSERT_LOG(handle_request_fn_.is_function(), "Could not find handle_request in matchmaking_server class");

		db_client_ = db_client::create();

		db_timer_.expires_from_now(boost::posix_time::milliseconds(10));
		db_timer_.async_wait(boost::bind(&matchmaking_server::db_process, this, boost::asio::placeholders::error));

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

	void db_process(const boost::system::error_code& error)
	{
		db_timer_.expires_from_now(boost::posix_time::milliseconds(10));
		db_timer_.async_wait(boost::bind(&matchmaking_server::db_process, this, boost::asio::placeholders::error));

		db_client_->process(1000);
	}

	void execute_command(variant cmd);

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

		const int nqueue_size = check_matchmaking_queue();

		variant_builder heartbeat_message;
		heartbeat_message.add("type", "heartbeat");
		heartbeat_message.add("users", sessions_.size());
		heartbeat_message.add("users_queued", nqueue_size);
		heartbeat_message.add("games", servers_.size());

		std::vector<variant> servers;
		for(auto p : servers_) {
			variant_builder server;
			server.add("port", p.second.port);

			std::vector<variant> users;
			for(int n = 0; n != p.second.users.num_elements(); ++n) {
				std::map<variant,variant> m;
				m[variant("user")] = p.second.users[n]["user"];
				users.push_back(variant(&m));
			}
			server.add("users", variant(&users));

			servers.push_back(server.build());
		}

		heartbeat_message.add("servers", variant(&servers));
		std::string heartbeat_msg = heartbeat_message.build().write_json();

		for(auto& p : sessions_) {
			if(p.second.current_socket && (p.second.sent_heartbeat == false || time_ms_ - p.second.last_contact >= 3000)) {

				send_msg(p.second.current_socket, "text/json", heartbeat_msg, "");
				p.second.last_contact = time_ms_;
				p.second.current_socket = socket_ptr();
				p.second.sent_heartbeat = true;
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
		int request_session_id = -1;
		std::map<std::string, std::string>::const_iterator i = env.find("cookie");
		if(i != env.end()) {
			const char* cookie_start = strstr(i->second.c_str(), " session=");
			if(cookie_start != NULL) {
				++cookie_start;
			} else {
				cookie_start = strstr(i->second.c_str(), "session=");
				if(cookie_start != i->second.c_str()) {
					cookie_start = NULL;
				}
			}
	
			if(cookie_start) {
				request_session_id = atoi(cookie_start+8);
			}
		}

		try {
			fprintf(stderr, "HANDLE POST: %s\n", doc.write_json().c_str());

			assert_recover_scope recover_scope;
			std::string request_type = doc["type"].as_string();

			if(request_type == "register") {
				std::string user = normalize_username(doc["user"].as_string());
				if(!username_valid(user)) {
					variant_builder response;
					response.add("type", "registration_fail");
					response.add("reason", "invalid_username");
					response.add("message", "Not a valid username");
					send_response(socket, response.build());
				}

				std::vector<variant> args;
				args.push_back(doc);
				variant account_info = create_account_fn_(args);

				std::string user_full = doc["user"].as_string();

				std::string passwd = doc["passwd"].as_string();
				const bool remember = doc["remember"].as_bool(false);
				db_client_->get("user:" + user, [=](variant user_info) {
					fprintf(stderr, "ZZZ: DB GET: %s\n", user_info.write_json().c_str());
					if(user_info.is_null() == false) {
						variant_builder response;
						response.add("type", "registration_fail");
						response.add("reason", "username_taken");
						response.add("message", "That username is already taken.");
						send_response(socket, response.build());
						return;
					}

					variant_builder new_user_info;
					new_user_info.add("user", user_full);
					new_user_info.add("passwd", passwd);
					new_user_info.add("info", account_info);

					//put the new user in the database. Note that we use
					//PUT_REPLACE so that if there is a race for two users
					//to register the same name only one will succeed.
					db_client_->put("user:" + user, new_user_info.build(),
					[=]() {
						const int session_id = g_session_id_gen++;
						SessionInfo& info = sessions_[session_id];
						info.session_id = session_id;
						info.user_id = user;
						info.last_contact = this->time_ms_;

						variant_builder response;
						response.add("type", "registration_success");
						response.add("session_id", variant(session_id));
						response.add("username", variant(user));
						response.add("info", account_info);

						if(remember) {
							std::string cookie = write_uuid(generate_uuid());
							response.add("cookie", cookie);
	
							variant_builder cookie_info;
							cookie_info.add("user", user);
							db_client_->put("cookie:" + cookie, cookie_info.build(),
							                [](){}, [](){});
						}

						send_response(socket, response.build());
					},
					[=]() {
						variant_builder response;
						response.add("type", "registration_fail");
						response.add("reason", "db_error");
						response.add("message", "There was an error with registering. Please try again.");
						send_response(socket, response.build());
					}, db_client::PUT_ADD);
				});

				

			} else if(request_type == "login") {
				std::string user = normalize_username(doc["user"].as_string());
				std::string passwd = doc["passwd"].as_string();
				const bool remember = doc["remember"].as_bool(false);
				db_client_->get("user:" + user, [=](variant user_info) {
					variant_builder response;
					if(user_info.is_null()) {
						response.add("type", "login_fail");
						response.add("reason", "user_not_found");
						response.add("message", "That user doesn't exist");

						send_response(socket, response.build());
						return;
					}

					std::string db_passwd = user_info["passwd"].as_string();
					if(passwd != db_passwd) {
						response.add("type", "login_fail");
						response.add("reason", "passwd_incorrect");
						response.add("message", "Incorrect password");

						send_response(socket, response.build());
						return;

					}

					const int session_id = g_session_id_gen++;
					SessionInfo& info = sessions_[session_id];
					info.session_id = session_id;
					info.user_id = user;
					info.last_contact = this->time_ms_;

					response.add("type", "login_success");
					response.add("session_id", variant(session_id));
					response.add("username", variant(info.user_id));
					response.add("info", user_info["info"]);

					if(remember) {
						std::string cookie = write_uuid(generate_uuid());
						response.add("cookie", cookie);

						variant_builder cookie_info;
						cookie_info.add("user", user);
						db_client_->put("cookie:" + cookie, cookie_info.build(),
						                [](){}, [](){});
					}

					send_response(socket, response.build());
				});

			} else if(request_type == "auto_login") {
				std::string cookie = doc["cookie"].as_string();
				db_client_->get("cookie:" + cookie, [=](variant user_info) {

					if(user_info.is_null()) {
						variant_builder response;
						response.add("type", "auto_login_fail");
						send_response(socket, response.build());
						return;
					}

					std::string username = user_info["user"].as_string();

					db_client_->get("user:" + username, [=](variant user_info) {
						variant_builder response;
						if(user_info.is_null()) {
							response.add("type", "auto_login_fail");
							send_response(socket, response.build());
							return;
						}

						const int session_id = g_session_id_gen++;
						SessionInfo& info = sessions_[session_id];
						info.session_id = session_id;
						info.user_id = username;
						info.last_contact = this->time_ms_;
	
						response.add("type", "login_success");
						response.add("session_id", variant(session_id));
						response.add("cookie", variant(cookie));
						response.add("username", variant(info.user_id));
						response.add("info", user_info["info"]);
						send_response(socket, response.build());
					});
				});
			} else if(request_type == "get_server_info") {
				static const std::string server_info = get_server_info_file().write_json();
				send_msg(socket, "text/json", server_info, "");
			} else if(request_type == "cancel_matchmake") {
				const int session_id = doc["session_id"].as_int(request_session_id);

				if(sessions_.count(session_id) == 0) {
					variant_builder response;
					response.add("type", "matchmaking_fail");
					response.add("reason", "bad_session");
					response.add("message", "Invalid session ID");
					send_response(socket, response.build());
					return;
				}

				SessionInfo& info = sessions_[session_id];
				info.session_id = session_id;
				info.last_contact = time_ms_;
				info.queued_for_game = false;

				std::map<variant,variant> response;
				response[variant("type")] = variant("matchmaking_cancelled");
				response[variant("session_id")] = variant(session_id);
				send_msg(socket, "text/json", variant(&response).write_json(), "");

			} else if(request_type == "matchmake") {
				const int session_id = doc["session_id"].as_int(request_session_id);

				if(sessions_.count(session_id) == 0) {
					variant_builder response;
					response.add("type", "matchmaking_fail");
					response.add("reason", "bad_session");
					response.add("message", "Invalid session ID");
					send_response(socket, response.build());
					return;
				}

				std::map<variant,variant> response;
				response[variant("type")] = variant("matchmaking_queued");
				response[variant("session_id")] = variant(session_id);
				send_msg(socket, "text/json", variant(&response).write_json(), "");

				SessionInfo& info = sessions_[session_id];
				info.session_id = session_id;
				info.last_contact = time_ms_;
				info.queued_for_game = true;

				check_matchmaking_queue();

			} else if(request_type == "request_updates") {

				int session_id = doc["session_id"].as_int(request_session_id);

				auto itor = sessions_.find(session_id);
				if(itor == sessions_.end()) {
					fprintf(stderr, "Error: Unknown session: %d\n", session_id);
					send_msg(socket, "text/json", "{ type: \"error\", message: \"unknown session\" }", "");
				} else {
					itor->second.last_contact = time_ms_;
					if(itor->second.game_details != "" && !itor->second.game_pending) {
						send_msg(socket, "text/json", itor->second.game_details, "");
						itor->second.game_details = "";
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
					const int session_id = g_session_id_gen++;
					response.add_attr(variant("session_id"), variant(session_id));
				}
				send_msg(socket, "text/json", response.write_json(), "");
			} else {
				std::vector<variant> args;
				args.push_back(variant(this));
				args.push_back(doc);
				variant cmd = handle_request_fn_(args);
				execute_command(cmd);

				send_msg(socket, "text/json", current_response_.write_json(), "");
				current_response_ = variant();
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

	int check_matchmaking_queue()
	{
		std::vector<int> match_sessions;
		int queue_size = 0;
		for(auto p : sessions_) {
			if(p.second.queued_for_game && p.second.game_details == "" && !session_timed_out(p.second.last_contact)) {
				if(match_sessions.size() < 2 && !p.second.game_pending && p.second.current_socket) {
					match_sessions.push_back(p.first);
				}

				++queue_size;
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
				session_info.queued_for_game = false;

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

		return queue_size;
	}

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

	void send_response(socket_ptr sock, variant msg) {
		send_msg(sock, "text/json", msg.write_json(), "");
	}

	boost::asio::io_service& io_service_;
	int port_;
	boost::asio::deadline_timer timer_;
	boost::asio::deadline_timer db_timer_;


	db_client_ptr db_client_;

	struct SessionInfo {
		SessionInfo() : game_pending(0), game_port(0), queued_for_game(false), sent_heartbeat(false) {}
		int session_id;
		std::string user_id;
		std::string game_details;
		int last_contact;
		int game_pending;
		int game_port;
		socket_ptr current_socket;
		bool queued_for_game;
		bool sent_heartbeat;
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

	boost::intrusive_ptr<game_logic::formula_object> controller_;
	variant create_account_fn_;
	variant handle_request_fn_;

	variant current_response_;

	DECLARE_CALLABLE(matchmaking_server);
};

BEGIN_DEFINE_CALLABLE_NOBASE(matchmaking_server)
DEFINE_FIELD(response, "any")
	return obj.current_response_;
DEFINE_SET_FIELD
	obj.current_response_ = value;
DEFINE_FIELD(db_client, "builtin db_client")
	return variant(obj.db_client_.get());
END_DEFINE_CALLABLE(matchmaking_server)

void matchmaking_server::execute_command(variant cmd)
{
	if(cmd.is_list()) {
		for(variant v : cmd.as_list()) {
			execute_command(v);
		}

		return;
	} else if(cmd.is_null()) {
		return;
	} else {
		const game_logic::command_callable* command = cmd.try_convert<game_logic::command_callable>();
		ASSERT_LOG(command, "Unrecognize command: " << cmd.write_json());

		command->run_command(*this);
	}
}

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
	boost::intrusive_ptr<matchmaking_server> server(new matchmaking_server(io_service, port));
	io_service.run();
}

COMMAND_LINE_UTILITY(db_script) {
	std::deque<std::string> arguments(args.begin(), args.end());

	ASSERT_LOG(arguments.size() >= 1, "Must provide name of script to run and any arguments");

	using namespace game_logic;

	std::string script = sys::read_file(arguments.front());
	arguments.pop_front();
	std::vector<variant> arg;
	for(auto s : arguments) {
		arg.push_back(variant(s));
	}

	formula f = formula(variant(script));

	map_formula_callable_ptr callable(new map_formula_callable);

	db_client_ptr db = db_client::create();
	callable->add("db", variant(db.get()));
	callable->add("args", variant(&arg));
	callable->add("lib", variant(game_logic::get_library_object().get()));

	variant commands = f.execute(*callable);

	boost::asio::io_service io_service;
	boost::intrusive_ptr<matchmaking_server> server(new matchmaking_server(io_service, 29543));

	server->execute_command(commands);

	while(db->process()) {
	}
}

#else

class matchmaking_server : public game_logic::formula_callable, public http::web_server
{
	DECLARE_CALLABLE(matchmaking_server);
};


BEGIN_DEFINE_CALLABLE_NOBASE(matchmaking_server)
DEFINE_FIELD(response, "any")
	return variant();
DEFINE_SET_FIELD
DEFINE_FIELD(db_client, "builtin db_client")
	return variant();
END_DEFINE_CALLABLE(matchmaking_server)

#endif // __linux__
