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
#ifndef TBS_CLIENT_HPP_INCLUDED
#define TBS_CLIENT_HPP_INCLUDED

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

#include "formula_callable.hpp"
#include "http_client.hpp"

namespace tbs {
using boost::asio::ip::tcp;

class game;

class client : public http_client
{
public:
	client(const std::string& host, const std::string& port, int session=-1, boost::asio::io_service* service=NULL);

	void send_request(variant request, 
		game_logic::map_formula_callable_ptr callable, 
		boost::function<void(std::string)> handler);

	virtual void process();
	void set_id(const std::string& id);

	void set_use_local_cache(bool value) { use_local_cache_ = value; }
private:
	boost::function<void(std::string)> handler_;
	game_logic::map_formula_callable_ptr callable_;

	void recv_handler(const std::string& msg);
	void error_handler(const std::string& err);
	variant get_value(const std::string& key) const;

	std::string connection_id_;

	bool use_local_cache_;
	tbs::game* local_game_cache_;
	boost::intrusive_ptr<game_logic::formula_callable> local_game_cache_holder_;
	int local_nplayer_;

	std::vector<std::string> local_responses_;
};

}

#endif
