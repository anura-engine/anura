/*
	Copyright (C) 2003-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "asserts.hpp"
#include "view3D_widget.hpp"
#include "widget_factory.hpp"

namespace gui
{
	View3DWidget::View3DWidget(int x, int y, int width, int height)
	{
		setLoc(x, y);
		setDim(width, height);

		init();
	}

	View3DWidget::View3DWidget(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e)
	{
		
		init();

		if(v.has_key("children")) {
			resetContents(v["children"]);
		}
	}

	void View3DWidget::resetContents(const variant& v)
	{
		children_.clear();
		if(v.is_null()) {
			return;
		}
		if(v.is_list()) {
			for(int n = 0; n != v.num_elements(); ++n) {
				children_.push_back(widget_factory::create(v[n],getEnvironment()));
			}
		} else {
			children_.push_back(widget_factory::create(v,getEnvironment()));
		}
	}

	View3DWidget::~View3DWidget()
	{
	}

	void View3DWidget::init()
	{
	}

	void View3DWidget::handleDraw() const
	{
		for(auto child : children_) {
			child->draw(x(),y(),getRotation(),getScale());
		}
	}

	bool View3DWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
	    for(auto child : children_) {
			claimed = child->processEvent(event, claimed);
			if(claimed) {
				return true;
			}
		}
		return false;
	}

	void View3DWidget::handleProcess()
	{
	    for(auto child : children_) {
			child->process();
		}
	}

	BEGIN_DEFINE_CALLABLE(View3DWidget, Widget)
	DEFINE_FIELD(children, "[widget]")
		std::vector<variant> v;
	    for(auto w : obj.children_) {
			v.push_back(variant(w.get()));
		}
		return variant(&v);
	DEFINE_SET_FIELD_TYPE("list|map")
		obj.resetContents(value);
	END_DEFINE_CALLABLE(View3DWidget)
}
