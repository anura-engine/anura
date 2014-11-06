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
#ifndef IMAGE_WIDGET_HPP_INCLUDED
#define IMAGE_WIDGET_HPP_INCLUDED

#include <string>

#include "geometry.hpp"
#include "gui_section.hpp"
#include "image_widget_fwd.hpp"
#include "texture.hpp"
#include "widget.hpp"

namespace gui {

class image_widget : public widget
{
public:
	explicit image_widget(const std::string& fname, int w=-1, int h=-1);
	explicit image_widget(graphics::texture tex, int w=-1, int h=-1);
	explicit image_widget(const variant& v, game_logic::formula_callable* e);

	void init(int w, int h);

	const rect& area() const { return area_; }
	const graphics::texture& tex() const { return texture_; }

	void set_rotation(GLfloat rotate) { rotate_ = rotate; }
	void set_area(const rect& area) { area_ = area; }

	void set_value(const std::string& key, const variant& v);
	variant get_value(const std::string& key) const;

private:
	void handle_draw() const;

	graphics::texture texture_;
	GLfloat rotate_;
	rect area_;
	std::string image_name_;
};

class gui_section_widget : public widget
{
public:
	explicit gui_section_widget(const std::string& id, int w=-1, int h=-1, int scale=1);
	explicit gui_section_widget(const variant& v, game_logic::formula_callable* e);

	//sets the GUI section. The dimensions of the widget will not change;
	//you should set a GUI section that is the same size.
	void set_gui_section(const std::string& id);

	void handle_draw() const;
protected:
	void set_value(const std::string& key, const variant& v);
	variant get_value(const std::string& key) const;
private:
	const_gui_section_ptr section_;
	int scale_;
};

typedef boost::intrusive_ptr<gui_section_widget> gui_section_widget_ptr;

}

#endif
