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

#include "label.hpp"
#include "image_widget.hpp"
#include "selector_widget.hpp"
#include "widget_factory.hpp"

namespace gui
{
	using namespace std::placeholders;

	namespace 
	{
		const std::string selector_left_arrow = "selector_left_arrow";
		const std::string selector_right_arrow = "selector_right_arrow";
	}

	SelectorWidget::SelectorWidget(const std::vector<std::string>& list)
		: current_selection_(0)
	{
		setEnvironment();
		for(const std::string& s : list) {
			list_.push_back(SelectorPair(s, WidgetPtr(new Label(s))));
		}
		init();
	}

	SelectorWidget::SelectorWidget(const SelectorList& list)
		: current_selection_(0), list_(list)
	{
		setEnvironment();
		init();
	}

	SelectorWidget::SelectorWidget(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v, e), current_selection_(v["selection"].as_int(0))
	{
		if(v.has_key("list") || v.has_key("children")) {
			const variant& l = v.has_key("list") ? v["list"] : v["children"];
			ASSERT_LOG(l.is_list(), "'list'/'children' attribute must be a list");
			for(const variant& child : l.as_list()) {
				if(child.is_list()) {
					ASSERT_LOG(child.num_elements() == 2, "items in the sub-list must have two elements.");
					WidgetPtr w;
					if(child[1].is_map()) {
						w = widget_factory::create(child[1], e);
					} else {
						w = child[1].try_convert<Widget>();
						ASSERT_LOG(w != nullptr, "Couldn't convert second element to widget.");
					}
					list_.push_back(SelectorPair(child[0].as_string(), w));
				} else if(child.is_string()) {
					const std::string& s = child.as_string();
					list_.push_back(SelectorPair(s, WidgetPtr(new Label(s))));
				} else {
					WidgetPtr w;
					std::string s;
					if(child.is_map()) {
						w = widget_factory::create(child, e);
						ASSERT_LOG(child.has_key("id") || child.has_key("select_string"), "list items must supply 'id' or 'select_string' attribute.");
						s = child.has_key("id") ? child["id"].as_string() : child["select_string"].as_string();
					} else {
						w = child.try_convert<Widget>();
						ASSERT_LOG(w != nullptr, "Couldn't convert item to widget.");
						ASSERT_LOG(!w->id().empty(), "list items must have 'id' attribute");
						s = w->id();
					}
					list_.push_back(SelectorPair(s, w));
				}
			}
		}

		if(v.has_key("on_change")) {
			change_handler_ = getEnvironment()->createFormula(v["on_change"]);
			on_change_ = std::bind(&SelectorWidget::changeDelegate, this, _1);
		}
		if(v.has_key("on_select")) {
			select_handler_ = getEnvironment()->createFormula(v["on_select"]);
			on_select_ = std::bind(&SelectorWidget::selectDelegate, this, _1);
		}
		init();
	}

	void SelectorWidget::setSelection(size_t sel)
	{
		size_t old_sel = current_selection_;
		current_selection_ = sel;
		ASSERT_LOG(current_selection_ < list_.size(), "Selector greater than list size.");
		list_[old_sel].second->enable(false);
		list_[current_selection_].second->enable();
		if(on_change_) {
			on_change_(list_[current_selection_].first);
		}
	}

	void SelectorWidget::setSelection(const std::string& sel)
	{
		SelectorList::iterator it = std::find_if(list_.begin(), list_.end(), 
			[=](const SelectorPair& s){ return s.first == sel; });
		ASSERT_LOG(it != list_.end(), "Selection not in list" << sel);
		setSelection(it - list_.begin());
	}

	void SelectorWidget::init()
	{
		left_arrow_ = WidgetPtr(new GuiSectionWidget(selector_left_arrow));
		right_arrow_ = WidgetPtr(new GuiSectionWidget(selector_right_arrow));

		int width = 16;
		int height = 16;
		int n = 0;
		for(const auto& p : list_) {
			if(p.second->width() > width) {
				width = p.second->width();
			}
			if(p.second->height() > height) {
				height = p.second->height();
			}
			if(n != current_selection_) {
				p.second->enable(false);
			} else {
				p.second->enable();
			}
			++n;
		}
		left_arrow_->setLoc(0, (abs(height-left_arrow_->height()))/2);
		//left_arrow_->setDim(left_arrow_->width(), height);
		right_arrow_->setLoc(left_arrow_->width()+10+width, (abs(height-right_arrow_->height()))/2);
		//right_arrow_->setDim(right_arrow_->width(), height);
		setDim(width + left_arrow_->width() + right_arrow_->width() + 10, height);
		for(int n = 0; n != list_.size(); ++n) {
			WidgetPtr& w = list_[n].second;
			w->setLoc((width - w->width())/2 + left_arrow_->width()+5, abs(height - w->height())/2);
		}
	}

