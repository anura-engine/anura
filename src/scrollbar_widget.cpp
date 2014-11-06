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
#include <boost/bind.hpp>

#include "image_widget.hpp"
#include "input.hpp"
#include "scrollbar_widget.hpp"
#include "widget_factory.hpp"

namespace gui {

namespace {
const std::string UpArrow = "scrollbar-vertical-up-arrow";
const std::string DownArrow = "scrollbar-vertical-down-arrow";
const std::string VerticalHandle = "scrollbar-vertical-handle-middle";
const std::string VerticalHandleBot = "scrollbar-vertical-handle-bottom";
const std::string VerticalHandleTop = "scrollbar-vertical-handle-top";
const std::string VerticalBackground = "scrollbar-vertical-background";
}

scrollbar_widget::scrollbar_widget(boost::function<void(int)> handler)
  : handler_(handler),
    up_arrow_(new gui_section_widget(UpArrow)),
    down_arrow_(new gui_section_widget(DownArrow)),
	handle_(new gui_section_widget(VerticalHandle)),
	handle_bot_(new gui_section_widget(VerticalHandleBot)),
	handle_top_(new gui_section_widget(VerticalHandleTop)),
	background_(new gui_section_widget(VerticalBackground)),
	
	window_pos_(0), window_size_(0), range_(0), step_(0), arrow_step_(0),
	dragging_handle_(false),
	drag_start_(0), drag_anchor_y_(0)
{
	set_environment();
}

scrollbar_widget::scrollbar_widget(const variant& v, game_logic::formula_callable* e)
	: widget(v,e),	window_pos_(0), window_size_(0), range_(0),
	step_(0), arrow_step_(0),
	dragging_handle_(false), drag_start_(0), drag_anchor_y_(0)
{
	handler_ = boost::bind(&scrollbar_widget::handler_delegate, this, _1);
	ASSERT_LOG(get_environment() != 0, "You must specify a callable environment");
	ffl_handler_ = get_environment()->create_formula(v["on_scroll"]);
	
    up_arrow_ = v.has_key("up_arrow") ? widget_factory::create(v["up_arrow"], e) : new gui_section_widget(UpArrow);
    down_arrow_ = v.has_key("down_arrow") ? widget_factory::create(v["down_arrow"], e) : new gui_section_widget(DownArrow);
	handle_ = v.has_key("handle") ? widget_factory::create(v["handle"], e) : new gui_section_widget(VerticalHandle);
	handle_bot_ = v.has_key("handle_bottom") ? widget_factory::create(v["handle_bottom"], e) : new gui_section_widget(VerticalHandleBot);
	handle_top_ = v.has_key("handle_top") ? widget_factory::create(v["handle_top"], e) : new gui_section_widget(VerticalHandleTop);
	background_ = v.has_key("background") ? widget_factory::create(v["background"], e) : new gui_section_widget(VerticalBackground);
	if(v.has_key("range")) {
		std::vector<int> range = v["range"].as_list_int();
		ASSERT_EQ(range.size(), 2);
		set_range(range[0], range[1]);
	}
}

void scrollbar_widget::handler_delegate(int yscroll)
{
	using namespace game_logic;
	if(get_environment()) {
		map_formula_callable_ptr callable(new map_formula_callable(get_environment()));
		callable->add("yscroll", variant(yscroll));
		variant value = ffl_handler_->execute(*callable);
		get_environment()->execute_command(value);
	} else {
		std::cerr << "scrollbar_widget::handler_delegate() called without environment!" << std::endl;
	}
}

void scrollbar_widget::set_range(int total_height, int window_height)
{
	window_size_ = window_height;
	range_ = total_height;
	if(window_pos_ < 0 || window_pos_ > range_ - window_size_) {
		window_pos_ = range_ - window_size_;
	}
}

void scrollbar_widget::set_loc(int x, int y)
{
	widget::set_loc(x, y);
	set_dim(width(), height());
}

void scrollbar_widget::set_dim(int w, int h)
{
	w = up_arrow_->width();
	up_arrow_->set_loc(x(), y());
	down_arrow_->set_loc(x(), y() + h - down_arrow_->height());
	background_->set_loc(x(), y() + up_arrow_->height());

	const int bar_height = h - (down_arrow_->height() + up_arrow_->height());
	background_->set_dim(background_->width(), bar_height);

	if(range_) {
		handle_->set_loc(x(), y() + up_arrow_->height() + (window_pos_*bar_height)/range_);
		handle_->set_dim(handle_->width(), std::max<int>(6, (window_size_*bar_height)/range_));
		handle_top_->set_loc(x(), y()+ up_arrow_->height() + (window_pos_*bar_height)/range_);
		handle_bot_->set_loc(x(), y()+ down_arrow_->height() + (window_pos_*bar_height)/range_ + (window_size_*bar_height)/range_ - handle_bot_->height() +1);
	}

	//TODO:  handle range < heightOfEndcaps
	
	widget::set_dim(w, h);
}

void scrollbar_widget::down_button_pressed()
{
}

void scrollbar_widget::up_button_pressed()
{
}

void scrollbar_widget::handle_draw() const
{
	up_arrow_->draw();
	down_arrow_->draw();
	background_->draw();
	handle_->draw();
	handle_bot_->draw();
	handle_top_->draw();
}

void scrollbar_widget::clip_window_position()
{
	if(window_pos_ < 0) {
		window_pos_ = 0;
	}

	if(window_pos_ > range_ - window_size_) {
		window_pos_ = range_ - window_size_;
	}
}

bool scrollbar_widget::handle_event(const SDL_Event& event, bool claimed)
{
	if(claimed) {
		return claimed;
	}

	if(event.type == SDL_MOUSEWHEEL) {
		int mx, my;
		input::sdl_get_mouse_state(&mx, &my);
		if(mx < x() || mx > x() + width() 
			|| my < y() || my > y() + height()) {
			return claimed;
		}

		const int start_pos = window_pos_;
		if(event.wheel.y > 0) {
			window_pos_ -= arrow_step_;
		} else {
			window_pos_ += arrow_step_;
		}

		clip_window_position();

		if(window_pos_ != start_pos) {
			set_dim(width(), height());
			handler_(window_pos_);
		}
		return claimed;
	} else
	if(event.type == SDL_MOUSEBUTTONDOWN) {
		const SDL_MouseButtonEvent& e = event.button;
		if(e.x < x() || e.x > x() + width() ||
		   e.y < y() || e.y > y() + height()) {
			return claimed;
		}

		const int start_pos = window_pos_;

		claimed = claim_mouse_events();

		if(e.y < up_arrow_->y() + up_arrow_->height()) {
			//on up arrow
			window_pos_ -= arrow_step_;
			while(arrow_step_ && window_pos_%arrow_step_) {
				//snap to a multiple of the step size.
				++window_pos_;
			}
		} else if(e.y > down_arrow_->y()) {
			//on down arrow
			window_pos_ += arrow_step_;
			while(arrow_step_ && window_pos_%arrow_step_) {
				//snap to a multiple of the step size.
				--window_pos_;
			}
		} else if(e.y < handle_->y()) {
			//page up
			window_pos_ -= window_size_ - arrow_step_;
		} else if(e.y > handle_->y() + handle_->height()) {
			//page down
			window_pos_ += window_size_ - arrow_step_;
		} else {
			//on handle
			dragging_handle_ = true;
			drag_start_ = window_pos_;
			drag_anchor_y_ = e.y;
		}

		std::cerr << "HANDLE: " << handle_->y() << ", " << handle_->height() << "\n";

		clip_window_position();

		if(window_pos_ != start_pos) {
			set_dim(width(), height());
			handler_(window_pos_);
		}

	} else if(event.type == SDL_MOUSEBUTTONUP) {
		dragging_handle_ = false;
	} else if(event.type == SDL_MOUSEMOTION) {
		const SDL_MouseMotionEvent& e = event.motion;

		int mousex, mousey;
		if(!input::sdl_get_mouse_state(&mousex, &mousey)) {
			dragging_handle_ = false;
		}

		if(dragging_handle_) {
			const int handle_height = height() - up_arrow_->height() - down_arrow_->height();
			const int move = e.y - drag_anchor_y_;
			const int window_move = (move*range_)/handle_height;
			window_pos_ = drag_start_ + window_move;
			if(step_) {
				window_pos_ -= window_pos_%step_;
			}

			clip_window_position();

			set_dim(width(), height());
			handler_(window_pos_);
		}
	}


	return claimed;
}

void scrollbar_widget::set_value(const std::string& key, const variant& v)
{
	if(key == "on_scroll") {
		ffl_handler_ = get_environment()->create_formula(v["on_scroll"]);
	} else if(key == "up_arrow") {
		up_arrow_ = widget_factory::create(v, get_environment());
	} else if(key == "down_arrow") {
		down_arrow_ = widget_factory::create(v, get_environment());
	} else if(key == "handle") {
		handle_ = widget_factory::create(v, get_environment());
	} else if(key == "handle_bottom") {
		handle_bot_ = widget_factory::create(v, get_environment());
	} else if(key == "handle_top") {
		handle_top_ = widget_factory::create(v, get_environment());
	} else if(key == "background") {
		background_ = widget_factory::create(v, get_environment());
	} else if(key == "range") {
		std::vector<int> range = v.as_list_int();
		ASSERT_EQ(range.size(), 2);
		set_range(range[0], range[1]);
	} else if(key == "position") {
		window_pos_ = v.as_int();
		clip_window_position();
	}
	
	widget::set_value(key, v);
}

variant scrollbar_widget::get_value(const std::string& key) const
{
	if(key == "range") {
		std::vector<variant> vv;
		vv.resize(2);
		vv.push_back(variant(range_));
		vv.push_back(variant(window_size_));
	} else if(key == "position") {
		return variant(window_pos_);
	}
	return widget::get_value(key);
}

}
