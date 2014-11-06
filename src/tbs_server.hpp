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
#ifndef SERVER_HPP_INCLUDED
#define SERVER_HPP_INCLUDED

#include "tbs_game.hpp"
#include "variant.hpp"

#include <deque>
#include <map>
#include <vector>

#include <boost/array.hpp>
#include <boost/asio.hpp>

#include "tbs_server_base.hpp"

namespace tbs {

typedef boost::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr;
typedef boost::shared_ptr<boost::array<char, 1024> > buffer_ptr;

class server : public server_base
{
public:
	explicit server(boost::asio::io_service& io_service);
	virtual ~server();
	 
	void adopt_ajax_socket(socket_ptr socket, int session_id, const variant& msg);

	static variant get_server_info();
private:
	void close_ajax(socket_ptr socket, client_info& cli_info);

	void send_msg(socket_ptr socket, const variant& msg);
	void send_msg(socket_ptr socket, const char* msg);
	void send_msg(socket_ptr socket, const std::string& msg);
	void handle_send(socket_ptr socket, const boost::system::error_code& e, size_t nbytes, boost::shared_ptr<std::string> buf, int session_id);
	virtual void heartbeat_internal(int send_heartbeat, std::map<int, client_info>& clients);

	socket_info& get_socket_info(socket_ptr socket);

	void disconnect(socket_ptr socket);

	virtual void queue_msg(int session_id, const std::string& msg, bool has_priority=false);

	std::map<int, socket_ptr> sessions_to_waiting_connections_;
	std::map<socket_ptr, std::string> waiting_connections_;

	std::map<socket_ptr, socket_info> connections_;
};

}

#endif
