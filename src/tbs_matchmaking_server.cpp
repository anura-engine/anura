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
#include "formatter.hpp"
#include "formula.hpp"
#include "formula_object.hpp"
#include "http_server.hpp"
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

PREF_INT(matchmaking_heartbeat_ms, 50, "Frequency of matchmaking heartbeats");

class matchmaking_server : public game_logic::FormulaCallable, public http::web_server

{
public:
	matchmaking_server(boost::asio::io_service& io_service, int port)
	  : http::web_server(io_service, port),
	    io_service_(io_service), port_(port),
		timer_(io_service), db_timer_(io_service),
		time_ms_(0), send_at_time_ms_(1000), terminated_servers_(0),
		controller_(game_logic::FormulaObject::create("matchmaking_server"))
	{

		create_account_fn_ = controller_->queryValue("create_account");
		ASSERT_LOG(create_account_fn_.is_function(), "Could not find create_account in matchmaking_server class");

		handle_request_fn_ = controller_->queryValue("handle_request");
		ASSERT_LOG(handle_request_fn_.is_function(), "Could not find handle_request in matchmaking_server class");

		matchmake_fn_ = controller_->queryValue("matchmake");
		ASSERT_LOG(matchmake_fn_.is_function(), "Could not find matchmake function in matchmaking_server class");

		db_client_ = DbClient::create();


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

	void executeCommand(variant cmd);

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
			if(itor != servers_.end()) {
				//This will only happen if a server exited without reporting
				//a game result.
				available_ports_.push_back(itor->second.port);
				servers_.erase(pid);
				++terminated_servers_;

				fprintf(stderr, "Child server exited without a result. %d servers running\n", (int)servers_.size());
			}
		}

		time_ms_ += g_matchmaking_heartbeat_ms;

