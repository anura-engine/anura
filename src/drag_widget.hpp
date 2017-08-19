/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#ifndef NO_EDITOR

#include "widget.hpp"

namespace gui
{
	typedef std::shared_ptr<SDL_Cursor> CursorPtr;

	class DragWidget : public Widget
	{
	public:
		enum class Direction {HORIZONTAL, VERTICAL};
		explicit DragWidget(const int x, const int y, const int w, const int h,
			const Direction dir,
			std::function<void(int, int)> drag_start, 
			std::function<void(int, int)> drag_end, 
			std::function<void(int, int)> drag_move);
		explicit DragWidget(const variant&, game_logic::FormulaCallable* e);

		WidgetPtr clone() const override;
	private:
		void init();
		void handleDraw() const override;
		bool handleEvent(const SDL_Event& event, bool claimed) override;
		bool handleMousedown(const SDL_MouseButtonEvent& event, bool claimed);
		bool handleMouseup(const SDL_MouseButtonEvent& event, bool claimed);
		bool handleMouseMotion(const SDL_MouseMotionEvent& event, bool claimed);
		rect getBorderRect() const;
		rect getDraggerRect() const;

		int x_, y_, w_, h_;
		std::function<void(int, int)> drag_start_;
		std::function<void(int, int)> drag_end_;
		std::function<void(int, int)> drag_move_;

		// delegates
		void drag(int dx, int dy);
		void dragStart(int x, int y);
		void dragEnd(int x, int y);
		// FFL formulas
		game_logic::FormulaPtr drag_handler_;
		game_logic::FormulaPtr drag_start_handler_;
		game_logic::FormulaPtr drag_end_handler_;

		WidgetPtr dragger_;
		Direction dir_;
		SDL_Cursor *old_cursor_;
		CursorPtr drag_cursor_;

		point start_pos_;
		int dragging_handle_;
	};

	typedef ffl::IntrusivePtr<DragWidget> DragWidgetPtr;
}

#endif // NO_EDITOR
