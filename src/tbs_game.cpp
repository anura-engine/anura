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


#include <algorithm>
#include <string>
#include <boost/algorithm/string.hpp>

#include <stdio.h>

#include "asserts.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "formula_object.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "random.hpp"
#include "tbs_ai_player.hpp"
#include "tbs_internal_server.hpp"
#include "tbs_game.hpp"
#include "tbs_web_server.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"
#include "wml_formula_callable.hpp"

namespace game_logic 
{
	void flush_all_backed_maps();
}

namespace tbs 
{
	extern http_client* g_game_server_http_client_to_matchmaking_server;
	
	struct game_type 
	{
		game_type() 
		{
		}

		explicit game_type(const variant& value)
		{
			variant functions_var = value["functions"];
			if(functions_var.is_string()) {
				functions.reset(new game_logic::FunctionSymbolTable);
				game_logic::Formula f(functions_var, functions.get());
			}

			variant handlers_var = value["handlers"];
			if(handlers_var.is_map()) {
				for(const variant& key : handlers_var.getKeys().as_list()) {
					handlers[key.as_string()] = game_logic::Formula::createOptionalFormula(handlers_var[key], functions.get());
				}
			}
		}

		std::string name;
		std::shared_ptr<game_logic::FunctionSymbolTable> functions;
		std::map<std::string, game_logic::ConstFormulaPtr> handlers;
	};

	std::map<std::string, game_type> generate_game_types() 
	{
		LOG_INFO("GENERATE GAME TYPES");

		std::map<std::string, game_type> result;

		std::vector<std::string> files;
		module::get_files_in_dir("data/tbs", &files);
		for(const std::string& fname : files) {
			if(fname.size() > 4 && std::string(fname.end()-4,fname.end()) == ".cfg") {
				std::string type(fname.begin(), fname.end()-4);
				boost::algorithm::to_lower(type);
				result[type] = game_type(json::parse_from_file("data/tbs/" + fname));
				result[type].name = type;
				LOG_INFO("LOADED TBS GAME TYPE: " << type);
			}
		}
		LOG_INFO("DONE GENERATE GAME TYPES");

		return result;
	}

	std::map<std::string, game_type>& all_types() 
	{
		static std::map<std::string, game_type> types = generate_game_types();
		return types;
	}

	extern std::string global_debug_str;

	void game::reload_game_types()
	{
		all_types() = generate_game_types();
	}

	namespace {
	game* current_game = nullptr;

	int generate_game_id() {
		static int id = int(time(nullptr));
		return id++;
	}

	using namespace game_logic;

	std::string string_tolower(const std::string& s) 
	{
		std::string res(s);
		boost::algorithm::to_lower(res);
		return res;
	}
	}

	game::error::error(const std::string& m) : msg(m)
	{
		LOG_INFO("game error: " << m);
	}

	game_context::game_context(game* g) : old_game_(current_game)
	{
		current_game = g;
		g->set_as_current_game(true);
	}

	game_context::~game_context()
	{
		current_game->set_as_current_game(false);
		current_game = old_game_;
	}

	void game_context::set(game* g)
	{
		current_game = g;
		g->set_as_current_game(true);
	}

	game* game::current()
	{
		return current_game;
	}

	boost::intrusive_ptr<game> game::create(const variant& v)
	{
		const variant type_var = v["game_type"];
		if(!type_var.is_string()) {
			return nullptr;
		}

		std::string type = type_var.as_string();
		boost::algorithm::to_lower(type);
		std::map<std::string, game_type>::const_iterator type_itor = all_types().find(type);
		if(type_itor == all_types().end()) {
			return nullptr;
		}

		boost::intrusive_ptr<game> result(new game(type_itor->second));
		game_logic::MapFormulaCallablePtr vars(new game_logic::MapFormulaCallable);
		vars->add("msg", v);
		result->handleEvent("create", vars.get());
		return result;
	}

	game::game(const game_type& type)
	  : type_(type), game_id_(generate_game_id()),
		started_(false), state_(STATE_SETUP), state_id_(0), rng_seed_(rng::get_seed()), cycle_(0), tick_rate_(50),
		backup_callable_(nullptr)
	{
	}