		if(time_ms_ >= send_at_time_ms_ && send_at_time_ms_ != -1) {
			send_at_time_ms_ += 1000;

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
			std::vector<variant> user_list;
			for(auto p : sessions_) {
				variant_builder b;
				b.add("id", p.second.user_id);

				std::string status = p.second.status;
				if(p.second.queued_for_game) {
					status = "queued";
				}

				b.add("status", status);
				user_list.push_back(b.build());
			}
			heartbeat_message.add("user_list", variant(&user_list));

			variant heartbeat_message_value = heartbeat_message.build();
			std::string heartbeat_msg = heartbeat_message_value.write_json();

			for(auto& p : sessions_) {
				if(p.second.current_socket && (p.second.sent_heartbeat == false || time_ms_ - p.second.last_contact >= 3000 || p.second.chat_messages.empty() == false)) {

					auto user_info_itor = user_info_.find(p.second.user_id);
					static const variant GamePortVariant("game_port");
					static const variant GameIdVariant("game_id");
					static const variant GameSessionVariant("game_session");
					bool added_game_details = false;
					fprintf(stderr, "SEARCH FOR USER: %s -> %s\n", p.second.user_id.c_str(), user_info_itor != user_info_.end() ? "FOUND" : "UNFOUND");
					if(user_info_itor != user_info_.end() && user_info_itor->second.game_pid != -1) {
						auto game_itor = servers_.find(user_info_itor->second.game_pid);
					fprintf(stderr, "SEARCH FOR GAME: %d -> %s\n", user_info_itor->second.game_pid, game_itor != servers_.end() ? "FOUND" : "UNFOUND");

						if(game_itor != servers_.end() && std::find(game_itor->second.users_list.begin(), game_itor->second.users_list.end(), p.second.user_id) != game_itor->second.users_list.end() && game_itor->second.game_id != -1) {
							fprintf(stderr, "SEARCH FOUND SEND\n");
							heartbeat_message_value.add_attr_mutation(GamePortVariant, variant(game_itor->second.port));
							heartbeat_message_value.add_attr_mutation(GameIdVariant, variant(game_itor->second.game_id));
							heartbeat_message_value.add_attr_mutation(GameSessionVariant, variant(user_info_itor->second.game_session));
							added_game_details = true;
						}
					}
	
					if(p.second.chat_messages.empty() == false) {
						heartbeat_message_value.add_attr_mutation(variant("chat_messages"), variant(&p.second.chat_messages));
						send_msg(p.second.current_socket, "text/json", heartbeat_message_value.write_json(), "");
						p.second.chat_messages.clear();

					} else {
						heartbeat_message_value.remove_attr_mutation(variant("chat_messages"));
						send_msg(p.second.current_socket, "text/json", heartbeat_message_value.write_json(), "");
					}

					if(added_game_details) {
						heartbeat_message_value.remove_attr_mutation(GamePortVariant);
						heartbeat_message_value.remove_attr_mutation(GameIdVariant);
					}

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
		}

		timer_.expires_from_now(boost::posix_time::milliseconds(g_matchmaking_heartbeat_ms));
		timer_.async_wait(boost::bind(&matchmaking_server::heartbeat, this, boost::asio::placeholders::error));
	}


	void handlePost(socket_ptr socket, variant doc, const http::environment& env)
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
					}, DbClient::PUT_ADD);
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
			} else if(request_type == "delete_account") {
				const int session_id = doc["session_id"].as_int(request_session_id);

				if(sessions_.count(session_id) == 0) {
					variant_builder response;
					response.add("type", "fail");
					response.add("reason", "bad_session");
					response.add("message", "Invalid session ID");
					send_response(socket, response.build());
					return;
				}

				SessionInfo& info = sessions_[session_id];

				db_client_->remove("user:" + info.user_id);

				variant_builder response;
				response.add("type", "account_deleted");
				send_response(socket, response.build());

				sessions_.erase(session_id);

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
				info.game_type_info = doc["game_info"];

				check_matchmaking_queue();

			} else if(request_type == "global_chat") {
				SessionInfo& info = sessions_[request_session_id];
				if(info.user_id.empty() == false && info.flood_mute_expires < time_ms_) {
					int time_segment = time_ms_/10000;

					if(info.time_segment != time_segment) {
						info.time_segment = time_segment;
						info.messages_this_time_segment = 0;
					}

					if(++info.messages_this_time_segment > 8) {
						//the user has sent too many messages.
						//Mute them for flooding.
						info.flood_mute_expires = time_ms_ + 20000;
					}

					std::string message = doc["message"].as_string();
					if(message.size() > 240) {
						message.resize(240);
					}

					variant_builder msg;
					msg.add("nick", variant(info.user_id));
					msg.add("message", message);

					variant v = msg.build();

					for(auto i = sessions_.begin(); i != sessions_.end(); ++i) {
						i->second.chat_messages.push_back(v);
					}

					send_msg(socket, "text/json", "{ type: \"ack\" }", "");

					schedule_send(200);
				}

			} else if(request_type == "request_updates") {

				int session_id = doc["session_id"].as_int(request_session_id);

				auto itor = sessions_.find(session_id);
				if(itor == sessions_.end()) {
					fprintf(stderr, "Error: Unknown session: %d\n", session_id);
					send_msg(socket, "text/json", "{ type: \"error\", message: \"unknown session\" }", "");
				} else {
					if(doc["status"].is_string()) {
						itor->second.status = doc["status"].as_string();
					}

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

				auto itor = servers_.find(doc["pid"].as_int());
				if(itor != servers_.end()) {
					itor->second.game_id = doc["game_id"].as_int();
				}

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
			} else if(request_type == "server_finished_game") {
				auto itor = servers_.find(doc["pid"].as_int());
				if(itor != servers_.end()) {
					available_ports_.push_back(itor->second.port);
					servers_.erase(itor);

					++terminated_servers_;
					
					fprintf(stderr, "Child server reported exit. %d servers running\n", (int)servers_.size());
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
				executeCommand(cmd);

				send_msg(socket, "text/json", current_response_.write_json(), "");
				current_response_ = variant();
			}

		} catch(validation_failure_exception& error) {
			fprintf(stderr, "ERROR HANDLING POST: %s\n", error.msg.c_str());
			disconnect(socket);
		}
	}

	void handleGet(socket_ptr socket, const std::string& url, const std::map<std::string, std::string>& args)
	{
		fprintf(stderr, "HANDLE GET: %s\n", url.c_str());

		if(url == "/tbs_monitor") {
			send_msg(socket, "text/json", build_status().write_json(), "");
		}
	}

