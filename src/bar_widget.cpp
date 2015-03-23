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

#include "asserts.hpp"
#include "bar_widget.hpp"

namespace gui
{
	BarWidget::BarWidget(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v, e), segments_(v["segments"].as_int(1)), 
		segment_length_(v["segment_length"].as_int(5)), 
		rotate_(v["rotation"].as_float(0)),
		tick_width_(v["tick_width"].as_int(1)), scale_(2.0f),
		drained_segments_(v["drained"].as_int(0)), animating_(false),
		drain_rate_(v["drain_rate"].as_double(10.0)),
		total_bar_length_(0), drained_bar_length_(0), active_bar_length_(0),
		left_cap_width_(0), right_cap_width_(0), 
		animation_end_point_unscaled_(0.0f),
		animation_current_position_(0.0f), drained_segments_after_anim_(0),
		bar_max_width_(v["max_width"].as_int())
	{
		if(v.has_key("bar_color")) {
			bar_color_ = KRE::Color(v["bar_color"]);
		} else {
			bar_color_ = KRE::Color("red");
		}
		if(v.has_key("tick_color")) {
			tick_mark_color_ = KRE::Color(v["tick_color"]);
		} else {
			tick_mark_color_ = KRE::Color("black");
		}
		if(v.has_key("drained_bar_color")) {
			drained_bar_color_ = KRE::Color(v["drained_bar_color"]);
		} else {
			drained_bar_color_ = KRE::Color("black");
		}
		if(v.has_key("drained_tick_color")) {
			drained_tick_mark_color_ = KRE::Color(v["drained_tick_color"]);
		} else {
			drained_tick_mark_color_ = KRE::Color("white");
		}

		if(v.has_key("scale")) {
			scale_ = v["scale"].as_float();
		}

		ASSERT_LOG(v.has_key("bar"), "Missing 'bar' attribute");
		initBarSection(v["bar"], &bar_);
		ASSERT_LOG(v.has_key("left_cap"), "Missing 'left_cap' attribute");
		initBarSection(v["left_cap"], &left_cap_);
		ASSERT_LOG(v.has_key("right_cap"), "Missing 'right_cap' attribute");
		initBarSection(v["right_cap"], &right_cap_);

		ASSERT_GT(segments_, 0);
		ASSERT_GT(segment_length_, 0);
		if(drained_segments_ > segments_) {
			drained_segments_ = segments_;
		}
		if(drained_segments_ < 0) {
			drained_segments_ = 0;
		}
		bar_height_ = height();
		init();
	}

	BarWidget::~BarWidget()
	{
	}

	void BarWidget::initBarSection(const variant&v, bar_section* b)
	{
		b->texture = KRE::Texture::createTexture(v["image"].as_string());
		if(v.has_key("area")) {
			ASSERT_LOG(v["area"].is_list() && v["area"].num_elements() == 4, "'area' attribute must be four element list.");
			b->area = rect(v["area"][0].as_int(), v["area"][1].as_int(), v["area"][2].as_int(), v["area"][3].as_int());
		} else {
			b->area = rect(0, 0, b->texture->width(), b->texture->height());
		}
	}