	game::game(const variant& value)
	  : type_(all_types()[string_tolower(value["type"].as_string())]),
		game_id_(generate_game_id()),
		started_(value["started"].as_bool(false)),
		state_(STATE_SETUP),
		state_id_(0), rng_seed_(rng::get_seed()),
		cycle_(value["cycle"].as_int(0)),
		tick_rate_(value["tick_rate"].as_int(50)),
		backup_callable_(nullptr)
	{
	}

	game::game(const std::string& game_type, const variant& doc)
	  : type_(all_types()[game_type]),
		game_id_(doc["id"].as_int()),
		started_(doc["started"].as_bool()),
		state_(started_ ? STATE_PLAYING : STATE_SETUP),
		state_id_(doc["state_id"].as_int()),
		rng_seed_(doc["rng_seed"].as_int()),
		cycle_(doc["cycle"].as_int(0)),
		tick_rate_(doc["tick_rate"].as_int(50)),
		backup_callable_(nullptr),
		doc_(doc["state"])
	{
		log_ = util::split(doc["log"].as_string(), '\n');

		variant players_val = doc["players"];
		for(int n = 0; n != players_val.num_elements(); ++n) {
			add_player(players_val[n].as_string());
		}
	}

	game::~game()
	{
		LOG_INFO("DESTROY GAME");
	}

	void game::cancel_game()
	{
		players_.clear();
		outgoing_messages_.clear();
		doc_ = variant();
		ai_.clear();
		bots_.clear();
		backup_callable_ = nullptr;
		LOG_INFO("CANCEL GAME: " << refcount());
	}

	variant game::write(int nplayer, int processing_ms) const
	{
		game_logic::wmlFormulaCallableSerializationScope serialization_scope;

		variant_builder result;
		result.add("id", game_id_);
		result.add("type", "game");
		result.add("game_type", variant(type_.name));
		result.add("started", variant::from_bool(started_));
		result.add("state_id", state_id_);
		result.add("rng_seed", rng_seed_);

		if(processing_ms != -1) {
			LOG_INFO("ZZZ: server_time: " << processing_ms);
			result.add("server_time", processing_ms);
		}

		//observers see the perspective of the first player for now
		result.add("nplayer", variant(nplayer < 0 ? 0 : nplayer));

		std::vector<variant> players_val;
		for(const player& p : players_) {
			players_val.push_back(variant(p.name));
		}

		result.add("players", variant(&players_val));

		if(current_message_.empty() == false) {
			result.add("message", current_message_);
		}

		if(nplayer < 0) {
			result.add("observer", true);
		}

		bool send_delta = false;

		if(nplayer >= 0 
			&& nplayer < static_cast<int>(players_.size()) 
			&& players_[nplayer].state_id_sent != -1 
			&& players_[nplayer].state_id_sent == players_[nplayer].confirmed_state_id 
			&& players_[nplayer].allow_deltas) {
			send_delta = true;
		}

		variant state_doc;

		if(type_.handlers.count("transform")) {
			variant msg = FormulaObject::deepClone(doc_);
			game_logic::MapFormulaCallablePtr vars(new game_logic::MapFormulaCallable);
			vars->add("message", msg);
			vars->add("nplayer", variant(nplayer < 0 ? 0 : nplayer));
			const_cast<game*>(this)->handleEvent("transform", vars.get());

			state_doc = msg;
		} else {
			state_doc = FormulaObject::deepClone(doc_);
		}

		if(send_delta) {
			result.add("delta", FormulaObject::generateDiff(players_[nplayer].state_sent, state_doc));
			result.add("delta_basis", players_[nplayer].confirmed_state_id);
		} else {
			result.add("state", state_doc);
		}

		if(nplayer >= 0 && nplayer < static_cast<int>(players_.size()) && players_[nplayer].allow_deltas) {
			players_[nplayer].state_id_sent = state_id_;
			players_[nplayer].state_sent = state_doc;
		}

		std::string log_str;
		for(const std::string& s : log_) {
			if(!log_str.empty()) {
				log_str += "\n";
			}
			log_str += s;
		}

		result.add("log", variant(log_str));

		variant res = result.build();
		res.add_attr(variant("serialized_objects"), serialization_scope.writeObjects(res));
		return res;
	}

