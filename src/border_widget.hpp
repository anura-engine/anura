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
#pragma once

#include "Color.hpp"
#include "widget.hpp"

namespace gui {

//a widget which draws a border around another widget it holds as its child.
class border_widget : public widget
{
public:
	border_widget(WidgetPtr child, const KRE::Color& col, int border_size=2);
	border_widget(const variant& v, game_logic::FormulaCallable* e);
	void setColor(const KRE::Color& col);
	virtual WidgetPtr getWidgetById(const std::string& id);
	ConstWidgetPtr getWidgetById(const std::string& id) const;
	std::vector<WidgetPtr> getChildren() const;
protected:
	virtual void handleDraw() const;
	virtual void handleProcess();
private:
	bool handleEvent(const SDL_Event& event, bool claimed);

	WidgetPtr child_;
	KRE::Color color_;
	int border_size_;
};

typedef boost::intrusive_ptr<border_widget> border_WidgetPtr;

}
