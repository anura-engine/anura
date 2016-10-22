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
#include <boost/interprocess/sync/named_semaphore.hpp>
#include <iostream>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula_object.hpp"
//#include "hex.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "shared_memory_pipe.hpp"
#include "string_utils.hpp"
#include "tbs_bot.hpp"
#include "tbs_server.hpp"
#include "tbs_web_server.hpp"
#include "unit_test.hpp"
#include "utils.hpp"
#include "variant.hpp"
#include "variant_utils.hpp"

namespace {
	PREF_STRING(tbs_server_semaphore, "", "");
	//PREF_BOOL(tbs_server_hexes, false, "Whether the tbs server should load hexes");
	boost::interprocess::named_semaphore* g_termination_semaphore;

#if defined(_MSC_VER)
	const std::string shared_sem_name = "anura_tbs_sem";
#else
	const std::string shared_sem_name = "/anura_tbs_sem";
#endif

	std::string get_semaphore_name(std::string id) {
		return shared_sem_name + id + g_tbs_server_semaphore;
	}
}

namespace tbs 
{
	namespace 
	{
		boost::asio::io_service* g_service;
		int g_listening_port = -1;
		web_server* web_server_instance = nullptr;
	}

	std::string global_debug_str;

	using boost::asio::ip::tcp;

	boost::asio::io_service* web_server::service() { return g_service; }
	boost::interprocess::named_semaphore* web_server::termination_semaphore() { return g_termination_semaphore; }
	int web_server::port() { return g_listening_port; }

	web_server::web_server(server& serv, boost::asio::io_service& io_service, int port)
		: http::web_server(io_service, port), server_(serv), timer_(io_service)
	{
		web_server_instance = this;
		timer_.expires_from_now(boost::posix_time::milliseconds(1000));
		timer_.async_wait(std::bind(&web_server::heartbeat, this, std::placeholders::_1));
	}

	web_server::~web_server()
	{
		timer_.cancel();
		web_server_instance = nullptr;
	}

	void web_server::handlePost(socket_ptr socket, variant doc, const http::environment& env, const std::string& raw_msg)
	{
#if defined(_MSC_VER)
		socket->socket.set_option(boost::asio::ip::tcp::no_delay(true));
#endif
		int session_id = -1;
		std::map<std::string, std::string>::const_iterator i = env.find("cookie");
		if(i != env.end()) {
			const char* cookie_start = strstr(i->second.c_str(), " session=");
			if(cookie_start != nullptr) {
				++cookie_start;
			} else {
				cookie_start = strstr(i->second.c_str(), "session=");
				if(cookie_start != i->second.c_str()) {
					cookie_start = nullptr;
				}
			}

			if(cookie_start) {
				session_id = atoi(cookie_start+8);
			}
		}

		if(doc["debug_session"].is_bool()) {
			session_id = doc["debug_session"].as_bool();
		}

		server_.adopt_ajax_socket(socket, session_id, doc);
		return;
	}

	namespace {
	struct KnownFile {
		const char* url;
		const char* fname;
		const char* type;
	};

	const KnownFile known_files[] = {
		{"/tbs_monitor.html", "data/tbs/tbs_monitor.html", "text/html"},
		{"/tbs_monitor.js", "data/tbs/tbs_monitor.js", "text/javascript"},
	};

	variant current_debug_state;
	int debug_state_id = 0;
	std::string current_debug_state_msg = "{ \"new_data\": false }";

	std::vector<socket_ptr> debug_state_sockets;

	}

	void web_server::set_debug_state(variant v)
	{
		debug_state_id = rand();
		current_debug_state = v;
		std::map<variant, variant> m;
		m[variant("info")] = current_debug_state;
		m[variant("state")] = variant(debug_state_id);
		m[variant("new_data")] = variant(true);
		current_debug_state_msg = variant(&m).write_json();
		for(socket_ptr sock : debug_state_sockets) {
			web_server_instance->send_msg(sock, "text/json", current_debug_state_msg, "");
		}
		debug_state_sockets.clear();
	}

	void web_server::heartbeat(const boost::system::error_code& error)
	{
		if(error == boost::asio::error::operation_aborted) {
			LOG_INFO("tbs_webserver::heartbeat cancelled");
			return;
		}
		for(socket_ptr sock : debug_state_sockets) {
			send_msg(sock, "text/json", "{ \"new_data\": false }", "");
			LOG_INFO("send no new data\n");

		}
		debug_state_sockets.clear();
		timer_.expires_from_now(boost::posix_time::milliseconds(1000));
		timer_.async_wait(std::bind(&web_server::heartbeat, this, std::placeholders::_1));
	}

	void web_server::handleGet(socket_ptr socket, 
		const std::string& url, 
		const std::map<std::string, std::string>& args)
	{
		if(url == "/tbs_monitor") {
			std::map<std::string,std::string>::const_iterator state_arg = args.find("state");
			if(state_arg != args.end()) {
				const int state_id = atoi(state_arg->second.c_str());
				if(state_id == debug_state_id) {
					debug_state_sockets.push_back(socket);
					return;
				}

				LOG_INFO("send debug msg: " << current_debug_state_msg.c_str());
				send_msg(socket, "text/json", current_debug_state_msg, "");
				return;
			}
		}

		for(const KnownFile& f : known_files) {
			if(url == f.url) {
				send_msg(socket, f.type, sys::read_file(f.fname), "");
				return;
			}
		}

		LOG_INFO("UNSUPPORTED GET REQUEST");
		disconnect(socket);
	}

	ffl::IntrusivePtr<http_client> g_game_server_http_client_to_matchmaking_server;
}

namespace 
{
	struct code_modified_exception {};

