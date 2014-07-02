/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#ifndef DRAG_WIDGET_HPP_INCLUDED
#define DRAG_WIDGET_HPP_INCLUDED
#ifndef NO_EDITOR

#include "kre/Geometry.hpp"
#include "widget.hpp"

namespace gui
{

typedef std::shared_ptr<SDL_Cursor> cursor_ptr;

class drag_widget : public Widget
{
public:
	enum drag_direction {DRAG_HORIZONTAL, DRAG_VERTICAL};
	explicit drag_widget(const int x, const int y, const int w, const int h,
		const drag_direction dir,
		std::function<void(int, int)> drag_start, 
		std::function<void(int, int)> drag_end, 
		std::function<void(int, int)> drag_move);
	explicit drag_widget(const variant&, game_logic::FormulaCallable* e);

private:
	void init();
	void handleDraw() const override;
	bool handleEvent(const SDL_Event& event, bool claimed) override;
	bool handleMousedown(const SDL_MouseButtonEvent& event, bool claimed);
	bool handleMouseup(const SDL_MouseButtonEvent& event, bool claimed);
	bool handleMouseMotion(const SDL_MouseMotionEvent& event, bool claimed);
	rect get_border_rect() const;
	rect get_dragger_rect() const;

	int x_, y_, w_, h_;
	std::function<void(int, int)> drag_start_;
	std::function<void(int, int)> drag_end_;
	std::function<void(int, int)> drag_move_;

	// delegates
	void drag(int dx, int dy);
	void drag_start(int x, int y);
	void drag_end(int x, int y);
	// FFL formulas
	game_logic::formula_ptr drag_handler_;
	game_logic::formula_ptr drag_start_handler_;
	game_logic::formula_ptr drag_end_handler_;

	WidgetPtr dragger_;
	drag_direction dir_;
	SDL_Cursor *old_cursor_;
	cursor_ptr drag_cursor_;

	point start_pos_;
	int dragging_handle_;
};

typedef boost::intrusive_ptr<drag_widget> drag_WidgetPtr;

}

#endif // NO_EDITOR
#endif // DRAG_WIDGET_HPP_INCLUDED