	void BarWidget::init()
	{
		left_cap_width_ = left_cap_.area.w() ? static_cast<int>(left_cap_.area.w()*scale_) : static_cast<int>(left_cap_.texture->width()*scale_);
		right_cap_width_ = right_cap_.area.w() ? static_cast<int>(right_cap_.area.w()*scale_) : static_cast<int>(right_cap_.texture->width()*scale_);

		total_bar_length_ = static_cast<int>(((segments_ * segment_length_ + (segments_-1) * tick_width_) * scale_));
		drained_bar_length_ = static_cast<int>((drained_segments_ * segment_length_ + (drained_segments_-1) * tick_width_) * scale_);
		active_bar_length_ = static_cast<int>(((segments_-drained_segments_) * segment_length_ + (segments_-(drained_segments_?drained_segments_:1)) * tick_width_) * scale_);
		int w = total_bar_length_ + left_cap_width_ + right_cap_width_;
		int h;
		if(bar_height_ == 0) {
			h = static_cast<int>(std::max(bar_.area.h(), std::max(left_cap_.area.h(), right_cap_.area.h()))*scale_);
		} else {
			h = static_cast<int>(bar_height_*scale_);
		}

		tick_distance_ = static_cast<int>((segment_length_ + tick_width_) * scale_);

		if(bar_max_width_ != 0 && w > bar_max_width_) {
			double ratio = bar_max_width_ / double(w);
			left_cap_width_ = static_cast<int>(double(left_cap_width_) * ratio);
			right_cap_width_ = static_cast<int>(double(right_cap_width_) * ratio);
			total_bar_length_ = static_cast<int>(double(total_bar_length_) * ratio);
			drained_bar_length_ = static_cast<int>(double(drained_bar_length_) * ratio);
			active_bar_length_ = static_cast<int>(double(active_bar_length_) * ratio);
			tick_distance_ = static_cast<int>(double(tick_distance_) * ratio);
			w = bar_max_width_;
		}

		setDim(w, h);
	}

	void BarWidget::setRotation(float rotate)
	{
		rotate_ = rotate;
	}

BEGIN_DEFINE_CALLABLE(BarWidget, Widget)
	DEFINE_FIELD(segments, "int")
		return variant(obj.segments_);
	DEFINE_SET_FIELD
		obj.segments_ = value.as_int();
		obj.init();
	DEFINE_FIELD(segment_length, "int")
		return variant(obj.segment_length_);
	DEFINE_SET_FIELD
		obj.segment_length_ = value.as_int();
		obj.init();
	DEFINE_FIELD(tick_width, "int")
		return variant(obj.tick_width_);
	DEFINE_SET_FIELD
		obj.tick_width_ = value.as_int();
		obj.init();
	DEFINE_FIELD(scale, "decimal")
		return variant(decimal(obj.scale_));
	DEFINE_SET_FIELD
		obj.scale_ = value.as_float();
		ASSERT_GT(obj.scale_, 0.0f);
		obj.init();
	DEFINE_FIELD(drained, "int")
		return variant(obj.drained_segments_);
	DEFINE_SET_FIELD
		int drain = value.as_int();
		if(drain != obj.drained_segments_) {
			int animation_start_position = obj.segments_-obj.drained_segments_;
			obj.animation_current_position_ = 0;
			obj.drained_segments_after_anim_ = drain;
			if(obj.drained_segments_after_anim_ < 0) {
				obj.drained_segments_after_anim_ = 0;
			}
			if(obj.drained_segments_after_anim_ > obj.segments_) {
				obj.drained_segments_after_anim_ = obj.segments_;
			}
			int animation_end_position = obj.segments_-obj.drained_segments_after_anim_;
			obj.animation_end_point_unscaled_ = static_cast<float>(animation_end_position - animation_start_position);
			obj.animating_ = true;
			obj.init();
		}
	DEFINE_FIELD(drain_rate, "int")
		return variant(obj.drain_rate_);
	DEFINE_SET_FIELD
		obj.drain_rate_ = value.as_int();
	DEFINE_FIELD(max_width, "int")
		return variant(obj.bar_max_width_);
	DEFINE_SET_FIELD
		obj.bar_max_width_ = value.as_int();
		obj.init();
	DEFINE_FIELD(animation_position, "decimal")
		return variant(decimal(0.0));
	DEFINE_SET_FIELD
		obj.animation_current_position_ = value.as_float();
END_DEFINE_CALLABLE(BarWidget)

