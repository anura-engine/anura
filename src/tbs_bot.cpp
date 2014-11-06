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

#include <SDL.h>

#include "asserts.hpp"
#include "foreach.hpp"
#include "formula.hpp"
#include "preferences.hpp"
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
		bot_ = NULL;
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

PREF_INT(tbs_bot_delay_ms, 100, "Artificial delay for tbs bots");

bot::bot(boost::asio::io_service& service, const std::string& host, const std::string& port, variant v)
  : session_id_(v["session_id"].as_int()), service_(service), timer_(service), host_(host), port_(port), script_(v["script"].as_list()),
    on_create_(game_logic::formula::create_optional_formula(v["on_create"])),
    on_message_(game_logic::formula::create_optional_formula(v["on_message"])),
	has_quit_(false), timer_proxy_(NULL)

{
	fprintf(stderr, "YYY: create bot: %p, (%s) -> %p\n", this, v["on_create"].write_json().c_str(), on_create_.get());
	timer_.expires_from_now(boost::posix_time::milliseconds(g_tbs_bot_delay_ms));

	timer_proxy_ = new tbs_bot_timer_proxy(this);
	timer_.async_wait(boost::bind(&tbs_bot_timer_proxy::signal, timer_proxy_, boost::asio::placeholders::error));
}

bot::~bot()
{
	fprintf(stderr, "YYY: destroy bot: %p\n", this);
//	has_quit_ = true;
	timer_.cancel();
	if(timer_proxy_ != NULL) {
		timer_proxy_->cancel();
	}
	fprintf(stderr, "YYY: done destroy bot: %p\n", this);
}

void bot::process(const boost::system::error_code& error)
{
	timer_proxy_ = NULL;
	if(has_quit_) {
		return;
	}
	//fprintf(stderr, "BOT: PROCESS @%d %d/%d\n", SDL_GetTicks(), (int)response_.size(), (int)script_.size());
	if(error == boost::asio::error::operation_aborted) {
		std::cerr << "tbs::bot::process cancelled" << std::endl;
		return;
	}
	if(on_create_) {
	fprintf(stderr, "YYY: call on_create: %p -> %p\n", this, on_create_.get());
		execute_command(on_create_->execute(*this));
		on_create_.reset();
	}

	if(((!client_ && !preferences::internal_tbs_server()) || (!internal_client_ && preferences::internal_tbs_server()))
		&& response_.size() < script_.size()) {
		variant script = script_[response_.size()];
		fprintf(stderr, "BOT: SEND @%d Sending response %d/%d: %p %s\n", SDL_GetTicks(), (int)response_.size(), (int)script_.size(), internal_client_.get(), script.write_json().c_str());
		variant send = script["send"];
		if(send.is_string()) {
			send = game_logic::formula(send).execute(*this);
		}

		int session_id = -1;
		if(script.has_key("session_id")) {
			session_id = script["session_id"].as_int();
		}

		ASSERT_LOG(send.is_map(), "NO REQUEST TO SEND: " << send.write_json() << " IN " << script.write_json());
		game_logic::map_formula_callable_ptr callable(new game_logic::map_formula_callable(this));
		if(preferences::internal_tbs_server()) {
			internal_client_.reset(new internal_client(session_id));
			internal_client_->send_request(send, session_id, callable, boost::bind(&bot::handle_response, this, _1, callable));
		} else {
			client_.reset(new client(host_, port_, session_id, &service_));
			client_->set_use_local_cache(false);
			client_->send_request(send, callable, boost::bind(&bot::handle_response, this, _1, callable));
		}
	} else if(response_.size() >= script_.size()) {
		fprintf(stderr, "BOT: NO PROCESS DUE TO %p, %d < %d\n", internal_client_.get(), (int)response_.size(), (int)script_.size());
	}

	timer_.expires_from_now(boost::posix_time::milliseconds(g_tbs_bot_delay_ms));
	timer_proxy_ = new tbs_bot_timer_proxy(this);
	timer_.async_wait(boost::bind(&tbs_bot_timer_proxy::signal, timer_proxy_, boost::asio::placeholders::error));
}

void bot::handle_response(const std::string& type, game_logic::formula_callable_ptr callable)
{
	if(has_quit_) {
		return;
	}
	fprintf(stderr, "BOT: @%d GOT RESPONSE: %s\n", SDL_GetTicks(), type.c_str());
	if(on_create_) {
		execute_command(on_create_->execute(*this));
		on_create_.reset();
	}

	if(on_message_) {
		message_type_ = type;
		message_callable_ = callable;

		variant msg = callable->query_value("message");
		if(msg.is_map() && msg["type"] == variant("player_quit")) {
			std::map<variant,variant> quit_msg;
			quit_msg[variant("session_id")] = variant(session_id_);
			std::map<variant,variant> msg;
			msg[variant("type")] = variant("quit");
			quit_msg[variant("send")] = variant(&msg);

			script_.push_back(variant(&quit_msg));

		} else if(msg.is_map() && msg["type"] == variant("bye")) {
			has_quit_ = true;
			client_.reset();
			internal_client_.reset();
			return;
		} else {
			const int ms = SDL_GetTicks();
			fprintf(stderr, "BOT: handle message @ %d : %s... ( %s )\n", ms, type.c_str(), callable->query_value("message").write_json().c_str());
			execute_command(on_message_->execute(*this));
			const int time_taken = SDL_GetTicks() - ms;
			fprintf(stderr, "BOT: handled message in %d\n", time_taken);
		}
	}

	ASSERT_LOG(type != "connection_error", "GOT ERROR BACK WHEN SENDING REQUEST: " << callable->query_value("message").write_json());

	ASSERT_LOG(type == "message_received", "UNRECOGNIZED RESPONSE: " << type);

	variant script = script_[response_.size()];
	std::vector<variant> validations;
	if(script.has_key("validate")) {
		variant validate = script["validate"];
		for(int n = 0; n != validate.num_elements(); ++n) {
			std::map<variant,variant> m;
			m[variant("validate")] = validate[n][variant("expression")] + variant(" EQUALS ") + validate[n][variant("equals")];

			game_logic::formula f(validate[n][variant("expression")]);
			variant expression_result = f.execute(*callable);

			if(expression_result != validate[n][variant("equals")]) {
				m[variant("error")] = variant(1);
			}
			m[variant("value")] = expression_result;

			validations.push_back(variant(&m));
		}
	}

	std::map<variant,variant> m;
	m[variant("message")] = callable->query_value("message");
	m[variant("validations")] = variant(&validations);

	response_.push_back(variant(&m));
	client_.reset();
	internal_client_.reset();

	tbs::web_server::set_debug_state(generate_report());
}

variant bot::get_value(const std::string& key) const
{
	if(key == "script") {
		std::vector<variant> s = script_;
		return variant(&s);
	} else if(key == "data") {
		return data_;
	} else if(key == "type") {
		return variant(message_type_);
	} else if(key == "me") {
		return variant(this);
	} else if(message_callable_) {
		return message_callable_->query_value(key);
	}
	return variant();
}

void bot::set_value(const std::string& key, const variant& value)
{
	if(key == "script") {
		script_ = value.as_list();
	} else if(key == "data") {
		data_ = value;
	} else {
		ASSERT_LOG(false, "ILLEGAL VALUE SET IN TBS BOT: " << key);
	}
}

variant bot::generate_report() const
{
	std::map<variant,variant> m;
	std::vector<variant> response = response_;
	m[variant("response")] = variant(&response);
	return variant(&m);
}

}