	void game::start_game()
	{
		if(started_) {
			send_notify("The game has started.");
		}

		state_ = STATE_PLAYING;
		started_ = true;
		handleEvent("start");

		send_game_state();

		ai_play();
	}

	void game::swap_outgoing_messages(std::vector<message>& msg)
	{
		outgoing_messages_.swap(msg);
		outgoing_messages_.clear();
	}

	void game::queue_message(const std::string& msg, int nplayer)
	{
		outgoing_messages_.push_back(message());
		outgoing_messages_.back().contents = msg;
		if(nplayer >= 0) {
			outgoing_messages_.back().recipients.push_back(nplayer);
		}

		//LOG_DEBUG("QUEUE MESSAGE " << nplayer << " (((" << msg << ")))");
	}

	void game::queue_message(const char* msg, int nplayer)
	{
		queue_message(std::string(msg), nplayer);
	}

	void game::queue_message(const variant& msg, int nplayer)
	{
		queue_message(msg.write_json(), nplayer);
	}

	void game::send_error(const std::string& msg, int nplayer)
	{
		variant_builder result;
		result.add("type", "error");
		result.add("message", msg);
		queue_message(result.build(), nplayer);
	}

	void game::send_notify(const std::string& msg, int nplayer)
	{
		variant_builder result;
		result.add("type", "message");
		result.add("message", msg);
		queue_message(result.build(), nplayer);
	}

	game::player::player() : confirmed_state_id(-1), state_id_sent(-1), allow_deltas(true)
	{
	}

	void game::add_player(const std::string& name)
	{
		players_.push_back(player());
		players_.back().name = name;
		players_.back().side = players_.size() - 1;
		players_.back().is_human = true;
	}

	void game::add_ai_player(const std::string& name, const variant& info)
	{
		players_.push_back(player());
		players_.back().name = name;
		players_.back().side = players_.size() - 1;
		players_.back().is_human = false;

		handleEvent("add_bot", map_into_callable(info).get());

	//	boost::intrusive_ptr<bot> new_bot(new bot(*web_server::service(), "localhost", formatter() << web_server::port(), info));
	//	bots_.push_back(new_bot);
	}

	void game::remove_player(const std::string& name)
	{
		for(int n = 0; n != players_.size(); ++n) {
			if(players_[n].name == name) {
				players_.erase(players_.begin() + n);
				for(int m = 0; m != ai_.size(); ++m) {
					if(ai_[m]->player_id() == n) {
						ai_.erase(ai_.begin() + m);
						break;
					}
				}

				break;
			}
		}
	}

	std::vector<std::string> game::get_ai_players() const
	{
		std::vector<std::string> result;
		for(std::shared_ptr<ai_player> a : ai_) {
			ASSERT_LOG(a->player_id() >= 0 && static_cast<unsigned>(a->player_id()) < players_.size(), "BAD AI INDEX: " << a->player_id());
			result.push_back(players_[a->player_id()].name);
		}

		return result;
	}

	int game::get_player_index(const std::string& nick) const
	{
		int nplayer = 0;
		for(const player& p : players_) {
			if(p.name == nick) {
				for(std::shared_ptr<ai_player> ai : ai_) {
					if(ai->player_id() == nplayer) {
						return -1;
					}
				}
				return nplayer;
			}

			++nplayer;
		}

		return -1;
	}

	void game::send_game_state(int nplayer, int processing_ms)
	{
		LOG_DEBUG("SEND GAME STATE: " << nplayer);
		if(nplayer == -1) {
			for(int n = 0; n != players().size(); ++n) {
				send_game_state(n, processing_ms);
			}

			//Send to observers.
			queue_message(write(-1));
			outgoing_messages_.back().recipients.push_back(-1);

			current_message_ = "";
		} else if(nplayer >= 0 && static_cast<unsigned>(nplayer) < players().size()) {
			queue_message(write(nplayer, processing_ms), nplayer);
		}
	}

