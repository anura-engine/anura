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
#ifndef SCROLLABLE_WIDGET_HPP_INCLUDED
#define SCROLLABLE_WIDGET_HPP_INCLUDED

#include <boost/shared_ptr.hpp>

#include "scrollbar_widget.hpp"
#include "widget.hpp"

namespace gui {

class scrollable_widget : public widget
{
public:
	scrollable_widget();
	scrollable_widget(const variant& v, game_logic::formula_callable* e);
	void set_yscroll(int yscroll);
	virtual void set_dim(int w, int h);

	virtual void handle_draw() const;
	virtual bool handle_event(const SDL_Event& event, bool claimed);

	virtual void set_loc(int x, int y);
protected:
	~scrollable_widget();
	void set_virtual_height(int height);
	void set_scroll_step(int step);
	void set_arrow_scroll_step(int step);
	void update_scrollbar();

	int yscroll() const { return yscroll_; }
	int virtual_height() const { return virtual_height_; }

	virtual void set_value(const std::string& key, const variant& v);
	virtual variant get_value(const std::string& key) const;
private:
	virtual void on_set_yscroll(int old_yscroll, int new_yscroll);

	int yscroll_;
	int virtual_height_;
	int step_;
	int arrow_step_;

	bool auto_scroll_bottom_;

	scrollbar_widget_ptr scrollbar_;
};

typedef boost::intrusive_ptr<scrollable_widget> scrollable_widget_ptr;
typedef boost::intrusive_ptr<const scrollable_widget> const_scrollable_widget_ptr;

}

#endif