	void on_code_modified()
	{
		LOG_INFO("code modified");
		game_logic::FormulaObject::reloadClasses();
		throw code_modified_exception();
	}
}

namespace {
struct IPCSession {
	std::string pipe_name;
	int session_id;
};
}

COMMAND_LINE_UTILITY(tbs_server) {
	//if(g_tbs_server_hexes) {
	//}

	std::vector<IPCSession> ipc_sessions;

	int port = 23456;
	std::vector<std::string> bot_id;
	variant config;
	if(args.size() > 0) {
		std::vector<std::string>::const_iterator it = args.begin();
		while(it != args.end()) {
			if(*it == "--sharedmem") {
				it++;
				IPCSession s;
				if(it != args.end()) {
					s.pipe_name = *it++;
					if(it != args.end()) {
						s.session_id = atoi(it->c_str());
						ipc_sessions.push_back(s);
						++it;
					}
				}
			} else if(*it == "--port" || *it == "--listen-port") {
				it++;
				if(it != args.end()) {
					port = atoi(it->c_str());
					ASSERT_LOG(port > 0 && port <= 65535, "tbs_server(): Port must lie in the range 1-65535.");
					++it;
				}
			} else if(*it == "--bot") {
				++it;
				if(it != args.end()) {
					bot_id.push_back(*it++);
				}
			} else if(*it == "--config") {
				++it;
				if(it != args.end()) {
					config = json::parse(sys::read_file(*it++));
				}
			} else {
				++it;
			}
		}
	}
/*
	const std::string MonitorDirs[] = { "data/tbs", "data/tbs_test", "data/classes" };
	for(const std::string& dir : MonitorDirs) {
		std::vector<std::string> files;
		module::get_files_in_dir(dir, &files);
		for(const std::string& fname : files) {
			if(fname.size() > 4 && std::string(fname.end()-4,fname.end()) == ".cfg") {
				std::string path = module::map_file(dir + "/" + fname);
				LOG_INFO("NOTIFY ON: " << path);
				sys::notify_on_file_modification(path, on_code_modified);
			}
		}
	}
*/
	LOG_INFO("MONITOR URL: " << "http://localhost:" << port << "/tbs_monitor.html");

	boost::asio::io_service io_service;

	tbs::g_service = &io_service;
	tbs::g_listening_port = port;

	tbs::server s(io_service);

	for(auto session : ipc_sessions) {
		SharedMemoryPipePtr pipe(new SharedMemoryPipe(session.pipe_name, false));
		s.add_ipc_client(session.session_id, pipe);

		LOG_INFO("opened shared memory pipe: " << session.pipe_name << " for session " << session.session_id);
	}

	boost::shared_ptr<tbs::web_server> ws;

	ws.reset(new tbs::web_server(s, io_service, ipc_sessions.empty() ? port : 0));
	s.set_http_server(ws.get());
	LOG_INFO("tbs_server(): Listening on port " << std::dec << port);

	if(!config.is_null()) {
		tbs::server_base::game_info_ptr result = s.create_game(config["game"]);
		ASSERT_LOG(result, "Passed in config game is invalid");

		tbs::g_game_server_http_client_to_matchmaking_server.reset(new http_client(config["matchmaking_host"].as_string(), formatter() << config["matchmaking_port"].as_int()));
		http_client& client = *tbs::g_game_server_http_client_to_matchmaking_server;

		variant_builder msg;
		msg.add("type", "server_created_game");
#if defined(_MSC_VER)
		msg.add("pid", static_cast<int>(_getpid()));
#else
		msg.add("pid", static_cast<int>(getpid()));
#endif
		msg.add("game", config["game"]);
		msg.add("game_id", result->game_state->game_id());
		msg.add("port", port);

		bool complete = false;

		LOG_INFO("Sending confirmation request to: " << config["matchmaking_host"].as_string() << " " << config["matchmaking_port"].as_int());

		client.send_request("POST /server", msg.build().write_json(),
		  [&complete](std::string response) {
			complete = true;
		  },
		  [&complete](std::string msg) {
			complete = true;
			ASSERT_LOG(false, "Could not connect to server: " << msg);
		  },
		  [](size_t a, size_t b, bool c) {
		  });
		
		while(!complete) {
			client.process();
		}

		LOG_INFO("Started server, reported game availability");
	}

	if(g_tbs_server_semaphore.empty() == false) {
		g_termination_semaphore = new boost::interprocess::named_semaphore(boost::interprocess::open_only_t(), get_semaphore_name("term").c_str());

		boost::interprocess::named_semaphore startup_semaphore(boost::interprocess::open_only_t(), get_semaphore_name("start").c_str());
		startup_semaphore.post();
	}

	std::vector<ffl::IntrusivePtr<tbs::bot> > bots;
	for(;;) {
		try {
			const assert_recover_scope assert_scope;
			for(const std::string& id : bot_id) {
				bots.push_back(ffl::IntrusivePtr<tbs::bot>(new tbs::bot(io_service, "127.0.0.1", formatter() << port, json::parse_from_file("data/tbs_test/" + id + ".cfg"))));
			}
		} catch(validation_failure_exception& e) {
			std::map<variant,variant> m;
			m[variant("error")] = variant(e.msg);
			tbs::web_server::set_debug_state(variant(&m));
		} catch(json::ParseError& e) {
			std::map<variant,variant> m;
			m[variant("error")] = variant(e.message);
			tbs::web_server::set_debug_state(variant(&m));
		}
	
		try {
			io_service.run();
		} catch(code_modified_exception&) {
			s.clear_games();
		} catch(tbs::exit_exception&) {
			break;
		}
	}
}
