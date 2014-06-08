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
#include <algorithm>

#include "graphics.hpp"
#include "progress_bar.hpp"
#include "raster.hpp"

namespace gui {

progress_bar::progress_bar(int progress, int minv, int maxv, const std::string& gui_set)
	: progress_(progress), min_(minv), max_(maxv), completion_called_(false),
	upscale_(false), color_(128,128,128,255), hpad_(10), vpad_(10)
{
	if(gui_set.empty() == false) {
		frame_image_set_ = FramedGuiElement::get(gui_set);
	}
}

progress_bar::progress_bar(const variant& v, game_logic::FormulaCallable* e)
	: widget(v,e), completion_called_(false),
	progress_(v["progress"].as_int(0)), 
	min_(v["min"].as_int(0)), max_(v["max"].as_int(100)),
	hpad_(10), vpad_(10)
{
	if(v.has_key("on_completion")) {
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
		completion_handler_ = getEnvironment()->createFormula(v["on_completion"]);
		oncompletion_ = boost::bind(&progress_bar::complete, this);
	}

	std::string frame_set = v["frame_set"].as_string_default("");
	if(frame_set != "none" && frame_set.empty() == false) {
		frame_image_set_ = FramedGuiElement::get(frame_set);
	} 
	upscale_ = v["resolution"].as_string_default("normal") == "normal" ? false : true;
	if(v.has_key("color")) {
		color_ = graphics::color(v["color"]);
	} else if(v.has_key("colour")) {
		color_ = graphics::color(v["colour"]);
	} else {
		color_ = graphics::color("grey");
	}
	if(v.has_key("padding")) {
		ASSERT_LOG(v["padding"].num_elements() == 2, "Padding field must be two element, found " << v["padding"].num_elements())
		hpad_ = v["padding"][0].as_int();
		vpad_ = v["padding"][1].as_int();
	}
}

void progress_bar::complete()
{
	if(getEnvironment()) {
		variant value = completion_handler_->execute(*getEnvironment());
		getEnvironment()->createFormula(value);
	} else {
		std::cerr << "progress_bar::complete() called without environment!" << std::endl;
	}
}

void progress_bar::setProgress(int value)
{
	progress_ = std::min(max_, value);
	progress_ = std::max(min_, progress_);
	if(progress_ >= max_ && completion_called_ == false) {
		completion_called_ = true;
		if(oncompletion_) {
			oncompletion_();
		}
	}
}

void progress_bar::update_progress(int delta)
{
	progress_ = std::min(max_, progress_ + delta);
	progress_ = std::max(min_, progress_);
	if(progress_ >= max_ && completion_called_ == false) {
		completion_called_ = true;
		if(oncompletion_) {
			oncompletion_();
		}
	}
}

void progress_bar::set_completion_handler(boost::function<void ()> oncompletion)
{
	oncompletion_ = oncompletion;
}

void progress_bar::reset()
{
	progress_ = min_;
	completion_called_ = false;
}


variant progress_bar::getValue(const std::string& key) const
{
	if(key == "progress") {
		return variant(progress_);
	} else if(key == "min") {
		return variant(min_);
	} else if(key == "max") {
		return variant(max_);
	} 
	return variant();
}

void progress_bar::setValue(const std::string& key, const variant& value)
{
	if(key == "progress") {
		setProgress(value.as_int());
	} else if(key == "min") {
		min_ = value.as_int();
	} else if(key == "max") {
		max_ = value.as_int();
		if(progress_ < max_) {
			completion_called_ = false;
		} else if(completion_called_ == false) {
			progress_ = max_;
			completion_called_ = true;
			if(oncompletion_) {
				oncompletion_();
			}
		}
	} else if(key == "resolution") {
		upscale_ = value.as_string_default("normal") == "normal" ? false : true;
	} else if(key == "color" || key == "colour") {
		color_ = graphics::color(value);
	} else if(key == "padding") {
		ASSERT_LOG(value.num_elements() == 2, "Padding field must be two element, found " << value.num_elements())
		hpad_ = value[0].as_int();
		vpad_ = value[1].as_int();
	}
	widget::setValue(key, value);
}

void progress_bar::handleDraw() const
{
	if(frame_image_set_) {
		frame_image_set_->blit(x(),y(),width(),height(), upscale_);
	}
	const int w = int((width()-hpad_*2) * float(progress_-min_)/float(max_-min_));
	graphics::draw_rect(rect(x()+hpad_,y()+vpad_,w,height()-vpad_*2), color_);
}

}
