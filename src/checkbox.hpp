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
#ifndef CHECKBOX_HPP_INCLUDED
#define CHECKBOX_HPP_INCLUDED

#include "button.hpp"

#include <string>

namespace gui {

class checkbox : public button
{
public:
	checkbox(const std::string& label, bool checked, boost::function<void(bool)> onclick, BUTTON_RESOLUTION button_resolution=BUTTON_SIZE_NORMAL_RESOLUTION);
	checkbox(widget_ptr label, bool checked, boost::function<void(bool)> onclick, BUTTON_RESOLUTION button_resolution=BUTTON_SIZE_NORMAL_RESOLUTION);
	checkbox(const variant& v, game_logic::formula_callable* e);
	bool checked() const { return checked_; }
protected:
	virtual variant get_value(const std::string& key) const;
private:
	void on_click();

	std::string label_;
	widget_ptr label_widget_;
	boost::function<void(bool)> onclick_;
	bool checked_;
	game_logic::formula_ptr click_handler_;
	void click(bool checked);
	int hpadding_;
};

typedef boost::intrusive_ptr<checkbox> checkbox_ptr;

}

#endif
