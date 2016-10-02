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

#include <SDL.h>

#include <boost/bind.hpp>
#include <cstdint>

#include "asserts.hpp"
#include "formula.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "tbs_bot.hpp"
#include "tbs_web_server.hpp"

namespace tbs
{
class tbs_bot_timer_proxy {
public:
	explicit tbs_bot_timer_proxy(tbs::bot* bot) : bot_(bot)
	{}

	void cancel()
	{
		bot_ = nullptr;
	}

	void signal(const boost::system::error_code& error)
	{
		if(bot_) {
			bot_->process(error);
		}
		delete this;
	}
private:
	tbs::bot* bot_;
};

	PREF_INT(tbs_bot_delay_ms, 20, "Artificial delay for tbs bots");

	bot::bot(boost::asio::io_service& service, const std::string& host, const std::string& port, variant v)
  		: session_id_(v["session_id"].as_int()), 
  		  service_(service), 
  		  timer_(service), 
  		  host_(host), 
  		  port_(port), 
  		  script_(v["script"].as_list()),
		  response_pos_(0),
		  script_pos_(0),
  		  has_quit_(false), 
  		  timer_proxy_(nullptr),
		  on_create_(game_logic::Formula::createOptionalFormula(v["on_create"])),
		  on_message_(game_logic::Formula::createOptionalFormula(v["on_message"]))

	{
		LOG_DEBUG("YYY: create_bot: " << intptr_t(this) << ", (" << v["on_create"].write_json() << ") -> " << intptr_t(on_create_.get()));

		timer_.expires_from_now(boost::posix_time::milliseconds(g_tbs_bot_delay_ms));

		timer_proxy_ = new tbs_bot_timer_proxy(this);
		timer_.async_wait(boost::bind(&tbs_bot_timer_proxy::signal, timer_proxy_, boost::asio::placeholders::error));
	}

	bot::~bot()
	{
	LOG_DEBUG("YYY: destroy bot: " << this);
//	has_quit_ = true;
		timer_.cancel();
	if(timer_proxy_ != nullptr) {
		timer_proxy_->cancel();
	}
	LOG_DEBUG("YYY: done destroy bot: " << this);
	}

	void bot::process(const boost::system::error_code& error)
	{
	timer_proxy_ = nullptr;
	if(has_quit_) {
		return;
	}
		if(error == boost::asio::error::operation_aborted) {
			LOG_INFO("tbs::bot::process cancelled");
			return;
		}
		if(on_create_) {
			///LOG_DEBUG("YYY: call on_create: " << intptr_t(this) << " -> " << intptr_t(on_create_.get()));
			executeCommand(on_create_->execute(*this));
			on_create_.reset();
		}

		if(ipc_client_) {
			ipc_client_->process();
		}

		if((((!client_ || client_->num_requests_in_flight() == 0) && !preferences::internal_tbs_server()) || ipc_client_) && script_pos_ < script_.size()) {
			variant script = script_[script_pos_++];
			//LOG_DEBUG("BOT: SEND @" << profile::get_tick_time() << " Sending response " << response_.size() << "/" << script_.size() << ": " << ipc_client_.get() << " " << script.write_json());
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
			if(ipc_client_) {
				LOG_INFO("tbs_bot send using ipc_client");
				ipc_client_->set_callable(callable);
				ipc_client_->set_handler(std::bind(&bot::handle_response, this, std::placeholders::_1, callable));
				ipc_client_->send_request(send);
			} else {
				if(!client_) {
					client_.reset(new client(host_, port_, session_id, &service_));
				}
				client_->set_use_local_cache(false);
				client_->send_request(send, callable, std::bind(&bot::handle_response, this, std::placeholders::_1, callable));
			}
		}

		timer_.expires_from_now(boost::posix_time::milliseconds(g_tbs_bot_delay_ms));
		timer_proxy_ = new tbs_bot_timer_proxy(this);
		timer_.async_wait(std::bind(&tbs_bot_timer_proxy::signal, timer_proxy_, std::placeholders::_1));
	}

	void bot::handle_response(const std::string& type, game_logic::FormulaCallablePtr callable)
	{
		if(has_quit_) {
			return;
		}
		if(on_create_) {
			executeCommand(on_create_->execute(*this));
			on_create_.reset();
		}

		if(on_message_) {
			message_type_ = type;
			message_callable_ = callable;

		variant msg = callable->queryValue("message");
		LOG_INFO("BOT: @" << profile::get_tick_time() << " GOT RESPONSE: " << type << ": " << msg.write_json());
		runGarbageCollectionDebug("server-gc.txt");
		//reapGarbageCollection();

		if(msg.is_map() && msg["type"] == variant("player_quit")) {
			std::map<variant,variant> quit_msg;
			quit_msg[variant("session_id")] = variant(session_id_);
			std::map<variant,variant> msg;
			msg[variant("type")] = variant("quit");
			quit_msg[variant("send")] = variant(&msg);

			script_.push_back(variant(&quit_msg));

		} else if(msg.is_map() && msg["type"] == variant("bye")) {
			has_quit_ = true;
			ipc_client_.reset();
			return;
		} else {
			const int ms = profile::get_tick_time();
			//LOG_INFO("BOT: handle_message @ " << ms << " : " << type << "... ( " << callable->queryValue("message").write_json() << " )");
			executeCommand(on_message_->execute(*this));
			const int time_taken = profile::get_tick_time() - ms;
			//LOG_INFO("BOT: handled message in " << time_taken << "ms");
		}
		}

		ASSERT_LOG(type != "connection_error", "GOT ERROR BACK WHEN SENDING REQUEST: " << callable->queryValue("message").write_json());

		ASSERT_LOG(type == "message_received", "UNRECOGNIZED RESPONSE: " << type);

		variant script = script_[response_pos_];
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

		++response_pos_;

		//tbs::web_server::set_debug_state(generate_report());
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
		return variant(&m);
	}

	void bot::surrenderReferences(GarbageCollector* collector)
	{
		for(variant& script : script_) {
			collector->surrenderVariant(&script, "script");
		}

		collector->surrenderPtr(&client_, "client");
		collector->surrenderPtr(&ipc_client_, "ipc_client");

		collector->surrenderVariant(&data_, "data");
		collector->surrenderPtr(&message_callable_, "message_callable");
	}
}
