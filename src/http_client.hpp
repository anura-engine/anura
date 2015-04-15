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


#pragma once

#include <boost/array.hpp>
#include <boost/asio.hpp>

#include <string>
#include <vector>

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

using boost::asio::ip::tcp;

namespace http
{
	std::map<std::string, std::string> parse_http_headers(std::string& str);
}

class http_client : public game_logic::FormulaCallable
{
public:
	http_client(const std::string& host, const std::string& port, int session=-1, boost::asio::io_service* service=nullptr);
	void send_request(const std::string& method_path,
	                  const std::string& request,
					  std::function<void(std::string)> handler,
					  std::function<void(std::string)> error_handler,
					  std::function<void(size_t,size_t,bool)> progress_handler);
	virtual void process();

private:
	DECLARE_CALLABLE(http_client)
	int session_id_;

	std::shared_ptr<boost::asio::io_service> io_service_buf_;
	boost::asio::io_service& io_service_;

	struct Connection {
		explicit Connection(boost::asio::io_service& serv) : socket(serv), nbytes_sent(0), expected_len(-1)
		{}
		tcp::socket socket;
		std::string method_path;
		std::string request, response;
		size_t nbytes_sent;
		std::function<void(size_t, size_t, bool)> progress_handler;
		std::function<void(std::string)> handler, error_handler;
		std::map<std::string,std::string> headers;
		game_logic::MapFormulaCallablePtr callable;

		boost::array<char, 65536> buf;
		
		int expected_len;
	};

	typedef std::shared_ptr<Connection> connection_ptr;

	void handle_resolve(const boost::system::error_code& err, tcp::resolver::iterator endpoint_iterator, connection_ptr conn);
	void handle_connect(const boost::system::error_code& error, connection_ptr conn, tcp::resolver::iterator resolve_itor);
	void write_connection_data(connection_ptr conn);
	void handle_send(connection_ptr conn, const boost::system::error_code& e, size_t nbytes, std::shared_ptr<std::string> buf_ptr);
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
