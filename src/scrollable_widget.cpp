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

#include "scrollable_widget.hpp"

namespace gui
{
	ScrollableWidget::ScrollableWidget()
		: yscroll_(0),
		  virtual_height_(0),
		  step_(0),
		  arrow_step_(0),
		  auto_scroll_bottom_(false)
	{
		setEnvironment();
	}

	ScrollableWidget::ScrollableWidget(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e),
		  yscroll_(0),
		  virtual_height_(0),
		  step_(0),
		  arrow_step_(0),
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

	ScrollableWidget::ScrollableWidget(const ScrollableWidget& s)
		: Widget(s),
		  yscroll_(s.yscroll_),
		  virtual_height_(s.virtual_height_),
		  step_(s.step_),
		  arrow_step_(s.arrow_step_),
		  auto_scroll_bottom_(s.auto_scroll_bottom_),
		  // for update of scrollbar widget next time updateScrollbar is called.
		  scrollbar_(nullptr)
	{
	}

	void ScrollableWidget::setYscroll(int yscroll)
	{
		LOG_DEBUG("setYscroll: " << yscroll);
		const int old = yscroll_;
		yscroll_ = yscroll;
		onSetYscroll(old, yscroll);
	}

	void ScrollableWidget::setDim(int w, int h)
	{
		Widget::setDim(w, h);
		updateScrollbar();
	}

	void ScrollableWidget::onSetYscroll(int old_yscroll, int new_yscroll)
	{
	}

	void ScrollableWidget::setVirtualHeight(int height)
	{
		virtual_height_ = height;
		if(auto_scroll_bottom_) {
			setYscroll(height - this->height());
		}
		updateScrollbar();
	}

	void ScrollableWidget::setScrollStep(int step)
	{
		step_ = step;
	}

	void ScrollableWidget::setArrowScrollStep(int step)
	{
		arrow_step_ = step;
	}

	void ScrollableWidget::updateScrollbar()
	{
		if(height() < virtual_height_) {
			if(!scrollbar_) {
				scrollbar_.reset(new ScrollBarWidget(std::bind(&ScrollableWidget::setYscroll, this, std::placeholders::_1)));
			}
			scrollbar_->setStep(step_);
			scrollbar_->setArrowStep(arrow_step_);
			scrollbar_->setRange(virtual_height_, height());
			scrollbar_->setWindowPos(yscroll_);
			scrollbar_->setLoc(x() + width(), y());
			scrollbar_->setDim(0, height());
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
			return scrollbar_->processEvent(point(getPos().x-x(), getPos().y-y()), event, claimed);
		}

		return claimed;
	}

	void ScrollableWidget::setLoc(int x, int y)
	{
		Widget::setLoc(x, y);
		if(scrollbar_) {
			scrollbar_->setLoc(x + width(), y);
		}
	}

	WidgetPtr ScrollableWidget::clone() const
	{
		ScrollableWidget* sw = new ScrollableWidget(*this);
		sw->updateScrollbar();
		return WidgetPtr(sw);
	}

	BEGIN_DEFINE_CALLABLE(ScrollableWidget, Widget)
		DEFINE_FIELD(yscroll, "int")
			return variant(obj.yscroll_);
		DEFINE_SET_FIELD
			obj.setYscroll(value.as_int());

		DEFINE_FIELD(virtual_height, "int")
			return variant(obj.virtual_height_);
		DEFINE_SET_FIELD
			obj.setVirtualHeight(value.as_int());

		DEFINE_FIELD(step, "int")
			return variant(obj.step_);
		DEFINE_SET_FIELD
			obj.setScrollStep(value.as_int());
	END_DEFINE_CALLABLE(ScrollableWidget)
}