	void game::ai_play()
	{
		for(int n = 0; n != ai_.size(); ++n) {
			for(;;) {
				variant msg = ai_[n]->play();
				if(msg.is_null()) {
					break;
				}

				handle_message(ai_[n]->player_id(), msg);
			}
		}
	}

	void game::set_message(const std::string& msg)
	{
		current_message_ = msg;
	}

	void game::process()
	{
		if(DbClient_) {
			DbClient_->process(100);
		}

		if(started_) {
			const int starting_state_id = state_id_;
			static const std::string ProcessStr = "process";
			handleEvent(ProcessStr);
			++cycle_;

			if(state_id_ != starting_state_id) {
				send_game_state();
			}
		}
	}

	variant game::getValueDefault(const std::string& key) const 
	{
		if(backup_callable_) {
			return backup_callable_->queryValue(key);
		} else {
			return variant();
		}
	}

	void game::setValueDefault(const std::string& key, const variant& value)
	{
		if(backup_callable_) {
			backup_callable_->mutateValue(key, value);
		}
	}

	PREF_BOOL(tbs_game_exit_on_winner, false, "If true, tbs games will immediately exit when there is a winner.");

	BEGIN_DEFINE_CALLABLE_NOBASE(game)
		DEFINE_FIELD(game, "builtin game")
			return variant(&obj_instance);
		
		DEFINE_FIELD(doc, "any")
			return obj.doc_;
		DEFINE_SET_FIELD
			obj.doc_ = value;

		DEFINE_FIELD(event, "null")
			return variant();
		DEFINE_SET_FIELD_TYPE("string|map")
			if(value.is_string()) {
				obj.handleEvent(value.as_string());
			} else if(value.is_map()) {
				obj.handleEvent(value["event"].as_string(), map_into_callable(value["arg"]).get());
			}

#ifdef USE_DB_CLIENT
		DEFINE_FIELD(db_client, "builtin DbClient")
		if(db_client_.get() == NULL) {
			db_client_ = DbClient::create();
		}

		return variant(db_client_.get());
#else
		DEFINE_FIELD(db_client, "null")
		return variant();
#endif
		DEFINE_FIELD(state_id, "int")
			return variant(obj.state_id_);
		DEFINE_SET_FIELD
			obj.state_id_ = value.as_int();
			LOG_DEBUG("XXX: @" << profile::get_tick_time() << " state_id = " << obj.state_id_);

		DEFINE_FIELD(log_message, "null")
			return variant();
		DEFINE_SET_FIELD_TYPE("string|null")
			if(!value.is_null()) {
				obj.log_.emplace_back(value.as_string());
			}

		DEFINE_FIELD(bots, "[builtin bot]")
			std::vector<variant> v;
			for(int n = 0; n != obj.bots_.size(); ++n) {
				v.emplace_back(obj.bots_[n].get());
			}
			return variant(&v);
		DEFINE_SET_FIELD_TYPE("[any]")
			obj.bots_.clear();
			for(unsigned n = 0; n != value.num_elements(); ++n) {
				LOG_INFO("BOT_ADD: " << value[n].write_json());
				if(value[n].is_callable() && value[n].try_convert<tbs::bot>()) {
					obj.bots_.push_back(boost::intrusive_ptr<tbs::bot>(value[n].try_convert<tbs::bot>()));
				} else if(preferences::internal_tbs_server()) {
					boost::intrusive_ptr<bot> new_bot(new bot(tbs::internal_server::get_io_service(), "localhost", "23456", value[n]));
					obj.bots_.push_back(new_bot);
				} else {
					boost::intrusive_ptr<bot> new_bot(new bot(*web_server::service(), "localhost", formatter() << web_server::port(), value[n]));
					obj.bots_.push_back(new_bot);
				}
			}

		DEFINE_FIELD(players_disconnected, "[int]")
			std::vector<variant> result;
			for(auto n : obj.players_disconnected_) {
				result.push_back(variant(n));
			}
			return variant(&result);

		DEFINE_FIELD(winner, "null")
			return variant();
		DEFINE_SET_FIELD_TYPE("any")
			std::cout << "WINNER: " << value.write_json() << std::endl;

