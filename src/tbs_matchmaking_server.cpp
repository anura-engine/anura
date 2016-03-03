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
#ifdef __linux__
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "asserts.hpp"
#include "db_client.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "formula_object.hpp"
#include "http_server.hpp"
#include "json_parser.hpp"
#include "md5.hpp"
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

using namespace tbs;

PREF_STRING(server_hostname, "theargentlark.com", "Hostname of the main tbs server");

namespace {

bool validateEmail(const std::string& email, std::string* message)
{
	if(email.size() > 64) {
		*message = "email too long";
		return false;
	}

	if(std::count(email.begin(), email.end(), '@') != 1) {
		*message = "multiple '@' characters";
		return false;
	}

	if(email.size() > 64) {
		*message = "Address too long";
		return false;
	}

	for(char c : email) {
		if(!isalnum(c) && c != '@' && c != '-' && c != '_' && c != '.') {
			*message = "Illegal characters";
			return false;
		}
	}

	return true;
}

void sendEmail(std::string email_addr, std::string subject, std::string message)
{
#ifdef __linux__

	std::ostringstream s;
	s << "/usr/sbin/sendmail " << email_addr;
	FILE* f = popen(s.str().c_str(), "w");

	fprintf(f, "From: %s\n", subject.c_str());
	fprintf(f, "Subject: %s\n\n", subject.c_str());
	fprintf(f, "%s\n", message.c_str());
	fflush(f);

	fclose(f);
#endif
}

struct RestartServerException {
	std::vector<char*> argv;
};

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

	if(username[0] == ' ' || username[username.size()-1] == ' ') {
		return false;
	}

	for(char c : username) {
		if(!isalnum(c) && c != '_' && c != ' ' && c != '^') {
			return false;
		}
	}

	return true;
}

std::string generate_beta_key()
{
	std::string result = write_uuid(generate_uuid());
	result.resize(5);
	for(char& c : result) {
		c = toupper(c);
	}
	return result;
}

PREF_STRING(beta_keys_file, "", "File to store beta keys in (default = no beta keys)");
PREF_INT(matchmaking_heartbeat_ms, 50, "Frequency of matchmaking heartbeats");

class matchmaking_server : public game_logic::FormulaCallable, public http::web_server

{
	struct SessionInfo;
public:
	matchmaking_server(boost::asio::io_service& io_service, int port)
	  : http::web_server(io_service, port),
	    io_service_(io_service), port_(port),
		timer_(io_service), db_timer_(io_service),
		time_ms_(0), send_at_time_ms_(1000), terminated_servers_(0),
		controller_(game_logic::FormulaObject::create("matchmaking_server")),
		status_doc_state_id_(1),
		child_admin_process_(-1),
		gen_game_id_(0)
	{
		variant_builder status_doc;
		status_doc.add("type", "server_state");
		status_doc.add("state_id", variant(status_doc_state_id_));
		status_doc.add("users", 0);
		status_doc.add("users_queued", 0);
		status_doc.add("games", 0);

		std::vector<variant> empty_list;
		status_doc.add("servers", variant(&empty_list));
		status_doc.add("user_list", variant(&empty_list));
		status_doc.add("chat", variant(&empty_list));

		status_doc_ = status_doc.build();

		create_account_fn_ = controller_->queryValue("create_account");
		ASSERT_LOG(create_account_fn_.is_function(), "Could not find create_account in matchmaking_server class");

		read_account_fn_ = controller_->queryValue("read_account");
		ASSERT_LOG(read_account_fn_.is_function(), "Could not find read_account in matchmaking_server class");

		handle_request_fn_ = controller_->queryValue("handle_request");
		ASSERT_LOG(handle_request_fn_.is_function(), "Could not find handle_request in matchmaking_server class");

		handle_game_over_message_fn_ = controller_->queryValue("handle_game_over_message");
		ASSERT_LOG(handle_game_over_message_fn_.is_function(), "Could not find handle_game_over_message in matchmaking_server class");

		process_account_fn_ = controller_->queryValue("process_account");
		ASSERT_LOG(process_account_fn_.is_function(), "Could not find process_account in matchmaking_server class");

		matchmake_fn_ = controller_->queryValue("matchmake");
		ASSERT_LOG(matchmake_fn_.is_function(), "Could not find matchmake function in matchmaking_server class");

		admin_account_fn_ = controller_->queryValue("admin_account");
		ASSERT_LOG(admin_account_fn_.is_function(), "Could not find admin_account in matchmaking_server class");

		user_account_fn_ = controller_->queryValue("user_account");
		ASSERT_LOG(user_account_fn_.is_function(), "Could not find user_account in matchmaking_server class");

		handle_anon_request_fn_ = controller_->queryValue("handle_anon_request");

		db_client_ = DbClient::create();
		db_client_->get("gen_game_id", [=](variant user_info) {
			this->gen_game_id_ = user_info.as_int(1);
		});

		db_timer_.expires_from_now(boost::posix_time::milliseconds(10));
		db_timer_.async_wait(boost::bind(&matchmaking_server::db_process, this, boost::asio::placeholders::error));

		timer_.expires_from_now(boost::posix_time::milliseconds(1000));
		timer_.async_wait(boost::bind(&matchmaking_server::heartbeat, this, boost::asio::placeholders::error));

		for(int i = 0; i != 256; ++i) {
			available_ports_.push_back(21156+i);
		}

		if(g_beta_keys_file.empty() == false) {
			if(sys::file_exists(g_beta_keys_file)) {
				variant v = json::parse(sys::read_file(g_beta_keys_file));
				for(auto p : v.as_map()) {
					beta_key_info_[p.first.as_string()] = p.second;
				}
			}
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
#if !defined(_MSC_VER)
		int pid_status = 0;
		const pid_t pid = waitpid(-1, &pid_status, WNOHANG);
		if(pid < 0) {
			if(errno != ECHILD) {
				switch(errno) {
				case EINVAL: fprintf(stderr, "waitpid() had invalid arguments\n"); break;
				default: fprintf(stderr, "waitpid() returns unknown error: %d\n", errno); break;
				}
			}
		} else if(pid == child_admin_process_) {
			child_admin_process_ = -1;
		} else if(pid > 0) {
			auto itor = servers_.find(pid);
			if(itor != servers_.end()) {
				//This will only happen if a server exited without reporting
				//a game result.
				available_ports_.push_back(itor->second.port);
				remove_game_server(itor->second.port);
				servers_.erase(pid);
				++terminated_servers_;

				fprintf(stderr, "Child server exited without a result. %d servers running\n", (int)servers_.size());
			}
		}
#endif //!defined(_MSC_VER)

		time_ms_ += g_matchmaking_heartbeat_ms;

		if(time_ms_ >= send_at_time_ms_ && send_at_time_ms_ != -1) {
			send_at_time_ms_ += 1000;

			update_status_doc();

			const int nqueue_size = check_matchmaking_queue();

			variant heartbeat_message_value = status_doc_;

			for(auto& p : sessions_) {
				if(p.second.current_socket && (p.second.sent_heartbeat == false || time_ms_ - p.second.last_contact >= 3000 || p.second.have_state_id < status_doc_state_id_)) {

					variant_builder msg;

					msg.add("type", "heartbeat");

					if(p.second.request_server_info) {

						if(p.second.have_state_id != status_doc_state_id_) {
							variant delta = build_status_delta(p.second.have_state_id);
							if(delta.is_null()) {
								msg.add("server_info", status_doc_);
							} else {
								msg.add("server_info", delta);
							}
						}

						auto user_info_itor = user_info_.find(p.second.user_id);
						bool added_game_details = false;
						fprintf(stderr, "SEARCH FOR USER: %s -> %s\n", p.second.user_id.c_str(), user_info_itor != user_info_.end() ? "FOUND" : "UNFOUND");
						if(user_info_itor != user_info_.end() && user_info_itor->second.game_pid != -1) {
							auto game_itor = servers_.find(user_info_itor->second.game_pid);
						fprintf(stderr, "SEARCH FOR GAME: %d -> %s\n", user_info_itor->second.game_pid, game_itor != servers_.end() ? "FOUND" : "UNFOUND");

							if(game_itor != servers_.end() && std::find(game_itor->second.users_list.begin(), game_itor->second.users_list.end(), p.second.user_id) != game_itor->second.users_list.end() && game_itor->second.game_id != -1) {
								fprintf(stderr, "SEARCH FOUND SEND\n");
								msg.add("game_port", game_itor->second.port);
								msg.add("game_id", game_itor->second.game_id);
								msg.add("game_session", user_info_itor->second.game_session);
								added_game_details = true;
							}
						}
					}

					send_msg(p.second.current_socket, "text/json", msg.build().write_json(), "");

					p.second.last_contact = time_ms_;
					p.second.current_socket = socket_ptr();
					p.second.sent_heartbeat = true;
					p.second.have_state_id = status_doc_state_id_;
				} else if(!p.second.current_socket && time_ms_ - p.second.last_contact >= 10000) {
					p.second.session_id = 0;
				}
			}

			for(auto itor = sessions_.begin(); itor != sessions_.end(); ) {
				if(itor->second.session_id == 0) {
					auto i = users_to_sessions_.find(itor->second.user_id);
					if(i != users_to_sessions_.end() && i->second == itor->first) {
						users_to_sessions_.erase(i);
					}

					remove_logged_in_user(itor->second.user_id);
					sessions_.erase(itor++);
				} else {
					++itor;
				}
			}

			for(auto i = sessions_.begin(); i != sessions_.end(); ++i) {
				SessionInfo& session = i->second;

				auto account_itor = account_info_.find(session.user_id);
				if(account_itor == account_info_.end()) {
					continue;
				}

				if(++session.send_process_counter >= 1) {
					session.send_process_counter = 0;

					std::vector<variant> args;
					args.push_back(variant(this));
					args.push_back(variant(session.user_id));
					args.push_back(account_itor->second["info"]);

					variant cmd = process_account_fn_(args);
					executeCommand(cmd);
				}
			}
		}

		timer_.expires_from_now(boost::posix_time::milliseconds(g_matchmaking_heartbeat_ms));
		timer_.async_wait(boost::bind(&matchmaking_server::heartbeat, this, boost::asio::placeholders::error));
	}

#define RESPOND_CUSTOM_MESSAGE(type, msg) { \
					variant_builder response; \
					response.add("type", type); \
					response.add("message", msg); \
					response.add("timestamp", static_cast<int>(time(NULL))); \
					send_response(socket, response.build()); \
				}

#define RESPOND_ERROR(msg) { \
					variant_builder response; \
					response.add("type", "error"); \
					response.add("message", msg); \
					response.add("timestamp", static_cast<int>(time(NULL))); \
					send_response(socket, response.build()); \
				}

