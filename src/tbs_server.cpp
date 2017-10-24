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
#include <iostream>
#include <string>
#include <ctime>

#include <boost/interprocess/sync/named_semaphore.hpp>

#include "asserts.hpp"
#include "compress.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "module.hpp"
#include "json_parser.hpp"
#include "preferences.hpp"
#include "tbs_server.hpp"
#include "tbs_web_server.hpp"
#include "string_utils.hpp"
#include "utils.hpp"
#include "variant_utils.hpp"

namespace {
	PREF_BOOL(quit_server_after_game, false, "");
	PREF_BOOL(quit_server_on_parent_exit, false, "");
	PREF_INT(tbs_server_player_timeout_ms, 20000, "");
}

namespace tbs 
{
	using std::placeholders::_1;
	using std::placeholders::_2;

	extern int g_tbs_server_delay_ms;
	extern int g_tbs_server_heartbeat_freq;

	namespace 
	{

		int time_between_heartbeats()
		{
			return g_tbs_server_delay_ms * g_tbs_server_heartbeat_freq;
		}

		bool g_exit_server = false;
	}

	server::game_info::game_info(const variant& value) : nlast_touch(-1)
	{
		game_state = game::create(value);
	}

	server::game_info::~game_info()
	{
		if(g_quit_server_after_game) {
			g_exit_server = true;
		}

		if(game_state) {
			game_state->cancel_game();
		}
	}

	server::client_info::client_info() : nplayer(0), last_contact(0)
	{}

	server::server(boost::asio::io_service& io_service)
	  : server_base(io_service), web_server_(nullptr)
	{
	}

	server::~server()
	{
	}

	void server::adopt_ajax_socket(socket_ptr socket, int session_id, const variant& msg)
	{
		handle_message(
			std::bind(static_cast<void(server::*)(socket_ptr,const variant&)>(&server::send_msg), this, socket, _1), 
			std::bind(&server::close_ajax, this, socket, _1),
			std::bind(&server::get_socket_info, this, socket),
			session_id, 
			msg);
	}

	server_base::socket_info& server::get_socket_info(socket_ptr socket)
	{
		return connections_[socket];
	}

	server_base::socket_info& server::get_ipc_info(int session_id)
	{
		return ipc_clients_[session_id].info;
	}

	void server::close_ajax(socket_ptr socket, client_info& cli_info)
	{
		socket_info& info = connections_[socket];
		ASSERT_LOG(info.session_id != -1, "UNKNOWN SOCKET");

		if(cli_info.msg_queue.empty() == false) {
			std::vector<socket_ptr> keepalive_sockets;

			for(std::map<socket_ptr,std::string>::iterator s = waiting_connections_.begin();
				s != waiting_connections_.end(); ) {
				if(s->second == info.nick && s->first != socket) {
					keepalive_sockets.push_back(s->first);
					sessions_to_waiting_connections_.erase(cli_info.session_id);
					waiting_connections_.erase(s++);
				} else {
					++s;
				}
			}

			if(cli_info.msg_queue.size() > 1 && socket->client_version >= 1) {
				std::vector<variant> items;
				for(const std::string& s : cli_info.msg_queue) {
					items.push_back(variant(s));
				}

				cli_info.msg_queue.clear();

				variant_builder b;
				b.add("items", variant(&items));
				b.add("__type", "multimessage");
				send_msg(socket, b.build());
			} else {
				const std::string msg = cli_info.msg_queue.front();
				cli_info.msg_queue.pop_front();
				send_msg(socket, msg);
			}

			for(socket_ptr socket : keepalive_sockets) {
				send_msg(socket, "{ \"type\": \"keepalive\" }");
			}
		} else {
			waiting_connections_[socket] = info.nick;
			sessions_to_waiting_connections_[cli_info.session_id] = socket;
		}
	}

