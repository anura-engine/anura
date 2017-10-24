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
#include <map>

#include "variant.hpp"

namespace http 
{
	typedef std::map<std::string, std::string> environment;

	std::map<std::string, std::string> parse_http_headers(std::string& str);

	struct WebServerProxyInfo;

	class web_server
	{
	public:

		struct SocketInfo {
			explicit SocketInfo(boost::asio::io_service& service);
			boost::asio::ip::tcp::socket socket;
			int client_version;
			bool supports_deflate;
		};

		typedef std::shared_ptr<SocketInfo> socket_ptr;
		typedef std::shared_ptr<boost::array<char, 64*1024> > buffer_ptr;

		explicit web_server(boost::asio::io_service& io_service, int port=23456);
		virtual ~web_server();

		void connect_proxy(uint32_t session_id, const std::string& host, const std::string& port);

		void keepalive_socket(socket_ptr socket);

		static void disconnect_socket(socket_ptr socket);

	protected:
		void start_accept();
		void handle_accept(socket_ptr socket, const boost::system::error_code& error);

		void send_msg(socket_ptr socket, const std::string& mime_type, const std::string& msg, const std::string& header_parms);
		void send_404(socket_ptr socket);

		void handle_send(socket_ptr socket, const boost::system::error_code& e, size_t nbytes, size_t max_bytes, std::shared_ptr<std::string> buf);

		virtual void disconnect(socket_ptr socket);

		virtual void handlePost(socket_ptr socket, variant doc, const environment& env, const std::string& raw_msg) = 0;
		virtual void handleGet(socket_ptr socket, const std::string& url, const std::map<std::string, std::string>& args) = 0;

	private:

		std::vector<std::shared_ptr<WebServerProxyInfo>> proxies_;

		struct receive_buf {
			receive_buf() : wanted(0) {}
			std::string msg;
			size_t wanted;
		};
		typedef std::shared_ptr<receive_buf> receive_buf_ptr;

	public:
		void start_receive(socket_ptr socket, receive_buf_ptr buf=receive_buf_ptr());
	
	private:

		void handle_receive(socket_ptr socket, buffer_ptr buf, const boost::system::error_code& e, size_t nbytes, receive_buf_ptr recv_buf);
		void handle_incoming_data(socket_ptr socket, const char* i1, const char* i2, receive_buf_ptr recv_buf);

		void handle_message(socket_ptr socket, receive_buf_ptr recv_buf);

		virtual variant parse_message(const std::string& msg) const;

		boost::asio::io_service& io_service_;
		std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
	};
}
