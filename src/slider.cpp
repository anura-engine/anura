/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>

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

#include "Canvas.hpp"

#include "gui_section.hpp"
#include "image_widget.hpp"
#include "joystick.hpp"
#include "logger.hpp"
#include "slider.hpp"
#include "widget_factory.hpp"

namespace gui
{
	Slider::Slider(int width, ChangeFn onchange, float position, int scale)
		: width_(width),
		  onchange_(onchange),
		  dragging_(false),
		  position_(position),
		  slider_left_(new GuiSectionWidget("slider_side_left", -1, -1, scale)),
		  slider_right_(new GuiSectionWidget("slider_side_right", -1, -1, scale)),
		  slider_middle_(new GuiSectionWidget("slider_middle", -1, -1, scale)),
		  slider_button_(new GuiSectionWidget("slider_button", -1, -1, scale))
	{
		setEnvironment();
		init();
		setDim(width_+slider_left_->width()*2, slider_button_->height());
	}

	Slider::Slider(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e),
		  dragging_(false)
	{
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
		onchange_ = std::bind(&Slider::changeDelegate, this, std::placeholders::_1);
		ffl_handler_ = getEnvironment()->createFormula(v["on_change"]);
		if(v.has_key("on_drag_end")) {
			ondragend_ = std::bind(&Slider::dragEndDelegate, this, std::placeholders::_1);
			ffl_end_handler_ = getEnvironment()->createFormula(v["on_drag_end"]);
		}

		position_ = v.has_key("position") ? v["position"].as_float() : 0.0f;

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

	void Slider::init() const
	{
		int slider_y = height()/2 - slider_middle_->height()/2;
		slider_left_->setLoc(0, slider_y);
		slider_middle_->setLoc(slider_left_->width(), slider_y);
		slider_middle_->setDim(width_, slider_middle_->height());
		slider_right_->setLoc(slider_left_->width()+width_, slider_y);
		slider_button_->setLoc(slider_left_->width()+static_cast<int>(position_*width_)-slider_button_->width()/2, 0);
	}

	bool Slider::inButton(int xloc, int yloc) const
	{
		xloc -= getPos().x;
		yloc -= getPos().y;

		int button_x = slider_left_->width() + int(position_*width_);
		return xloc > button_x-40 && xloc < button_x + slider_button_->width()+40 && yloc > -10 && yloc < height()+10;
	}

	bool Slider::inSlider(int xloc, int yloc) const
	{
		return xloc > x() && xloc < x() + width() &&
		yloc > y() && yloc < y() + height();
	}

	void Slider::handleDraw() const
	{
		init();
		if(hasFocus()) {
			KRE::Canvas::getInstance()->drawHollowRect(rect(x()-1, y()-1, width()+2, height()+2), KRE::Color(128,128,128,128));
		}
		slider_left_->draw(x(), y(), getRotation(), getScale());
		slider_middle_->draw(x(), y(), getRotation(), getScale());
		slider_right_->draw(x(), y(), getRotation(), getScale());
		slider_button_->draw(x(), y(), getRotation(), getScale());
	}

	void Slider::changeDelegate(float position)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable(getEnvironment()));
			callable->add("position", variant(position));
			variant value = ffl_handler_->execute(*getEnvironment());
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("slider::changeDelegate() called without environment!");
		}
	}

	void Slider::dragEndDelegate(float position)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable(getEnvironment()));
			callable->add("position", variant(position));
			variant value = ffl_end_handler_->execute(*getEnvironment());
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("slider::dragEndDelegate() called without environment!");
		}
	}

	void Slider::handleProcess()
	{
		Widget::handleProcess();

		if(hasFocus()) {
			static int control_lockout = 0;
			if(joystick::left() && !control_lockout) {
				control_lockout = 5;
				if(position() <= 1.0f/25.0f) {
					setPosition(0.0f);
				} else {
					setPosition(position() - 1.0f/25.0f);
				}
				if(onchange_) {
					onchange_(position());
				}
			}
			if(joystick::right() && !control_lockout) {
				control_lockout = 5;
				if(position() >= 1.0f-1.0f/25.0f) {
					setPosition(1.0f);
				} else {
					setPosition(position() + 1.0f/25.0f);
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

	bool Slider::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(claimed) {
			dragging_ = false;
		}

		if(event.type == SDL_MOUSEMOTION && dragging_) {
			const SDL_MouseMotionEvent& e = event.motion;
			int mouse_x = e.x - getPos().x;
			int mouse_y = e.y - getPos().y;

			int rel_x = mouse_x - slider_left_->width();
			if (rel_x < 0) rel_x = 0;
			if (rel_x > width_) rel_x = width_;
			float pos = static_cast<float>(rel_x)/width_;
			if (pos != position_)
			{
				position_ = pos;
				onchange_(pos);
			}

			return claimMouseEvents();
		} else if(event.type == SDL_MOUSEBUTTONDOWN) {
			const SDL_MouseButtonEvent& e = event.button;
			if(inButton(e.x,e.y)) {
				dragging_ = true;
				return claimMouseEvents();
			}
		} else if(event.type == SDL_MOUSEBUTTONUP && dragging_) {
			dragging_ = false;
			claimed = claimMouseEvents();
			if(ondragend_) {
				const SDL_MouseButtonEvent& e = event.button;
				int mouse_x = e.x - getPos().x;
				int mouse_y = e.y - getPos().y;

				int rel_x = mouse_x - slider_left_->width();
				if (rel_x < 0) rel_x = 0;
				if (rel_x > width_) rel_x = width_;
				float pos = static_cast<float>(rel_x)/width_;
				ondragend_(pos);
			}
		}

		if(event.type == SDL_KEYDOWN && hasFocus()) {
			if(event.key.keysym.sym == SDLK_LEFT) {
				if(position() <= 1.0f/20.0f) {
					setPosition(0.0f);
				} else {
					setPosition(position() - 1.0f/20.0f);
				}
				if(onchange_) {
					onchange_(position());
				}
				claimed = true;
			} else if(event.key.keysym.sym == SDLK_RIGHT) {
				if(position() >= 1.0f-1.0f/20.0f) {
					setPosition(1.0f);
				} else {
					setPosition(position() + 1.0f/20.0f);
				}
				if(onchange_) {
					onchange_(position());
				}
				claimed = true;
			}
		}

		return claimed;
	}

	WidgetPtr Slider::clone() const
	{
		Slider* s = new Slider(*this);
		if(slider_left_) {
			s->slider_left_ = slider_left_->clone();
		}
		if(slider_right_) {
			s->slider_right_ = slider_right_->clone();
		}
		if(slider_middle_) {
			s->slider_middle_ = slider_middle_->clone();
		}
		if(slider_button_) {
			s->slider_button_ = slider_button_->clone();
		}
		return WidgetPtr(s);
	}

	BEGIN_DEFINE_CALLABLE(Slider, Widget)
		DEFINE_FIELD(position, "decimal")
			return variant(obj.position());
		DEFINE_SET_FIELD
			obj.setPosition(value.as_float());
	END_DEFINE_CALLABLE(Slider)
}
