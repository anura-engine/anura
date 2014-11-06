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
#include <boost/bind.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "asserts.hpp"
#include "foreach.hpp"
#include "json_parser.hpp"
#include "preferences.hpp"
#include "tbs_client.hpp"
#include "tbs_game.hpp"
#include "wml_formula_callable.hpp"

#if defined(_MSC_VER)
#define strtoll _strtoi64
#endif

namespace tbs {

PREF_BOOL(tbs_client_prediction, false, "Use client-side prediction for tbs games");

client::client(const std::string& host, const std::string& port,
               int session, boost::asio::io_service* service)
  : http_client(host, port, session, service), use_local_cache_(g_tbs_client_prediction),
    local_game_cache_(NULL), local_nplayer_(-1)
{
}

void client::send_request(variant request, game_logic::map_formula_callable_ptr callable, boost::function<void(std::string)> handler)
{
	handler_ = handler;
	callable_ = callable;

	std::string request_str = game_logic::serialize_doc_with_objects(request);
	//fprintf(stderr, "SEND ((%s))\n", request_str.c_str());

	http_client::send_request("POST /tbs", 
		request_str,
		boost::bind(&client::recv_handler, this, _1), 
		boost::bind(&client::error_handler, this, _1), 
		0);

	if(local_game_cache_ && request["type"].as_string() == "moves" && request["state_id"].as_int() == local_game_cache_->state_id()) {

		variant request_clone = game_logic::deserialize_doc_with_objects(request_str);

		local_game_cache_->handle_message(local_nplayer_, request_clone);
		std::vector<game::message> messages;
		local_game_cache_->swap_outgoing_messages(messages);
		foreach(const game::message& msg, messages) {
			std::cerr << "LOCAL: RECIPIENTS: ";
			for(int i = 0; i != msg.recipients.size(); ++i) {
				std::cerr << msg.recipients[i] << " ";
			}
			std::cerr << "\n";
			if(std::count(msg.recipients.begin(), msg.recipients.end(), local_nplayer_)) {
				local_responses_.push_back(msg.contents);
			}
		}
		std::cerr << "LOCAL: HANDLE MESSAGE LOCALLY: " << local_responses_.size() << "/" << messages.size() << "\n";
	}
}

void client::recv_handler(const std::string& msg)
{
	if(handler_) {
		variant v = game_logic::deserialize_doc_with_objects(msg);

		if(use_local_cache_ && v["type"].as_string() == "game") {
			local_game_cache_ = new tbs::game(v["game_type"].as_string(), v);
			local_game_cache_holder_.reset(local_game_cache_);

			local_nplayer_= v["nplayer"].as_int();
			std::cerr << "LOCAL: UPDATE CACHE: " << local_game_cache_->state_id() << "\n";
			v = game_logic::deserialize_doc_with_objects(msg);
		}

		//fprintf(stderr, "RECV: (((%s)))\n", msg.c_str());
		//fprintf(stderr, "SERIALIZE: (((%s)))\n", v.write_json().c_str());
		callable_->add("message", v);

//		try {
			handler_(connection_id_ + "message_received");
//		} catch(...) {
//			std::cerr << "ERROR PROCESSING TBS MESSAGE\n";
//			throw;
//		}
	}
}

void client::error_handler(const std::string& err)
{
	std::cerr << "ERROR IN TBS CLIENT: " << err << (handler_ ? " SENDING TO HANDLER...\n" : " NO HANDLER\n");
	if(handler_) {
		variant v;
		try {
			v = json::parse(err, json::JSON_NO_PREPROCESSOR);
		} catch(const json::parse_error&) {
			std::cerr << "Unable to parse message \"" << err << "\" assuming it is a string." << std::endl;
		}
		callable_->add("error", v.is_null() ? variant(err) : v);
		handler_(connection_id_ + "connection_error");
	}
}

variant client::get_value(const std::string& key) const
{
	return http_client::get_value(key);
}

void client::process()
{
	std::vector<std::string> local_responses;
	local_responses.swap(local_responses_);
	foreach(const std::string& response, local_responses) {
		std::cerr << "LOCAL: PROCESS LOCAL RESPONSE: " << response.size() << "\n";
		recv_handler(response);
	}

	http_client::process();
}

void client::set_id(const std::string& id)
{
	connection_id_ = id;
	if(id != "") {
		connection_id_ += "_";
	}
}

}
