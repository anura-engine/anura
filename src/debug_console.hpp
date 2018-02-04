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

#include "decimal.hpp"
#include "dialog.hpp"
#include "entity.hpp"
#include "level.hpp"
#include "text_editor_widget.hpp"

class Level;
class Entity;

#include <string>

namespace debug_console
{
	void add_graph_sample(const std::string& id, decimal value);
	void process_graph();
	void draw_graph();

	void addMessage(const std::string& msg);
	void clearMessages();
	void draw();
	void enable_screen_output(bool en=true);

	bool isExecutingDebugConsoleCommand();

	struct ExecuteDebugConsoleScope {
		ExecuteDebugConsoleScope();
		~ExecuteDebugConsoleScope();
	};


	class ConsoleDialog : public gui::Dialog
	{
	public:
		ConsoleDialog(Level& lvl, game_logic::FormulaCallable& obj);
		~ConsoleDialog();

		bool hasKeyboardFocus() const;

		void clearMessages();
		void addMessage(const std::string& msg);

		void setFocus(game_logic::FormulaCallablePtr e);
		game_logic::FormulaCallablePtr getFocus() const { return focus_; }
	private:
		std::string getEnteredCommand();
		ConsoleDialog(const ConsoleDialog&);
		void init();
		bool handleEvent(const SDL_Event& event, bool claimed) override;

		void changeFontSize(int delta);

		ffl::IntrusivePtr<gui::TextEditorWidget> text_editor_;

		ffl::IntrusivePtr<Level> lvl_;
		game_logic::FormulaCallablePtr focus_;

		void onMoveCursor();
		bool onBeginEnter();
		void onEnter();

		void loadHistory();
		std::vector<std::string> history_;
		int history_pos_;

		int prompt_pos_;

		bool dragging_, resizing_;
	};
}
