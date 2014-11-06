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
#pragma once
#ifndef TBS_INTERNAL_SERVER_HPP_INCLUDED
#define TBS_INTERNAL_SERVER_HPP_INCLUDED

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <deque>
#include <list>

#include "formula_callable.hpp"
#include "tbs_server_base.hpp"
#include "variant.hpp"

namespace tbs
{
	struct internal_server_manager {
		explicit internal_server_manager(bool use_internal_server);
		~internal_server_manager();
	};

	class internal_server : public server_base
	{
	public:
		internal_server();
		virtual ~internal_server();

		void handle_process();

		static void send_request(const variant& request, 
			int session_id,
			game_logic::map_formula_callable_ptr callable, 
			boost::function<void(const std::string&)> handler);		
		static void process();
		static boost::asio::io_service& get_io_service() { return io_service_; }

		static int requests_in_flight(int session_id);
	protected:
		virtual void heartbeat_internal(int send_heartbeat, std::map<int, client_info>& clients);
	private:
		void send_msg(const variant& resp, 
			int session_id,
			boost::function<void(const std::string&)> handler, 
			game_logic::map_formula_callable_ptr callable);
		static boost::asio::io_service io_service_;

		void write_queue(send_function send_fn, const variant& v, int session_id);
		bool read_queue(send_function* send_fn, variant* v, int *session_id);

		void finish_socket(send_function send_fn, client_info& cli_info);
		
		socket_info& create_socket_info(send_function send_fn);
		void disconnect(int session_id);
		void queue_msg(int session_id, const std::string& msg, bool has_priority);

		std::list<std::pair<send_function, socket_info> > connections_;
		std::deque<boost::tuple<send_function,variant,int> > msg_queue_;
	};

	typedef boost::shared_ptr<internal_server> internal_server_ptr;
}

#endif
