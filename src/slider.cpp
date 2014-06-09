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
#include <boost/bind.hpp>

#include "color_chart.hpp"
#include "slider.hpp"
#include "image_widget.hpp"
#include "iphone_controls.hpp"
#include "joystick.hpp"
#include "raster.hpp"
#include "surface_cache.hpp"
#include "gui_section.hpp"
#include "widget_factory.hpp"

namespace gui {
	
slider::slider(int width, boost::function<void (double)> onchange, double position, int scale)
	: width_(width), onchange_(onchange), dragging_(false), position_(position),
	slider_left_(new GuiSectionWidget("slider_side_left", -1, -1, scale)),
	slider_right_(new GuiSectionWidget("slider_side_right", -1, -1, scale)),
	slider_middle_(new GuiSectionWidget("slider_middle", -1, -1, scale)),
	slider_button_(new GuiSectionWidget("slider_button", -1, -1, scale))
{
	setEnvironment();
	init();
	setDim(width_+slider_left_->width()*2, slider_button_->height());
}

slider::slider(const variant& v, game_logic::FormulaCallable* e)
	: widget(v,e), dragging_(false)
{
	ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
	onchange_ = boost::bind(&slider::change_delegate, this, _1);
	ffl_handler_ = getEnvironment()->createFormula(v["on_change"]);
	if(v.has_key("on_drag_end")) {
		ondragend_ = boost::bind(&slider::dragend_delegate, this, _1);
		ffl_end_handler_ = getEnvironment()->createFormula(v["on_drag_end"]);
	}

	position_ = v.has_key("position") ? v["position"].as_decimal().as_float() : 0.0;
	
	slider_left_ = v.has_key("slider_left") 
		? widget_factory::create(v["slider_left"], e) 
		: new GuiSectionWidget("slider_side_left", -1, -1, 2);
	slider_right_ = v.has_key("slider_right") 
		? widget_factory::create(v["slider_right"], e) 
		: new GuiSectionWidget("slider_side_right", -1, -1, 2);
	slider_middle_ = v.has_key("slider_middle") 
		? widget_factory::create(v["slider_middle"], e) 
		: new GuiSectionWidget("slider_middle", -1, -1, 2);
	slider_button_ = v.has_key("slider_button") 
		? widget_factory::create(v["slider_button"], e) 
		: new GuiSectionWidget("slider_button", -1, -1, 2);

	init();
	setDim(width_+slider_left_->width()*2, slider_button_->height());
}
	
void slider::init() const
{
	int slider_y = y() + height()/2 - slider_middle_->height()/2;
	slider_left_->setLoc(x(), slider_y);
	slider_middle_->setLoc(x()+slider_left_->width(), slider_y);
	slider_middle_->setDim(width_, slider_middle_->height());
	slider_right_->setLoc(x()+slider_left_->width()+width_, slider_y);
	slider_button_->setLoc(x()+slider_left_->width()+position_*width_-slider_button_->width()/2, y());
}

bool slider::in_button(int xloc, int yloc) const
{
	int button_x = x() + slider_left_->width() + int(position_*width_);
	return xloc > button_x-40 && xloc < button_x + slider_button_->width()+40 &&
	yloc > y()-10 && yloc < y() + height()+10;
}
	
bool slider::in_slider(int xloc, int yloc) const
{
	return xloc > x() && xloc < x() + width() &&
	yloc > y() && yloc < y() + height();
}
	
void slider::handleDraw() const
{
	init();
	if(hasFocus()) {
		graphics::draw_hollow_rect(rect(x()-1, y()-1, width()+2, height()+2), graphics::color(128,128,128,128));
	}
	//int slider_y = y() + height()/2 - slider_middle_->height()/2;
	//slider_left_->blit(x(), slider_y);
	//slider_middle_->blit(x()+slider_left_->width(), slider_y, width_, slider_middle_->height());
	//slider_right_->blit(x()+slider_left_->width()+width_, slider_y);
	//slider_button_->blit(x()+slider_left_->width()+position_*width_-slider_button_->width()/2, y());
	slider_left_->draw();
	slider_middle_->draw();
	slider_right_->draw();
	slider_button_->draw();
}

void slider::change_delegate(double position)
{
	using namespace game_logic;
	if(getEnvironment()) {
		map_FormulaCallablePtr callable(new game_logic::map_FormulaCallable(getEnvironment()));
		callable->add("position", variant(position));
		variant value = ffl_handler_->execute(*getEnvironment());
		getEnvironment()->createFormula(value);
	} else {
		std::cerr << "slider::change_delegate() called without environment!" << std::endl;
	}
}

void slider::dragend_delegate(double position)
{
	using namespace game_logic;
	if(getEnvironment()) {
		map_FormulaCallablePtr callable(new game_logic::map_FormulaCallable(getEnvironment()));
		callable->add("position", variant(position));
		variant value = ffl_end_handler_->execute(*getEnvironment());
		getEnvironment()->createFormula(value);
	} else {
		std::cerr << "slider::dragend_delegate() called without environment!" << std::endl;
	}
}

void slider::handleProcess()
{
	widget::handleProcess();

	if(hasFocus()) {
		static int control_lockout = 0;
		if(joystick::left() && !control_lockout) {
			control_lockout = 5;
			if(position() <= 1.0/25.0) {
				set_position(0.0);
			} else {
				set_position(position() - 1.0/25.0);
			}
			if(onchange_) {
				onchange_(position());
			}
		}
		if(joystick::right() && !control_lockout) {
			control_lockout = 5;
			if(position() >= 1.0-1.0/25.0) {
				set_position(1.0);
			} else {
				set_position(position() + 1.0/25.0);
			}
			if(onchange_) {
				onchange_(position());
			}
		}
		if(control_lockout) {
			--control_lockout;
		}
	}
}

bool slider::handleEvent(const SDL_Event& event, bool claimed)
{
	if(claimed) {
		dragging_ = false;
	}
		
	if(event.type == SDL_MOUSEMOTION && dragging_) {
		const SDL_MouseMotionEvent& e = event.motion;
		int mouse_x = e.x;
		int mouse_y = e.y;

		int rel_x = mouse_x - x() - slider_left_->width();
		if (rel_x < 0) rel_x = 0;
		if (rel_x > width_) rel_x = width_;
		float pos = (float)rel_x/width_;
		if (pos != position_)
		{
			position_ = pos;
			onchange_(pos);
		}

		return claimMouseEvents();
	} else if(event.type == SDL_MOUSEBUTTONDOWN) {
		const SDL_MouseButtonEvent& e = event.button;
		if(in_button(e.x,e.y)) {
			dragging_ = true;
			return claimMouseEvents();
		}
	} else if(event.type == SDL_MOUSEBUTTONUP && dragging_) {
		dragging_ = false;
		claimed = claimMouseEvents();
		if(ondragend_) {
			const SDL_MouseButtonEvent& e = event.button;
			int mouse_x = e.x;
			int mouse_y = e.y;

			int rel_x = mouse_x - x() - slider_left_->width();
			if (rel_x < 0) rel_x = 0;
			if (rel_x > width_) rel_x = width_;
			float pos = (float)rel_x/width_;
			ondragend_(pos);
		}
	}

	if(event.type == SDL_KEYDOWN && hasFocus()) {
		if(event.key.keysym.sym == SDLK_LEFT) {
			if(position() <= 1.0/20.0) {
				set_position(0.0);
			} else {
				set_position(position() - 1.0/20.0);
			}
			if(onchange_) {
				onchange_(position());
			}
			claimed = true;
		} else if(event.key.keysym.sym == SDLK_RIGHT) {
			if(position() >= 1.0-1.0/20.0) {
				set_position(1.0);
			} else {
				set_position(position() + 1.0/20.0);
			}
			if(onchange_) {
				onchange_(position());
			}
			claimed = true;
		}
	}

	return claimed;
}

void slider::setValue(const std::string& key, const variant& v)
{
	if(key == "position") {
		position_  = v.as_decimal().as_float();
	}
	widget::setValue(key, v);
}

variant slider::getValue(const std::string& key) const
{
	if(key == "position") {
		return variant(position_);
	}
	return widget::getValue(key);
}
	
}