	void server::queue_msg(int session_id, const std::string& msg, bool has_priority)
	{
		if(session_id == -1) {
			return;
		}

		auto ipc_itor = ipc_clients_.find(session_id);
		if(ipc_itor != ipc_clients_.end()) {
			ipc_itor->second.pipe->write(msg);
			LOG_INFO("queue to ipc: " << ipc_clients_.size());
			return;
		}

		std::map<int, socket_ptr>::iterator itor = sessions_to_waiting_connections_.find(session_id);
		if(itor != sessions_to_waiting_connections_.end()) {
			const int session_id = itor->first;
			const socket_ptr sock = itor->second;
			waiting_connections_.erase(sock);
			sessions_to_waiting_connections_.erase(session_id);
			send_msg(sock, msg);
			return;
		}

		server_base::queue_msg(session_id, msg, has_priority);
	}

	void server::add_ipc_client(int session_id, SharedMemoryPipePtr pipe)
	{
		LOG_INFO("server::add_ipc_client: " << session_id);
		IPCClientInfo& info = ipc_clients_[session_id];
		info.pipe = pipe;
	}

	void server::connect_relay_session(const std::string& host, const std::string& port, int session_id)
	{
		if(web_server_) {
			LOG_INFO("Connnect relay session: " << host << ":" << port << " session = " << session_id);
			web_server_->connect_proxy(static_cast<uint32_t>(session_id), host, port);
		}
	}

	void server::send_msg(socket_ptr socket, const variant& msg)
	{
		send_msg(socket, msg.write_json(true, variant::JSON_COMPLIANT));
	}

	void server::send_msg(socket_ptr socket, const char* msg)
	{
		send_msg(socket, std::string(msg));
	}

	void server::send_msg(socket_ptr socket, const std::string& msg_ref)
	{
		LOG_INFO("DO send_msg: " << msg_ref);
		std::string compressed_buf;
		std::string compress_header;
		const std::string* msg_ptr = &msg_ref;
		if(socket->supports_deflate && msg_ref.size() > 1024) {
			compressed_buf = zip::compress(msg_ref);
			msg_ptr = &compressed_buf;

			compress_header = "Content-Encoding: deflate\r\n"; 
		}

		const std::string& msg = *msg_ptr;

		std::map<socket_ptr, socket_info>::const_iterator connections_itor = connections_.find(socket);
		const int session_id = connections_itor == connections_.end() ? -1 : connections_itor->second.session_id;

		std::stringstream buf;
		buf <<
			"HTTP/1.1 200 OK\r\n"
			"Date: " << get_http_datetime() << "\r\n"
			"Connection: close\r\n"
			"Server: Wizard/1.0\r\n"
			"Accept-Ranges: bytes\r\n"
			"Access-Control-Allow-Origin: *\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: " << std::dec << (int)msg.size() << "\r\n" <<
			compress_header <<
			"Last-Modified: " << get_http_datetime() << "\r\n\r\n";
		std::string header = buf.str();

		std::shared_ptr<std::string> str_buf(new std::string(header.empty() ? msg : (header + msg)));
		boost::asio::async_write(socket->socket, boost::asio::buffer(*str_buf),
										 std::bind(&server::handle_send, this, socket, _1, _2, str_buf, session_id));
	}

	void server::handle_send(socket_ptr socket, const boost::system::error_code& e, size_t nbytes, std::shared_ptr<std::string> buf, int session_id)
	{
		if(e) {
			LOG_ERROR("ERROR SENDING DATA: " << e.message());
			queue_msg(session_id, *buf, true); //re-queue the message.
		}

		disconnect(socket);
	}

	void server::disconnect(socket_ptr socket)
	{
		std::map<socket_ptr, socket_info>::iterator itor = connections_.find(socket);
		if(itor != connections_.end()) {
			std::map<int, socket_ptr>::iterator sessions_itor = sessions_to_waiting_connections_.find(itor->second.session_id);
			if(sessions_itor != sessions_to_waiting_connections_.end() && sessions_itor->second == socket) {
				sessions_to_waiting_connections_.erase(sessions_itor);
			}

			connections_.erase(itor);
		}

		waiting_connections_.erase(socket);

		if(web_server_) {
			web_server_->keepalive_socket(socket);
		} else {
			socket->socket.close();
		}
	}

