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
#ifndef TBS_SERVER_VIRT_HPP_INCLUDED
#define TBS_SERVER_VIRT_HPP_INCLUDED

#include <boost/function.hpp>

#include <map>
#include <vector>

#include "tbs_game.hpp"
#include "variant.hpp"

namespace tbs
{
	typedef boost::function<void(variant)> send_function;

	struct exit_exception {};

	class server_base
	{
	public:
		server_base(boost::asio::io_service& io_service);
		virtual ~server_base();
		
		void clear_games();
		static variant get_server_info();

		struct game_info 
		{
			explicit game_info(const variant& value);
			~game_info();

			game_ptr game_state;
			std::vector<int> clients;
			int nlast_touch;
			bool quit_server_on_exit;
		};

		typedef boost::shared_ptr<game_info> game_info_ptr;

		game_info_ptr create_game(variant msg);
	protected:

		struct client_info 
		{
			client_info();

			std::string user;	
			game_info_ptr game;
			int nplayer;
			int last_contact;

			int session_id;

			std::deque<std::string> msg_queue;
		};

		struct socket_info 
		{
			socket_info() : session_id(-1) {}
			std::vector<char> partial_message;
			std::string nick;
			int session_id;
		};

		virtual void handle_message(send_function send_fn, 
			boost::function<void(client_info&)> close_fn,
			boost::function<socket_info&(void)> socket_info_fn,
			int session_id, 
			const variant& msg);

		virtual void queue_msg(int session_id, const std::string& msg, bool has_priority=false);

		virtual void heartbeat_internal(int send_heartbeat, std::map<int, client_info>& clients) = 0;

		variant create_heartbeat_packet(const client_info& cli_info);

	private:
		variant create_lobby_msg() const;
		variant create_game_info_msg(game_info_ptr g) const;
		void status_change();
		void quit_games(int session_id);
		void flush_game_messages(game_info& info);
		void schedule_write();
		void handle_message_internal(client_info& cli_info, const variant& msg);
		void heartbeat(const boost::system::error_code& error);

		int nheartbeat_;
		int scheduled_write_;
		int status_id_;

		std::map<int, client_info> clients_;
		std::vector<game_info_ptr> games_;

		boost::asio::deadline_timer timer_;

		// send_fn's waiting on status info.
		std::vector<send_function> status_fns_;
	};
}

#endif
