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
#ifndef DEBUG_CONSOLE_HPP_INCLUDED
#define DEBUG_CONSOLE_HPP_INCLUDED

#include <boost/intrusive_ptr.hpp>

#include "decimal.hpp"
#include "dialog.hpp"
#include "entity.hpp"
#include "level.hpp"
#include "text_editor_widget.hpp"

class level;
class entity;

#include <string>

namespace debug_console
{

void add_graph_sample(const std::string& id, decimal value);
void process_graph();
void draw_graph();

void add_message(const std::string& msg);
void draw();
void enable_screen_output(bool en=true);

class console_dialog : public gui::dialog
{
public:
	console_dialog(level& lvl, game_logic::formula_callable& obj);
	~console_dialog();

	bool has_keyboard_focus() const;

	void add_message(const std::string& msg);

	void set_focus(game_logic::formula_callable_ptr e);
private:
	console_dialog(const console_dialog&);
	void init();
	bool handle_event(const SDL_Event& event, bool claimed);

	gui::text_editor_widget* text_editor_;

	boost::intrusive_ptr<level> lvl_;
	game_logic::formula_callable_ptr focus_;

	void on_move_cursor();
	bool on_begin_enter();
	void on_enter();

	void load_history();
	std::vector<std::string> history_;
	int history_pos_;
};

}

#endif
