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
#pragma once
#ifndef SELECTOR_WIDGET_HPP_INCLUDED
#define SELECTOR_WIDGET_HPP_INCLUDED

#include <vector>
#include <boost/intrusive_ptr.hpp>
#include <boost/function.hpp>

#include "widget.hpp"

namespace gui
{
typedef std::pair<std::string, widget_ptr> selector_pair;
typedef std::vector<selector_pair> selector_list;

class selector_widget : public widget
{
public:
	explicit selector_widget(const std::vector<std::string>& list);
	explicit selector_widget(const selector_list& list);
	explicit selector_widget(const variant& v, game_logic::formula_callable* e);
	virtual ~selector_widget() {}

	void set_on_change_handler(boost::function<void(const std::string&)> fn) { on_change_ = fn; }
	void set_on_select_handler(boost::function<void(const std::string&)> fn) { on_select_ = fn; }
	void set_selection(const std::string& sel);
	void set_selection(size_t sel);
	std::string get_selection();
protected:
	virtual void handle_draw() const;
	virtual bool handle_event(const SDL_Event& event, bool claimed);

	virtual void set_value(const std::string& key, const variant& v);
	virtual variant get_value(const std::string& key) const;
	void init();
private:
	bool handle_mousedown(const SDL_MouseButtonEvent& event, bool claimed);
	bool handle_mouseup(const SDL_MouseButtonEvent& event, bool claimed);
	bool handle_mousemotion(const SDL_MouseMotionEvent& event, bool claimed);
	void select_left(size_t n=1);
	void select_right(size_t n=1);

	selector_list list_;
	size_t current_selection_;
	boost::function<void(const std::string&)> on_change_;
	boost::function<void(const std::string&)> on_select_;

	widget_ptr left_arrow_;
	widget_ptr right_arrow_;

	// delgate 
	void change_delegate(const std::string& s);
	void select_delegate(const std::string& s);
	// FFL formula
	game_logic::formula_ptr change_handler_;
	game_logic::formula_ptr select_handler_;
};

typedef boost::intrusive_ptr<selector_widget> selector_widget_ptr;
typedef boost::intrusive_ptr<const selector_widget> const_selector_widget_ptr;
}

#endif
