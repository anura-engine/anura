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

#include "http_server.hpp"

class web_server : public http::web_server
{
public:
	explicit web_server(boost::asio::io_service& io_service, int port=23456);
private:
	void heartbeat();

	virtual void handlePost(socket_ptr socket, variant doc, const http::environment& env, const std::string& raw_msg) override;
	virtual void handleGet(socket_ptr socket, const std::string& url, const std::map<std::string, std::string>& args) override;

	boost::asio::deadline_timer timer_;
	int nheartbeat_;
};

