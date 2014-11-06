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
#ifndef POLY_LINE_WIDGET_HPP_INCLUDED
#define POLY_LINE_WIDGET_HPP_INCLUDED

#include <vector>

#include "color_utils.hpp"
#include "foreach.hpp"
#include "graphics.hpp"
#include "geometry.hpp"
#include "random.hpp"
#include "raster.hpp"
#include "widget.hpp"

namespace gui {

class poly_line_widget : public widget
{
public:
	poly_line_widget(std::vector<point>* points, const SDL_Color& c, GLfloat width=1.0)
		: color_(c), width_(width)

	{
		points_.swap(*points);
		calc_coords();
	}
	poly_line_widget(const point& p1, const point& p2, const SDL_Color& c, GLfloat width=1.0)
		: color_(c), width_(width)
	{
		points_.push_back(p1);
		points_.push_back(p2);
		calc_coords();
	}
	poly_line_widget(const variant& v, game_logic::formula_callable* e)
		: widget(v,e)
	{
		width_ = v.has_key("width") ? v["width"].as_int() : 1.0f;
		color_ = v.has_key("color") 
			? graphics::color(v["color"]).as_sdl_color() 
			: graphics::color(255,255,255,255).as_sdl_color();
		if(v.has_key("points")) {
			foreach(const variant& pp, v["points"].as_list()) {
				points_.push_back(point(pp.as_list_int()[0], pp.as_list_int()[1]));
			}
			calc_coords();
		}
	}
	virtual ~poly_line_widget()
	{}
	void add_point(const point& p)
	{
		points_.push_back(p);
		calc_coords();
	}
protected:
	virtual bool handle_event(const SDL_Event& event, bool claimed)
	{
		return claimed;
	}
	virtual void handle_draw() const
	{
		std::vector<GLfloat>& varray = graphics::global_vertex_array();
		varray.clear();
		foreach(const point&p, points_) {
			varray.push_back(GLfloat(p.x));
			varray.push_back(GLfloat(p.y));
		}
#if defined(USE_SHADERS)
		glColor4ub(color_.r, color_.g, color_.b, 255);
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray.front());
		glDrawArrays(GL_LINE_STRIP, 0, varray.size()/2);
		glColor4ub(255, 255, 255, 255);
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glLineWidth(width_);
		glColor4ub(color_.r, color_.g, color_.b, 255);
		glVertexPointer(2, GL_FLOAT, 0, &varray.front());
		glDrawArrays(GL_LINE_STRIP, 0, varray.size()/2);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
		glColor4ub(255, 255, 255, 255);
#endif
	}
	virtual void set_value(const std::string& key, const variant& v)
	{
		if(key == "points") {
			points_.clear();
			foreach(const variant& pp, v.as_list()) {
				points_.push_back(point(pp.as_list_int()[0], pp.as_list_int()[1]));
			}
			calc_coords();
		} else if(key == "width") {
			width_ = GLfloat(v["width"].as_decimal().as_float());
		} else if(key == "color") {
			color_ = graphics::color(v).as_sdl_color();
		}
		widget::set_value(key, v);
	}
	virtual variant get_value(const std::string& key) const
	{
		if(key == "points") {
			std::vector<variant> v;
			foreach(const point&p, points_) {
				v.push_back(p.write());
			}
			return variant(&v);
		} else if(key == "width") {
			return variant(width_);
		} else if(key == "color") {
			return graphics::color(color_).write();
		}
		return widget::get_value(key);
	}
private:
	void calc_coords()
	{
		int min_x = INT_MAX, max_x = INT_MIN, min_y = INT_MAX, max_y = INT_MIN;
		foreach(const point& p, points_) {
			if(p.x < min_x) {
				min_x = p.x;
			}
			if(p.x > max_x) {
				max_x = p.x;
			}
			if(p.y < min_y) {
				min_y = p.y;
			}
			if(p.y > max_y) {
				max_y = p.y;
			}
		}
		set_loc(min_x, min_y);
		set_dim(max_x-min_x, max_y-min_y);
	}
	SDL_Color color_;
	GLfloat width_;
	std::vector<point> points_;
};

typedef boost::intrusive_ptr<poly_line_widget> poly_line_widget_ptr;
typedef boost::intrusive_ptr<const poly_line_widget> const_poly_line_widget_ptr;

}

#endif // POLY_LINE_WIDGET_HPP_INCLUDED
