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
#include <SDL.h>

#include "asserts.hpp"
#include "graphics.hpp"
#include "border_widget.hpp"
#include "raster.hpp"
#include "widget_factory.hpp"

namespace gui {

border_widget::border_widget(widget_ptr child, graphics::color col, int border_size)
  : child_(child), color_(col), border_size_(border_size)
{
	set_environment();
	set_dim(child->width() + border_size*2, child->height() + border_size*2);
	child_->set_loc(border_size, border_size);
}

border_widget::border_widget(widget_ptr child, const SDL_Color& color, int border_size)
	: child_(child), color_(color.r, color.g, color.b, color.a), border_size_(border_size)
{
	set_environment();
	set_dim(child->width() + border_size*2, child->height() + border_size*2);
	child_->set_loc(border_size, border_size);
}

border_widget::border_widget(const variant& v, game_logic::formula_callable* e) : widget(v,e)
{
	ASSERT_LOG(v.is_map(), "TYPE ERROR: parameter to border widget must be a map");
	color_ = v.has_key("color") ? graphics::color(0,0,0,255) : graphics::color(v["color"]);
	border_size_ = v.has_key("border_size") ? v["border_size"].as_int() : 2;
	child_ = widget_factory::create(v["child"], e);
}

void border_widget::set_color(const graphics::color& col)
{
	color_ = col;
}

void border_widget::set_color(const SDL_Color& col)
{
	set_color(graphics::color(col.r, col.g, col.b, col.a));
}

void border_widget::handle_process()
{
	widget::handle_process();
	child_->process();
}

void border_widget::handle_draw() const
{
	glPushMatrix();
	graphics::draw_rect(rect(x(),y(),width(),height()), color_);
	glTranslatef(GLfloat(x()), GLfloat(y()), 0.0);
	child_->draw();
	glPopMatrix();
}

bool border_widget::handle_event(const SDL_Event& event, bool claimed)
{
	SDL_Event ev = event;
	normalize_event(&ev);
	return child_->process_event(ev, claimed);
}

const_widget_ptr border_widget::get_widget_by_id(const std::string& id) const
{
	widget_ptr wx = child_->get_widget_by_id(id);
	if(wx) {
		return wx;
	}
	return widget::get_widget_by_id(id);
}

widget_ptr border_widget::get_widget_by_id(const std::string& id)
{
	widget_ptr wx = child_->get_widget_by_id(id);
	if(wx) {
		return wx;
	}
	return widget::get_widget_by_id(id);
}

std::vector<widget_ptr> border_widget::get_children() const
{
	std::vector<widget_ptr> result;
	result.push_back(child_);
	return result;
}

}
