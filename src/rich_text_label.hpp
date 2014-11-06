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
#ifndef RICH_TEXT_LABEL_HPP_INCLUDED
#define RICH_TEXT_LABEL_HPP_INCLUDED

#include <string>
#include <vector>

#include "formula_callable.hpp"
#include "scrollable_widget.hpp"
#include "widget.hpp"

namespace gui
{

class rich_text_label : public scrollable_widget
{
public:
	rich_text_label(const variant& v, game_logic::formula_callable* e);

	std::vector<widget_ptr> get_children() const;
private:

	void handle_process();
	void handle_draw() const;
	bool handle_event(const SDL_Event& event, bool claimed);

	variant get_value(const std::string& key) const;
	void set_value(const std::string& key, const variant& v);

	std::vector<std::vector<widget_ptr> > children_;
};

}

#endif
