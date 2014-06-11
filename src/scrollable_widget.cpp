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

#include "scrollable_widget"

namespace gui {

ScrollableWidget::ScrollableWidget() : yscroll_(0), virtual_height_(0), step_(0), arrow_step_(0), auto_scroll_bottom_(false)
{
	setEnvironment();
}

ScrollableWidget::ScrollableWidget(const variant& v, game_logic::FormulaCallable* e)
	: widget(v,e), yscroll_(0), virtual_height_(0), step_(0), arrow_step_(0),
	  auto_scroll_bottom_(v["auto_scroll_bottom"].as_bool())
{
	if(v.has_key("yscroll")) {
		yscroll_ = v["yscroll"].as_int();
	}
	if(v.has_key("virtual_height")) {
		virtual_height_ = v["virtual_height"].as_int();
	}
	if(v.has_key("step")) {
		arrow_step_ = step_ = v["step"].as_int();
	}
}

ScrollableWidget::~ScrollableWidget()
{}

void ScrollableWidget::set_yscroll(int yscroll)
{
	const int old = yscroll_;
	yscroll_ = yscroll;
	on_set_yscroll(old, yscroll);
}

void ScrollableWidget::setDim(int w, int h)
{
	widget::setDim(w, h);
	update_scrollbar();
}

void ScrollableWidget::on_set_yscroll(int old_yscroll, int new_yscroll)
{}

void ScrollableWidget::set_virtual_height(int height)
{
	virtual_height_ = height;
	if(auto_scroll_bottom_) {
		set_yscroll(height - this->height());
	}
	update_scrollbar();
}

void ScrollableWidget::set_scroll_step(int step)
{
	step_ = step;
}

void ScrollableWidget::set_arrow_scroll_step(int step)
{
	arrow_step_ = step;
}

void ScrollableWidget::update_scrollbar()
{
	if(height() < virtual_height_) {
		if(!scrollbar_) {
			scrollbar_.reset(new scrollBarWidget(boost::bind(&ScrollableWidget::set_yscroll, this, _1)));
		}
		scrollbar_->set_step(step_);
//		if(step_ != arrow_step_) {
			scrollbar_->set_arrow_step(arrow_step_);
//		}
		scrollbar_->set_range(virtual_height_, height());
		scrollbar_->set_window_pos(yscroll_);
		scrollbar_->setLoc(x() + width(), y());
		scrollbar_->setDim(10, height());
	} else {
		scrollbar_.reset();
	}

}

void ScrollableWidget::handleDraw() const
{
	if(scrollbar_) {
		scrollbar_->draw();
	}
}

bool ScrollableWidget::handleEvent(const SDL_Event& event, bool claimed)
{
	if(scrollbar_) {
		return scrollbar_->processEvent(event, claimed);
	}

	return claimed;
}

void ScrollableWidget::setLoc(int x, int y)
{
	widget::setLoc(x, y);
	if(scrollbar_) {
		scrollbar_->setLoc(x + width(), y);
	}
}

void ScrollableWidget::setValue(const std::string& key, const variant& v)
{
	if(key == "yscroll") {
		set_yscroll(v.as_int());
	} else if(key == "virtual_height") {
		set_virtual_height(v.as_int());
	} else if(key == "step") {
		set_scroll_step(v.as_int());
	}
	widget::setValue(key, v);
}

variant ScrollableWidget::getValue(const std::string& key) const
{
	if(key == "yscroll") {
		return variant(yscroll_);
	} else if(key == "virtual_height") {
		return variant(virtual_height_);
	} else if(key == "step") {
		return variant(step_);
	}
	return widget::getValue(key);
}

}
