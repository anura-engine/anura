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
#ifndef HTTP_CLIENT_HPP_INCLUDED
#define HTTP_CLIENT_HPP_INCLUDED

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

#include "formula_callable.hpp"

using boost::asio::ip::tcp;

class http_client : public game_logic::formula_callable
{
public:
	http_client(const std::string& host, const std::string& port, int session=-1, boost::asio::io_service* service=NULL);
	void send_request(const std::string& method_path,
	                  const std::string& request,
					  boost::function<void(std::string)> handler,
					  boost::function<void(std::string)> error_handler,
					  boost::function<void(int,int,bool)> progress_handler);
	virtual void process();

protected:
	variant get_value(const std::string& key) const;

private:
	
	int session_id_;

	boost::shared_ptr<boost::asio::io_service> io_service_buf_;
	boost::asio::io_service& io_service_;

	struct Connection {
		explicit Connection(boost::asio::io_service& serv) : socket(serv), nbytes_sent(0), expected_len(-1)
		{}
		tcp::socket socket;
		std::string method_path;
		std::string request, response;
		int nbytes_sent;
		boost::function<void(int,int,bool)> progress_handler;
		boost::function<void(std::string)> handler, error_handler;
		game_logic::map_formula_callable_ptr callable;

		boost::array<char, 65536> buf;
		
		int expected_len;
	};

	typedef boost::shared_ptr<Connection> connection_ptr;

	void handle_resolve(const boost::system::error_code& err, tcp::resolver::iterator endpoint_iterator, connection_ptr conn);
	void handle_connect(const boost::system::error_code& error, connection_ptr conn, tcp::resolver::iterator resolve_itor);
	void write_connection_data(connection_ptr conn);
	void handle_send(connection_ptr conn, const boost::system::error_code& e, size_t nbytes, boost::shared_ptr<std::string> buf_ptr);
	void handle_receive(connection_ptr conn, const boost::system::error_code& e, size_t nbytes);

	void async_connect(connection_ptr conn);

	//enum which represents whether the endpoint_iterator_ points to a
	//valid endpoint that we can connect to.
	enum RESOLUTION_STATE { RESOLUTION_NOT_STARTED,
	                        RESOLUTION_IN_PROGRESS,
							RESOLUTION_DONE };
	
	RESOLUTION_STATE resolution_state_;

	tcp::resolver resolver_;
	tcp::resolver::query resolver_query_;
	tcp::resolver::iterator endpoint_iterator_;
	std::string host_;

	int in_flight_;

	std::vector<connection_ptr> connections_waiting_on_dns_;
};


#endif