	void BarWidget::handleProcess()
	{
		if(animating_) {
			int end_point_unscaled = static_cast<int>(animation_end_point_unscaled_ * segment_length_);
			if(animation_end_point_unscaled_ > 0) {
				// gaining segments
				animation_current_position_ += static_cast<float>((1.0 / drain_rate_) * segment_length_);
				if(animation_current_position_ >= end_point_unscaled) {
					animation_current_position_ = 0;
					drained_segments_ = drained_segments_after_anim_;
					init();
					animating_ = false;
				}
			} else {
				// loosing segments
				animation_current_position_ -= static_cast<float>((1.0 / drain_rate_) * segment_length_);
				if(animation_current_position_ <= end_point_unscaled) {
					animation_current_position_ = 0;
					drained_segments_ = drained_segments_after_anim_;
					init();
					animating_ = false;
				}
			}
		}

		Widget::handleProcess();
	}

	void BarWidget::drawTicks(float x_offset, int segments, const KRE::Color& color) const
	{
		// tick marks
		if(segments > 1) {
			std::vector<glm::vec2> varray;
			varray.reserve(segments-1);
			for(int n = 1; n < segments; ++n) {
				const float lx = static_cast<float>(x_offset + tick_distance_ * n);
				varray.emplace_back(lx, static_cast<float>(y()));
				varray.emplace_back(lx, static_cast<float>(y()+height()));
			}
			KRE::Canvas::getInstance()->drawLines(varray, static_cast<float>(tick_width_)*scale_, color);
		}
	}

	void BarWidget::handleDraw() const
	{
		auto canvas = KRE::Canvas::getInstance();
		int x_offset = x();
		{
			// draw color under end caps.			
			canvas->drawSolidRect(rect(static_cast<int>(x()+scale_), static_cast<int>(y()+scale_), static_cast<int>(left_cap_width_-2*scale_), static_cast<int>(height()-2*scale_)), bar_color_);
			canvas->drawSolidRect(rect(x()+left_cap_width_+total_bar_length_, static_cast<int>(y()+scale_), static_cast<int>(right_cap_width_-scale_), static_cast<int>(height()-2*scale_)), drained_segments_ ? drained_bar_color_ : bar_color_);

			// background for active segments.
			int anim_offset = static_cast<int>(animation_current_position_*scale_);
			canvas->drawSolidRect(rect(x()+left_cap_width_, y(), active_bar_length_+anim_offset, height()), bar_color_);

			// background for drained segments.
			if(drained_segments_ || animating_) {
				canvas->drawSolidRect(rect(x()+active_bar_length_+left_cap_width_+anim_offset, y(), drained_bar_length_-anim_offset, height()), drained_bar_color_);
			}
			
			drawTicks(static_cast<float>(x()+left_cap_width_), segments_-drained_segments_+(drained_segments_?1:0), tick_mark_color_);
			drawTicks(static_cast<float>(x()+left_cap_width_+active_bar_length_), drained_segments_, drained_tick_mark_color_);
		}

		// left cap
		if(left_cap_.area.w() == 0) {
			canvas->blitTexture(left_cap_.texture, rotate_, rect(x_offset, y(), left_cap_width_, height()));
		} else {
			canvas->blitTexture(left_cap_.texture, left_cap_.area, rotate_, rect(x_offset, y(), left_cap_width_, height()));
		}
		x_offset += left_cap_width_;
		// bar
		if(bar_.area.w() == 0) {
			canvas->blitTexture(bar_.texture, rotate_, rect(x_offset, y(), total_bar_length_, height()));
		} else {
			canvas->blitTexture(bar_.texture, bar_.area, rotate_, rect(x_offset, y(), total_bar_length_, height()));
		}
		x_offset += total_bar_length_;

		// right cap
		if(right_cap_.area.w() == 0) {
			canvas->blitTexture(right_cap_.texture, rotate_, rect(x_offset, y(), right_cap_width_, height()));
		} else {
			canvas->blitTexture(right_cap_.texture, right_cap_.area, rotate_, rect(x_offset, y(), right_cap_width_, height()));
		}
	}

	bool BarWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		return claimed;
	}

}