	void SelectorWidget::handleDraw() const
	{
		if(left_arrow_) {
			left_arrow_->draw(x(),y(),getRotation(),getScale());
		}
		if(right_arrow_) {
			right_arrow_->draw(x(),y(),getRotation(),getScale());
		}
		if(current_selection_ < list_.size()) {
			if(list_[current_selection_].second) {
				list_[current_selection_].second->draw(x(),y(),getRotation(),getScale());
			}
		}
	}

	bool SelectorWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		SDL_Event ev = event;
		//normalizeEvent(&ev);

		if(claimed) {
			return claimed;
		}
		if(event.type == SDL_MOUSEMOTION) {
			return handleMouseMotion(event.motion, claimed);
		} else if(event.type == SDL_MOUSEBUTTONDOWN) {
			return handleMousedown(event.button, claimed);
		} else if(event.type == SDL_MOUSEBUTTONUP) {
			return handleMouseup(event.button, claimed);
		} else if(event.type == SDL_KEYDOWN) {
			const SDL_Keycode key = event.key.keysym.sym;
			if(key == SDLK_LEFT || key == SDLK_PAGEUP) {
				selectLeft();
			} else if(key == SDLK_RIGHT || key == SDLK_PAGEDOWN) {
				selectRight();
			} else if(key == SDLK_HOME) {
				if(!list_.empty()) {
					setSelection(0);
				}
			} else if(key == SDLK_END) {
				if(!list_.empty()) {
					setSelection(list_.size()-1);
				}
			} else if(key == SDLK_RETURN && on_select_) {
				on_select_(list_[current_selection_].first);
			}
		}
		return claimed;
	}

	BEGIN_DEFINE_CALLABLE(SelectorWidget, Widget)
		DEFINE_FIELD(selection, "string")
			return variant(obj.list_[obj.current_selection_].first);
		DEFINE_SET_FIELD
			obj.setSelection(value.as_string());

		DEFINE_FIELD(keys, "[string]")
			std::vector<variant> v;
			for(const SelectorPair& p : obj.list_) {
				v.push_back(variant(p.first));
			}
			return variant(&v);

	END_DEFINE_CALLABLE(SelectorWidget)

	bool SelectorWidget::handleMousedown(const SDL_MouseButtonEvent& event, bool claimed)
	{
		return claimed;
	}

	bool SelectorWidget::handleMouseup(const SDL_MouseButtonEvent& event, bool claimed)
	{
		point p(event.x, event.y);
		if(pointInRect(p, rect(left_arrow_->x(), 
			left_arrow_->y(), 
			left_arrow_->width(), 
			left_arrow_->height()))) {
			selectLeft();
			claimed = claimMouseEvents();
		}
		if(pointInRect(p, rect(right_arrow_->x(), 
			right_arrow_->y(), 
			right_arrow_->width(), 
			right_arrow_->height()))) {
			selectRight();
			claimed = claimMouseEvents();
		}
		WidgetPtr& cur = list_[current_selection_].second;
		if(pointInRect(p, rect(cur->x(), cur->y(), cur->width(), cur->height())) && on_select_) {
			on_select_(list_[current_selection_].first);
		}
		return claimed;
	}

	bool SelectorWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, bool claimed)
	{
		return claimed;
	}

	void SelectorWidget::changeDelegate(const std::string& s)
	{
		if(getEnvironment()) {
			game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable(getEnvironment());
			callable->add("selection", variant(s));
			callable->add("selected", variant(static_cast<int>(current_selection_)));
			variant v(callable);
			variant value = change_handler_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("SelectorWidget::changeDelegate() called without environment!");
		}
	}

	void SelectorWidget::selectDelegate(const std::string& s)
	{
		if(getEnvironment()) {
			game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable(getEnvironment());
			callable->add("selection", variant(s));
			callable->add("selected", variant(static_cast<int>(current_selection_)));
			variant v(callable);
			variant value = change_handler_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("SelectorWidget::selectDelegate() called without environment!");
		}
	}

	void SelectorWidget::selectLeft(size_t n)
	{
		int new_sel = int(current_selection_) - int(n);
		while(new_sel < 0) {
			new_sel += int(list_.size());
		}
		setSelection(new_sel);
	}

	void SelectorWidget::selectRight(size_t n)
	{
		setSelection(list_[(current_selection_ + n) % list_.size()].first);
	}
}
