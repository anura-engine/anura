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
#include "json_parser.hpp"
#include "preferences.hpp"
#include "tbs_client.hpp"
#include "tbs_game.hpp"
#include "wml_formula_callable.hpp"

#if defined(_MSC_VER)
#define strtoll _strtoi64
#endif

namespace tbs 
{
	PREF_BOOL(tbs_client_prediction, false, "Use client-side prediction for tbs games");
	PREF_INT(tbs_fake_error_rate, 0, "Percentage error rate for tbs connections; used to debug issues");

	client::client(const std::string& host, const std::string& port,
				   int session, boost::asio::io_service* service)
	  : http_client(host, port, session, service), use_local_cache_(g_tbs_client_prediction),
		local_game_cache_(nullptr), local_nplayer_(-1)
	{
	}

	void client::send_request(variant request, game_logic::MapFormulaCallablePtr callable, std::function<void(std::string)> handler)
	{
		using std::placeholders::_1;

		handler_ = handler;
		callable_ = callable;

		std::string request_str = game_logic::serialize_doc_with_objects(request).write_json();

		http_client::send_request("POST /tbs", 
			request_str,
			std::bind(&client::recv_handler, this, _1), 
			std::bind(&client::error_handler, this, _1), 
			[](size_t,size_t,bool){});

		if(local_game_cache_ && request["type"].as_string() == "moves" && request["state_id"].as_int() == local_game_cache_->state_id()) {

			variant request_clone = game_logic::deserialize_doc_with_objects(request_str);

			local_game_cache_->handle_message(local_nplayer_, request_clone);
			std::vector<game::message> messages;
			local_game_cache_->swap_outgoing_messages(messages);
			for(const game::message& msg : messages) {
				std::ostringstream ss;
				ss << "LOCAL: RECIPIENTS:";
				for(int i = 0; i != msg.recipients.size(); ++i) {
					ss << " " << msg.recipients[i];
				}
				LOG_INFO(ss.str());
				if(std::count(msg.recipients.begin(), msg.recipients.end(), local_nplayer_)) {
					local_responses_.push_back(msg.contents);
				}
			}
			LOG_INFO("LOCAL: HANDLE MESSAGE LOCALLY: " << local_responses_.size() << "/" << messages.size());
		}
	}

	void client::recv_handler(const std::string& msg)
	{
		if(handler_) {
			variant v;
			
			try {
				const assert_recover_scope guard;
				v = game_logic::deserialize_doc_with_objects(msg);
			} catch(validation_failure_exception& e) {
				error_handler("FSON Parse error");
				return;
			} catch(json::ParseError& e) {
				error_handler("FSON Parse error");
				return;
			}

			if(g_tbs_fake_error_rate > 0 && rand()%100 < g_tbs_fake_error_rate) {
				error_handler("Fake error");
				return;
			}

			static const std::string UnderTypeStr = "__type";
			static const variant MultiMessageVariant("multimessage");

			if(v.is_map() && v[UnderTypeStr] == MultiMessageVariant) {
				for(const std::string& s : v["items"].as_list_string()) {
					recv_handler(s);
				}
				return;
			}

			if(use_local_cache_ && v["type"].as_string() == "game") {
				//local cache currently disabled.
				//local_game_cache_ = new tbs::game(v["game_type"].as_string(), v);
				//local_game_cache_holder_.reset(local_game_cache_);

				local_nplayer_= v["nplayer"].as_int();
				LOG_INFO("LOCAL: UPDATE CACHE: " << local_game_cache_->state_id());
				v = game_logic::deserialize_doc_with_objects(msg);
			}

			handle_message(v);
		}
	}

	void client::handle_message(variant v)
	{
		if(v.is_map() && v["__message_bundle"].as_bool(false)) {
			for(variant msg : v["__messages"].as_list()) {
				handle_message(msg);
			}
			return;
		}

		callable_->add("message", v);

		handler_(connection_id_ + "message_received");
	}

	void client::error_handler(const std::string& err)
	{
		LOG_ERROR("ERROR IN TBS CLIENT: " << err << (handler_ ? " SENDING TO HANDLER..." : " NO HANDLER"));
		if(handler_) {
			variant v;
			try {
				v = json::parse(err, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
			} catch(const json::ParseError&) {
				LOG_ERROR("Unable to parse message \"" << err << "\" assuming it is a string.");
			}
			callable_->add("error", v.is_null() ? variant(err) : v);
			handler_(connection_id_ + "connection_error");
		}
	}

	variant client::getValue(const std::string& key) const
	{
		return http_client::getValue(key);
	}

	void client::process()
	{
		std::vector<std::string> local_responses;
		local_responses.swap(local_responses_);
		for(const std::string& response : local_responses) {
			LOG_INFO("LOCAL: PROCESS LOCAL RESPONSE: " << response.size());
			recv_handler(response);
		}

		http_client::process();
	}

	void client::setId(const std::string& id)
	{
		connection_id_ = id;
		if(id != "") {
			connection_id_ += "_";
		}
	}
}
