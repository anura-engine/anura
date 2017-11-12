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

#include <cstddef>
#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
#include <deque>
#include <iostream>

#include <SDL.h>

#if !defined(_MSC_VER)
#include <sys/time.h>
#endif

#include "asserts.hpp"
#include "compress.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "json_parser.hpp"
#include "http_server.hpp"
#include "string_utils.hpp"
#include "utils.hpp"
#include "unit_test.hpp"
#include "variant.hpp"

using boost::asio::ip::tcp;

namespace http 
{
	namespace {
		typedef web_server::socket_ptr socket_ptr;
	}
	struct WebServerProxyInfo {
		WebServerProxyInfo(web_server& server, uint32_t session_id, boost::asio::io_service& io_service, const std::string& host, const std::string& port);
		web_server* server;
		uint32_t session_id;
		std::string host;
		std::string port;
		std::shared_ptr<tcp::resolver> resolver;
		std::shared_ptr<tcp::resolver::query> resolver_query;
		tcp::resolver::iterator endpoint_iterator;
		socket_ptr socket;
	};

	namespace {
		typedef std::shared_ptr<WebServerProxyInfo> WebServerProxyInfoPtr;

		void proxy_connect(WebServerProxyInfoPtr info);

		void handle_proxy_send(WebServerProxyInfoPtr info, std::shared_ptr<std::string> msg, const boost::system::error_code& error, size_t nbytes)
		{
			if(error) {
				LOG_ERROR("Proxy send failed: " << info->host << ":" << info->port);
			} else {
				if(info->server) {
					info->server->start_receive(info->socket);
				}
			}
		}

		void handle_proxy_connect(WebServerProxyInfoPtr info, const boost::system::error_code& error)
		{
			if(error) {
				++(info->endpoint_iterator);
				proxy_connect(info);
			} else {
				std::shared_ptr<std::string> msg(new std::string);
				msg->resize(4);
				memcpy(&(*msg)[0], &info->session_id, 4);
				boost::asio::async_write(info->socket->socket, boost::asio::buffer(*msg),
				  std::bind(handle_proxy_send, info, msg, std::placeholders::_1, std::placeholders::_2));
			}
		}

		void proxy_connect(WebServerProxyInfoPtr info)
		{
			if(info->endpoint_iterator == tcp::resolver::iterator()) {
				LOG_ERROR("Failed to connect to proxy: " << info->host);
				return;
			}

			boost::asio::async_connect(info->socket->socket,
			  info->endpoint_iterator,
			  std::bind(&handle_proxy_connect, info, std::placeholders::_1));
		}

		void handle_resolve_proxy(WebServerProxyInfoPtr info, const boost::system::error_code& error, tcp::resolver::iterator endpoint_iterator)
		{
			if(!error) {
				info->endpoint_iterator = endpoint_iterator;
				proxy_connect(info);

			} else {
				LOG_ERROR("Could not resolve proxy server: " << info->host << ":" << info->port);
			}
		}

		std::shared_ptr<WebServerProxyInfo> create_web_server_proxy(web_server& server_, uint32_t session_id_, boost::asio::io_service& io_service_, const std::string& host_, const std::string& port_)
		{
			std::shared_ptr<WebServerProxyInfo> result(new WebServerProxyInfo(server_, session_id_,io_service_, host_, port_));
			result->resolver->async_resolve(*result->resolver_query,
		  	   std::bind(handle_resolve_proxy, result, std::placeholders::_1, std::placeholders::_2));
			return result;
		}
	}

	WebServerProxyInfo::WebServerProxyInfo(web_server& server_, uint32_t session_id_, boost::asio::io_service& io_service_, const std::string& host_, const std::string& port_)
	  : server(&server_), session_id(session_id_), host(host_), port(port_),
	    resolver(new tcp::resolver(io_service_)),
		resolver_query(new tcp::resolver::query(tcp::resolver::query::protocol_type::v4(), host_.c_str(), port_.c_str())),
		socket(new web_server::SocketInfo(io_service_))
	{
	}

