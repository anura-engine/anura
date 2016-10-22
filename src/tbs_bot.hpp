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

#include <boost/asio.hpp>

#include <string>
#include <vector>

#include "formula.hpp"
#include "formula_callable.hpp"
#include "tbs_client.hpp"
#include "tbs_internal_client.hpp"
#include "tbs_ipc_client.hpp"
#include "variant.hpp"

namespace tbs 
{
class tbs_bot_timer_proxy;

class bot : public game_logic::FormulaCallable
	{
	public:
		bot(boost::asio::io_service& io_service, const std::string& host, const std::string& port, variant v);
		~bot();

		void set_ipc_client(ffl::IntrusivePtr<ipc_client> ipc_client) { ipc_client_ = ipc_client; }

		void process(const boost::system::error_code& error);

		void surrenderReferences(GarbageCollector* collector) override;

	private:
		DECLARE_CALLABLE(bot)
		variant getValueDefault(const std::string& key) const override;
		void handle_response(const std::string& type, game_logic::FormulaCallablePtr callable);

		variant generate_report() const;

		int session_id_;

		std::string host_, port_;
		std::vector<variant> script_;
		int response_pos_;
		int script_pos_;
		ffl::IntrusivePtr<client> client_;
		ffl::IntrusivePtr<ipc_client> ipc_client_;

		boost::asio::io_service& service_;
		boost::asio::deadline_timer timer_;

		game_logic::FormulaPtr on_create_, on_message_;

		variant data_;

		std::string message_type_;
		game_logic::FormulaCallablePtr message_callable_;

		bool has_quit_;

		tbs_bot_timer_proxy* timer_proxy_;
	};
}
