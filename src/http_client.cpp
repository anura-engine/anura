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

#include <boost/algorithm/string/replace.hpp>

#include "asserts.hpp"
#include "http_client.hpp"

http_client::http_client(const std::string& host, const std::string& port, int session, boost::asio::io_service* service)
  : session_id_(session),
    io_service_buf_(service ? nullptr : new boost::asio::io_service),
	io_service_(service ? *service : *io_service_buf_),
	resolution_state_(RESOLUTION_NOT_STARTED),
    resolver_(io_service_),
	host_(host),
	resolver_query_(host.c_str(), port.c_str()),
	in_flight_(0)
{
}

void http_client::send_request(const std::string& method_path, const std::string& request, std::function<void(std::string)> handler, std::function<void(std::string)> error_handler, std::function<void(int,int,bool)> progress_handler)
{
	++in_flight_;
	connection_ptr conn(new Connection(io_service_));
	conn->method_path = method_path;
	conn->request = request;
	conn->handler = handler;
	conn->error_handler = error_handler;
	conn->progress_handler = progress_handler;

	if(resolution_state_ == RESOLUTION_NOT_STARTED) {
		resolution_state_ = RESOLUTION_IN_PROGRESS;

		resolver_.async_resolve(resolver_query_,
			std::bind(&http_client::handle_resolve, this,
				std::placeholders::_1,
				std::placeholders::_2,
				conn));
	} else if(resolution_state_ == RESOLUTION_IN_PROGRESS) {
		connections_waiting_on_dns_.push_back(conn);
	} else {
		ASSERT_EQ(resolution_state_, RESOLUTION_DONE);
		async_connect(conn);
	}
}

void http_client::handle_resolve(const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator, connection_ptr conn)
{
	if(!error)
	{
		endpoint_iterator_ = endpoint_iterator;
		// Attempt a connection to each endpoint in the list until we
		// successfully establish a connection.
		async_connect(conn);

	} else {
		--in_flight_;
		conn->error_handler("Error resolving connection");
	}
}

void http_client::async_connect(connection_ptr conn)
{

#if BOOST_VERSION >= 104700
		boost::asio::async_connect(conn->socket, 
			endpoint_iterator_,
			std::bind(&http_client::handle_connect, this,
				std::placeholders::_1, conn, endpoint_iterator_));
#else
		conn->socket.async_connect(*endpoint_iterator_,
			std::bind(&http_client::handle_connect, this,
				std::placeholders::_1, conn, endpoint_iterator_));
#endif
}

void http_client::handle_connect(const boost::system::error_code& error, connection_ptr conn, tcp::resolver::iterator resolve_itor)
{
	if(error) {
		std::cerr << "HANDLE_CONNECT_ERROR: " << error << std::endl;
		if(endpoint_iterator_ == resolve_itor) {
			++endpoint_iterator_;
		}
		//ASSERT_LOG(endpoint_iterator_ != tcp::resolver::iterator(), "COULD NOT RESOLVE TBS SERVER: " << resolve_itor->endpoint().address().to_string() << ":" << resolve_itor->endpoint().port());
		if(endpoint_iterator_ == tcp::resolver::iterator()) {
			resolution_state_ = RESOLUTION_NOT_STARTED;
			--in_flight_;
			conn->error_handler("Error establishing connection");
			return;
		}

		async_connect(conn);

		return;
	}
#if defined(_MSC_VER)
	conn->socket.set_option(boost::asio::ip::tcp::no_delay(true));
#endif

	//we've connected okay, mark DNS resolution as good.
	if(resolution_state_ != RESOLUTION_DONE) {
		resolution_state_ = RESOLUTION_DONE;

		//all those connections waiting on DNS resolution can now connect.
		for(const connection_ptr conn : connections_waiting_on_dns_) {
			async_connect(conn);
		}

		connections_waiting_on_dns_.clear();
	}

	//do async write.
	std::ostringstream msg;
	msg << conn->method_path << " HTTP/1.1\r\n"
		   "Host: " << host_ << "\r\n"
		   "Accept: */*\r\n"
	       "User-Agent: Frogatto 1.0\r\n"
		   "Content-Type: text/plain\r\n"
		   "Connection: close\r\n";
	
	if(session_id_ != -1) {
		msg << "Cookie: session=" << session_id_ << "\r\n";
	}
	// replace all the tab characters with spaces before calculating the request size, so 
	// some http server doesn't get all upset about the length being wrong.
	boost::replace_all(conn->request, "\t", "    ");
	msg << "Content-Length: " << conn->request.length() << "\r\n\r\n" << conn->request;

	conn->request = msg.str();

	write_connection_data(conn);
}

void http_client::write_connection_data(connection_ptr conn)
{
	const int bytes_to_send = conn->request.size() - conn->nbytes_sent;
	const int nbytes = std::min<int>(bytes_to_send, 1024*64);

	const std::shared_ptr<std::string> msg(new std::string(conn->request.begin() + conn->nbytes_sent, conn->request.begin() + conn->nbytes_sent + nbytes));
	boost::asio::async_write(conn->socket, boost::asio::buffer(*msg),
	      std::bind(&http_client::handle_send, this, conn, std::placeholders::_1, std::placeholders::_2, msg));

}