			if(g_game_server_http_client_to_matchmaking_server != NULL) {
				http_client& client = *g_game_server_http_client_to_matchmaking_server;
				variant_builder msg;
				msg.add("type", "server_finished_game");
#if defined(_MSC_VER)
				msg.add("pid", static_cast<int>(_getpid()));
#else
				msg.add("pid", static_cast<int>(getpid()));
#endif

				bool complete = false;

				client.send_request("POST /server", msg.build().write_json(),
				  [&complete](std::string response) {
					complete = true;
				  },
				  [&complete](std::string msg) {
					complete = true;
					ASSERT_LOG(false, "Could not connect to server: " << msg);
				  },
				  [](int a, int b, bool c) {
				  });
			
				while(!complete) {
					client.process();
				}
			}
			if(g_tbs_game_exit_on_winner) {
				game_logic::flush_all_backed_maps();
				_exit(0);
			}
	END_DEFINE_CALLABLE(game)


	void game::handle_message(int nplayer, const variant& msg)
	{
		//LOG_DEBUG("HANDLE MESSAGE (((" << msg.write_json() << ")))");
		rng::set_seed(rng_seed_);
		const std::string type = msg["type"].as_string();
		if(type == "start_game") {
			start_game();
			return;
		} else if(type == "request_updates") {
			if(msg.has_key("state_id") && !doc_.is_null()) {
				if(nplayer >= 0 && nplayer < static_cast<int>(players_.size()) && msg.has_key("allow_deltas") && msg["allow_deltas"].as_bool() == false) {
					players_[nplayer].allow_deltas = false;
				}

				const variant state_id = msg["state_id"];
				if(state_id.as_int() != state_id_ && nplayer >= 0) {
					players_[nplayer].confirmed_state_id = state_id.as_int();
					send_game_state(nplayer);
				} else if(state_id.as_int() == state_id_ && nplayer >= 0 && static_cast<unsigned>(nplayer) < players_.size() && players_[nplayer].confirmed_state_id != state_id_) {
					LOG_DEBUG("XXX: @" << profile::get_tick_time() << " player " << nplayer << " confirm sync " << state_id_);
					players_[nplayer].confirmed_state_id = state_id_;

					std::ostringstream s;
					s << "{ type: \"confirm_sync\", player: " << nplayer << ", state_id: " << state_id_ << " }";
					std::string msg = s.str();
					for(int n = 0; n != players_.size(); ++n) {
						if(n != nplayer && players_[n].is_human) {
							queue_message(msg, nplayer);
						}
					}
				}
			}
			return;
		} else if(type == "chat_message") {
			variant m = msg;
			if(nplayer >= 0) {
				m.add_attr_mutation(variant("nick"), variant(players_[nplayer].name));
			} else {
				std::string nick = "observer";
				if(m.has_key(variant("nick"))) {
					nick = m["nick"].as_string();
					nick += " (obs)";
				}

				m.add_attr_mutation(variant("nick"), variant(nick));
			}
			queue_message(m);
			return;
		} else if(type == "ping_game") {
			variant_builder response;
			response.add("type", "pong_game");
			response.add("payload", msg);
			queue_message(response.build().write_json(), nplayer);
			return;
		}

		game_logic::MapFormulaCallablePtr vars(new game_logic::MapFormulaCallable);
		auto start_time = profile::get_tick_time();
		vars->add("message", msg);
		vars->add("player", variant(nplayer));
		rng::set_seed(rng_seed_);
		handleEvent("message", vars.get());
		rng_seed_ = rng::get_seed();

		const auto time_taken = profile::get_tick_time() - start_time;

		LOG_DEBUG("XXX: @" << profile::get_tick_time() << " HANDLED MESSAGE " << type);

		send_game_state(-1, time_taken);
	}

	void game::setup_game()
	{
	}

	namespace 
	{
		struct backup_callable_scope 
		{
			game_logic::FormulaCallable** ptr_;
			game_logic::FormulaCallable* backup_;
			backup_callable_scope(game_logic::FormulaCallable** ptr, game_logic::FormulaCallable* var) 
				: ptr_(ptr), 
				  backup_(*ptr) 
			{
				if(var) {
					*ptr_ = var;
				} else {
					ptr_ = nullptr;
				}
			}

