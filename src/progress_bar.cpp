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

#include <algorithm>

#include "Canvas.hpp"

#include "progress_bar.hpp"

namespace gui 
{
	ProgressBar::ProgressBar(int progress, int minv, int maxv, const std::string& gui_set)
		: progress_(progress), 
		min_(minv), 
		max_(maxv), 
		completion_called_(false),
		upscale_(false), 
		color_(128,128,128,255), 
		hpad_(10), 
		vpad_(10)
	{
		if(gui_set.empty() == false) {
			frame_image_set_ = FramedGuiElement::get(gui_set);
		}
	}

	ProgressBar::ProgressBar(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e), 
		completion_called_(false),
		progress_(v["progress"].as_int(0)), 
		min_(v["min"].as_int(0)), 
		max_(v["max"].as_int(100)),
		hpad_(10), vpad_(10)
	{
		if(v.has_key("on_completion")) {
			ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
			completion_handler_ = getEnvironment()->createFormula(v["on_completion"]);
			oncompletion_ = std::bind(&ProgressBar::complete, this);
		}

		std::string frame_set = v["frame_set"].as_string_default("");
		if(frame_set != "none" && frame_set.empty() == false) {
			frame_image_set_ = FramedGuiElement::get(frame_set);
		} 
		upscale_ = v["resolution"].as_string_default("normal") == "normal" ? false : true;
		if(v.has_key("color")) {
			color_ = KRE::Color(v["color"]);
		} else if(v.has_key("colour")) {
			color_ = KRE::Color(v["colour"]);
		} else {
			color_ = KRE::Color::colorGray();
		}
		if(v.has_key("padding")) {
			ASSERT_LOG(v["padding"].num_elements() == 2, "Padding field must be two element, found " << v["padding"].num_elements());
			hpad_ = v["padding"][0].as_int();
			vpad_ = v["padding"][1].as_int();
		}
	}

	void ProgressBar::complete()
	{
		if(getEnvironment()) {
			variant value = completion_handler_->execute(*getEnvironment());
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("ProgressBar::complete() called without environment!");
		}
	}

	void ProgressBar::setProgress(int value)
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

	void ProgressBar::updateProgress(int delta)
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

	void ProgressBar::setCompletionHandler(std::function<void ()> oncompletion)
	{
		oncompletion_ = oncompletion;
	}

	void ProgressBar::reset()
	{
		progress_ = min_;
		completion_called_ = false;
	}

	void ProgressBar::setMinValue(int min_val)
	{
		min_ = min_val;
	}

	void ProgressBar::setMaxValue(int max_val)
	{
		max_ = max_val;
		if(progress_ < max_) {
			completion_called_ = false;
		} else if(completion_called_ == false) {
			progress_ = max_;
			completion_called_ = true;
			if(oncompletion_) {
				oncompletion_();
			}
		}
	}

	WidgetPtr ProgressBar::clone() const
	{
		return WidgetPtr(new ProgressBar(*this));
	}

	BEGIN_DEFINE_CALLABLE(ProgressBar, Widget)
		DEFINE_FIELD(progress, "int")
			return variant(obj.progress_);
		DEFINE_SET_FIELD
			obj.setProgress(value.as_int());
		
		DEFINE_FIELD(min, "int")
			return variant(obj.min_);
		DEFINE_SET_FIELD
			obj.setMinValue(value.as_int());

		DEFINE_FIELD(max, "int")
			return variant(obj.max_);
		DEFINE_SET_FIELD
			obj.setMaxValue(value.as_int());

		DEFINE_FIELD(resolution, "string")
			return variant(obj.upscale_ ? "double" : "normal");
		DEFINE_SET_FIELD
			obj.upscale_ = value.as_string_default("normal") == "normal" ? false : true;

		//DEFINE_FIELD(color, "builtin Color")
		DEFINE_FIELD(padding, "[int,int]")
			std::vector<variant> v;
			v.emplace_back(obj.hpad_);
			v.emplace_back(obj.vpad_);
			return variant(&v);
		DEFINE_SET_FIELD
			obj.hpad_ = value[0].as_int();
			obj.vpad_ = value[1].as_int();

	END_DEFINE_CALLABLE(ProgressBar)

	void ProgressBar::handleDraw() const
	{
		if(frame_image_set_) {
			frame_image_set_->blit(x(),y(),width(),height(), upscale_);
		}
		const int w = int((width()-hpad_*2) * float(progress_-min_)/float(max_-min_));
		auto canvas = KRE::Canvas::getInstance();
		canvas->drawSolidRect(rect(x()+hpad_,y()+vpad_,w,height()-vpad_*2), color_);
	}
}
