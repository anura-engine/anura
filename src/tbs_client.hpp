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

#include "http_client.hpp"

namespace tbs 
{
	using boost::asio::ip::tcp;

	class game;

	class client : public http_client
	{
	public:
		client(const std::string& host, const std::string& port, int session=-1, boost::asio::io_service* service=nullptr);

		void send_request(variant request, 
			game_logic::MapFormulaCallablePtr callable, 
			std::function<void(std::string)> handler);

		virtual void process() override;
		void setId(const std::string& id);

		void set_use_local_cache(bool value) { use_local_cache_ = value; }
	private:
		std::function<void(std::string)> handler_;
		game_logic::MapFormulaCallablePtr callable_;

		void recv_handler(const std::string& msg);
		void error_handler(const std::string& err);
		variant getValue(const std::string& key) const override;

		void handle_message(variant node);

		std::string connection_id_;

		bool use_local_cache_;
		tbs::game* local_game_cache_;
		ffl::IntrusivePtr<game_logic::FormulaCallable> local_game_cache_holder_;
		int local_nplayer_;

		std::vector<std::string> local_responses_;
	};
}
