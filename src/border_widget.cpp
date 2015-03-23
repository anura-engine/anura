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

#include "Canvas.hpp"

#include "asserts.hpp"
#include "border_widget.hpp"
#include "widget_factory.hpp"

namespace gui 
{
	BorderWidget::BorderWidget(WidgetPtr child, const KRE::Color& col, int border_size)
	  : child_(child), color_(col), border_size_(border_size)
	{
		setEnvironment();
		setDim(child->width() + border_size*2, child->height() + border_size*2);
		child_->setLoc(border_size, border_size);
	}

	BorderWidget::BorderWidget(const variant& v, game_logic::FormulaCallable* e) : Widget(v,e)
	{
		ASSERT_LOG(v.is_map(), "TYPE ERROR: parameter to border widget must be a map");
		color_ = v.has_key("color") ? KRE::Color(0,0,0,255) : KRE::Color(v["color"]);
		border_size_ = v.has_key("border_size") ? v["border_size"].as_int() : 2;
		child_ = widget_factory::create(v["child"], e);
	}

	void BorderWidget::setColor(const KRE::Color& col)
	{
		color_ = col;
	}

	void BorderWidget::handleProcess()
	{
		Widget::handleProcess();
		if(child_) {
			child_->process();
		}
	}

	void BorderWidget::handleDraw() const
	{
		auto canvas = KRE::Canvas::getInstance();
		canvas->drawSolidRect(rect(x(),y(),width(),height()), color_);
		if(child_) {
			child_->draw(x(), y());
		}
	}

	bool BorderWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		SDL_Event ev = event;
		//normalizeEvent(&ev);
		if(child_) {
			return child_->processEvent(getPos(), ev, claimed);
		}
		return claimed;
	}

	ConstWidgetPtr BorderWidget::getWidgetById(const std::string& id) const
	{
		WidgetPtr wx = child_->getWidgetById(id);
		if(wx) {
			return wx;
		}
		return Widget::getWidgetById(id);
	}

	WidgetPtr BorderWidget::getWidgetById(const std::string& id)
	{
		WidgetPtr wx = child_->getWidgetById(id);
		if(wx) {
			return wx;
		}
		return Widget::getWidgetById(id);
	}

	std::vector<WidgetPtr> BorderWidget::getChildren() const
	{
		std::vector<WidgetPtr> result;
		result.push_back(child_);
		return result;
	}

	BEGIN_DEFINE_CALLABLE(BorderWidget, Widget)
		DEFINE_FIELD(child, "builtin widget")
			return variant(obj.child_.get());
		DEFINE_SET_FIELD
			obj.child_ = widget_factory::create(value, obj.getEnvironment());
	END_DEFINE_CALLABLE(BorderWidget)
}
