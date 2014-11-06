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
#ifndef BORDER_WIDGET_HPP_INCLUDED
#define BORDER_WIDGET_HPP_INCLUDED

#include "graphics.hpp"
#include "color_utils.hpp"
#include "widget.hpp"

namespace gui {

//a widget which draws a border around another widget it holds as its child.
class border_widget : public widget
{
public:
	border_widget(widget_ptr child, graphics::color col, int border_size=2);
	border_widget(widget_ptr child, const SDL_Color& color, int border_size=2);
	border_widget(const variant& v, game_logic::formula_callable* e);
	void set_color(const graphics::color& col);
	void set_color(const SDL_Color& col);
	virtual widget_ptr get_widget_by_id(const std::string& id);
	const_widget_ptr get_widget_by_id(const std::string& id) const;
	std::vector<widget_ptr> get_children() const;
protected:
	virtual void handle_draw() const;
	virtual void handle_process();
private:
	bool handle_event(const SDL_Event& event, bool claimed);

	widget_ptr child_;
	graphics::color color_;
	int border_size_;
};

typedef boost::intrusive_ptr<border_widget> border_widget_ptr;

}

#endif
