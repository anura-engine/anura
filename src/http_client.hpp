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

#include <array>
#include <boost/asio.hpp>

#include <deque>
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
	~http_client();
	void send_request(std::string method_path,
	                  std::string request,
					  std::function<void(std::string)> handler,
					  std::function<void(std::string)> error_handler,
					  std::function<void(size_t,size_t,bool)> progress_handler,
					  int num_retries=0, int attempt_num=1);
	virtual void process();

	void set_allow_keepalive();

	int num_requests_in_flight() const { return in_flight_; }

	void set_timeout_and_retry(bool value=true) { timeout_and_retry_ = value; }

private:
	DECLARE_CALLABLE(http_client)
	int session_id_;

	std::shared_ptr<boost::asio::io_service> io_service_buf_;
	boost::asio::io_service* io_service_;

	struct Connection {
		explicit Connection(boost::asio::io_service& serv) : socket(new tcp::socket(serv)), nbytes_sent(0), expected_len(-1), retry_on_error(0), timeout_deadline(-1), timeout_period(-1), timeout_nbytes_needed(-1), aborted(false)
		{}
		explicit Connection(std::shared_ptr<tcp::socket> sock) : socket(sock), nbytes_sent(0), expected_len(-1), retry_on_error(0), timeout_deadline(-1), timeout_period(-1), timeout_nbytes_needed(-1), aborted(false)
		{}
		~Connection();
		std::shared_ptr<tcp::socket> socket;
		std::string method_path;
		std::string request, response;
		size_t nbytes_sent;
		std::function<void(size_t, size_t, bool)> progress_handler;
		std::function<void(std::string)> handler, error_handler;
		std::map<std::string,std::string> headers;
		game_logic::MapFormulaCallablePtr callable;

		std::array<char, 65536> buf;

		int expected_len;

		std::function<void()> retry_fn;
		int retry_on_error;

		int timeout_deadline, timeout_period;
		size_t timeout_nbytes_needed;
		bool aborted;
	};

	typedef std::shared_ptr<Connection> connection_ptr;

	std::deque<std::shared_ptr<tcp::socket> > usable_connections_;

	void send_connection_request(connection_ptr conne);

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

	std::shared_ptr<tcp::resolver> resolver_;
	std::shared_ptr<tcp::resolver::query> resolver_query_;
	tcp::resolver::iterator endpoint_iterator_;
	std::string host_, port_;

	int in_flight_;
	int send_request_at_;

	std::vector<std::pair<int,connection_ptr> > connections_waiting_on_fake_lag_;

	std::vector<connection_ptr> connections_waiting_on_dns_;

	bool allow_keepalive_;
	bool timeout_and_retry_;

	std::vector<std::weak_ptr<Connection> > connections_monitor_timeout_;
};
