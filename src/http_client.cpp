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
#include "compress.hpp"
#include "http_client.hpp"
#include "preferences.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"

PREF_INT(http_fake_lag, 0, "fake lag to add to http requests");

http_client::http_client(const std::string& host, const std::string& port, int session, boost::asio::io_service* service)
  : session_id_(session),
    io_service_buf_(service ? nullptr : new boost::asio::io_service),
	io_service_(service ? service : io_service_buf_.get()),
	resolution_state_(RESOLUTION_NOT_STARTED),
    resolver_(new tcp::resolver(*io_service_)),
	host_(host),
	port_(port),
	resolver_query_(new tcp::resolver::query(tcp::resolver::query::protocol_type::v4(), host.c_str(), port.c_str())),
	in_flight_(0),
	allow_keepalive_(false),
	timeout_and_retry_(false)
{
}

http_client::~http_client()
{
}

http_client::Connection::~Connection()
{
}

void http_client::set_allow_keepalive()
{
	allow_keepalive_ = true;
}

void http_client::send_request(std::string method_path, std::string request, std::function<void(std::string)> handler, std::function<void(std::string)> error_handler, std::function<void(size_t,size_t,bool)> progress_handler, int num_retries, int attempt_num)
{
	LOG_INFO("http_client::send_request(this = " << (void*)this << " @" << SDL_GetTicks() << " method_path = " << method_path << " request.size() = " << (int)request.size() << " num_retries = " << num_retries << " attempt_num = " << attempt_num << ")\n");
	if(in_flight_ == 0 && attempt_num > 6) {
		LOG_INFO("HTTP client failing to receive data after " << attempt_num << " tries, timed out.\n");
		error_handler("timeout");
		return;
	}

	++in_flight_;
	
	bool already_connected = false;
	connection_ptr conn;
	if(usable_connections_.empty() == false) {
		conn.reset(new Connection(usable_connections_.front()));
		usable_connections_.pop_front();
		already_connected = true;

		//because this is an existing connection it might have a problem.
		//if it does we should just retry on a different connection
		//rather than report an error.
		conn->retry_fn = std::bind(&http_client::send_request, this, method_path, request, handler, error_handler, progress_handler, num_retries, attempt_num+1);
		conn->retry_on_error = num_retries+1;
	} else {
		conn.reset(new Connection(*io_service_));
		conn->retry_on_error = num_retries;
		if(num_retries || timeout_and_retry_) {
			conn->retry_fn = std::bind(&http_client::send_request, this, method_path, request, handler, error_handler, progress_handler, num_retries-1, attempt_num+1);
		}
	}

	conn->method_path = method_path;
	conn->request = request;
	conn->handler = handler;
	conn->error_handler = error_handler;
	conn->progress_handler = progress_handler;

	if(timeout_and_retry_) {
		connections_monitor_timeout_.push_back(std::weak_ptr<Connection>(conn));
		conn->timeout_period = 2000<<(attempt_num > 5 ? 5 : attempt_num);
		conn->timeout_deadline = SDL_GetTicks() + conn->timeout_period;
		conn->timeout_nbytes_needed = 1024*16;
	}

	if(already_connected) {
		send_connection_request(conn);
	} else if(resolution_state_ == RESOLUTION_NOT_STARTED) {
		resolution_state_ = RESOLUTION_IN_PROGRESS;

		try {
			resolver_->async_resolve(*resolver_query_,
				std::bind(&http_client::handle_resolve, this,
					std::placeholders::_1,
					std::placeholders::_2,
					conn));
		} catch(std::exception& e) {
			LOG_ERROR("Error in http resolve: " << e.what() << "\n");
		}
	} else if(resolution_state_ == RESOLUTION_IN_PROGRESS) {
		connections_waiting_on_dns_.push_back(conn);
	} else {
		ASSERT_EQ(resolution_state_, RESOLUTION_DONE);
		async_connect(conn);
	}
}

