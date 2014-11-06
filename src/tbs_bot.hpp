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
#ifndef TBS_BOT_HPP_INCLUDED
#define TBS_BOT_HPP_INCLUDED

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

#include "formula.hpp"
#include "formula_callable.hpp"
#include "tbs_client.hpp"
#include "tbs_internal_client.hpp"
#include "variant.hpp"

namespace tbs {

class tbs_bot_timer_proxy;

class bot : public game_logic::formula_callable
{
public:
	bot(boost::asio::io_service& io_service, const std::string& host, const std::string& port, variant v);
	~bot();

	void process(const boost::system::error_code& error);

private:
	void handle_response(const std::string& type, game_logic::formula_callable_ptr callable);
	variant get_value(const std::string& key) const;
	void set_value(const std::string& key, const variant& value);

	variant generate_report() const;

	int session_id_;

	std::string host_, port_;
	std::vector<variant> script_;
	std::vector<variant> response_;
	boost::shared_ptr<client> client_;
	boost::shared_ptr<internal_client> internal_client_;

	boost::asio::io_service& service_;
	boost::asio::deadline_timer timer_;

	game_logic::formula_ptr on_create_, on_message_;

	variant data_;

	std::string message_type_;
	game_logic::formula_callable_ptr message_callable_;

	bool has_quit_;

	tbs_bot_timer_proxy* timer_proxy_;
};

}

#endif
