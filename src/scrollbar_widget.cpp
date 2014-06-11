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

scrollBarWidget::scrollBarWidget(boost::function<void(int)> handler)
  : handler_(handler),
    up_arrow_(new GuiSectionWidget(UpArrow)),
    down_arrow_(new GuiSectionWidget(DownArrow)),
	handle_(new GuiSectionWidget(VerticalHandle)),
	handle_bot_(new GuiSectionWidget(VerticalHandleBot)),
	handle_top_(new GuiSectionWidget(VerticalHandleTop)),
	background_(new GuiSectionWidget(VerticalBackground)),
	
	window_pos_(0), window_size_(0), range_(0), step_(0), arrow_step_(0),
	dragging_handle_(false),
	drag_start_(0), drag_anchor_y_(0)
{
	setEnvironment();
}

scrollBarWidget::scrollBarWidget(const variant& v, game_logic::FormulaCallable* e)
	: widget(v,e),	window_pos_(0), window_size_(0), range_(0),
	step_(0), arrow_step_(0),
	dragging_handle_(false), drag_start_(0), drag_anchor_y_(0)
{
	handler_ = boost::bind(&scrollBarWidget::handler_delegate, this, _1);
	ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
	ffl_handler_ = getEnvironment()->createFormula(v["on_scroll"]);
	
    up_arrow_ = v.has_key("up_arrow") ? widget_factory::create(v["up_arrow"], e) : new GuiSectionWidget(UpArrow);
    down_arrow_ = v.has_key("down_arrow") ? widget_factory::create(v["down_arrow"], e) : new GuiSectionWidget(DownArrow);
	handle_ = v.has_key("handle") ? widget_factory::create(v["handle"], e) : new GuiSectionWidget(VerticalHandle);
	handle_bot_ = v.has_key("handle_bottom") ? widget_factory::create(v["handle_bottom"], e) : new GuiSectionWidget(VerticalHandleBot);
	handle_top_ = v.has_key("handle_top") ? widget_factory::create(v["handle_top"], e) : new GuiSectionWidget(VerticalHandleTop);
	background_ = v.has_key("background") ? widget_factory::create(v["background"], e) : new GuiSectionWidget(VerticalBackground);
	if(v.has_key("range")) {
		std::vector<int> range = v["range"].as_list_int();
		ASSERT_EQ(range.size(), 2);
		set_range(range[0], range[1]);
	}
}

void scrollBarWidget::handler_delegate(int yscroll)
{
	using namespace game_logic;
	if(getEnvironment()) {
		map_FormulaCallablePtr callable(new map_FormulaCallable(getEnvironment()));
		callable->add("yscroll", variant(yscroll));
		variant value = ffl_handler_->execute(*callable);
		getEnvironment()->createFormula(value);
	} else {
		std::cerr << "scrollBarWidget::handler_delegate() called without environment!" << std::endl;
	}
}

void scrollBarWidget::set_range(int total_height, int window_height)
{
	window_size_ = window_height;
	range_ = total_height;
	if(window_pos_ < 0 || window_pos_ > range_ - window_size_) {
		window_pos_ = range_ - window_size_;
	}
}

void scrollBarWidget::setLoc(int x, int y)
{
	widget::setLoc(x, y);
	setDim(width(), height());
}

void scrollBarWidget::setDim(int w, int h)
{
	w = up_arrow_->width();
	up_arrow_->setLoc(x(), y());
	down_arrow_->setLoc(x(), y() + h - down_arrow_->height());
	background_->setLoc(x(), y() + up_arrow_->height());

	const int bar_height = h - (down_arrow_->height() + up_arrow_->height());
	background_->setDim(background_->width(), bar_height);

	if(range_) {
		handle_->setLoc(x(), y() + up_arrow_->height() + (window_pos_*bar_height)/range_);
		handle_->setDim(handle_->width(), std::max<int>(6, (window_size_*bar_height)/range_));
		handle_top_->setLoc(x(), y()+ up_arrow_->height() + (window_pos_*bar_height)/range_);
		handle_bot_->setLoc(x(), y()+ down_arrow_->height() + (window_pos_*bar_height)/range_ + (window_size_*bar_height)/range_ - handle_bot_->height() +1);
	}

	//TODO:  handle range < heightOfEndcaps
	
	widget::setDim(w, h);
}

void scrollBarWidget::down_button_pressed()
{
}

void scrollBarWidget::up_button_pressed()
{
}

void scrollBarWidget::handleDraw() const
{
	up_arrow_->draw();
	down_arrow_->draw();
	background_->draw();
	handle_->draw();
	handle_bot_->draw();
	handle_top_->draw();
}

void scrollBarWidget::clip_window_position()
{
	if(window_pos_ < 0) {
		window_pos_ = 0;
	}

	if(window_pos_ > range_ - window_size_) {
		window_pos_ = range_ - window_size_;
	}
}

bool scrollBarWidget::handleEvent(const SDL_Event& event, bool claimed)
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
			setDim(width(), height());
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

		claimed = claimMouseEvents();

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
			setDim(width(), height());
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

			setDim(width(), height());
			handler_(window_pos_);
		}
	}


	return claimed;
}

void scrollBarWidget::setValue(const std::string& key, const variant& v)
{
	if(key == "on_scroll") {
		ffl_handler_ = getEnvironment()->createFormula(v["on_scroll"]);
	} else if(key == "up_arrow") {
		up_arrow_ = widget_factory::create(v, getEnvironment());
	} else if(key == "down_arrow") {
		down_arrow_ = widget_factory::create(v, getEnvironment());
	} else if(key == "handle") {
		handle_ = widget_factory::create(v, getEnvironment());
	} else if(key == "handle_bottom") {
		handle_bot_ = widget_factory::create(v, getEnvironment());
	} else if(key == "handle_top") {
		handle_top_ = widget_factory::create(v, getEnvironment());
	} else if(key == "background") {
		background_ = widget_factory::create(v, getEnvironment());
	} else if(key == "range") {
		std::vector<int> range = v.as_list_int();
		ASSERT_EQ(range.size(), 2);
		set_range(range[0], range[1]);
	} else if(key == "position") {
		window_pos_ = v.as_int();
		clip_window_position();
	}
	
	widget::setValue(key, v);
}

variant scrollBarWidget::getValue(const std::string& key) const
{
	if(key == "range") {
		std::vector<variant> vv;
		vv.resize(2);
		vv.push_back(variant(range_));
		vv.push_back(variant(window_size_));
	} else if(key == "position") {
		return variant(window_pos_);
	}
	return widget::getValue(key);
}

}