void http_client::handle_send(connection_ptr conn, const boost::system::error_code& e, size_t nbytes, std::shared_ptr<std::string> buf_ptr)
{

	if(e) {
		--in_flight_;
		if(conn->error_handler) {
			conn->error_handler("ERROR SENDING DATA");
		}

		return;
	}

	conn->nbytes_sent += nbytes;

	if(conn->progress_handler) {
		conn->progress_handler(conn->nbytes_sent, conn->request.size(), false);
	}

	if(static_cast<unsigned>(conn->nbytes_sent) < conn->request.size()) {
		write_connection_data(conn);
	} else {
		conn->socket.async_read_some(boost::asio::buffer(conn->buf), std::bind(&http_client::handle_receive, this, conn, std::placeholders::_1, std::placeholders::_2));
	}
}

void http_client::handle_receive(connection_ptr conn, const boost::system::error_code& e, size_t nbytes)
{
	if(e) {
		--in_flight_;
		fprintf(stderr, "ERROR IN HTTP RECEIVE: (%d(%s), %s)\n", e.value(), e.message().c_str(), conn->response.c_str());
		if(e.value() == boost::system::errc::no_such_file_or_directory) {
			fprintf(stderr, "Error no such file or directory\n");
			//fprintf(stderr, "HAVE FULL RESPONSE: %d (((%s)))\n", (int)conn->response.size(), conn->response.c_str());
			const char* end_headers = strstr(conn->response.c_str(), "\n\n");
			const char* end_headers2 = strstr(conn->response.c_str(), "\r\n\r\n");
			if(end_headers2 && (end_headers == nullptr || end_headers2 < end_headers)) {
				end_headers = end_headers2 + 2;
			}

			if(end_headers) {
				fprintf(stderr, "HEADERS: (((%s)))\n", std::string(conn->response.c_str(), end_headers).c_str());
				conn->handler(std::string(end_headers+2));
				return;
			}
		}

		ASSERT_LOG(conn->error_handler, "Error in HTTP receive with no error handler");
		conn->error_handler("ERROR RECEIVING DATA");

		return;
	}

	conn->response.insert(conn->response.end(), &conn->buf[0], &conn->buf[0] + nbytes);
	if(conn->expected_len == -1) {
		int header_term_len = 2;
		const char* end_headers = strstr(conn->response.c_str(), "\n\n");
		const char* end_headers2 = strstr(conn->response.c_str(), "\r\n\r\n");
		if(end_headers2 && (end_headers == nullptr || end_headers2 < end_headers)) {
			end_headers = end_headers2;
			header_term_len = 4;
		}
		if(end_headers) {
			const char* content_length = strstr(conn->response.c_str(), "Content-Length:");
			if(!content_length) {
				content_length = strstr(conn->response.c_str(), "Content-length:");
			}

			if(!content_length) {
				content_length = strstr(conn->response.c_str(), "content-length:");
			}

			if(content_length) {
				content_length += strlen("content-length:");
				const int payload_len = strtol(content_length, nullptr, 10);
				if(payload_len > 0) {
					conn->expected_len = (end_headers - conn->response.c_str()) + payload_len + header_term_len;
				}
			}
		}
	}

	if(conn->expected_len != -1 && conn->response.size() >= static_cast<unsigned>(conn->expected_len)) {
		ASSERT_LOG(conn->expected_len == conn->response.size(), "UNEXPECTED RESPONSE SIZE " << conn->expected_len << " VS " << conn->response << " " << conn->response.size());

		//We have the full response now -- handle it.
		//fprintf(stderr, "HAVE FULL RESPONSE: %d (((%s)))\n", (int)conn->response.size(), conn->response.c_str());
		const char* end_headers = strstr(conn->response.c_str(), "\n\n");
		int header_term_len = 2;
		const char* end_headers2 = strstr(conn->response.c_str(), "\r\n\r\n");
		if(end_headers2 && (end_headers == nullptr || end_headers2 < end_headers)) {
			end_headers = end_headers2;
			header_term_len = 4;
		}
		ASSERT_LOG(end_headers, "COULD NOT FIND END OF HEADERS IN MESSAGE: " << conn->response);
		fprintf(stderr, "HEADERS: (((%s)))\n", std::string(conn->response.c_str(), end_headers).c_str());
		--in_flight_;
		const char* payload = end_headers + header_term_len;
		conn->handler(std::string(payload, conn->response.c_str() + conn->response.size()));
	} else {
		if(conn->expected_len != -1 && conn->progress_handler) {
			conn->progress_handler(conn->response.size(), conn->expected_len, true);
		}
		conn->socket.async_read_some(boost::asio::buffer(conn->buf), std::bind(&http_client::handle_receive, this, conn, std::placeholders::_1, std::placeholders::_2));
	}
}

void http_client::process()
{
	io_service_.poll();
	io_service_.reset();
}

BEGIN_DEFINE_CALLABLE_NOBASE(http_client)
	DEFINE_FIELD(in_flight, "int")
		return variant(obj.in_flight_);
END_DEFINE_CALLABLE(http_client)
