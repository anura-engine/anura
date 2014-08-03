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

#ifndef NO_EDITOR

#include <vector>

#include "asserts.hpp"
#include "image_widget.hpp"
#include "input.hpp"
#include "drag_widget.hpp"

namespace
{
	const int HOT_X = 16;
	const int HOT_Y = 16;
	
	const int CURSOR_WIDTH	= 32;
	const int CURSOR_HEIGHT	= 32;

	const int BORDER_THICKNESS = 14;

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
}

namespace gui 
{

	namespace 
	{
		const std::string DraggerImageVert  = "drag-widget-vertical";
		const std::string DraggerImageHoriz = "drag-widget-horizontal";
	}	

	DragWidget::DragWidget(const int x, const int y, const int w, const int h,
		const Direction dir,
		std::function<void(int, int)> drag_start, 
		std::function<void(int, int)> drag_end, 
		std::function<void(int, int)> drag_move)
		: x_(x), y_(y), w_(w), h_(h), dir_(dir), 
		drag_start_(drag_start), drag_end_(drag_end), drag_move_(drag_move),
	
		old_cursor_(NULL), dragging_handle_(0)
	{
		setEnvironment();
		init();
	}

	DragWidget::DragWidget(const variant& v, game_logic::FormulaCallable* e) 
		: Widget(v,e), old_cursor_(NULL), dragging_handle_(0)
	{
		using std::placeholders::_1;
		using std::placeholders::_2;
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
		dir_ = v["direction"].as_string_default("horizontal") == "horizontal" ? Direction::HORIZONTAL : Direction::VERTICAL;
		if(v.has_key("on_drag_start")) {
			drag_start_ = std::bind(&DragWidget::dragStart, this, _1, _2);
			drag_start_handler_ = getEnvironment()->createFormula(v["on_drag_start"]);
		}
		if(v.has_key("on_drag_end")) {
			drag_end_ = std::bind(&DragWidget::dragEnd, this, _1, _2);
			drag_end_handler_ = getEnvironment()->createFormula(v["on_drag_end"]);
		}
		if(v.has_key("on_drag")) {
			drag_move_ = std::bind(&DragWidget::drag, this, _1, _2);
			drag_handler_ = getEnvironment()->createFormula(v["on_drag"]);
		}
		std::vector<int> r = v["rect"].as_list_int();
		ASSERT_EQ(r.size(), 4);
		x_ = r[0];
		y_ = r[1];
		w_ = r[2];
		h_ = r[3];
		init();
	}

	void DragWidget::init()
	{
		SDL_Cursor* curs;
		if(dir_ == Direction::HORIZONTAL) {
			curs = SDL_CreateCursor(const_cast<Uint8*>(horiz_cursor_data), const_cast<Uint8*>(horiz_cursor_mask), 
				CURSOR_WIDTH, CURSOR_HEIGHT, HOT_X, HOT_Y);
			dragger_ = WidgetPtr(new GuiSectionWidget(DraggerImageHoriz));
		} else if(dir_ == Direction::VERTICAL) {
			curs = SDL_CreateCursor(const_cast<Uint8*>(vert_cursor_data), const_cast<Uint8*>(vert_cursor_mask), 
				CURSOR_WIDTH, CURSOR_HEIGHT, HOT_X, HOT_Y);
			drag_cursor_ = CursorPtr(curs, SDL_FreeCursor);

			dragger_ = WidgetPtr(new GuiSectionWidget(DraggerImageVert));
		} else {
			ASSERT_LOG(false, "Drag direction not given as horizontal or vertical " << static_cast<int>(dir_));
		}
		drag_cursor_ = CursorPtr(curs, SDL_FreeCursor);

		dragger_->setLoc(0, h_/2 - dragger_->height()/2);
	}

	void DragWidget::drag(int dx, int dy)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("drag_dx", variant(dx));
			callable->add("drag_dy", variant(dy));
			variant value = drag_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "drag_widget::drag() called without environment!" << std::endl;
		}
	}

	void DragWidget::dragStart(int x, int y)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("drag_x", variant(x));
			callable->add("drag_y", variant(y));
			variant value = drag_start_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "drag_widget::drag_start() called without environment!" << std::endl;
		}
	}

	void DragWidget::dragEnd(int x, int y)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("drag_x", variant(x));
			callable->add("drag_y", variant(y));
			variant value = drag_end_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "drag_widget::drag_end() called without environment!" << std::endl;
		}
	}

	void DragWidget::handleDraw() const
	{
		if(dragger_) {
			dragger_->draw();
		}
	}

	bool DragWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(claimed) {
			return claimed;
		}

		if(event.type == SDL_MOUSEMOTION) {
			return handleMouseMotion(event.motion, claimed);
		} else if(event.type == SDL_MOUSEBUTTONDOWN) {
			return handleMousedown(event.button, claimed);
		} else if(event.type == SDL_MOUSEBUTTONUP) {
			return handleMouseup(event.button, claimed);
		}

		return claimed;
	}

	rect DragWidget::getBorderRect() const
	{
		if(dir_ == Direction::HORIZONTAL) {
			return rect(x_ - BORDER_THICKNESS/2, y_, BORDER_THICKNESS, h_);
		} 
		return rect(x_, y_ - BORDER_THICKNESS/2, w_, BORDER_THICKNESS);
	}

	rect DragWidget::getDraggerRect() const
	{
		return rect(x_, y_ + h_/2 - dragger_->height()/2, dragger_->width(), dragger_->height() );
	}

	bool DragWidget::handleMousedown(const SDL_MouseButtonEvent& event, bool claimed)
	{
		point p;
		int button_state = input::sdl_get_mouse_state(&p.x, &p.y);
		if(pointInRect(p, getBorderRect()) || pointInRect(p, getDraggerRect())) {
			if(dragging_handle_ == 0) {
				dragging_handle_ = event.button;
				start_pos_ = p;
				if(drag_start_) {
					drag_start_(p.x, p.y);
				}
				claimed = claimMouseEvents();
			}
		}
		return claimed;
	}

	bool DragWidget::handleMouseup(const SDL_MouseButtonEvent& event, bool claimed)
	{
		int mousex, mousey;
		int button_state = input::sdl_get_mouse_state(&mousex, &mousey);
		if(dragging_handle_ == event.button) {
			dragging_handle_ = 0;
			if(drag_end_) {
				drag_end_(mousex, mousey);
			}
			claimed = claimMouseEvents();
		}
		return claimed;
	}

	bool DragWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, bool claimed)
	{
		point p;
		int button_state = input::sdl_get_mouse_state(&p.x, &p.y);
		if(dragging_handle_) {
			int dx = start_pos_.x - p.x;
			int dy = start_pos_.y - p.y;
			if(drag_move_) {
				drag_move_(dx, dy);
				start_pos_.x = p.x;
				start_pos_.y = p.y;
				if(dir_ == Direction::HORIZONTAL) {
					x_ += dx;
					w_ += dx;
				} else if(dir_ == Direction::VERTICAL) {
					y_ += dy;
					h_ += dy;
				}
			}
		} else {
			if(pointInRect(p, getDraggerRect()) || pointInRect(p, getBorderRect())) {
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
