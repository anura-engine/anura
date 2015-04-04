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

#include <algorithm>
#include "asserts.hpp"
#include "formula_callable_visitor.hpp"
#include "layout_widget.hpp"
#include "widget_factory.hpp"

namespace gui
{
	LayoutWidget::LayoutWidget(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e), fixed_width_(0), fixed_height_(0), layout_type_(LayoutType::ABSOLUTE)
	{
		if(v.has_key("style")) {
			const std::string style = v["style"].as_string();
			if(style == "absolute") {
				layout_type_ = LayoutType::ABSOLUTE;
			} else if(style == "relative") {
				layout_type_ = LayoutType::RELATIVE;
			} else {
				ASSERT_LOG(false, "Unrecognised layout style: " << style);
			}
		}

		ASSERT_LOG(v.has_key("children"), "layout widget must have a 'children' attribute.");
		ASSERT_LOG(v["children"].is_list(), "layout widget must have 'children' attribute that is a list.");
		const variant& children = v["children"];
		for(size_t n = 0; n != children.num_elements(); ++n) {
			children_.insert(widget_factory::create(children[n], e));
		}
		if(width()) {
			fixed_width_ = width();
		}
		if(height()) {
			fixed_height_ = height();
		}

		reflowChildren();
	}

	LayoutWidget::~LayoutWidget()
	{
	}

	void LayoutWidget::reflowChildren()
	{
		int lx = 0;
		int ly = 0;
		int lw = 0;
		int lh = 0;
		if(layout_type_ == LayoutType::RELATIVE) {
			for(auto w : children_) {
				ASSERT_LOG(w->width() < fixed_width_, "width of child widget is greater than width of layout widget");
				if(lx + w->width() > fixed_width_) {
					ly += lh;
					lh = 0;
				}
				lh = std::max(lh, w->height());
				w->setLoc(lx, ly);
				lx += w->width();
				lw = std::max(lw, lx);
			}
		} else if(layout_type_ == LayoutType::ABSOLUTE) {
			// do nothing
			for(auto w : children_) {
				lw = std::max(lw, w->width());
				lh = std::max(lh, w->height());
			}
		} else {
			ASSERT_LOG(false, "Incorrect layout style");
		}
		if(fixed_height_ == 0 && fixed_width_ == 0) {
			setDim(lw, lh);
		}
	}

	void LayoutWidget::recalcLoc()
	{
		Widget::recalcLoc();
		if(width()) {
			fixed_width_ = width();
		}
		if(height()) {
			fixed_height_ = height();
		}
	}

	void LayoutWidget::handleDraw() const
	{
		for(auto w : children_) {
			w->draw(x(), y());
		}
	}

	bool LayoutWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		for(auto w : children_) {
			claimed = w->processEvent(getPos(), event, claimed);
			if(claimed) {
				return claimed;
			}
		}
		return claimed;
	}

	std::vector<WidgetPtr> LayoutWidget::getChildren() const
	{

		std::vector<WidgetPtr> v;
		for(auto w : children_) {
			v.emplace_back(w);
		}
		return v;
	}

	void LayoutWidget::visitValues(game_logic::FormulaCallableVisitor& visitor)
	{
		for(auto w : children_) {
			visitor.visit(&w);
		}
	}

	variant LayoutWidget::handleWrite()
	{
		variant_builder res;
		res.add("type", "layout");
		switch(layout_type_) {
			case LayoutType::ABSOLUTE: res.add("style", "absolute"); break;
			case LayoutType::RELATIVE: res.add("style", "relative"); break;
			default:
				ASSERT_LOG(false, "Incorrect layout style");
		}
		for(auto w : children_) {
			res.add("children", w->write());
		}
		return res.build();
	}

	WidgetPtr LayoutWidget::clone() const
	{
		LayoutWidget* lw = new LayoutWidget(*this);
		lw->children_.clear();
		for(const auto& w : children_) {
			lw->children_.emplace(w->clone());
		}
		return WidgetPtr(lw);
	}

	BEGIN_DEFINE_CALLABLE(LayoutWidget, Widget)
		DEFINE_FIELD(dummy, "int")
			return variant();
	END_DEFINE_CALLABLE(LayoutWidget)
}
