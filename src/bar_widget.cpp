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
#include "asserts.hpp"
#include "bar_widget.hpp"
#include "raster.hpp"

namespace gui
{
	bar_widget::bar_widget(const variant& v, game_logic::formula_callable* e)
		: widget(v, e), segments_(v["segments"].as_int(1)), 
		segment_length_(v["segment_length"].as_int(5)), 
		rotate_(GLfloat(v["rotation"].as_decimal().as_float())),
		tick_width_(v["tick_width"].as_int(1)), scale_(2.0f),
		drained_segments_(v["drained"].as_int(0)), animating_(false),
		drain_rate_(v["drain_rate"].as_decimal(decimal(10.0)).as_float()),
		total_bar_length_(0), drained_bar_length_(0), active_bar_length_(0),
		left_cap_width_(0), right_cap_width_(0), 
		animation_end_point_unscaled_(0.0f),
		animation_current_position_(0.0f), drained_segments_after_anim_(0),
		bar_max_width_(v["max_width"].as_int())
	{
		if(v.has_key("bar_color")) {
			bar_color_ = graphics::color(v["bar_color"]).as_sdl_color();
		} else {
			bar_color_ = graphics::color("red").as_sdl_color();
		}
		if(v.has_key("tick_color")) {
			tick_mark_color_ = graphics::color(v["tick_color"]).as_sdl_color();
		} else {
			tick_mark_color_ = graphics::color("black").as_sdl_color();
		}
		if(v.has_key("drained_bar_color")) {
			drained_bar_color_ = graphics::color(v["drained_bar_color"]).as_sdl_color();
		} else {
			drained_bar_color_ = graphics::color("black").as_sdl_color();
		}
		if(v.has_key("drained_tick_color")) {
			drained_tick_mark_color_ = graphics::color(v["drained_tick_color"]).as_sdl_color();
		} else {
			drained_tick_mark_color_ = graphics::color("white").as_sdl_color();
		}

		if(v.has_key("scale")) {
			scale_ = GLfloat(v["scale"].as_decimal().as_float());
		}

		ASSERT_LOG(v.has_key("bar"), "Missing 'bar' attribute");
		init_bar_section(v["bar"], &bar_);
		ASSERT_LOG(v.has_key("left_cap"), "Missing 'left_cap' attribute");
		init_bar_section(v["left_cap"], &left_cap_);
		ASSERT_LOG(v.has_key("right_cap"), "Missing 'right_cap' attribute");
		init_bar_section(v["right_cap"], &right_cap_);

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

	bar_widget::~bar_widget()
	{
	}

	void bar_widget::init_bar_section(const variant&v, bar_section* b)
	{
		b->texture = graphics::texture::get(v["image"].as_string());
		if(v.has_key("area")) {
			ASSERT_LOG(v["area"].is_list() && v["area"].num_elements() == 4, "'area' attribute must be four element list.");
			b->area = rect(v["area"][0].as_int(), v["area"][1].as_int(), v["area"][2].as_int(), v["area"][3].as_int());
		} else {
			b->area = rect(0, 0, b->texture.width(), b->texture.height());
		}
	}

	void bar_widget::init()
	{
		left_cap_width_ = left_cap_.area.w() ? left_cap_.area.w()*scale_ : left_cap_.texture.width()*scale_;
		right_cap_width_ = right_cap_.area.w() ? right_cap_.area.w()*scale_ : right_cap_.texture.width()*scale_;

		total_bar_length_ = (segments_ * segment_length_ + (segments_-1) * tick_width_) * scale_;
		drained_bar_length_ = (drained_segments_ * segment_length_ + (drained_segments_-1) * tick_width_) * scale_;
		active_bar_length_ = ((segments_-drained_segments_) * segment_length_ + (segments_-(drained_segments_?drained_segments_:1)) * tick_width_) * scale_;
		int w = total_bar_length_ + left_cap_width_ + right_cap_width_;
		int h;
		if(bar_height_ == 0) {
			h = std::max(bar_.area.h(), std::max(left_cap_.area.h(), right_cap_.area.h()))*scale_;
		} else {
			h = bar_height_*scale_;
		}

		tick_distance_ = (segment_length_ + tick_width_) * scale_;

		if(bar_max_width_ != 0 && w > bar_max_width_) {
			double ratio = bar_max_width_ / double(w);
			left_cap_width_ = int(double(left_cap_width_) * ratio);
			right_cap_width_ = int(double(right_cap_width_) * ratio);
			total_bar_length_ = int(double(total_bar_length_) * ratio);
			drained_bar_length_ = int(double(drained_bar_length_) * ratio);
			active_bar_length_ = int(double(active_bar_length_) * ratio);
			tick_distance_ = int(double(tick_distance_) * ratio);
			w = bar_max_width_;
		}

		set_dim(w, h);
	}

	void bar_widget::set_rotation(GLfloat rotate)
	{
		rotate_ = rotate;
	}

BEGIN_DEFINE_CALLABLE(bar_widget, widget)
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
		obj.scale_ = value.as_decimal().as_float();
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
			obj.animation_end_point_unscaled_ = animation_end_position - animation_start_position;
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
		obj.animation_current_position_ = value.as_decimal().as_float();
END_DEFINE_CALLABLE(bar_widget)

	void bar_widget::handle_process()
	{
		if(animating_) {
			int end_point_unscaled = animation_end_point_unscaled_ * segment_length_;
			if(animation_end_point_unscaled_ > 0) {
				// gaining segments
				animation_current_position_ += (1.0 / drain_rate_) * segment_length_;
				if(animation_current_position_ >= end_point_unscaled) {
					animation_current_position_ = 0;
					drained_segments_ = drained_segments_after_anim_;
					init();
					animating_ = false;
				}
			} else {
				// loosing segments
				animation_current_position_ -= (1.0 / drain_rate_) * segment_length_;
				if(animation_current_position_ <= end_point_unscaled) {
					animation_current_position_ = 0;
					drained_segments_ = drained_segments_after_anim_;
					init();
					animating_ = false;
				}
			}
		}

		widget::handle_process();
	}

	void bar_widget::draw_ticks(GLfloat x_offset, int segments, const SDL_Color& color) const
	{
		// tick marks
		if(segments > 1) {
			std::vector<GLfloat>& varray = graphics::global_vertex_array();
			varray.clear();
			for(int n = 1; n < segments; ++n) {
				//GLfloat lx = x_offset + GLfloat((segment_length_ * n + (n - 1) * tick_width_ + 1) * scale_);
				GLfloat lx = x_offset + tick_distance_ * n;
				varray.push_back(lx);
				varray.push_back(GLfloat(y()));
				varray.push_back(lx);
				varray.push_back(GLfloat(y()+height()));
			}
			glLineWidth(GLfloat(tick_width_) * scale_);
			glColor4ub(color.r, color.g, color.b, 255);
#if defined(USE_SHADERS)
			gles2::manager gles2_manager(gles2::get_simple_shader());
			gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray.front());
			glDrawArrays(GL_LINES, 0, varray.size()/2);
#else
			glDisable(GL_TEXTURE_2D);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glVertexPointer(2, GL_FLOAT, 0, &varray.front());
			glDrawArrays(GL_LINES, 0, varray.size()/2);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnable(GL_TEXTURE_2D);
#endif
			glLineWidth(1.0f);
		}
	}

