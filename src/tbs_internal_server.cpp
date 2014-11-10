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

#include "asserts.hpp"
#include "json_parser.hpp"
#include "tbs_internal_server.hpp"
#include "variant_utils.hpp"
#include "wml_formula_callable.hpp"

namespace tbs
{
	using std::placeholders::_1;

	namespace 
	{
		internal_server_ptr server_ptr;
	}

	boost::asio::io_service internal_server::io_service_;

	internal_server::internal_server()
		: server_base(io_service_)
	{
	}

	internal_server::~internal_server()
	{
	}

	void internal_server::send_request(const variant& request, 
		int session_id,
		game_logic::MapFormulaCallablePtr callable, 
		std::function<void(const std::string&)> handler)
	{
		ASSERT_LOG(server_ptr != NULL, "Internal server pointer is NULL");
		send_function send_fn = std::bind(&internal_server::send_msg, server_ptr.get(), _1, session_id, handler, callable);
		server_ptr->write_queue(send_fn, request, session_id);
	}

	void internal_server::send_msg(const variant& resp, 
		int session_id,
		std::function<void(const std::string&)> handler, 
		game_logic::MapFormulaCallablePtr callable)
	{
		if(handler) {
			callable->add("message", resp);
			handler("message_received");
		}
		disconnect(session_id);
	}

	void internal_server::process()
	{
		ASSERT_LOG(server_ptr != NULL, "Internal server pointer is NULL");
		server_ptr->handleProcess();
	}

	internal_server_manager::internal_server_manager(bool use_internal_server)
	{
		if(use_internal_server) {
			server_ptr = internal_server_ptr(new internal_server);
		}
	}

	internal_server_manager::~internal_server_manager()
	{
		server_ptr.reset();
	}

	int internal_server::requests_in_flight(int session_id)
	{
		int result = 0;
		for(auto i = server_ptr->connections_.begin(); i != server_ptr->connections_.end(); ++i) {
			if(i->second.session_id == session_id) {
				//implementing this causes problems. Investigate.
			}
		}

		return result;
	}

	server_base::socket_info& internal_server::create_socket_info(send_function send_fn)
	{
		connections_.push_back(std::pair<send_function, socket_info>(send_fn, socket_info()));
		return connections_.back().second;
	}

	void internal_server::disconnect(int session_id)
	{
		if(session_id == -1) {
			return;
		}

		for(auto i = connections_.begin(); i != connections_.end(); ++i) {
			if(i->second.session_id == session_id) {
				connections_.erase(i);
				return;
			}
		}
		ASSERT_LOG(false, "Trying to erase unknown session_id: " << session_id);
	}

	void internal_server::heartbeat_internal(int send_heartbeat, std::map<int, client_info>& clients)
	{
		std::vector<std::pair<send_function, variant> > messages;

		for(auto i = connections_.begin(); i != connections_.end(); ++i) {
			send_function send_fn = i->first;
			socket_info& info = i->second;
			ASSERT_LOG(info.session_id != -1, "UNKNOWN SOCKET");

			client_info& cli_info = clients[info.session_id];
			if(cli_info.msg_queue.empty() == false) {
				messages.push_back(std::pair<send_function,variant>(send_fn, game_logic::deserialize_doc_with_objects(cli_info.msg_queue.front())));
				cli_info.msg_queue.pop_front();
			} else if(send_heartbeat) {
				if(!cli_info.game) {
					variant_builder v;
					v.add("type", variant("heartbeat"));
					messages.push_back(std::pair<send_function,variant>(send_fn, v.build()));
				} else {
					variant v = create_heartbeat_packet(cli_info);
					messages.push_back(std::pair<send_function,variant>(send_fn, v));
				}
			}
		}

		for(int i = 0; i != messages.size(); ++i) {
			messages[i].first(messages[i].second);
		}
	}

	void internal_server::handleProcess()
	{
		send_function send_fn;
		variant request;
		int session_id;
		while(read_queue(&send_fn, &request, &session_id)) {
			server_ptr->handle_message(
				send_fn,
				std::bind(&internal_server::finish_socket, this, send_fn, _1),
				std::bind(&internal_server::create_socket_info, server_ptr.get(), send_fn),
				session_id, 
				request);
		}
		io_service_.poll();
		io_service_.reset();
	}


	void internal_server::queue_msg(int session_id, const std::string& msg, bool has_priority)
	{
		if(session_id == -1) {
			return;
		}

		server_base::queue_msg(session_id, msg, has_priority);
	}

	void internal_server::write_queue(send_function send_fn, const variant& v, int session_id)
	{
		msg_queue_.push_back(boost::make_tuple(send_fn,v,session_id));
	}

	bool internal_server::read_queue(send_function* send_fn, variant* v, int *session_id)
	{
		ASSERT_LOG(send_fn != NULL && v != NULL && session_id != NULL,
			"read_queue called with NULL parameter.");
		if(msg_queue_.empty()) {
			return false;
		}
		boost::tie(*send_fn, *v, *session_id) = msg_queue_.front();
		msg_queue_.pop_front();
		return true;
	}

	void internal_server::finish_socket(send_function send_fn, client_info& cli_info)
	{
		if(cli_info.msg_queue.empty() == false) {
			const std::string msg = cli_info.msg_queue.front();
			cli_info.msg_queue.pop_front();
			send_fn(game_logic::deserialize_doc_with_objects(msg));
		}
	}
}