private:

	int check_matchmaking_queue()
	{
		//build a list of queued users and then pass to our FFL matchmake() function
		//to try to make an eligible match.
		std::vector<int> session_ids;
		std::vector<variant> info;
		for(auto p : sessions_) {
			if(p.second.queued_for_game && p.second.game_details == "" && !session_timed_out(p.second.last_contact)) {
				if(!p.second.game_pending && p.second.current_socket) {
					session_ids.push_back(p.first);
					info.push_back(p.second.game_type_info);
				}
			}
		}


		std::vector<variant> args;
		args.push_back(variant(&info));
		variant result = matchmake_fn_(args);

		if(result.is_list() == false) {
			return static_cast<int>(session_ids.size());
		}

		std::vector<int> indexes = result.as_list_int();
		std::vector<int> match_sessions;
		for(int index : indexes) {
			ASSERT_INDEX_INTO_VECTOR(index, session_ids);
			match_sessions.push_back(session_ids[index]);
		}


		if(!available_ports_.empty()) {
			//spawn off a server to play this game.
			std::string fname = formatter() << "/tmp/anura_tbs_server." << match_sessions.front();
			std::string fname_out = formatter() << "/tmp/anura.out." << match_sessions.front();

			variant_builder game;
			game.add("game_type", "citadel");

			variant game_info;

			std::vector<std::string> users_list;
			
			std::vector<variant> users;
			for(int i : match_sessions) {
				SessionInfo& session_info = sessions_[i];
				session_info.game_pending = time_ms_;
				session_info.queued_for_game = false;

				variant_builder user;
				user.add("user", session_info.user_id);
				user.add("session_id", session_info.session_id);
				users.push_back(user.build());

				user_info_[session_info.user_id].game_session = session_info.session_id;

				users_list.push_back(session_info.user_id);

				if(session_info.game_type_info.is_map()) {
					game_info = session_info.game_type_info["info"];
				}

				if(i == match_sessions.back() && session_info.game_type_info.has_key("bot_users")) {
					for(variant item : session_info.game_type_info["bot_users"].as_list()) {
						int index = item["index"].as_int();
						if(index < 0 || index > users.size()) {
							index = users.size();
						}

						users.insert(users.begin() + index, item);
					}
				}
			}

			variant users_info(&users);

			game.add("users", users_info);

			variant_builder server_config;
			variant game_config = game.build();
			if(game_info.is_map()) {
				game_config = game_config + game_info;
			}
			server_config.add("game", game_config);

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
			args.push_back("--no-tbs-server");
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
				info.users_list = users_list;

				for(const std::string& user : users_list) {
					user_info_[user].game_pid = pid;
				}
			}


		} else {
			fprintf(stderr, "ERROR: AVAILABLE PORTS EXHAUSTED\n");
		}

		return static_cast<int>(session_ids.size() - match_sessions.size());
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


	DbClientPtr db_client_;

	struct SessionInfo {
		SessionInfo() : game_pending(0), game_port(0), queued_for_game(false), sent_heartbeat(false), messages_this_time_segment(0), time_segment(0), flood_mute_expires(0) {}
		int session_id;
		std::string user_id;
		std::string game_details;
		std::string status;
		int last_contact;
		int game_pending;
		int game_port;
		socket_ptr current_socket;
		bool queued_for_game;
		variant game_type_info;
		bool sent_heartbeat;

		std::vector<variant> chat_messages;

		//record number of messages from this session so we can
		//disconnect for flooding if necessary.
		int messages_this_time_segment;
		int time_segment;
		int flood_mute_expires;
	};

	std::map<int, SessionInfo> sessions_;

	struct UserInfo {
		UserInfo() : game_pid(-1), game_session(-1) {}
		int game_pid;
		int game_session;
	};

	std::map<std::string, UserInfo> user_info_;

	int time_ms_;
	int send_at_time_ms_;
	void schedule_send(int ms) {
		if(send_at_time_ms_ == -1 || send_at_time_ms_ > time_ms_ + ms) {
			send_at_time_ms_ = time_ms_ + ms;
		}
	}

	int session_timed_out(int last_contact) const {
		return time_ms_ - last_contact > 10000;
	}


	struct ProcessInfo {
		ProcessInfo() : port(-1), game_id(-1) {}
		int port;
		int game_id;
		std::vector<int> sessions;
		variant users;
		std::vector<std::string> users_list;
	};

	std::deque<int> available_ports_;
	std::map<int, ProcessInfo> servers_;

	//stats
	int terminated_servers_;

	boost::intrusive_ptr<game_logic::FormulaObject> controller_;
	variant create_account_fn_;
	variant handle_request_fn_;
	variant matchmake_fn_;

	variant current_response_;

	DECLARE_CALLABLE(matchmaking_server);
};

BEGIN_DEFINE_CALLABLE_NOBASE(matchmaking_server)
DEFINE_FIELD(response, "any")
	return obj.current_response_;
DEFINE_SET_FIELD
	obj.current_response_ = value;
DEFINE_FIELD(db_client, "builtin DbClient")
	return variant(obj.db_client_.get());
END_DEFINE_CALLABLE(matchmaking_server)

void matchmaking_server::executeCommand(variant cmd)
{
	if(cmd.is_list()) {
		for(variant v : cmd.as_list()) {
			executeCommand(v);
		}

		return;
	} else if(cmd.is_null()) {
		return;
	} else {
		const game_logic::CommandCallable* command = cmd.try_convert<game_logic::CommandCallable>();
		ASSERT_LOG(command, "Unrecognize command: " << cmd.write_json());

		command->runCommand(*this);
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

	Formula f = Formula(variant(script));

	MapFormulaCallablePtr callable(new MapFormulaCallable);

	auto db = DbClient::create();
	callable->add("db", variant(db.get()));
	callable->add("args", variant(&arg));
	callable->add("lib", variant(game_logic::get_library_object().get()));

	variant commands = f.execute(*callable);

	boost::asio::io_service io_service;
	boost::intrusive_ptr<matchmaking_server> server(new matchmaking_server(io_service, 29543));

	server->executeCommand(commands);

	while(db->process()) {
	}
}

#else

class matchmaking_server : public game_logic::FormulaCallable, public http::web_server
{
	DECLARE_CALLABLE(matchmaking_server);
};


BEGIN_DEFINE_CALLABLE_NOBASE(matchmaking_server)
DEFINE_FIELD(response, "any")
	return variant();
DEFINE_SET_FIELD
DEFINE_FIELD(db_client, "builtin DbClient")
	return variant();
END_DEFINE_CALLABLE(matchmaking_server)

#endif // __linux__
