/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef KEY_BUTTON_HPP_INCLUDED
#define KEY_BUTTON_HPP_INCLUDED

#include <boost/function.hpp>

#include "button.hpp"
#include "controls.hpp"
#include "texture.hpp"
#include "widget.hpp"
#include "framed_gui_element.hpp"


namespace gui {

std::string get_key_name(key_type key);

//a key selection button widget. Does not derive from button as we don't need the onclick event.
class key_button : public widget
{
public:
	key_button(key_type key, buttonResolution buttonResolution);
	key_button(const variant& v, game_logic::FormulaCallable* e);

	key_type get_key();

	void setValue(const std::string& key, const variant& v);
	variant getValue(const std::string& key) const;
private:
	bool in_button(int x, int y) const;
	void handleDraw() const;
	bool handleEvent(const SDL_Event& event, bool claimed);

	buttonResolution button_resolution_;
	WidgetPtr label_;
	key_type key_;
	bool grab_keys_;

	ConstFramedGuiElementPtr normal_button_image_set_,depressed_button_image_set_,focus_button_image_set_,current_button_image_set_;
};

typedef boost::intrusive_ptr<key_button> key_ButtonPtr;

}

#endif