	void bar_widget::handle_draw() const
	{
		int x_offset = x();
		{
			color_save_context color_saver;

			// draw color under end caps.			
			graphics::draw_rect(rect(x()+scale_, y()+scale_, left_cap_width_-2*scale_, height()-2*scale_), graphics::color(bar_color_));
			graphics::draw_rect(rect(x()+left_cap_width_+total_bar_length_, y()+scale_, right_cap_width_-scale_, height()-2*scale_), graphics::color(drained_segments_ ? drained_bar_color_ : bar_color_));

			// background for active segments.
			int anim_offset = animation_current_position_*scale_;
			graphics::draw_rect(rect(x()+left_cap_width_, y(), active_bar_length_+anim_offset, height()), graphics::color(bar_color_));

			// background for drained segments.
			if(drained_segments_ || animating_) {
				graphics::draw_rect(rect(x()+active_bar_length_+left_cap_width_+anim_offset, y(), drained_bar_length_-anim_offset, height()), graphics::color(drained_bar_color_));
			}
			
			draw_ticks(x()+left_cap_width_, segments_-drained_segments_+(drained_segments_?1:0), tick_mark_color_);
			draw_ticks(x()+left_cap_width_+active_bar_length_, drained_segments_, drained_tick_mark_color_);
		}

		// left cap
		if(left_cap_.area.w() == 0) {
			graphics::blit_texture(left_cap_.texture, x_offset, y(), left_cap_width_, height(), rotate_);
		} else {
			graphics::blit_texture(left_cap_.texture, x_offset, y(), left_cap_width_, height(), rotate_,
				GLfloat(left_cap_.area.x())/left_cap_.texture.width(),
				GLfloat(left_cap_.area.y())/left_cap_.texture.height(),
				GLfloat(left_cap_.area.x2())/left_cap_.texture.width(),
				GLfloat(left_cap_.area.y2())/left_cap_.texture.height());
		}
		x_offset += left_cap_width_;
		// bar
		if(bar_.area.w() == 0) {
			graphics::blit_texture(bar_.texture, x_offset, y(), total_bar_length_, height(), rotate_);
		} else {
			graphics::blit_texture(bar_.texture, x_offset, y(), total_bar_length_, height(), rotate_,
				GLfloat(bar_.area.x())/bar_.texture.width(),
				GLfloat(bar_.area.y())/bar_.texture.height(),
				GLfloat(bar_.area.x2())/bar_.texture.width(),
				GLfloat(bar_.area.y2())/bar_.texture.height());
		}
		x_offset += total_bar_length_;

		// right cap
		if(right_cap_.area.w() == 0) {
			graphics::blit_texture(right_cap_.texture, x_offset, y(), right_cap_width_, height(), rotate_);
		} else {
			graphics::blit_texture(right_cap_.texture, x_offset, y(), right_cap_width_, height(), rotate_,
				GLfloat(right_cap_.area.x())/right_cap_.texture.width(),
				GLfloat(right_cap_.area.y())/right_cap_.texture.height(),
				GLfloat(right_cap_.area.x2())/right_cap_.texture.width(),
				GLfloat(right_cap_.area.y2())/right_cap_.texture.height());
		}
	}

	bool bar_widget::handle_event(const SDL_Event& event, bool claimed)
	{
		return claimed;
	}

}