			~backup_callable_scope() 
			{
				if(ptr_) {
					*ptr_ = backup_;
				}
			}
		};
	}

	void game::handleEvent(const std::string& name, game_logic::FormulaCallable* variables)
	{
		const backup_callable_scope backup_scope(&backup_callable_, variables);

		std::map<std::string, game_logic::ConstFormulaPtr>::const_iterator itor = type_.handlers.find(name);
		if(itor == type_.handlers.end() || !itor->second) {
			return;
		}

		variant v = itor->second->execute(*this);
		executeCommand(v);
	}

	void game::executeCommand(variant cmd)
	{
		if(cmd.is_list()) {
			for(int n = 0; n != cmd.num_elements(); ++n) {
				executeCommand(cmd[n]);
			}
		} else if(cmd.is_callable()) {
			const game_logic::CommandCallable* command = cmd.try_convert<game_logic::CommandCallable>();
			if(command) {
				command->runCommand(*this);
			}
		} else if(cmd.is_map()) {
			if(cmd.has_key("execute")) {
				game_logic::Formula f(cmd["execute"]);
				game_logic::FormulaCallablePtr callable(map_into_callable(cmd["arg"]));
				ASSERT_LOG(callable.get(), "NO ARG SPECIFIED IN EXECUTE AT " << cmd.debug_location());
				variant v = f.execute(*callable);
				executeCommand(v);
			}
		}
	}

	void game::player_disconnect(int nplayer)
	{
		variant_builder result;
		result.add("type", "player_disconnect");
		result.add("player", players()[nplayer].name);
		variant msg = result.build();
		for(int n = 0; n != players().size(); ++n) {
			if(n != nplayer) {
				queue_message(msg, n);
			}
		}
	
	}

	void game::player_reconnect(int nplayer)
	{
		variant_builder result;
		result.add("type", "player_reconnect");
		result.add("player", players()[nplayer].name);
		variant msg = result.build();
		for(int n = 0; n != players().size(); ++n) {
			if(n != nplayer) {
				queue_message(msg, n);
			}
		}
	}

	void game::player_disconnected_for(int nplayer, int time_ms)
	{
		if(time_ms >= 60000 && std::find(players_disconnected_.begin(), players_disconnected_.end(), nplayer) == players_disconnected_.end()) {
			players_disconnected_.push_back(nplayer);
			handleEvent("player_disconnected");
			send_game_state();
		}
	}
}

namespace 
{
	bool g_create_bot_game = false;
	void create_game_return(const std::string& msg) 
	{
		g_create_bot_game = true;
		LOG_INFO("GAME CREATED");
	}

	void start_game_return(const std::string& msg) 
	{
		LOG_INFO("GAME STARTED");
	}
}

COMMAND_LINE_UTILITY(tbs_bot_game) 
{
	using namespace tbs;
	using namespace game_logic;

	bool found_create_game = false;
	variant create_game_request = json::parse("{type: 'create_game', game_type: 'citadel', users: [{user: 'a', bot: true, bot_type: 'goblins', session_id: 1}, {user: 'b', bot: true, bot_type: 'goblins', session_id: 2}]}");
	for(int i = 0; i != args.size(); ++i) {
		if(args[i] == "--request" && i+1 != args.size()) {
			create_game_request = json::parse(args[i+1]);
			found_create_game = true;
		}
	}

	ASSERT_LOG(found_create_game, "MUST PROVIDE --request");

	variant start_game_request = json::parse("{type: 'start_game'}");

	boost::intrusive_ptr<MapFormulaCallable> callable(new MapFormulaCallable);
	boost::intrusive_ptr<internal_client> client(new internal_client);
	client->send_request(create_game_request, -1, callable, create_game_return);
	while(!g_create_bot_game) {
		internal_server::process();
	}

	client->send_request(start_game_request, 1, callable, start_game_return);

	for(;;) {
		internal_server::process();
	}
}
