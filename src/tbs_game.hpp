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

#include <boost/scoped_ptr.hpp>
#include "intrusive_ptr.hpp"
#include <deque>
#include <set>

#include "db_client.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "tbs_ai_player.hpp"
#include "tbs_bot.hpp"
#include "variant.hpp"

namespace tbs 
{
	class GameType;

	class server_base;

	class game : public game_logic::FormulaCallable
	{
	public:

		struct error {
			error(const std::string& m);
			std::string msg;
		};

		static ffl::IntrusivePtr<game> create(const variant& v);
		static game* current();

		explicit game(const variant& node);
		virtual ~game();

		void set_server(server_base* server) { server_ = server; }

		void cancel_game();

		virtual variant write(int nplayer=-1, int processing_ms=-1) const;
		virtual void handle_message(int nplayer, const variant& msg);

		int game_id() const { return game_id_; }

		struct message {
			std::vector<int> recipients;
			std::string contents;
		};

		void swap_outgoing_messages(std::vector<message>& msg);

		bool started() const { return started_; }
		virtual bool full() const { return players_.size() == 2; }

		virtual void add_player(const std::string& name);
		virtual void add_ai_player(const std::string& name, const variant& info);

		std::vector<std::string> get_ai_players() const;

		virtual void remove_player(const std::string& name);

		struct player {
			player();
			std::string name;
			variant info;
			int side;
			bool is_human;
			int confirmed_state_id;

			mutable variant state_sent;
			mutable int state_id_sent;
			bool allow_deltas;

		};

		int get_player_index(const std::string& nick) const;

		std::vector<player>& players() { return players_; }
		const std::vector<player>& players() const { return players_; }

		void queue_message(const std::string& msg, int nplayer=-1);
		void queue_message(const char* msg, int nplayer=-1);
		void queue_message(const variant& msg, int nplayer=-1);

		virtual void setup_game();

		virtual void set_as_current_game(bool set) {}

		void process();
	
		int state_id() const { return state_id_; }

		void player_disconnect(int nplayer);
		void player_reconnect(int nplayer);
		void player_disconnected_for(int nplayer, int time_ms);

		void observer_connect(int nclient, const std::string& username);
		void observer_disconnect(const std::string& username);

	protected:
		void start_game();
		virtual void send_game_state(int nplayer=-1, int processing_ms=-1);

		void ai_play();

		void send_notify(const std::string& msg, int nplayer=-1);
		void send_error(const std::string& msg, int nplayer=-1);

		void set_message(const std::string& msg);

		void surrenderReferences(GarbageCollector* collector) override;

	private:
		DECLARE_CALLABLE(game)
		virtual variant getValueDefault(const std::string& key) const override;
		virtual void setValueDefault(const std::string& key, const variant& value) override;

		virtual ai_player* create_ai() const { return nullptr; }

		server_base* server_;

		boost::scoped_ptr<GameType> game_type_;

		int game_id_;

		bool started_;

		int state_id_; //upward counting integer keeping track of the game state.

		int cycle_;
		int tick_rate_;

		//a message which explains the reason for the last game state change.
		std::string current_message_;

		std::vector<player> players_;

		std::vector<int> players_disconnected_;

		std::vector<message> outgoing_messages_;

		std::vector<std::string> log_;

		enum GAME_STATE { STATE_SETUP, STATE_PLAYING };
		GAME_STATE state_;

		std::vector<std::shared_ptr<ai_player> > ai_;

		game_logic::FormulaCallable* backup_callable_;

		std::vector<ffl::IntrusivePtr<tbs::bot> > bots_;

		void executeCommand(variant cmd);

		mutable DbClientPtr db_client_;

		std::vector<std::string> observers_;

		variant player_waiting_on_;
		int started_waiting_for_player_at_;

		std::vector<std::string> replay_;
		mutable variant replay_last_;
		variant winner_;
		variant server_report_;

		int start_timestamp_;

		void finished_upload_state();
		void finished_download_state(std::string data);
		void download_state(const std::string& id);
		void upload_state(const std::string& id);
		void save_state(const std::string& fname);
		void load_state(const std::string& fname);

		variant write_replay() const;
		void restore_replay(int state_id) const;
		void verify_replay();
	};

	class game_context 
	{
		game* old_game_;
	public:
		explicit game_context(game* g);
		void set(game* g);
		~game_context();
	};

	typedef ffl::IntrusivePtr<game> game_ptr;
	typedef ffl::IntrusivePtr<const game> const_game_ptr;
}