	void server::heartbeat_internal(int send_heartbeat, std::map<int, client_info>& clients)
	{
#if defined(__linux__) || defined(__APPLE__)
		if(g_quit_server_on_parent_exit && getppid() == 1) {
			g_exit_server = true;
		}
#endif

		if (g_exit_server || (web_server::termination_semaphore() && web_server::termination_semaphore()->try_wait())) {
			throw tbs::exit_exception();
			exit(0);
		}

		for(auto i = ipc_clients_.begin(); i != ipc_clients_.end(); ++i) {
			i->second.pipe->process();
		}

		for(auto i = ipc_clients_.begin(); i != ipc_clients_.end(); ++i) {
			std::vector<std::string> messages;
			i->second.pipe->read(messages);

			for(const std::string& msg : messages) {
				//LOG_INFO("read IPC message " << msg);
				SharedMemoryPipePtr pipe = i->second.pipe;
				variant v(json::parse(msg, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR));
				handle_message(
					[=](variant v) {
						pipe->write(v.write_json());
					},
					[](client_info& info) {
					},
					std::bind(&server::get_ipc_info, this, i->first),
					i->first, v
				);
			}
		}

		std::vector<std::pair<socket_ptr, std::string> > messages;

		for(std::map<socket_ptr, std::string>::iterator i = waiting_connections_.begin(); i != waiting_connections_.end(); ++i) {
			socket_ptr socket = i->first;

			socket_info& info = connections_[socket];
			ASSERT_LOG(info.session_id != -1, "UNKNOWN SOCKET");

			client_info& cli_info = clients[info.session_id];
			if(cli_info.msg_queue.empty() == false) {
				messages.push_back(std::pair<socket_ptr,std::string>(socket, cli_info.msg_queue.front()));
				cli_info.msg_queue.pop_front();

			sessions_to_waiting_connections_.erase(info.session_id);
			} else if(send_heartbeat) {
				if(!cli_info.game) {
					messages.push_back(std::pair<socket_ptr,std::string>(socket, "{ \"type\": \"heartbeat\" }"));
				} else {
					variant v = create_heartbeat_packet(cli_info);
					messages.push_back(std::pair<socket_ptr,std::string>(socket, v.write_json()));
				}

				sessions_to_waiting_connections_.erase(info.session_id);
			}
		}

		for(int i = 0; i != messages.size(); ++i) {
			waiting_connections_.erase(messages[i].first);
		}

		for(int i = 0; i != messages.size(); ++i) {
			send_msg(messages[i].first, messages[i].second);
		}

		if(get_num_heartbeat()%5 == 0) {
			for(auto g : games()) {
				for(int n = 0; n < static_cast<int>(g->clients.size()) && n < static_cast<int>(g->game_state->players().size()); ++n) {
					const int session_id = g->clients[n];
					if(ipc_clients_.count(session_id)) {
						continue;
					}

					int time_since_last_contact = 0;
					if(sessions_to_waiting_connections_.count(session_id) == 0) {
						time_since_last_contact = get_ms_since_last_contact(session_id);
					}

					const int DisconnectTimeoutMS = g_tbs_server_player_timeout_ms;

					const bool disconnected = time_since_last_contact > DisconnectTimeoutMS;
					const bool recorded_as_disconnected = g->clients_disconnected.count(session_id) == 1;
					if(disconnected != recorded_as_disconnected) {
						if(disconnected) {
							g->clients_disconnected.insert(session_id);
							g->game_state->player_disconnect(n);
						} else {
							g->clients_disconnected.erase(session_id);
							g->game_state->player_reconnect(n);
						}

					}

					if(disconnected) {
						g->game_state->player_disconnected_for(n, time_since_last_contact - DisconnectTimeoutMS);
					}
				}
			}
		}
	}
}