	web_server::SocketInfo::SocketInfo(boost::asio::io_service& service)
	  : socket(service), client_version(0), supports_deflate(false)
	{
	}

	web_server::web_server(boost::asio::io_service& io_service, int port)
	  : io_service_(io_service)
	{
		if(port) {
			acceptor_.reset(new boost::asio::ip::tcp::acceptor(io_service, tcp::endpoint(tcp::v4(), port)));
			boost::asio::socket_base::reuse_address option(true);
			acceptor_->set_option(option);
		}

		start_accept();
	}

	web_server::~web_server()
	{
		if(acceptor_) {
			acceptor_->close();
		}

		for(auto p : proxies_) {
			p->server = nullptr;
		}
	}

	void web_server::connect_proxy(uint32_t session_id, const std::string& host, const std::string& port)
	{
		std::shared_ptr<WebServerProxyInfo> proxy = create_web_server_proxy(*this, session_id, io_service_, host, port);
		proxies_.push_back(proxy);
		
	}

	void web_server::start_accept()
	{
		if(!acceptor_) {
			return;
		}

		socket_ptr socket(new SocketInfo(io_service_));
		acceptor_->async_accept(socket->socket, std::bind(&web_server::handle_accept, this, socket, std::placeholders::_1));
	}

	namespace {
	int nconnections = 0;
	}

	void web_server::handle_accept(socket_ptr socket, const boost::system::error_code& error)
	{
		if(error) {
			LOG_ERROR("ERROR IN ACCEPT");
			return;
		}

		start_receive(socket);
		start_accept();
	}

	void web_server::keepalive_socket(socket_ptr socket)
	{
		start_receive(socket);
	}

	void web_server::start_receive(socket_ptr socket, receive_buf_ptr recv_buf)
	{
		if(!recv_buf) {
			recv_buf.reset(new receive_buf);
		}

		buffer_ptr buf(new boost::array<char, 64*1024>);
		socket->socket.async_read_some(boost::asio::buffer(*buf), std::bind(&web_server::handle_receive, this, socket, buf, std::placeholders::_1, std::placeholders::_2, recv_buf));
	}

	void web_server::handle_receive(socket_ptr socket, buffer_ptr buf, 
		const boost::system::error_code& e, 
		size_t nbytes, 
		receive_buf_ptr recv_buf)
	{
		if(e) {
			//TODO: handle error
			LOG_ERROR("SOCKET ERROR: " << e.message());
			disconnect(socket);
			return;
		}

		handle_incoming_data(socket, &(*buf)[0], &(*buf)[0] + nbytes, recv_buf);
	}

	void web_server::handle_incoming_data(socket_ptr socket, const char* i1, const char* i2, receive_buf_ptr recv_buf)
	{
		recv_buf->msg += std::string(i1, i2);
		LOG_INFO("HANDLE INCOMING: " << (int)recv_buf->msg.size() << " / " << (int)recv_buf->wanted);

		if(recv_buf->wanted > 0 && recv_buf->msg.size() < recv_buf->wanted) {
			start_receive(socket, recv_buf);
			return;
		}

		timeval before, after;
		gettimeofday(&before, nullptr);
		handle_message(socket, recv_buf);
		gettimeofday(&after, nullptr);

		const int ms = (after.tv_sec - before.tv_sec)*1000 + (after.tv_usec - before.tv_usec)/1000;
		LOG_INFO("handle_incoming_data time: " << ms << "ms");
	}

	namespace {
	struct Request {
		std::string path;
		std::map<std::string, std::string> args;
	};

	Request parse_request(const std::string& str) {
		std::string::const_iterator end_path = std::find(str.begin(), str.end(), '?');
		Request request;
		request.path.assign(str.begin(), end_path);

		LOG_INFO("PATH: '" << request.path << "'");

		if(end_path != str.end()) {
			++end_path;

			std::vector<std::string> args = util::split(std::string(end_path, str.end()), '&');
			for(const std::string& a : args) {
				std::string::const_iterator equal_itor = std::find(a.begin(), a.end(), '=');
				if(equal_itor != a.end()) {
					const std::string name(a.begin(), equal_itor);
					const std::string value(equal_itor+1, a.end());
					request.args[name] = value;
				}
			}
		}

		return request;
	}

	}

