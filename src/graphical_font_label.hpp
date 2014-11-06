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
#ifndef GRAPHICAL_FONT_LABEL_HPP_INCLUDED
#define GRAPHICAL_FONT_LABEL_HPP_INCLUDED

#include "graphical_font.hpp"
#include "widget.hpp"

namespace gui {

class graphical_font_label : public widget
{
public:
	graphical_font_label(const std::string& text, const std::string& font, int size=1);
	graphical_font_label(const variant& v, game_logic::formula_callable* e);

	void set_text(const std::string& text);
	void reset_text_dimensions();
	
	void set_value(const std::string& key, const variant& v);
	variant get_value(const std::string& key) const;
private:
	void handle_draw() const;

	std::string text_;
	const_graphical_font_ptr font_;
	int size_;
};

typedef boost::intrusive_ptr<graphical_font_label> graphical_font_label_ptr;

}

#endif
