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

#include "asserts.hpp"
#include "formula.hpp"
#include "preferences.hpp"
#include "tbs_bot.hpp"
#include "tbs_web_server.hpp"

namespace tbs
{
	PREF_INT(tbs_bot_delay_ms, 100, "Artificial delay for tbs bots");

	bot::bot(boost::asio::io_service& service, const std::string& host, const std::string& port, variant v)
	  : service_(service), 
		timer_(service), 
		host_(host), 
		port_(port), 
		script_(v["script"].as_list()),
		on_create_(game_logic::Formula::createOptionalFormula(v["on_create"])),
		on_message_(game_logic::Formula::createOptionalFormula(v["on_message"]))

	{
		LOG_INFO("CREATE BOT");
		timer_.expires_from_now(boost::posix_time::milliseconds(g_tbs_bot_delay_ms));
		timer_.async_wait(std::bind(&bot::process, this, std::placeholders::_1));
	}

	bot::~bot()
	{
		timer_.cancel();
	}

	void bot::process(const boost::system::error_code& error)
	{
		if(error == boost::asio::error::operation_aborted) {
			LOG_INFO("tbs::bot::process cancelled");
			return;
		}
		if(on_create_) {
			executeCommand(on_create_->execute(*this));
			on_create_.reset();
		}

		if(((!client_ && !preferences::internal_tbs_server()) || (!internal_client_ && preferences::internal_tbs_server()))
			&& response_.size() < script_.size()) {
			variant script = script_[response_.size()];
			variant send = script["send"];
			if(send.is_string()) {
				send = game_logic::Formula(send).execute(*this);
			}

			int session_id = -1;
			if(script.has_key("session_id")) {
				session_id = script["session_id"].as_int();
			}

			ASSERT_LOG(send.is_map(), "NO REQUEST TO SEND: " << send.write_json() << " IN " << script.write_json());
			game_logic::MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable(this));
			if(preferences::internal_tbs_server()) {
				internal_client_.reset(new internal_client(session_id));
				internal_client_->send_request(send, session_id, callable, std::bind(&bot::handle_response, this, std::placeholders::_1, callable));
			} else {
				client_.reset(new client(host_, port_, session_id, &service_));
				client_->set_use_local_cache(false);
				client_->send_request(send, callable, std::bind(&bot::handle_response, this, std::placeholders::_1, callable));
			}
		}

		timer_.expires_from_now(boost::posix_time::milliseconds(g_tbs_bot_delay_ms));
		timer_.async_wait(std::bind(&bot::process, this, std::placeholders::_1));
	}

	void bot::handle_response(const std::string& type, game_logic::FormulaCallablePtr callable)
	{
		if(on_create_) {
			executeCommand(on_create_->execute(*this));
			on_create_.reset();
		}

		if(on_message_) {
			message_type_ = type;
			message_callable_ = callable;
			executeCommand(on_message_->execute(*this));
		}

		ASSERT_LOG(type != "connection_error", "GOT ERROR BACK WHEN SENDING REQUEST: " << callable->queryValue("message").write_json());

		ASSERT_LOG(type == "message_received", "UNRECOGNIZED RESPONSE: " << type);

		variant script = script_[response_.size()];
		std::vector<variant> validations;
		if(script.has_key("validate")) {
			variant validate = script["validate"];
			for(int n = 0; n != validate.num_elements(); ++n) {
				std::map<variant,variant> m;
				m[variant("validate")] = validate[n][variant("expression")] + variant(" EQUALS ") + validate[n][variant("equals")];

				game_logic::Formula f(validate[n][variant("expression")]);
				variant expression_result = f.execute(*callable);

				if(expression_result != validate[n][variant("equals")]) {
					m[variant("error")] = variant(1);
				}
				m[variant("value")] = expression_result;

				validations.push_back(variant(&m));
			}
		}

		std::map<variant,variant> m;
		m[variant("message")] = callable->queryValue("message");
		m[variant("validations")] = variant(&validations);

		response_.push_back(variant(&m));
		client_.reset();
		internal_client_.reset();

		tbs::web_server::set_debug_state(generate_report());
	}

	variant bot::getValueDefault(const std::string& key) const
	{
		if(message_callable_) {
			return message_callable_->queryValue(key);
		}
		return variant();
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(bot)
		DEFINE_FIELD(script, "[any]")
			std::vector<variant> s = obj.script_;
			return variant(&s);
		DEFINE_SET_FIELD
			obj.script_ = value.as_list();

		DEFINE_FIELD(data, "any")
			return variant(obj.data_);
		DEFINE_SET_FIELD
			obj.data_ = value;

		DEFINE_FIELD(type, "string")
			return variant(obj.message_type_);

		DEFINE_FIELD(me, "any")
			return variant(&obj_instance);
	END_DEFINE_CALLABLE(bot)

	variant bot::generate_report() const
	{
		std::map<variant,variant> m;
		std::vector<variant> response = response_;
		m[variant("response")] = variant(&response);
		return variant(&m);
	}
}