	void web_server::handle_message(socket_ptr socket, receive_buf_ptr recv_buf)
	{
		for(auto& p : proxies_) {
			if(p->socket == socket) {
				p->socket.reset(new SocketInfo(io_service_));
				proxy_connect(p);
				break;
			}
		}

		const std::string& msg = recv_buf->msg;
		if(msg.size() < 16) {
			LOG_INFO("CLOSESOCKB");
			disconnect(socket);
			return;
		}

		if(std::equal(msg.begin(), msg.begin()+5, "POST ")) {

			const char* payload = nullptr;
			const char* payload1 = strstr(msg.c_str(), "\n\n");
			const char* payload2 = strstr(msg.c_str(), "\r\n\r\n");
			if(payload1) {
				payload1 += 2;
				payload = payload1;
			}

			if(payload2 && (!payload1 || payload2 < payload1)) {
				payload2 += 4;
				payload = payload2;
			}

			std::string headers;
			if(payload) {
				headers = std::string(msg.c_str(), payload);
			} else {
				headers = msg;
			}

			environment env = parse_http_headers(headers);

			static const std::string UserAgentStr("user-agent");
			auto user_agent_itor = env.find(UserAgentStr);
			if(user_agent_itor != env.end()) {
				const char* p = strstr(user_agent_itor->second.c_str(), " 1.");
				if(p != nullptr) {
					p += 3;

					socket->client_version = atoi(p);
				}
			}

			static const std::string AcceptEncodingStr("accept-encoding");
			auto encoding_itor = env.find(AcceptEncodingStr);
			if(encoding_itor != env.end()) {
				if(strstr(encoding_itor->second.c_str(), "deflate") || strstr(encoding_itor->second.c_str(), "Deflate")) {
					socket->supports_deflate = true;
				}
			}

			const int content_length = atoi(env["content-length"].c_str());
			LOG_DEBUG("PARSE content-length: " << content_length);

			const auto payload_len = payload ? (msg.c_str() + msg.size() - payload) : 0;

			LOG_DEBUG("PAYLOAD LEN: " << payload_len << " < " << content_length);
			if(!payload || payload_len < content_length) {
				if(payload_len) {
					recv_buf->wanted = msg.size() + (content_length - payload_len);
				}
				start_receive(socket, recv_buf);
				return;
			}

			variant doc;

			try {
				doc = parse_message(std::string(payload, payload + payload_len));
			} catch(json::ParseError& e) {
				LOG_ERROR("ERROR PARSING JSON: " << e.errorMessage());
				sys::write_file("./error_payload2.txt", std::string(payload));
			} catch(...) {
				LOG_ERROR("UNKNOWN ERROR PARSING JSON");
			}

			if(!doc.is_null()) {
				handlePost(socket, doc, env, msg);
				return;
			}
		} else if(std::equal(msg.begin(), msg.begin()+4, "GET ")) {
			std::string::const_iterator begin_url = msg.begin() + 4;
			std::string::const_iterator end_url = std::find(begin_url, msg.end(), ' ');
			std::string::const_iterator begin_args = std::find(begin_url, end_url, '?');
			std::map<std::string, std::string> args;
			std::string url_base(begin_url, begin_args);
			if(begin_args != end_url) {
				begin_args++;
				while(begin_args != end_url) {
					std::string::const_iterator eq = std::find(begin_args, end_url, '=');
					if(eq == end_url) {
						break;
					}

					std::string::const_iterator amp = std::find(eq, end_url, '&');
					std::string name(begin_args, eq);
					std::string value(eq+1, amp);
					args[name] = value;

					begin_args = amp;
					if(begin_args == end_url) {
						break;
					}

					++begin_args;
				}
			}

			handleGet(socket, url_base, args);

			return;
		}

		disconnect(socket);
	}

