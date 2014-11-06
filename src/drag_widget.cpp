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
#ifndef NO_EDITOR
#include <boost/bind.hpp>
#include <vector>

#include "asserts.hpp"
#include "geometry.hpp"
#include "image_widget.hpp"
#include "drag_widget.hpp"
#include "input.hpp"
#include "raster.hpp"

enum {
	HOT_X = 16,
	HOT_Y = 16,
};
enum {
	CURSOR_WIDTH	= 32,
	CURSOR_HEIGHT	= 32,
};

enum {
	BORDER_THICKNESS = 14,
};

static const unsigned char horiz_cursor_data[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x60,
        0x0a, 0x00, 0x00, 0x50, 0x12, 0x00, 0x00, 0x48, 0x23, 0xff, 0xff, 0xc4, 0x40, 0x00, 0x00, 0x02,
        0x23, 0xff, 0xff, 0xc4, 0x12, 0x00, 0x00, 0x48, 0x0a, 0x00, 0x00, 0x50, 0x06, 0x00, 0x00, 0x60,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char horiz_cursor_mask[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x60,
        0x0e, 0x00, 0x00, 0x70, 0x1e, 0x00, 0x00, 0x78, 0x3f, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xff, 0xfe,
        0x3f, 0xff, 0xff, 0xfc, 0x1e, 0x00, 0x00, 0x78, 0x0e, 0x00, 0x00, 0x70, 0x06, 0x00, 0x00, 0x60,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char vert_cursor_data[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x02, 0x20, 0x00,
        0x00, 0x04, 0x10, 0x00, 0x00, 0x08, 0x08, 0x00, 0x00, 0x0f, 0x78, 0x00, 0x00, 0x01, 0x40, 0x00,
        0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x40, 0x00,
        0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x40, 0x00,
        0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x40, 0x00,
        0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x40, 0x00,
        0x00, 0x01, 0x40, 0x00, 0x00, 0x0f, 0x78, 0x00, 0x00, 0x08, 0x08, 0x00, 0x00, 0x04, 0x10, 0x00,
        0x00, 0x02, 0x20, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char vert_cursor_mask[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x03, 0xe0, 0x00,
        0x00, 0x07, 0xf0, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x01, 0xc0, 0x00,
        0x00, 0x01, 0xc0, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x01, 0xc0, 0x00,
        0x00, 0x01, 0xc0, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x01, 0xc0, 0x00,
        0x00, 0x01, 0xc0, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x01, 0xc0, 0x00,
        0x00, 0x01, 0xc0, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x01, 0xc0, 0x00,
        0x00, 0x01, 0xc0, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x07, 0xf0, 0x00,
        0x00, 0x03, 0xe0, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
};

namespace gui {

namespace {
const std::string DraggerImageVert  = "drag-widget-vertical";
const std::string DraggerImageHoriz = "drag-widget-horizontal";
}

drag_widget::drag_widget(const int x, const int y, const int w, const int h,
	const drag_direction dir,
    boost::function<void(int, int)> drag_start, 
    boost::function<void(int, int)> drag_end, 
    boost::function<void(int, int)> drag_move)
    : x_(x), y_(y), w_(w), h_(h), dir_(dir), 
    drag_start_(drag_start), drag_end_(drag_end), drag_move_(drag_move),
	
	old_cursor_(NULL), dragging_handle_(0)
{
	set_environment();
	init();
}

drag_widget::drag_widget(const variant& v, game_logic::formula_callable* e) 
	: widget(v,e), old_cursor_(NULL), dragging_handle_(0)
{
	ASSERT_LOG(get_environment() != 0, "You must specify a callable environment");
	dir_ = v["direction"].as_string_default("horizontal") == "horizontal" ? DRAG_HORIZONTAL : DRAG_VERTICAL;
	if(v.has_key("on_drag_start")) {
		drag_start_ = boost::bind(&drag_widget::drag_start, this, _1, _2);
		drag_start_handler_ = get_environment()->create_formula(v["on_drag_start"]);
	}
	if(v.has_key("on_drag_end")) {
		drag_end_ = boost::bind(&drag_widget::drag_end, this, _1, _2);
		drag_end_handler_ = get_environment()->create_formula(v["on_drag_end"]);
	}
	if(v.has_key("on_drag")) {
		drag_move_ = boost::bind(&drag_widget::drag, this, _1, _2);
		drag_handler_ = get_environment()->create_formula(v["on_drag"]);
	}
	std::vector<int> r = v["rect"].as_list_int();
	ASSERT_EQ(r.size(), 4);
	x_ = r[0];
	y_ = r[1];
	w_ = r[2];
	h_ = r[3];
	init();
}

void drag_widget::init()
{
	SDL_Cursor* curs;
	if(dir_ == DRAG_HORIZONTAL) {
		curs = SDL_CreateCursor(const_cast<Uint8*>(horiz_cursor_data), const_cast<Uint8*>(horiz_cursor_mask), 
			CURSOR_WIDTH, CURSOR_HEIGHT, HOT_X, HOT_Y);
		dragger_ = widget_ptr(new gui_section_widget(DraggerImageHoriz));
	} else if(dir_ == DRAG_VERTICAL) {
		curs = SDL_CreateCursor(const_cast<Uint8*>(vert_cursor_data), const_cast<Uint8*>(vert_cursor_mask), 
			CURSOR_WIDTH, CURSOR_HEIGHT, HOT_X, HOT_Y);
		drag_cursor_ = cursor_ptr(curs, SDL_FreeCursor);

		dragger_ = widget_ptr(new gui_section_widget(DraggerImageVert));
	} else {
		ASSERT_LOG(false, "Drag direction not given as horizontal or vertical " << dir_);
	}
	drag_cursor_ = cursor_ptr(curs, SDL_FreeCursor);

	dragger_->set_loc(0, h_/2 - dragger_->height()/2);
	//std::cerr << "DRAGGER LOC: " << dragger_dims_ << std::endl;
	//std::cerr << "LEFT EDGE BORDER: " << border_ << std::endl;
}

void drag_widget::drag(int dx, int dy)
{
	using namespace game_logic;
	if(get_environment()) {
		map_formula_callable_ptr callable = map_formula_callable_ptr(new map_formula_callable(get_environment()));
		callable->add("drag_dx", variant(dx));
		callable->add("drag_dy", variant(dy));
		variant value = drag_handler_->execute(*callable);
		get_environment()->execute_command(value);
	} else {
		std::cerr << "drag_widget::drag() called without environment!" << std::endl;
	}
}

void drag_widget::drag_start(int x, int y)
{
	using namespace game_logic;
	if(get_environment()) {
		map_formula_callable_ptr callable = map_formula_callable_ptr(new map_formula_callable(get_environment()));
		callable->add("drag_x", variant(x));
		callable->add("drag_y", variant(y));
		variant value = drag_start_handler_->execute(*callable);
		get_environment()->execute_command(value);
	} else {
		std::cerr << "drag_widget::drag_start() called without environment!" << std::endl;
	}
}

void drag_widget::drag_end(int x, int y)
{
	using namespace game_logic;
	if(get_environment()) {
		map_formula_callable_ptr callable = map_formula_callable_ptr(new map_formula_callable(get_environment()));
		callable->add("drag_x", variant(x));
		callable->add("drag_y", variant(y));
		variant value = drag_end_handler_->execute(*callable);
		get_environment()->execute_command(value);
	} else {
		std::cerr << "drag_widget::drag_end() called without environment!" << std::endl;
	}
}

void drag_widget::handle_draw() const
{
	if(dragger_) {
		dragger_->draw();
	}
}

bool drag_widget::handle_event(const SDL_Event& event, bool claimed)
{
	if(claimed) {
		return claimed;
	}

	if(event.type == SDL_MOUSEMOTION) {
		return handle_mousemotion(event.motion, claimed);
	} else if(event.type == SDL_MOUSEBUTTONDOWN) {
		return handle_mousedown(event.button, claimed);
	} else if(event.type == SDL_MOUSEBUTTONUP) {
		return handle_mouseup(event.button, claimed);
	}

	return claimed;
}

rect drag_widget::get_border_rect() const
{
	if(dir_ == DRAG_HORIZONTAL) {
		return rect(x_ - BORDER_THICKNESS/2, y_, BORDER_THICKNESS, h_);
	} 
	return rect(x_, y_ - BORDER_THICKNESS/2, w_, BORDER_THICKNESS);
}

rect drag_widget::get_dragger_rect() const
{
	return rect(x_, y_ + h_/2 - dragger_->height()/2, dragger_->width(), dragger_->height() );
}

bool drag_widget::handle_mousedown(const SDL_MouseButtonEvent& event, bool claimed)
{
	point p;
	int button_state = input::sdl_get_mouse_state(&p.x, &p.y);
	if(point_in_rect(p, get_border_rect()) || point_in_rect(p, get_dragger_rect())) {
		if(dragging_handle_ == 0) {
			dragging_handle_ = event.button;
			start_pos_ = p;
			if(drag_start_) {
				drag_start_(p.x, p.y);
			}
			claimed = claim_mouse_events();
		}
	}
	return claimed;
}

bool drag_widget::handle_mouseup(const SDL_MouseButtonEvent& event, bool claimed)
{
	int mousex, mousey;
	int button_state = input::sdl_get_mouse_state(&mousex, &mousey);
	if(dragging_handle_ == event.button) {
		dragging_handle_ = 0;
		if(drag_end_) {
			drag_end_(mousex, mousey);
		}
		claimed = claim_mouse_events();
	}
	return claimed;
}

bool drag_widget::handle_mousemotion(const SDL_MouseMotionEvent& event, bool claimed)
{
	point p;
	int button_state = input::sdl_get_mouse_state(&p.x, &p.y);
	if(dragging_handle_) {
		int dx = start_pos_.x - p.x;
		int dy = start_pos_.y - p.y;
		//std::cerr << "SDL_MOUSEMOTION: " << p.x << ", " << p.y << "; " << dx << "," << dy << std::endl;
		if(drag_move_) {
			drag_move_(dx, dy);
			start_pos_.x = p.x;
			start_pos_.y = p.y;
			if(dir_ == DRAG_HORIZONTAL) {
				x_ += dx;
				w_ += dx;
			} else if(dir_ == DRAG_VERTICAL) {
				y_ += dy;
				h_ += dy;
			}
		}
	} else {
		if(point_in_rect(p, get_dragger_rect()) || point_in_rect(p, get_border_rect())) {
			if(old_cursor_ == NULL) {
				old_cursor_ = SDL_GetCursor();
				SDL_SetCursor(drag_cursor_.get());
			}
		} else {
			if(old_cursor_) {
				SDL_SetCursor(old_cursor_);
				old_cursor_ = NULL;
			}
		}
	}
	return claimed;
}

}

#endif