#define RESPOND_MESSAGE(msg) { \
					variant_builder response; \
					response.add("type", "message"); \
					response.add("message", msg); \
					response.add("timestamp", static_cast<int>(time(NULL))); \
					send_response(socket, response.build()); \
				}

	void handlePost(socket_ptr socket, variant doc, const http::environment& env, const std::string& raw_msg) override
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

		const int session_id = doc["session_id"].as_int(request_session_id);

		try {
			fprintf(stderr, "HANDLE POST: %s\n", doc.write_json().c_str());

			assert_recover_scope recover_scope;
			std::string request_type = doc["type"].as_string();

			if(request_type == "anon_request") {

				if(handle_anon_request_fn_.is_function()) {
					std::vector<variant> args;
					args.push_back(variant(this));
					args.push_back(doc);
					send_response(socket, handle_anon_request_fn_(args));
				} else {
					send_response(socket, variant());
				}
				return;
			} else if(request_type == "register") {
				std::string user = normalize_username(doc["user"].as_string());
				if(user.size() > 16) {
					RESPOND_ERROR("Username may not be more than 12 characters");
					return;
				}

				if(!username_valid(user)) {
					RESPOND_ERROR("Not a valid username");
					return;
				}

				std::string beta_key;
				if(g_beta_keys_file.empty() == false) {
					if(!doc["beta_key"].is_string()) {
						RESPOND_ERROR("Must specify a beta key");
						return;
					}

					beta_key = doc["beta_key"].as_string();

					for(char& c : beta_key) {
						c = toupper(c);
					}
				}

				std::vector<variant> args;
				args.push_back(doc);
				variant account_info = create_account_fn_(args);

				std::string email_address = doc["email"].as_string_default("");
				if(email_address.empty() == false) {
					std::string message;
					bool valid = validateEmail(email_address, &message);
					if(!valid) {
						RESPOND_ERROR("Invalid email address: " + message);
						return;
					}
				}

				std::string user_full = doc["user"].as_string();

				std::string passwd = doc["passwd"].as_string();
				const bool remember = doc["remember"].as_bool(false);
				db_client_->get("user:" + user, [=](variant user_info) {
					if(user_info.is_null() == false) {
						RESPOND_ERROR("That username is already taken");
						return;
					}

					std::string beta_error;
					if(g_beta_keys_file.empty() == false && !can_redeem_beta_key(beta_key, beta_error)) {
						RESPOND_ERROR(beta_error);
						return;
					}

					variant_builder new_user_info;
					new_user_info.add("user", user_full);
					new_user_info.add("passwd", passwd);
					new_user_info.add("info_version", variant(0));
					new_user_info.add("info", account_info);

					if(std::find(email_address.begin(), email_address.end(), '@') != email_address.end()) {
						new_user_info.add("email", email_address);
					}

					variant new_user_info_variant = new_user_info.build();

					//put the new user in the database. Note that we use
					//PUT_REPLACE so that if there is a race for two users
					//to register the same name only one will succeed.
					db_client_->put("user:" + user, new_user_info_variant,
					[=]() {
						add_logged_in_user(user);
						
						const int session_id = g_session_id_gen++;
						SessionInfo& info = sessions_[session_id];
						info.session_id = session_id;
						info.user_id = user;
						info.last_contact = this->time_ms_;

						users_to_sessions_[user] = session_id;

						this->account_info_[user] = new_user_info_variant;

						if(g_beta_keys_file.empty() == false) {
							redeem_beta_key(beta_key, user_full);
						}

						variant_builder response;
						response.add("type", "registration_success");
						response.add("session_id", variant(session_id));
						response.add("username", variant(user_full));
						response.add("info_version", variant(0));
						response.add("info", account_info);
						response.add("timestamp", static_cast<int>(time(NULL)));

						if(remember) {
							std::string cookie = write_uuid(generate_uuid());
							response.add("cookie", cookie);
	
							variant_builder cookie_info;
							cookie_info.add("user", user);
							db_client_->put("cookie:" + cookie, cookie_info.build(),
							                [](){}, [](){});
						}

						send_response(socket, response.build());

						if(email_address.empty() == false) {
							const std::string email_key = "email:" + email_address;
							db_client_->get(email_key, [=](variant email_info) {
								std::vector<variant> accounts;
								if(email_info.is_list()) {
									accounts = email_info.as_list();
								}

								accounts.push_back(variant(user_full));
								db_client_->put(email_key, variant(&accounts),
							            [](){}, [](){});
							});
						}
					},
					[=]() {
						RESPOND_ERROR("There was an error with registering. Please try again.");
					}, DbClient::PUT_ADD);
				});

				

			} else if(request_type == "login") {
				std::string given_user = doc["user"].as_string();
				std::string user = normalize_username(given_user);
				std::string passwd = doc["passwd"].as_string();
				const bool remember = doc["remember"].as_bool(false);
				bool impersonate = false;
				if(doc.has_key("impersonate")) {
					std::string override_pass = sys::read_file("./impersonation-pass");
					override_pass.erase(std::remove(override_pass.begin(), override_pass.end(), '\n'), override_pass.end());
					if(override_pass != "" && override_pass == doc["impersonate"].as_string()) {
						impersonate = true;
					}
				}

				db_client_->get("user:" + user, [=](variant user_info) {
					if(user_info.is_null()) {
						RESPOND_CUSTOM_MESSAGE("login_fail", "That user doesn't exist");
						return;
					}

					std::string db_passwd = user_info["passwd"].as_string();
					if(passwd != db_passwd && !impersonate) {
						RESPOND_CUSTOM_MESSAGE("login_fail", "Incorrect password");
						return;

					}

					variant_builder response;

					repair_account(&user_info);

					this->account_info_[user] = user_info;

					add_logged_in_user(user);

					const int session_id = g_session_id_gen++;
					SessionInfo& info = sessions_[session_id];
					info.session_id = session_id;
					info.user_id = user;
					info.last_contact = this->time_ms_;

					users_to_sessions_[user] = session_id;

					response.add("type", "login_success");
					response.add("session_id", variant(session_id));
					response.add("username", given_user);
					response.add("info_version", user_info["info_version"].as_int(0));
					response.add("info", user_info["info"]);
					response.add("timestamp", static_cast<int>(time(NULL)));

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

					std::string username = normalize_username(user_info["user"].as_string());

					db_client_->get("user:" + username, [=](variant user_info) {
						variant_builder response;
						if(user_info.is_null()) {
							response.add("type", "auto_login_fail");
							send_response(socket, response.build());
							return;
						}

						repair_account(&user_info);
						this->account_info_[username] = user_info;

						add_logged_in_user(username);

						const int session_id = g_session_id_gen++;
						SessionInfo& info = sessions_[session_id];
						info.session_id = session_id;
						info.user_id = username;
						info.last_contact = this->time_ms_;

						users_to_sessions_[username] = session_id;
	
						response.add("type", "login_success");
						response.add("session_id", variant(session_id));
						response.add("cookie", variant(cookie));
						response.add("username", user_info["user"]);
						response.add("info", user_info["info"]);
						response.add("info_version", user_info["info_version"].as_int(0));
						response.add("timestamp", static_cast<int>(time(NULL)));

						send_response(socket, response.build());
					});
				});
			} else if(request_type == "recover_account") {
				std::string given_user = doc["user"].as_string();
				std::string user = normalize_username(given_user);
				db_client_->get("user:" + user, [=](variant user_info) {
					if(user_info.is_null()) {
						RESPOND_ERROR("That user doesn't exist");
						return;
					}

					variant email = user_info["email"];
					if(email.is_null()) {
						RESPOND_ERROR("There is no email address associated with this account");
						return;
					}

					auto existing = user_id_to_recover_account_requests_.find(user);
					if(existing != user_id_to_recover_account_requests_.end()) {
						recover_account_requests_.erase(existing->second);
						user_id_to_recover_account_requests_.erase(existing);
					}

					std::string request_id = write_uuid(generate_uuid());
					request_id.resize(8);

					recover_account_requests_[request_id] = user;
					user_id_to_recover_account_requests_[user] = request_id;

					std::ostringstream msg;
					msg << "We have received a request to reset the password on your " << module::get_module_pretty_name() << " account. To reset your password please visit this URL: http://" << g_server_hostname << ":" << port_ << "/reset_password?user=" << user << "&id=" << request_id;

					sendEmail(email.as_string(), formatter() << "Reset your " << module::get_module_pretty_name() << " password", msg.str());

					RESPOND_MESSAGE("You have been sent an email to reset your password!");
				});

			} else if(request_type == "get_server_info") {
				static const std::string server_info = get_server_info_file().write_json();
				send_msg(socket, "text/json", server_info, "");
			} else if(request_type == "reset_passwd") {
				if(sessions_.count(session_id) == 0) {
					RESPOND_ERROR("Invalid session ID");
					return;
				}

				std::string passwd = doc["passwd"].as_string();

				SessionInfo& info = sessions_[session_id];
				db_client_->get("user:" + info.user_id, [=](variant user_info) {
					user_info.add_attr_mutation(variant("passwd"), variant(passwd));
					db_client_->put("user:" + info.user_id, user_info,
					[=]() {
						RESPOND_MESSAGE("Your password has been reset.");
					}, 
					[=]() {
						RESPOND_ERROR("There was an error with resetting the password. Please try again.");
					},
					DbClient::PUT_REPLACE);
				});

			} else if(request_type == "modify_account") {

				if(sessions_.count(session_id) == 0) {
					RESPOND_ERROR("Invalid session ID");
					return;
				}

				SessionInfo& info = sessions_[session_id];

				std::string user = info.user_id;

				variant v = account_info_[user];
				if(v.is_null()) {
					RESPOND_ERROR("Could not find user info");
					return;
				}

				std::string passwd = doc["passwd"].as_string();

				if(passwd != v["passwd"].as_string()) {
					RESPOND_ERROR("Invalid password");
					return;
				}

				if(doc["email"].is_string()) {
					const std::string email = doc["email"].as_string();
					std::string message;
					bool valid = validateEmail(email, &message);
					if(!valid) {
						RESPOND_ERROR("Invalid email: " + message);
						return;
					}
				}

				db_client_->put("user:" + user, v, [](){}, [](){});

				RESPOND_MESSAGE("Your account has been modified");

			} else if(request_type == "delete_account") {


				if(sessions_.count(session_id) == 0) {
					RESPOND_MESSAGE("Invalid session ID");
					return;
				}

				SessionInfo& info = sessions_[session_id];

				db_client_->remove("user:" + info.user_id);

				variant_builder response;
				response.add("type", "account_deleted");
				send_response(socket, response.build());

				remove_logged_in_user(info.user_id);
				sessions_.erase(session_id);

			} else if(request_type == "quit_game") {

				if(sessions_.count(session_id) == 0) {
					RESPOND_MESSAGE("Invalid session ID");
					return;
				}

				SessionInfo& info = sessions_[session_id];

				variant_builder response;
				response.add("type", "quit_ack");
				send_response(socket, response.build());

				fprintf(stderr, "GOT QUIT: %s\n", info.user_id.c_str());
				auto i = users_to_sessions_.find(info.user_id);
				if(i != users_to_sessions_.end() && i->second == session_id) {
					users_to_sessions_.erase(i);
				}

				remove_logged_in_user(info.user_id);
				sessions_.erase(session_id);
				
			} else if(request_type == "cancel_matchmake") {

				if(sessions_.count(session_id) == 0) {
					RESPOND_MESSAGE("Invalid session ID");
					return;
				}

				SessionInfo& info = sessions_[session_id];
				info.session_id = session_id;
				info.last_contact = time_ms_;
				info.queued_for_game = false;

				change_user_status(info.user_id, "idle");

				std::map<variant,variant> response;
				response[variant("type")] = variant("matchmaking_cancelled");
				response[variant("session_id")] = variant(session_id);
				send_msg(socket, "text/json", variant(&response).write_json(), "");

			} else if(request_type == "challenge") {

				if(sessions_.count(session_id) == 0) {
					RESPOND_MESSAGE("Invalid session ID");
					return;
				}

				SessionInfo& info = sessions_[session_id];
				info.session_id = session_id;
				info.last_contact = time_ms_;

				std::string user = doc["user"].as_string();

				fprintf(stderr, "CCC: Challenge received: vs: %s\n", user.c_str());


				{
					//see if the other player already made a challenge in which
					//case we just accept it here.
					MatchChallengePtr challenge;
					SessionInfo& info = sessions_[session_id];
					for(auto c : info.challenges_received) {
						if(c->challenger == user) {
							challenge = c;
							break;
						}
					}

					auto challenger_it = sessions_.end();
					if(challenge.get() != nullptr) {
						challenger_it = sessions_.find(challenge->challenger_session);
						if(challenger_it != sessions_.end()) {
							if(std::find(challenger_it->second.challenges_made.begin(), challenger_it->second.challenges_made.end(), challenge) == challenger_it->second.challenges_made.end()) {
								//can't find the challenge, it must have been canceled.
								challenge.reset();
							}
						}
					}

					if(challenger_it != sessions_.end()) {

						std::map<variant,variant> response;
						response[variant("type")] = variant("challenge_queued");
						response[variant("session_id")] = variant(session_id);
						send_msg(socket, "text/json", variant(&response).write_json(), "");

						fprintf(stderr, "CCC: Challenge match made!\n");
						std::vector<int> match_sessions;
						match_sessions.push_back(challenger_it->first);
						match_sessions.push_back(session_id);
						begin_match(match_sessions);
						return;
					}
				}

				std::vector<std::map<int, SessionInfo>::iterator> opponent;

				for(auto it = sessions_.begin(); it != sessions_.end(); ++it) {
					if(it->second.user_id == user) {
						opponent.push_back(it);
					}
				}

				std::map<variant,variant> response;

				if(opponent.empty()) {
					response[variant("type")] = variant("challenge_failed");
				} else {
					response[variant("type")] = variant("challenge_queued");
				}
				response[variant("session_id")] = variant(session_id);
				send_msg(socket, "text/json", variant(&response).write_json(), "");

				if(opponent.empty()) {
					return;
				}


				MatchChallengePtr challenge(new MatchChallenge);
				challenge->challenger_session = session_id;
				challenge->challenger = info.user_id;
				challenge->challenged = user;
				challenge->game_type_info = doc["game_type_info"];

				info.challenges_made.push_back(challenge);

				for(auto it : opponent) {
					it->second.challenges_received.push_back(challenge);
					if(it->second.current_socket) {
						std::map<variant,variant> msg;
						msg[variant("type")] = variant("challenge");
						msg[variant("challenger")] = variant(info.user_id);
						send_msg(it->second.current_socket, "text/json", variant(&msg).write_json(), "");
						it->second.current_socket.reset();
						challenge->received = true;
					}
				}


			} else if(request_type == "matchmake") {

				if(sessions_.count(session_id) == 0) {
					RESPOND_MESSAGE("Invalid session ID");
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

				change_user_status(info.user_id, "queued");

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

					msg.add("timestamp", variant(static_cast<int>(time(NULL))));

					variant v = msg.build();

					add_chat_message(v);

					send_msg(socket, "text/json", "{ type: \"ack\" }", "");

					schedule_send(200);
				}

			} else if(request_type == "status_change") {
				int session_id = doc["session_id"].as_int(request_session_id);
				auto itor = sessions_.find(session_id);
				if(itor == sessions_.end()) {
					fprintf(stderr, "Error: Unknown session: %d\n", session_id);
					send_msg(socket, "text/json", "{ type: \"error\", message: \"unknown session\" }", "");
				} else {
					if(doc["status"].is_string()) {
						std::string status = doc["status"].as_string();
						if(status != itor->second.status) {
							itor->second.status = doc["status"].as_string();
							change_user_status(itor->second.user_id, status);
							fprintf(stderr, "CHANGE USER STATUS: %s -> %s\n", itor->second.user_id.c_str(), status.c_str());
						}
					}
					send_msg(socket, "text/json", "{ type: \"ack\" }", "");
				}

			} else if(request_type == "request_observe") {
				int session_id = doc["session_id"].as_int(request_session_id);
				auto itor = sessions_.find(session_id);
				if(itor == sessions_.end()) {
					RESPOND_ERROR("unknown session");
					return;
				}

				SessionInfo* target = get_session(doc["target_user"].as_string());
				if(!target) {
					RESPOND_ERROR(formatter() << "User " << doc["target_user"].as_string() << " is no longer online");
					return;
				}

				variant_builder builder;
				builder.add("type", "request_observe");
				builder.add("requester", itor->second.user_id);

				queue_message(*target, builder.build());

				RESPOND_MESSAGE(formatter() << "Sent request to " << doc["target_user"].as_string() << " to observe their game");
				
			} else if(request_type == "allow_observe") {

				static int relay_session = 100000;
				++relay_session;

				int session_id = doc["session_id"].as_int(request_session_id);
				auto itor = sessions_.find(session_id);
				if(itor == sessions_.end()) {
					RESPOND_ERROR("No session");
					return;
				}

				SessionInfo* target = get_session(doc["requester"].as_string());
				if(target == nullptr) {
					RESPOND_ERROR(formatter() << doc["requester"].as_string() << " is no longer online");
					return;
				}

				{
					LOG_INFO("SEND CONNECT RELAY");
					variant_builder builder;
					builder.add("type", "connect_relay_server");
					builder.add("relay_session", relay_session);
					send_response(socket, builder.build());
				}

				{
					variant_builder builder;
					builder.add("type", "grant_observe");
					builder.add("relay_session", relay_session);
					queue_message(*target, builder.build());
				}

			} else if(request_type == "deny_observe") {

				int session_id = doc["session_id"].as_int(request_session_id);
				auto itor = sessions_.find(session_id);

				if(itor != sessions_.end()) {
					SessionInfo* target = get_session(doc["requester"].as_string());
					if(target) {
						variant_builder response;
						response.add("type", "message");
						response.add("message", formatter() << itor->second.user_id << " has declined your request to observe their game");
						response.add("timestamp", static_cast<int>(time(NULL)));
						queue_message(*target, response.build());
					}
				}

				send_msg(socket, "text/json", "{ type: \"ack\" }", "");

			} else if(request_type == "request_updates") {

				int session_id = doc["session_id"].as_int(request_session_id);
				auto itor = sessions_.find(session_id);
				if(itor == sessions_.end()) {
					fprintf(stderr, "Error: Unknown session: %d\n", session_id);
					send_msg(socket, "text/json", "{ type: \"error\", message: \"unknown session\" }", "");
				} else {
					users_to_sessions_[itor->second.user_id] = session_id;

					if(doc["status"].is_string()) {
						std::string status = doc["status"].as_string();
						if(status != itor->second.status) {
							itor->second.status = doc["status"].as_string();
							change_user_status(itor->second.user_id, status);
							fprintf(stderr, "CHANGE USER STATUS: %s -> %s\n", itor->second.user_id.c_str(), status.c_str());
						}
					}

					variant state_id_var = doc["state_id"];
					if(state_id_var.is_int()) {
						itor->second.have_state_id = state_id_var.as_int();
					}

					itor->second.request_server_info = doc["request_server_info"].as_bool(true);

					itor->second.last_contact = time_ms_;

					const int has_version = doc["info_version"].as_int(-1);
					bool send_new_version = false;
					if(has_version != -1) {
						auto account_itor = account_info_.find(itor->second.user_id);
						if(account_itor != account_info_.end() && account_itor->second["info_version"].as_int(0) != has_version) {
							send_new_version = true;

							variant_builder doc;
							doc.add("type", "account_info");
							doc.add("info", account_itor->second["info"]);
							doc.add("info_version", account_itor->second["info_version"]);
							send_msg(socket, "text/json", doc.build().write_json(), "");
						}
					}

					if(send_new_version) {
						//nothing, already done above
					} else if(itor->second.game_details != "" && !itor->second.game_pending) {
						send_msg(socket, "text/json", itor->second.game_details, "");
						itor->second.game_details = "";
					} else {

						for(auto challenge : itor->second.challenges_received) {
							if(challenge->received == false) {

								std::map<variant,variant> msg;
								msg[variant("type")] = variant("challenge");
								msg[variant("challenger")] = variant(challenge->challenger);
								send_msg(socket, "text/json", variant(&msg).write_json(), "");
								challenge->received = true;
								return;
							}
						}

						if(itor->second.message_queue.empty() == false) {
							send_msg(socket, "text/json", itor->second.message_queue.front().write_json(), "");
							itor->second.message_queue.pop_front();
							return;
						}

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
					variant info = doc["info"];
					if(info.is_map()) {
						std::vector<variant> args;
						args.push_back(variant(this));
						args.push_back(info);

						variant cmd = handle_game_over_message_fn_(args);
						executeCommand(cmd);
					}

					available_ports_.push_back(itor->second.port);
					remove_game_server(itor->second.port);
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
			} else if(request_type == "user_operation") {
				int session_id = doc["session_id"].as_int(request_session_id);
				auto itor = sessions_.find(session_id);
				if(itor == sessions_.end()) {
					fprintf(stderr, "Error: Unknown session: %d\n", session_id);
					send_msg(socket, "text/json", "{ type: \"error\", message: \"unknown session\" }", "");
				} else {
					handleUserPost(socket, doc, itor->second);

				}

			} else if(request_type == "admin_operation") {
				int session_id = doc["session_id"].as_int(request_session_id);
				auto itor = sessions_.find(session_id);
				if(itor == sessions_.end()) {
					fprintf(stderr, "Error: Unknown session: %d\n", session_id);
					send_msg(socket, "text/json", "{ type: \"error\", message: \"unknown session\" }", "");
				} else {
					const SessionInfo& session = itor->second;
					auto account_itor = account_info_.find(session.user_id);
					if(account_itor == account_info_.end()) {
						fprintf(stderr, "Error: Unknown account: %s\n", session.user_id.c_str());
						send_msg(socket, "text/json", "{ type: \"error\", message: \"unknown account\" }", "");
					} else {
						variant privileged = account_itor->second["info"]["privileged"];
						if(!privileged.is_bool() || privileged.as_bool() != true) {
							fprintf(stderr, "Error: Unprivileged account account: %s\n", session.user_id.c_str());
							send_msg(socket, "text/json", "{ type: \"error\", message: \"account does not have admin privileges\" }", "");
						} else {
							handleAdminPost(socket, doc);
						}
					}

				}
			} else if(request_type == "get_replay") {

				std::string game_id = doc["id"].as_string();
				db_client_->get("replay:" + game_id, [=](variant user_info) {
					user_info.add_attr_mutation(variant("type"), variant("replay"));
					send_msg(socket, "text/json", user_info.write_json(), "");
				});
			} else if(request_type == "get_recent_games") {
				if(doc["user"].is_string()) {
					std::string user = doc["user"].as_string();
					queryUserGameInfo(user, socket);
					return;
				}

				std::vector<std::string> id;
				for(int i = gen_game_id_-1; i >= 1 && i > gen_game_id_-10; --i) {
					id.push_back(formatter() << i);
				}

				queryGameInfo(id, socket);
				return;

			} else {
				int session_id = doc["session_id"].as_int(request_session_id);
				auto itor = sessions_.find(session_id);
				if(itor == sessions_.end()) {
					fprintf(stderr, "Error: Unknown session: %d\n", session_id);
					send_msg(socket, "text/json", "{ type: \"error\", message: \"unknown session\" }", "");
				} else {

					auto account_itor = account_info_.find(itor->second.user_id);

					if(account_itor == account_info_.end()) {
						fprintf(stderr, "Error: Unknown user: %s / %d\n", itor->second.user_id.c_str(), (int)account_info_.size());
						send_msg(socket, "text/json", "{ type: \"error\", message: \"unknown user\" }", "");
					} else {
						std::vector<variant> args;
						args.push_back(variant(this));
						args.push_back(doc);
						args.push_back(variant(itor->second.user_id));
						args.push_back(account_itor->second["info"]);

						variant cmd = handle_request_fn_(args);
						executeCommand(cmd);

						send_msg(socket, "text/json", current_response_.write_json(), "");
						current_response_ = variant();
					}
				}
			}

		} catch(validation_failure_exception& error) {
			fprintf(stderr, "ERROR HANDLING POST: %s\n", error.msg.c_str());
			disconnect(socket);
		}
	}

	void handleUserPost(socket_ptr socket, variant doc, const SessionInfo& session)
	{
		auto account_itor = account_info_.find(session.user_id);
		if(account_itor == account_info_.end()) {
			fprintf(stderr, "Error: Unknown account: %s\n", session.user_id.c_str());
			send_msg(socket, "text/json", "{ type: \"error\", message: \"unknown account\" }", "");
		} else {

			std::vector<variant> v;
			v.push_back(variant(this));
			v.push_back(variant(session.user_id));
			v.push_back(account_itor->second["info"]);
			v.push_back(doc);
			variant cmd = user_account_fn_(v);
			executeCommand(cmd);
		}
	}

	void handleAdminPost(socket_ptr socket, variant doc)
	{
		if(doc["msg"].is_map()) {
			std::string user = normalize_username(doc["msg"]["user"].as_string());
			db_client_->get("user:" + user, [=](variant user_info) {
				if(user_info.is_null()) {
					return;
				}

				repair_account(&user_info);

				account_info_[user] = user_info;

				std::vector<variant> v;
				v.push_back(variant(this));
				v.push_back(variant(user));
				v.push_back(user_info["info"]);
				v.push_back(doc["msg"]);
				variant cmd = admin_account_fn_(v);
				executeCommand(cmd);
			});

			return;
		}

#if !defined(_MSC_VER)
		if(child_admin_process_ != -1) {
			int status = 0;
			const int res = waitpid(child_admin_process_, &status, WNOHANG);
			fprintf(stderr, "FORK: waitpid -> %d\n", res);
			if(res != child_admin_process_) {
				fprintf(stderr, "FORK: BUSY\n");
				send_msg(socket, "text/json", "{ type: \"admin_busy\" }", "");
				return;
			}

			fprintf(stderr, "FORK: TERM PROC\n");

			child_admin_process_ = -1;
		}

		if(doc["get_command_output"].as_bool(false)) {
			fprintf(stderr, "FORK: get_command_output\n");
			variant_builder msg;
			msg.add("type", "admin_message");
			msg.add("complete", variant::from_bool(true));
			msg.add("message", sys::read_file("stdout_admin.txt"));

			send_msg(socket, "text/json", msg.build().write_json(), "");
			return;
		}

		const std::string script = doc["script"].as_string();
		bool valid_name = true;
		for(char c : script) {
			if(!isalnum(c)) {
				valid_name = false;
				break;
			}
		}

		if(!valid_name) {
			return;
		}

		const bool replace_process = doc["replace_process"].as_bool(false);

		const std::string command = "./server-admin-" + script + ".sh";

		std::vector<char*> argv;
		argv.push_back(strdup(command.c_str()));
		argv.push_back(nullptr);

		if(replace_process) {
			RestartServerException e;
			e.argv = argv;
			throw e;
		} else {
			const pid_t pid = fork();
			ASSERT_LOG(pid >= 0, "Could not fork process");

			if(pid == 0) {
				//FILE* fout = std::freopen("stdout_admin.txt","w", stdout);
				//FILE* ferr = std::freopen("stderr_admin.txt","w", stderr);
				//std::cerr.sync_with_stdio(true);

				execv(argv[0], &argv[0]);

				fprintf(stderr, "FORK: FAILED TO START COMMAND: %s\n", argv[0]);
				_exit(-1);
			}

			child_admin_process_ = static_cast<int>(pid);

			variant_builder msg;
			msg.add("type", "admin_message");
			msg.add("message", "Executing...");
			msg.add("timestamp", static_cast<int>(time(NULL)));
			send_msg(socket, "text/json", msg.build().write_json(), "");
		}

#endif
	}

	void queryGameInfo(const std::vector<std::string>& game_id, socket_ptr socket)
	{
		std::shared_ptr<int> request_count(new int(game_id.size()));
		std::shared_ptr<std::vector<variant>> results(new std::vector<variant>(game_id.size()));
		int index = 0;
		for(std::string id : game_id) {
			db_client_->get("game:" + id, [=](variant data) {
				(*results)[index] = data;
				--*request_count;

				if(*request_count == 0) {
					results->erase(std::remove(results->begin(), results->end(), variant()), results->end());
					variant_builder response;
					response.add("type", "recent_games");
					response.add("game_info", variant(results.get()));
					this->send_msg(socket, "text/json", response.build().write_json(), "");
				}
			});
			++index;
		}
	}

	void queryUserGameInfo(const std::string& user, socket_ptr socket)
	{
		db_client_->get("user:" + user, [=](variant info) {
			if(info.is_null()) {
				send_msg(socket, "text/json", "{ type: 'error', message: 'No such user'}", "");
				return;
			}

			variant recent_games = info["recent_games"];
			if(recent_games.is_list() == false || recent_games.num_elements() == 0) {
				//no games, send empty response
				variant_builder response;
				std::vector<variant> v;
				response.add("type", "recent_games");
				response.add("game_info", variant(&v));
				this->send_msg(socket, "text/json", response.build().write_json(), "");
				return;
			}

			this->queryGameInfo(recent_games.as_list_string(), socket);
		});
	}

	void handleGet(socket_ptr socket, const std::string& url, const std::map<std::string, std::string>& args) override
	{
		fprintf(stderr, "handleGet(%s)\n", url.c_str());
		if(url == "/tbs_monitor") {
			send_msg(socket, "text/json", build_status().write_json(), "");
		} else if(url == "/recent_games") {

			auto user = args.find("user");
			if(user != args.end()) {
				queryUserGameInfo(user->second, socket);
				return;
			}

			std::vector<std::string> id;
			for(int i = gen_game_id_-1; i >= 1 && i > gen_game_id_-10; --i) {
				id.push_back(formatter() << i);
			}

			queryGameInfo(id, socket);
			return;
		} else if(url == "/query" && handle_anon_request_fn_.is_function()) {
			std::map<variant,variant> a;
			for(auto p : args) {
				a[variant(p.first)] = variant(p.second);
			}

			std::vector<variant> fn_args;
			fn_args.push_back(variant(this));
			fn_args.push_back(variant(&a));
			send_response(socket, handle_anon_request_fn_(fn_args));
			return;
		} else if(url == "/generate_beta_key") {
			std::string key = get_beta_key();
			std::map<variant,variant> a;
			a[variant("key")] = variant(key);

			send_response(socket, variant(&a));
		} else if(url == "/beta_key_status") {
			std::map<variant,variant> v;
			for(auto p : beta_key_info_) {
				v[variant(p.first)] = p.second;
			}

			send_response(socket, variant(&v));
		} else if(url == "/reset_password") {
			auto user = args.find("user");
			auto id = args.find("id");
			if(user == args.end() || id == args.end() || recover_account_requests_.count(id->second) == 0 || recover_account_requests_.find(id->second)->second != user->second) {
				send_msg(socket, "text/plain", "Invalid or expired request", "");
				return;
			}

			std::string username = user->second;
			std::string recovery_id = id->second;

			db_client_->get("user:" + username, [=](variant user_info) {
				if(user_info.is_null()) {
					send_msg(socket, "text/plain", "Invalid or expired request", "");
					return;
				}

				std::string new_passwd = write_uuid(generate_uuid());
				new_passwd.resize(8);


				user_info.add_attr_mutation(variant("passwd"), variant(md5::sum(new_passwd)));
				db_client_->put("user:" + username, user_info, [](){}, [](){});

				send_msg(socket, "text/json", "Your account password has been reset. Your new password is " + new_passwd, "");

			});

			recover_account_requests_.erase(recovery_id);
			user_id_to_recover_account_requests_.erase(username);
		} else if(url == "/get_replay") {
			auto id = args.find("id");
			if(id == args.end()) {
				send_msg(socket, "text/plain", "Need id in arguments", "");
				return;
			}

			std::string game_id = id->second;

			db_client_->get("replay:" + game_id, [=](variant user_info) {
				send_msg(socket, "text/json", user_info.write_json(), "");
			});
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

		begin_match(match_sessions);

		return static_cast<int>(session_ids.size() - match_sessions.size());
	}

	void begin_match(const std::vector<int>& match_sessions)
	{
#if defined(_MSC_VER)
		return;
#else
		if(!available_ports_.empty()) {
			//spawn off a server to play this game.
			std::string fname = formatter() << "/tmp/anura_tbs_server." << match_sessions.front();
			std::string fname_out = formatter() << "/tmp/anura.out." << match_sessions.front();

			std::string game_id = (formatter() << gen_game_id_++);

			db_client_->put("gen_game_id", variant(gen_game_id_),
			[=]() {
			},
			[=]() {
			});

			variant_builder db_game_info;
			db_game_info.add("id", game_id);
			db_game_info.add("timestamp", static_cast<unsigned int>(time(nullptr)));

			variant_builder game;
			game.add("game_type", module::get_module_name());

			variant game_info;

			std::vector<std::string> users_list;
			
			std::vector<variant> users;
			for(int i : match_sessions) {
				SessionInfo& session_info = sessions_[i];
				session_info.game_pending = time_ms_;
				session_info.queued_for_game = false;

				session_info.challenges_made.clear();
				session_info.challenges_received.clear();

				auto account_info_itor = account_info_.find(session_info.user_id);
				ASSERT_LOG(account_info_itor != account_info_.end(), "Could not find user's account info: " << session_info.user_id);

				std::vector<variant> recent_games_var;
				variant recent_games = account_info_itor->second["recent_games"];
				if(recent_games.is_list()) {
					recent_games_var = recent_games.as_list();
				}
				recent_games_var.push_back(variant(game_id));
				account_info_itor->second.add_attr_mutation(variant("recent_games"), variant(&recent_games_var));

				variant_builder user;
				user.add("user", session_info.user_id);
				user.add("session_id", session_info.session_id);
				user.add("account_info", account_info_itor->second["info"]);
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

			db_game_info.add("players", vector_to_variant(users_list));

			db_client_->put("game:" + game_id, db_game_info.build(),
			[=]() {
			},
			[=]() {
			});

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
			args.push_back("--tbs-server-save-replay=" + game_id);
			args.push_back("--no-tbs-server");
			args.push_back("--quit-server-after-game");
			args.push_back("--utility=tbs_server");
			args.push_back("--port");
			args.push_back(formatter() << new_port);
			args.push_back("--config");
			args.push_back(fname);

			std::vector<char*> cstr_argv;
			for(std::string& s : args) {
				s.push_back('\0');
				cstr_argv.push_back(&s[0]);
			}

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

				add_game_server(new_port, users_list);

				for(const std::string& user : users_list) {
					user_info_[user].game_pid = pid;
				}
			}


		} else {
			fprintf(stderr, "ERROR: AVAILABLE PORTS EXHAUSTED\n");
		}

#endif //!_MSC_VER
	}

	variant build_status() const {

		variant_builder doc;

		doc.add("type", "server_status");
		doc.add("uptime", time_ms_/1000);
		doc.add("port", port_);
		doc.add("terminated_servers", terminated_servers_);
		doc.add("status_doc", status_doc_);

		std::map<variant,variant> logged_in_user_set;
		for(auto p : logged_in_user_set_) {
			logged_in_user_set.insert(std::pair<variant,variant>(variant(p.first), variant(p.second)));
		}

		doc.add("logged_in_user_set", variant(&logged_in_user_set));

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

	struct MatchChallenge {
		MatchChallenge() : challenger_session(-1), received(false) {}
		std::string challenger, challenged;
		int challenger_session;
		bool received;
		variant game_type_info;
	};

	typedef std::shared_ptr<MatchChallenge> MatchChallengePtr;

	struct SessionInfo {
		SessionInfo() : session_id(-1), status("idle"), last_contact(-1), game_pending(0), game_port(0), queued_for_game(false), sent_heartbeat(false), send_process_counter(60), messages_this_time_segment(0), time_segment(0), flood_mute_expires(0), have_state_id(-1), request_server_info(false) {}
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

		int send_process_counter;

		//record number of messages from this session so we can
		//disconnect for flooding if necessary.
		int messages_this_time_segment;
		int time_segment;
		int flood_mute_expires;

		int have_state_id;

		bool request_server_info;

		std::vector<MatchChallengePtr> challenges_made, challenges_received;

		std::deque<variant> message_queue;
	};

	std::map<int, SessionInfo> sessions_;
	std::map<std::string, int> users_to_sessions_;

	SessionInfo* get_session(const std::string& user_id) {
		auto itor = users_to_sessions_.find(user_id);
		if(itor == users_to_sessions_.end()) {
			return nullptr;
		}

		auto res = sessions_.find(itor->second);
		if(res == sessions_.end()) {
			return nullptr;
		}

		return &res->second;
	}

	void queue_message(SessionInfo& session, variant msg)
	{
		if(session.current_socket) {
			send_msg(session.current_socket, "text/json", msg.write_json(), "");
			session.current_socket = socket_ptr();
		} else {
			session.message_queue.push_back(msg);
		}
	}

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

	std::map<std::string, variant> account_info_;

	void repair_account(variant* input) const {
		variant info = (*input)["info"];
		std::vector<variant> args;
		args.push_back(info);
		input->add_attr_mutation(variant("info"), read_account_fn_(args));
	}

	//stats
	int terminated_servers_;

	boost::intrusive_ptr<game_logic::FormulaObject> controller_;
	variant create_account_fn_;
	variant read_account_fn_;
	variant process_account_fn_;
	variant handle_request_fn_;
	variant handle_game_over_message_fn_;
	variant matchmake_fn_;
	variant admin_account_fn_;
	variant user_account_fn_;
	variant handle_anon_request_fn_;

	variant current_response_;

	//the current list of players/servers/etc which is maintained.
	variant status_doc_;
	int status_doc_state_id_;

	std::map<std::string, int> logged_in_user_set_;

	std::deque<variant> status_doc_deltas_;

	std::vector<std::string> status_doc_new_users_;
	std::vector<std::string> status_doc_delete_users_;
	std::map<std::string,std::string> status_doc_user_status_changes_;
	std::vector<variant> status_doc_chat_messages_;

	std::vector<variant> status_doc_new_servers_;
	std::vector<int> status_doc_delete_servers_;

	variant build_status_delta(int have_state_id)
	{
		const int ndeltas = status_doc_state_id_ - have_state_id;
		if(ndeltas <= 0 || ndeltas > status_doc_deltas_.size()) {
			return variant();
		}

		std::vector<variant> deltas(status_doc_deltas_.end() - ndeltas, status_doc_deltas_.end());
		variant_builder v;
		v.add("type", "delta");
		v.add("deltas", variant(&deltas));
		v.add("state_id", variant(status_doc_state_id_)),
		v.add("users", status_doc_["users"]);
		v.add("users_queued", status_doc_["users_queued"]);
		v.add("games", status_doc_["games"]);
		return v.build();
	}

	bool update_status_doc()
	{
		if(status_doc_new_users_.empty() && status_doc_delete_users_.empty() && status_doc_user_status_changes_.empty() && status_doc_chat_messages_.empty() && status_doc_new_servers_.empty() && status_doc_delete_servers_.empty()) {
			return false;
		}

		int nusers_queued = 0;
		for(auto itor = sessions_.begin(); itor != sessions_.end(); ++itor) {
			if(itor->second.queued_for_game) {
				++nusers_queued;
			}
		}

		status_doc_.add_attr_mutation(variant("users_queued"), variant(nusers_queued));
		
		status_doc_.add_attr_mutation(variant("games"), variant(static_cast<int>(servers_.size())));

		status_doc_state_id_++;
		status_doc_.add_attr_mutation(variant("state_id"), variant(status_doc_state_id_));

		if(status_doc_new_users_.empty() == false || status_doc_delete_users_.empty() == false) {

			static variant IdVariant("id");

			std::vector<variant> list = status_doc_["user_list"].as_list();
			for(auto s : status_doc_delete_users_) {
				for(auto i = list.begin(); i != list.end(); ) {
					if((*i)[IdVariant].as_string() == s) {
						i = list.erase(i);
					} else {
						++i;
					}
				}
			}

			for(auto u : status_doc_new_users_) {
				bool already_present = false;
				for(auto i = list.begin(); i != list.end(); ++i) {
					if((*i)[IdVariant].as_string() == u) {
						already_present = true;
						break;
					}
				}

				if(already_present) {
					continue;
				}

				variant_builder builder;
				builder.add("id", u);
				builder.add("status", "idle");
				list.push_back(builder.build());
			}

			status_doc_.add_attr_mutation(variant("users"), variant(static_cast<int>(list.size())));

			status_doc_.add_attr_mutation(variant("user_list"), variant(&list));
		}

		if(status_doc_user_status_changes_.empty() == false) {
			variant users = status_doc_["user_list"];
			for(int n = 0; n != users.num_elements(); ++n) {
				const std::string& user_id = users[n]["id"].as_string();
				auto itor = status_doc_user_status_changes_.find(user_id);
				if(itor != status_doc_user_status_changes_.end()) {
					variant v = users[n];
					v.add_attr_mutation(variant("status"), variant(itor->second));
				}
			}
		}

		variant new_servers;

		if(status_doc_new_servers_.empty() == false) {
		 	new_servers = variant(&status_doc_new_servers_);
			variant servers = status_doc_["servers"] + new_servers;
			status_doc_.add_attr_mutation(variant("servers"), servers);
		}

		if(status_doc_delete_servers_.empty() == false) {
			std::vector<variant> servers = status_doc_["servers"].as_list();

			for(variant& v : servers) {
				if(std::find(status_doc_delete_servers_.begin(), status_doc_delete_servers_.end(), v["port"].as_int()) != status_doc_delete_servers_.end()) {
					v = variant();
				}
			}

			servers.erase(std::remove(servers.begin(), servers.end(), variant()), servers.end());

			status_doc_.add_attr_mutation(variant("servers"), variant(&servers));
		}


		variant chat;
		if(status_doc_chat_messages_.empty() == false) {
			chat = variant(&status_doc_chat_messages_);
		}

		if(chat.is_null() == false) {
			variant new_chat = status_doc_["chat"] + chat;
			if(new_chat.num_elements() > 24) {
				std::vector<variant> v = new_chat.as_list();
				v.erase(v.begin(), v.begin()+8);
				new_chat = variant(&v);
			}

			status_doc_.add_attr_mutation(variant("chat"), new_chat);
		}

		variant_builder delta;
		delta.add("state_id_basis", status_doc_state_id_-1);
		delta.add("state_id", status_doc_state_id_);

		if(status_doc_new_users_.empty() == false) {
			std::vector<variant> v;
			for(auto s : status_doc_new_users_) {
				v.push_back(variant(s));
			}

			delta.add("new_users", variant(&v));
		}

		if(status_doc_delete_users_.empty() == false) {
			std::vector<variant> v;
			for(auto s : status_doc_delete_users_) {
				v.push_back(variant(s));
			}

			delta.add("delete_users", variant(&v));
		}

		if(status_doc_user_status_changes_.empty() == false) {
			std::map<variant,variant> m;
			for(auto p : status_doc_user_status_changes_) {
				variant key(p.first);
				m[key] = variant(p.second);
			}

			delta.add("status_changes", variant(&m));
		}

		if(new_servers.is_null() == false) {
			delta.add("new_servers", new_servers);
		}

		if(status_doc_delete_servers_.empty() == false) {
			std::vector<variant> v;
			v.reserve(status_doc_delete_servers_.size());
			for(auto value : status_doc_delete_servers_) {
				v.push_back(variant(value));
			}

			delta.add("delete_servers", variant(&v));
		}

		if(chat.is_null() == false) {
			delta.add("chat", chat);
		}

		status_doc_deltas_.push_back(delta.build());
		while(status_doc_deltas_.size() > 8) {
			status_doc_deltas_.pop_front();
		}

		status_doc_new_users_.clear();
		status_doc_delete_users_.clear();
		status_doc_delete_servers_.clear();
		status_doc_user_status_changes_.clear();

		status_doc_.add_attr_mutation(variant("users_queued"), variant(nusers_queued));

		return true;
	}

	void add_logged_in_user(const std::string& user_id)
	{
		if(logged_in_user_set_[user_id]++ == 0) {
			schedule_send(500);
			auto itor = std::find(status_doc_delete_users_.begin(), status_doc_delete_users_.end(), user_id);
			if(itor != status_doc_delete_users_.end()) {
				status_doc_delete_users_.erase(itor);
			}
			status_doc_new_users_.push_back(user_id);
		}
	}

	void remove_logged_in_user(const std::string& user_id)
	{
		auto itor = logged_in_user_set_.find(user_id);
		if(itor != logged_in_user_set_.end()) {
			--itor->second;
			if(itor->second <= 0) {
				schedule_send(500);
				logged_in_user_set_.erase(itor);
				auto itor = std::find(status_doc_new_users_.begin(), status_doc_new_users_.end(), user_id);
				if(itor != status_doc_new_users_.end()) {
					status_doc_new_users_.erase(itor);
				}
				status_doc_delete_users_.push_back(user_id);
			}
		}
	}

	void add_game_server(int port, const std::vector<std::string>& users)
	{
		variant_builder b;
		b.add("port", port);
		b.add("users", vector_to_variant(users));
		status_doc_new_servers_.push_back(b.build());
	}

	void remove_game_server(int port)
	{
		status_doc_delete_servers_.push_back(port);
	}

	void change_user_status(const std::string& user_id, const std::string& status)
	{
		status_doc_user_status_changes_[user_id] = status;
	}

	void add_chat_message(variant v)
	{
		status_doc_chat_messages_.push_back(v);
	}


	int child_admin_process_;

	std::map<std::string, std::string> recover_account_requests_;
	std::map<std::string, std::string> user_id_to_recover_account_requests_;

	std::map<std::string, variant> beta_key_info_;
	std::vector<std::string> pending_beta_keys_;

	void save_beta_keys()
	{
		std::map<variant,variant> v;
		for(auto p : beta_key_info_) {
			v[variant(p.first)] = p.second;
		}

		for(auto s : pending_beta_keys_) {
			v[variant(s)] = variant();
		}

		sys::write_file(g_beta_keys_file, variant(&v).write_json());
	}

	std::string get_beta_key()
	{
		if(pending_beta_keys_.empty() == false) {
			std::string result = pending_beta_keys_.back();
			pending_beta_keys_.pop_back();
			return result;
		}

		for(int i = 0; i != 8; ++i) {
			std::string key = generate_beta_key();
			if(beta_key_info_.count(key)) {
				continue;
			}

			beta_key_info_[key] = variant();
			pending_beta_keys_.push_back(key);
		}

		if(pending_beta_keys_.empty()) {
			return "";
		}

		save_beta_keys();

		return get_beta_key();
	}

	void redeem_beta_key(const std::string& key, const std::string& username)
	{
		auto itor = beta_key_info_.find(key);
		if(itor != beta_key_info_.end()) {
			beta_key_info_[key] = variant(username);
			save_beta_keys();
		}
	}

	bool can_redeem_beta_key(const std::string& key, std::string& error)
	{
		auto itor = beta_key_info_.find(key);
		if(itor == beta_key_info_.end()) {
			error = "No such beta key";
			return false;
		} else if(itor->second.is_null() == false) {
			error = "Key already used";
			return false;
		} else {
			return true;
		}
	}

	int gen_game_id_;

	DECLARE_CALLABLE(matchmaking_server);
};

class write_account_command : public game_logic::CommandCallable
{
	DbClient& db_client_;
	std::string account_;
	mutable variant value_;
	bool silent_;
public:
	write_account_command(DbClient& db_client, const std::string& account, const variant& value, bool silent)
	  : db_client_(db_client), account_(account), value_(value), silent_(silent)
	{}

	void execute(game_logic::FormulaCallable& obj) const override {
		if(silent_ == false) {
			static const variant VersionVar("info_version");
			const int cur_version = value_[VersionVar].as_int(0);
			value_.add_attr_mutation(VersionVar, variant(cur_version+1));
		}
		db_client_.put("user:" + account_, value_, [](){}, [](){});
	}
};

BEGIN_DEFINE_CALLABLE_NOBASE(matchmaking_server)
DEFINE_FIELD(response, "any")
	return obj.current_response_;
DEFINE_SET_FIELD
	obj.current_response_ = value;
DEFINE_FIELD(db_client, "builtin db_client")
	return variant(obj.db_client_.get());
BEGIN_DEFINE_FN(get_account_info, "(string) ->map")
	const std::string& key = FN_ARG(0).as_string();

	auto itor = obj.account_info_.find(key);
	ASSERT_LOG(itor != obj.account_info_.end(), "Could not find user account: " << key);

	return itor->second["info"];
END_DEFINE_FN
BEGIN_DEFINE_FN(write_account, "(string, [string]|null=null) ->commands")
	const std::string& key = FN_ARG(0).as_string();

	bool silent_update = false;

	if(NUM_FN_ARGS >= 2) {
		variant flags = FN_ARG(1);
		if(flags.is_list()) {
			for(int i = 0; i != flags.num_elements(); ++i) {
				if(flags[i].as_string() == "silent") {
					silent_update = true;
				}
			}
		}
	}

	auto itor = obj.account_info_.find(key);
	ASSERT_LOG(itor != obj.account_info_.end(), "Could not find user account: " << key);

	return variant(new write_account_command(*obj.db_client_, key, itor->second, silent_update));
END_DEFINE_FN
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

PREF_BOOL(internal_tbs_matchmaking_server, false, "Run an in-process tbs matchmaking server");

void process_tbs_matchmaking_server()
{
	int port = 23456;
	if(g_internal_tbs_matchmaking_server) {
		static boost::asio::io_service io_service;
		static boost::intrusive_ptr<matchmaking_server> server(new matchmaking_server(io_service, port));
		io_service.poll();
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

	try {
		boost::asio::io_service io_service;
		boost::intrusive_ptr<matchmaking_server> server(new matchmaking_server(io_service, port));
		io_service.run();
	} catch(const RestartServerException& e) {
#if !defined(_MSC_VER)
		execv(e.argv[0], &e.argv[0]);
		ASSERT_LOG(false, "execv failed when restarting server: " << e.argv[0] << ": " << errno);
#endif
	}
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