void http_client::handle_resolve(const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator, connection_ptr conn)
{
	if(conn->aborted) {
		return;
	}

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
	if(conn->aborted) {
		return;
	}

	try {
#if BOOST_VERSION >= 104700
		boost::asio::async_connect(*conn->socket, 
			endpoint_iterator_,
			std::bind(&http_client::handle_connect, this,
				std::placeholders::_1, conn, endpoint_iterator_));
#else
		conn->socket->async_connect(*endpoint_iterator_,
			std::bind(&http_client::handle_connect, this,
				std::placeholders::_1, conn, endpoint_iterator_));
#endif
	} catch(std::exception& e) {
		LOG_ERROR("Error in async_connect: " << e.what() << "\n");
	}
}

void http_client::handle_connect(const boost::system::error_code& error, connection_ptr conn, tcp::resolver::iterator resolve_itor)
{
	if(conn->aborted) {
		return;
	}

	if(error) {
		LOG_WARN("HANDLE_CONNECT_ERROR: " << error);
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
	conn->socket->set_option(boost::asio::ip::tcp::no_delay(true));
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

	send_connection_request(conn);
}

void http_client::send_connection_request(connection_ptr conn)
{
	if(conn->aborted) {
		return;
	}

	//do async write.
	std::ostringstream msg;
	msg << conn->method_path << " HTTP/1.1\r\n"
		   "Host: " << host_ << "\r\n"
		   "Accept: */*\r\n"
	       "User-Agent: Frogatto 1.1\r\n"
		   "Content-Type: text/plain\r\n"
		   "Accept-Encoding: deflate\r\n"
		   "Connection: close\r\n";
	
	if(session_id_ != -1) {
		msg << "Cookie: session=" << session_id_ << "\r\n";
	}
	// replace all the tab characters with spaces before calculating the request size, so 
	// some http server doesn't get all upset about the length being wrong.
	boost::replace_all(conn->request, "\t", "    ");
	msg << "Content-Length: " << conn->request.length() << "\r\n\r\n" << conn->request;

	conn->request = msg.str();

	if(g_http_fake_lag > 0) {
		connections_waiting_on_fake_lag_.push_back(std::pair<int,connection_ptr>(SDL_GetTicks()+g_http_fake_lag, conn));
	} else {
		write_connection_data(conn);
	}
}

void http_client::write_connection_data(connection_ptr conn)
{
	const auto bytes_to_send = conn->request.size() - conn->nbytes_sent;
	const auto nbytes = std::min<size_t>(bytes_to_send, 1024*64);

	const std::shared_ptr<std::string> msg(new std::string(conn->request.begin() + conn->nbytes_sent, conn->request.begin() + conn->nbytes_sent + nbytes));
	try {
		boost::asio::async_write(*conn->socket, boost::asio::buffer(*msg),
		      std::bind(&http_client::handle_send, this, conn, std::placeholders::_1, std::placeholders::_2, msg));
	} catch(std::exception& e) {
		LOG_ERROR("Error: exception in async_write: " << e.what() << "\n");
	}

}

void http_client::handle_send(connection_ptr conn, const boost::system::error_code& e, size_t nbytes, std::shared_ptr<std::string> buf_ptr)
{
	if(conn->aborted) {
		return;
	}

	if(e) {
		--in_flight_;

		const bool retry = conn->retry_fn && conn->retry_on_error > 0;
		LOG_INFO("http_client::handle_send: error: " << (void*)this << " @" << SDL_GetTicks() << " retry = " << retry << "\n");
		if(retry) {
			conn->retry_fn();
		} else if(conn->error_handler) {
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
		try {
			conn->socket->async_read_some(boost::asio::buffer(conn->buf), std::bind(&http_client::handle_receive, this, conn, std::placeholders::_1, std::placeholders::_2));
		} catch(std::exception& e) {
			LOG_ERROR("Error: handle_send: " << e.what() << "\n");
		}
	}
}

namespace http
{
	std::map<std::string, std::string> parse_http_headers(std::string& str)
	{
		str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());

		std::map<std::string, std::string> env;
		std::vector<std::string> lines = util::split(str, '\n');
		for(const std::string& line : lines) {
			if(line.empty()) {
				break;
			}

			std::string::const_iterator colon = std::find(line.begin(), line.end(), ':');
			if(colon == line.end() || colon+1 == line.end()) {
				continue;
			}

			std::string key(line.begin(), colon);
			const std::string value(colon+2, line.end());

			std::transform(key.begin(), key.end(), key.begin(), tolower);
			env[key] = value;
		}

		return env;
	}
}



void http_client::handle_receive(connection_ptr conn, const boost::system::error_code& e, size_t nbytes)
{
	if(conn->aborted) {
		return;
	}
	
	if(e) {
		--in_flight_;
		const bool retry = conn->retry_fn && conn->retry_on_error;
		LOG_INFO("http_client::handle_receive: error: " << (void*)this << " @" << SDL_GetTicks() << " retry = " << retry << "\n");
		if(retry) {
			conn->retry_fn();
			return;
		}

		LOG_ERROR("ERROR IN HTTP RECEIVE: (" << e.value() << "(" << e.message() << "), " << conn->response << ")");
		if(e.value() == boost::system::errc::no_such_file_or_directory) {
			LOG_ERROR("Error no such file or directory");
			const char* end_headers = strstr(conn->response.c_str(), "\n\n");
			const char* end_headers2 = strstr(conn->response.c_str(), "\r\n\r\n");
			if(end_headers2 && (end_headers == nullptr || end_headers2 < end_headers)) {
				end_headers = end_headers2 + 2;
			}

			if(end_headers) {
				LOG_ERROR("HEADERS: (((" << std::string(conn->response.c_str(), end_headers) << ")))");
				conn->handler(std::string(end_headers+2));
				conn->aborted = true;

				if(allow_keepalive_) {
					usable_connections_.push_back(conn->socket);
				}
				return;
			}
		}

		ASSERT_LOG(conn->error_handler, "Error in HTTP receive with no error handler");
		conn->error_handler("ERROR RECEIVING DATA");

		return;
	}

	//LOG_INFO("Received http data: " << nbytes << " bytes, expecting " << conn->expected_len << "\n");

	conn->response.insert(conn->response.end(), &conn->buf[0], &conn->buf[0] + nbytes);
	if(conn->headers.empty()) {
		int header_term_len = 2;
		const char* end_headers = strstr(conn->response.c_str(), "\n\n");
		const char* end_headers2 = strstr(conn->response.c_str(), "\r\n\r\n");
		if(end_headers2 && (end_headers == nullptr || end_headers2 < end_headers)) {
			end_headers = end_headers2;
			header_term_len = 4;
		}
		if(end_headers) {
			std::string header_str(conn->response.c_str(), end_headers);

			static const std::string content_length_str("content-length");

			conn->headers = http::parse_http_headers(header_str);

			auto itor = conn->headers.find(content_length_str);
			if(itor != conn->headers.end()) {
				const int payload_len = atoi(conn->headers[content_length_str].c_str());
				conn->expected_len = static_cast<int>((end_headers - conn->response.c_str()) + payload_len + header_term_len);
			}
		}
	}

	if(conn->expected_len != -1 && conn->response.size() >= static_cast<unsigned>(conn->expected_len)) {
		ASSERT_LOG(conn->expected_len == conn->response.size(), "UNEXPECTED RESPONSE SIZE " << conn->expected_len << " VS " << conn->response.size() << ": " << conn->response);

		//LOG_INFO("Received full http response\n");

		//We have the full response now -- handle it.
		const char* end_headers = strstr(conn->response.c_str(), "\n\n");
		int header_term_len = 2;
		const char* end_headers2 = strstr(conn->response.c_str(), "\r\n\r\n");
		if(end_headers2 && (end_headers == nullptr || end_headers2 < end_headers)) {
			end_headers = end_headers2;
			header_term_len = 4;
		}

		ASSERT_LOG(end_headers, "COULD NOT FIND END OF HEADERS IN MESSAGE: " << conn->response);
		//LOG_DEBUG("HEADERS: (((" << std::string(conn->response.c_str(), end_headers) << ")))");
		--in_flight_;
		const char* payload = end_headers + header_term_len;

		std::string payload_str(payload, conn->response.c_str() + conn->response.size());

		static const std::string encoding_str("content-encoding");
		auto encoding_itor = conn->headers.find(encoding_str);
		if(encoding_itor != conn->headers.end()) {
			std::string encoding = encoding_itor->second;
			std::transform(encoding.begin(), encoding.end(), encoding.begin(), tolower);
			if(encoding == "deflate") {
				std::vector<char> data(payload_str.begin(), payload_str.end());
				auto v = zip::decompress(data);
				std::string(v.begin(), v.end()).swap(payload_str);
			} else {
				ASSERT_LOG(encoding == "identity", "Unsupported HTTP encoding: " << encoding);
			}
		}

		LOG_INFO("http_client::handle_recv: " << (void*)this << " @" << SDL_GetTicks() << " payload_str.size() = " << payload_str.size() << "\n");

		conn->aborted = true;
		conn->handler(payload_str);
		usable_connections_.push_back(conn->socket);
	} else {
		if(conn->expected_len != -1 && conn->progress_handler) {
			conn->progress_handler(conn->response.size(), conn->expected_len, true);
		}

		try {
			conn->socket->async_read_some(boost::asio::buffer(conn->buf), std::bind(&http_client::handle_receive, this, conn, std::placeholders::_1, std::placeholders::_2));
		} catch(std::exception& e) {
			LOG_ERROR("Error in async_read_from: " << e.what() << "\n");
		}
	}
}

void http_client::process()
{
	while(connections_waiting_on_fake_lag_.empty() == false && connections_waiting_on_fake_lag_.front().first < SDL_GetTicks()) {
		write_connection_data(connections_waiting_on_fake_lag_.front().second);
		connections_waiting_on_fake_lag_.erase(connections_waiting_on_fake_lag_.begin());
	}

	connections_monitor_timeout_.erase(std::remove_if(connections_monitor_timeout_.begin(), connections_monitor_timeout_.end(), [](const std::weak_ptr<Connection>& ptr) { return ptr.lock().get() == nullptr || ptr.lock()->aborted; }), connections_monitor_timeout_.end());

	std::vector<connection_ptr> connections_to_monitor;
	
	for(auto& p : connections_monitor_timeout_) {
		auto conn = p.lock();
		if(!conn || !conn->retry_fn || conn->aborted) {
			continue;
		}

		connections_to_monitor.push_back(conn);
	}

	for(auto conn : connections_to_monitor) {
		if(conn->timeout_deadline >= 0 && SDL_GetTicks() > conn->timeout_deadline) {
			const size_t nbytes = conn->nbytes_sent + conn->response.size();
			LOG_INFO("HTTP client reached timeout: " << (void*)this << ": period = " << conn->timeout_period << " nbytes = " << (int)nbytes << " needed = " << (int)conn->timeout_nbytes_needed << " timeout = " << (nbytes < conn->timeout_nbytes_needed) << "\n");
			if(nbytes < conn->timeout_nbytes_needed) {
				LOG_INFO("HTTP client timed out: " << (void*)this << " resetting connection\n");
				if(resolution_state_ == RESOLUTION_IN_PROGRESS) {
					resolution_state_ = RESOLUTION_NOT_STARTED;
				}

				conn->aborted = true;
				conn->socket->close();
				--in_flight_;
				conn->retry_fn();
			}
			else {
				conn->timeout_deadline = SDL_GetTicks() + conn->timeout_period;
				conn->timeout_nbytes_needed = nbytes + 1024*16;
			}
		}
	}

	try {
		io_service_->poll();
		io_service_->reset();
	} catch(std::exception& e) {
		LOG_ERROR("Error in http client: " << e.what() << "\n");
	}
}

BEGIN_DEFINE_CALLABLE_NOBASE(http_client)
	DEFINE_FIELD(in_flight, "int")
		return variant(obj.in_flight_);
END_DEFINE_CALLABLE(http_client)

namespace {
bool g_done_test_http_client = false;
}

COMMAND_LINE_UTILITY(test_http_client) {
	http_client* client = new http_client("localhost", "23456");
	client->send_request("POST /server", "{}",
	  [](std::string response) { fprintf(stderr, "RESPONSE %d: %s\n", SDL_GetTicks(), response.c_str()); g_done_test_http_client = true; },
	  [](std::string error) { fprintf(stderr, "ERROR IN RESPONSE\n"); },
	  [](size_t a, size_t b, bool c) { fprintf(stderr, "PROGRESS...\n"); }
	);

	while(!g_done_test_http_client) {
		client->process();
	}
}
