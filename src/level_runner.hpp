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

#include "intrusive_ptr.hpp"

#include <string>

#include "button.hpp"
#include "debug_console.hpp"
#include "editor.hpp"
#include "geometry.hpp"
#include "level.hpp"
#include "pause_game_dialog.hpp"
#include "slider.hpp"

//an exception which is thrown if we go through a portal which takes us
//to a level with a different number of players, which indicates we are going
//into or out of multiplayer.
struct multiplayer_exception {
};

class editor;
class EditorResolutionManager;

void addProcessFunction(std::function<void()> fn, void* tag);
void removeProcessFunction(void* tag);

void addAsynchronousWorkItem(std::function<void()> fn);

void mapSDLEventScreenCoordinatesToVirtual(SDL_Event& event);

class LevelRunner 
{
public:
	static LevelRunner* getCurrent();
	LevelRunner(ffl::IntrusivePtr<Level>& lvl, std::string& level_cfg, std::string& original_level_cfg);

	const debug_console::ConsoleDialog* get_debug_console() const {
#ifndef NO_EDITOR
		return console_.get();
#endif
		return nullptr;
	}

	void quit_game();
	bool is_quitting() { return quit_; }
	void set_quitting(bool value) { quit_ = value; }

	ffl::IntrusivePtr<editor> get_editor() const { return editor_; }

	bool is_paused() const { return paused; }

	bool play_level();
	bool play_cycle();

	void force_return(bool quit=false) { force_return_ = true; quit_ = quit; }

	void toggle_pause();
	void toggle_history_trails();

	void video_resize_event(const SDL_Event &event);

	void on_player_set(EntityPtr e);

#ifndef NO_EDITOR
	void replay_level_from_start();
#endif
private:
	void start_editor();
	void close_editor();
	void reverse_cycle();
	void handle_pause_game_result(PAUSE_GAME_RESULT result);
	LevelPtr& lvl_;
	std::string& level_cfg_;
	std::string& original_level_cfg_;

	bool quit_;
	bool force_return_;
	time_t current_second_;

	int current_max_, next_max_, current_fps_, next_fps_, current_cycles_, next_cycles_, current_delay_, next_delay_,
	    current_draw_, next_draw_, current_process_, next_process_,
		current_flip_, next_flip_, current_events_;
	std::string profiling_summary_;
	int nskip_draw_;

	int cycle;
	int die_at;
	bool paused;
	bool done;
	int start_time_;
	int pause_time_;

	point last_stats_point_;
	std::string last_stats_point_level_;
	bool handle_mouse_events(const SDL_Event &event);

	//mouse event handling state
	bool mouse_clicking_;

	int mouse_drag_count_;

	void show_pause_title();

	ffl::IntrusivePtr<editor> editor_;
#ifndef NO_EDITOR
	std::unique_ptr<EditorResolutionManager> editor_resolution_manager_;
	gui::SliderPtr history_slider_;
	gui::ButtonPtr history_button_;
	std::vector<EntityPtr> history_trails_;
	std::string history_trails_label_;
	int history_trails_state_id_;
	int object_reloads_state_id_;
	int tile_rebuild_state_id_;
	void initHistorySlider();
	void onHistoryChange(float value);
	void update_history_trails();

	ffl::IntrusivePtr<debug_console::ConsoleDialog> console_;
#endif
};

class pause_scope
{
	int ticks_;
	bool active_;
public:
	pause_scope();
	~pause_scope();
};

void begin_skipping_game();
void end_skipping_game();
bool is_skipping_game();

void video_resize(const SDL_Event &event );