	void web_server::handle_send(socket_ptr socket, const boost::system::error_code& e, size_t nbytes, size_t max_bytes, std::shared_ptr<std::string> buf)
	{
		if(e) {
			disconnect(socket);
		} else if(nbytes == max_bytes) {
			//LOG_INFO("COMPLETE_MSG(((" << *buf << ")))");
			keepalive_socket(socket);
		}
	}

	void web_server::disconnect_socket(socket_ptr socket)
	{
		socket->socket.close();
		--nconnections;
	}

	void web_server::disconnect(socket_ptr socket)
	{
		for(auto& p : proxies_) {
			if(p->socket == socket) {
				p->socket.reset(new SocketInfo(io_service_));
				proxy_connect(p);
				break;
			}
		}

		disconnect_socket(socket);
	}

	void web_server::send_msg(socket_ptr socket, const std::string& type, const std::string& msg_ref, const std::string& header_parms)
	{
		std::string compressed_buf;
		std::string compress_header;
		const std::string* msg_ptr = &msg_ref;
		if(socket->supports_deflate && msg_ref.size() > 1024 && (header_parms.empty() || strstr(header_parms.c_str(), "Content-Encoding") == nullptr)) {
			compressed_buf = zip::compress(msg_ref);
			msg_ptr = &compressed_buf;

			compress_header = "Content-Encoding: deflate\r\n"; 
		}

		const std::string& msg = *msg_ptr;

		std::stringstream buf;
		buf <<
			"HTTP/1.1 200 OK\r\n"
			"Date: " << get_http_datetime() << "\r\n"
			"Connection: close\r\n"
			"Server: Wizard/1.0\r\n"
			"Accept-Ranges: bytes\r\n"
			"Access-Control-Allow-Origin: *\r\n"
			"Content-Type: " << type << "\r\n"
			"Content-Length: " << std::dec << (int)msg.size() << "\r\n" <<
			compress_header <<
			"Last-Modified: " << get_http_datetime() << "\r\n" <<
			(header_parms.empty() ? "" : header_parms + "\r\n")
			<< "\r\n";

		std::shared_ptr<std::string> str(new std::string(buf.str()));
		*str += msg;

		boost::asio::async_write(socket->socket, boost::asio::buffer(*str),
								 std::bind(&web_server::handle_send, this, socket, std::placeholders::_1, std::placeholders::_2, str->size(), str));
	}

	void web_server::send_404(socket_ptr socket)
	{
		std::stringstream buf;
		buf << 
			"HTTP/1.1 404 NOT FOUND\r\n"
			"Date: " << get_http_datetime() << "\r\n"
			"Connection: close\r\n"
			"Server: Wizard/1.0\r\n"
			"Accept-Ranges: none\r\n"
			"\r\n";
		std::shared_ptr<std::string> str(new std::string(buf.str()));
		boost::asio::async_write(socket->socket, boost::asio::buffer(*str),
					std::bind(&web_server::handle_send, this, socket, std::placeholders::_1, std::placeholders::_2, str->size(), str));
	}

	variant web_server::parse_message(const std::string& msg) const
	{
		return json::parse(msg, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
	}

}

namespace {
using namespace http;
class test_web_server : public http::web_server {
public:
	test_web_server(boost::asio::io_service& io_service) : web_server(io_service) {}
	void handlePost(socket_ptr socket, variant doc, const environment& env, const std::string& raw_msg) override {

		send_msg(socket, "text/json", "{ \"type\": \"ok\" }", "");
	}
	void handleGet(socket_ptr socket, const std::string& url, const std::map<std::string, std::string>& args) override {
		send_msg(socket, "text/json", "{ \"type\": \"ok\" }", "");
	}

private:
};
}

COMMAND_LINE_UTILITY(test_http_server) {
	using namespace http;

	boost::asio::io_service io_service;
	test_web_server server(io_service);


	io_service.run();
}
