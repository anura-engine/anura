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
	key_button(key_type key, BUTTON_RESOLUTION button_resolution);
	key_button(const variant& v, game_logic::formula_callable* e);

	key_type get_key();

	void set_value(const std::string& key, const variant& v);
	variant get_value(const std::string& key) const;
private:
	bool in_button(int x, int y) const;
	void handle_draw() const;
	bool handle_event(const SDL_Event& event, bool claimed);

	BUTTON_RESOLUTION button_resolution_;
	widget_ptr label_;
	key_type key_;
	bool grab_keys_;

	const_framed_gui_element_ptr normal_button_image_set_,depressed_button_image_set_,focus_button_image_set_,current_button_image_set_;
};

typedef boost::intrusive_ptr<key_button> key_button_ptr;

}

#endif
