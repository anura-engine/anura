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
#pragma once
#ifndef DRAG_WIDGET_HPP_INCLUDED
#define DRAG_WIDGET_HPP_INCLUDED
#ifndef NO_EDITOR

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include "geometry.hpp"
#include "widget.hpp"

namespace gui
{

typedef boost::shared_ptr<SDL_Cursor> cursor_ptr;

class drag_widget : public widget
{
public:
	enum drag_direction {DRAG_HORIZONTAL, DRAG_VERTICAL};
	explicit drag_widget(const int x, const int y, const int w, const int h,
		const drag_direction dir,
		boost::function<void(int, int)> drag_start, 
		boost::function<void(int, int)> drag_end, 
		boost::function<void(int, int)> drag_move);
	explicit drag_widget(const variant&, game_logic::formula_callable* e);

private:
	void init();
	void handle_draw() const;
	bool handle_event(const SDL_Event& event, bool claimed);
	bool handle_mousedown(const SDL_MouseButtonEvent& event, bool claimed);
	bool handle_mouseup(const SDL_MouseButtonEvent& event, bool claimed);
	bool handle_mousemotion(const SDL_MouseMotionEvent& event, bool claimed);
	rect get_border_rect() const;
	rect get_dragger_rect() const;

	int x_, y_, w_, h_;
	boost::function<void(int, int)> drag_start_;
	boost::function<void(int, int)> drag_end_;
	boost::function<void(int, int)> drag_move_;

	// delegates
	void drag(int dx, int dy);
	void drag_start(int x, int y);
	void drag_end(int x, int y);
	// FFL formulas
	game_logic::formula_ptr drag_handler_;
	game_logic::formula_ptr drag_start_handler_;
	game_logic::formula_ptr drag_end_handler_;

	widget_ptr dragger_;
	drag_direction dir_;
	SDL_Cursor *old_cursor_;
	cursor_ptr drag_cursor_;

	point start_pos_;
	int dragging_handle_;
};

typedef boost::intrusive_ptr<drag_widget> drag_widget_ptr;

}

#endif // NO_EDITOR
#endif // DRAG_WIDGET_HPP_INCLUDED
