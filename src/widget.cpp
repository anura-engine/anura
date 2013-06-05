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

#include "asserts.hpp"
#include "foreach.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "tooltip.hpp"
#include "i18n.hpp"
#include "widget.hpp"
#include "iphone_controls.hpp"

#include <iostream>

namespace gui {

widget::widget() 
	: x_(0), y_(0), w_(0), h_(0), align_h_(HALIGN_LEFT), align_v_(VALIGN_TOP),
	true_x_(0), true_y_(0), disabled_(false), disabled_opacity_(127),
	tooltip_displayed_(false), visible_(true), zorder_(0), environ_(0),
	tooltip_display_delay_(0), tooltip_ticks_(INT_MAX), resolution_(0),
	display_alpha_(256), pad_h_(0), pad_w_(0), claim_mouse_events_(true),
	draw_with_object_shader_(true)
	{}

widget::widget(const variant& v, game_logic::formula_callable* e) 
	: environ_(e), w_(0), h_(0), x_(0), y_(0), zorder_(0), 
	true_x_(0), true_y_(0), disabled_(false), disabled_opacity_(v["disabled_opacity"].as_int(127)),
	tooltip_displayed_(false), id_(v["id"].as_string_default()), align_h_(HALIGN_LEFT), align_v_(VALIGN_TOP),
	tooltip_display_delay_(v["tooltip_delay"].as_int(500)), tooltip_ticks_(INT_MAX),
	resolution_(v["frame_size"].as_int(0)), display_alpha_(v["alpha"].as_int(256)),
	pad_w_(0), pad_h_(0), claim_mouse_events_(v["claim_mouse_events"].as_bool(true)),
	draw_with_object_shader_(v["draw_with_object_shader"].as_bool(true))
{
	set_alpha(display_alpha_ < 0 ? 0 : (display_alpha_ > 256 ? 256 : display_alpha_));
	if(v.has_key("width")) {
		w_ = v["width"].as_int();
	} 
	if(v.has_key("height")) {
		h_ = v["height"].as_int();
	} 
	if(v.has_key("wh")) {
		std::vector<int> iv = v["wh"].as_list_int();
		ASSERT_LOG(iv.size() == 2, "WH attribute must be 2 integer elements.");
		w_ = iv[0];
		h_ = iv[1];
	}
	if(v.has_key("rect")) {
		std::vector<int> r = v["rect"].as_list_int();
		ASSERT_LOG(r.size() == 4, "Four values must be supplied to the rect attribute");
		set_loc(r[0], r[1]);
		set_dim(r[2], r[3]);
	} 
	if(v.has_key("draw_area")) {
		std::vector<int> r = v["draw_area"].as_list_int();
		ASSERT_LOG(r.size() == 4, "Four values must be supplied to the rect attribute");
		set_loc(r[0], r[1]);
		set_dim(r[2], r[3]);
	} 
	if(v.has_key("x")) {
		true_x_ = x_ = v["x"].as_int();
	} 
	if(v.has_key("y")) {
		true_y_ = y_ = v["y"].as_int();
	}
	if(v.has_key("xy")) {
		std::vector<int> iv = v["xy"].as_list_int();
		ASSERT_LOG(iv.size() == 2, "XY attribute must be 2 integer elements.");
		true_x_ = x_ = iv[0];
		true_y_ = y_ = iv[1];
	}
	zorder_ = v["zorder"].as_int(0);
	if(v.has_key("on_process")) {
		on_process_ = boost::bind(&widget::process_delegate, this);
		ffl_on_process_ = get_environment()->create_formula(v["on_process"]);
	}
	if(v.has_key("tooltip")) {
		if(v["tooltip"].is_string()) {
			SDL_Color color = v.has_key("tooltip_color") ? graphics::color(v["tooltip_color"]).as_sdl_color() : graphics::color_yellow();
			set_tooltip(v["tooltip"].as_string(), v["tooltip_size"].as_int(18), color, v["tooltip_font"].as_string_default());
		} else if(v["tooltip"].is_map()) {
			SDL_Color color = v["tooltip"].has_key("color") ? graphics::color(v["tooltip"]["color"]).as_sdl_color() : graphics::color_yellow();
			set_tooltip(v["tooltip"]["text"].as_string(), v["tooltip"]["size"].as_int(18), color, v["tooltip"]["font"].as_string_default());
		} else {
			ASSERT_LOG(false, "Specify the tooltip as a string, e.g. \"tooltip\":\"Text to display on mouseover\", "
				"or a map, e.g. \"tooltip\":{\"text\":\"Text to display.\", \"size\":14}");
		}
	}
	visible_ = v["visible"].as_bool(true);
	if(v.has_key("align_h")) {
		std::string align = v["align_h"].as_string();
		if(align == "left") {
			align_h_ = HALIGN_LEFT;
		} else if(align == "middle" || align == "center" || align == "centre") {
			align_h_ = HALIGN_CENTER;
		} else if(align == "right") {
			align_h_ = HALIGN_RIGHT;
		} else {
			ASSERT_LOG(false, "Invalid align_h attribute given: " << align);
		}
	}
	if(v.has_key("align_v")) {
		std::string align = v["align_v"].as_string();
		if(align == "top") {
			align_v_ = VALIGN_TOP;
		} else if(align == "middle" || align == "center" || align == "centre") {
			align_v_ = VALIGN_CENTER;
		} else if(align == "bottom") {
			align_v_ = VALIGN_BOTTOM;
		} else {
			ASSERT_LOG(false, "Invalid align_v attribute given: " << align);
		}
	}
	disabled_ = !v["enabled"].as_bool(true);
	if(v.has_key("frame")) {
		set_frame_set(v["frame"].as_string());
	}
	if(v.has_key("frame_padding")) {
		ASSERT_LOG(v["frame_padding"].is_list() && v["frame_padding"].num_elements() == 2, "'pad' must be two element list");
		set_padding(v["frame_padding"][0].as_int(), v["frame_padding"][1].as_int());
	} 
	if(v.has_key("frame_pad_width")) {
		set_padding(v["frame_pad_width"].as_int(), get_pad_height());
	}
	if(v.has_key("frame_pad_height")) {
		set_padding(get_pad_width(), v["frame_pad_height"].as_int());
	}
	recalc_loc();
}

widget::~widget()
{
	if(tooltip_displayed_) {
		gui::remove_tooltip(tooltip_);
	}
}

void widget::recalc_loc()
{
	if( align_h_ == HALIGN_LEFT) {
		x_ = true_x_;
	} else if(align_h_ == HALIGN_CENTER) {
		x_ = true_x_ - w_/2;
	} else {
		x_ = true_x_ - w_;
	}

	if( align_v_ == VALIGN_TOP) {
		y_ = true_y_;
	} else if(align_v_ == VALIGN_CENTER) {
		y_ = true_y_ - h_/2;
	} else {
		y_ = true_y_ - h_;
	}
}

void widget::process_delegate()
{
	if(get_environment()) {
		variant value = ffl_on_process_->execute(*get_environment());
		get_environment()->execute_command(value);
	} else {
		std::cerr << "widget::process_delegate() called without environment!" << std::endl;
	}
}

void widget::handle_process()
{
	if(!tooltip_displayed_ && SDL_GetTicks() > tooltip_ticks_ && tooltip_ != NULL) {
		gui::set_tooltip(tooltip_);
		tooltip_displayed_ = true;
	}

	if(on_process_) {
		on_process_();
	}
}

void widget::process() {
	handle_process();
}

void widget::normalize_event(SDL_Event* event, bool translate_coords)
{
	int tx, ty; //temp x, y
	switch(event->type) {
	case SDL_MOUSEMOTION:
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
		event->motion.x = (event->motion.x*graphics::screen_width())/preferences::virtual_screen_width();
		event->motion.y = (event->motion.y*graphics::screen_height())/preferences::virtual_screen_height();
#else
		event->motion.x = (event->motion.x*preferences::virtual_screen_width())/preferences::actual_screen_width();
		event->motion.y = (event->motion.y*preferences::virtual_screen_height())/preferences::actual_screen_height();
#endif
		tx = event->motion.x; ty = event->motion.y;
		translate_mouse_coords(&tx, &ty);
		event->motion.x = tx-x();
		event->motion.y = ty-y();
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
		event->button.x = (event->button.x*graphics::screen_width())/preferences::virtual_screen_width();
		event->button.y = (event->button.y*graphics::screen_height())/preferences::virtual_screen_height();
#else
		event->button.x = (event->button.x*preferences::virtual_screen_width())/preferences::actual_screen_width();
		event->button.y = (event->button.y*preferences::virtual_screen_height())/preferences::actual_screen_height();
#endif
		tx = event->button.x; ty = event->button.y;
		translate_mouse_coords(&tx, &ty);
		event->button.x = tx-x();
		event->button.y = ty-y();
		break;
	default:
		break;
	}
}

void widget::set_tooltip(const std::string& str, int fontsize, const SDL_Color& color, const std::string& font)
{
	if(tooltip_displayed_ && tooltip_ != NULL) {
		if(tooltip_->text == str) {
			return;
		}
		gui::remove_tooltip(tooltip_);
		tooltip_displayed_ = false;
	}
	tooltip_.reset(new gui::tooltip_item(std::string(i18n::tr(str)), fontsize, color, font));
}

bool widget::process_event(const SDL_Event& event, bool claimed)
{
	if(disabled_) {
		tooltip_ticks_ = INT_MAX;
		return claimed;
	}
	if(!claimed) {
		if(tooltip_ && event.type == SDL_MOUSEMOTION) {
			if(event.motion.x >= x() && event.motion.x <= x()+width() &&
				event.motion.y >= y() && event.motion.y <= y()+height()) {
				if(!tooltip_displayed_) {
					if(tooltip_display_delay_ == 0 || SDL_GetTicks() > tooltip_ticks_) {
						gui::set_tooltip(tooltip_);
						tooltip_displayed_ = true;
					} else if(tooltip_ticks_ == INT_MAX) {
						tooltip_ticks_ = SDL_GetTicks() + tooltip_display_delay_;
					}
				}
			} else {
				tooltip_ticks_ = INT_MAX;
				if(tooltip_displayed_) {
					gui::remove_tooltip(tooltip_);
					tooltip_displayed_ = false;
				}
			}
		}
	} else {
		tooltip_ticks_ = INT_MAX;
	}

	return handle_event(event, claimed);
}

void widget::draw() const
{
	if(visible_) {
		color_save_context color_saver;

		GLint src = 0;
		GLint dst = 0;
#if !defined(USE_GLES2)
			glGetIntegerv(GL_BLEND_SRC, &src);
			glGetIntegerv(GL_BLEND_DST, &dst);
#endif
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		if(disabled_) {
			glColor4ub(255, 255, 255, disabled_opacity_);
		} else if(display_alpha_ < 256) {
			glColor4ub(255, 255, 255, display_alpha_);
		}
		if(frame_set_ != NULL) {
			frame_set_->blit(x() - get_pad_width() - frame_set_->corner_height(),
				y() - get_pad_height() - frame_set_->corner_height(), 
				width() + get_pad_width()*2 + 2*frame_set_->corner_height(), 
				height() + get_pad_height()*2 + 2*frame_set_->corner_height(), resolution_ != 0);
		}
		handle_draw();
#if !defined(USE_GLES2)
		glBlendFunc(src, dst);
#endif
	}
}

int widget::x() const
{
	return x_;
}

int widget::y() const
{
	return y_;
}

int widget::width() const
{
	return w_;
}

int widget::height() const
{
	return h_;
}

const_widget_ptr widget::get_widget_by_id(const std::string& id) const
{
	if(id_ == id) {
		return const_widget_ptr(this);
	}
	return widget_ptr();
}

widget_ptr widget::get_widget_by_id(const std::string& id)
{
	if(id_ == id) {
		return widget_ptr(this);
	}
	return widget_ptr();
}

BEGIN_DEFINE_CALLABLE(widget, 0)

DEFINE_FIELD(0, draw_area, "[int]")
	std::vector<variant> v;
	v.push_back(variant(x_));
	v.push_back(variant(y_));
	v.push_back(variant(w_));
	v.push_back(variant(h_));
	value = variant(&v);
DEFINE_SET_FIELD
	std::vector<int> r = value.as_list_int();
	ASSERT_LOG(r.size() == 4, "Four values must be supplied to the draw_area attribute");
	set_loc(r[0], r[1]);
	set_dim(r[2], r[3]);

DEFINE_FIELD(1, rect, "[int]")
	std::vector<variant> v;
	v.push_back(variant(x_));
	v.push_back(variant(y_));
	v.push_back(variant(w_));
	v.push_back(variant(h_));
	value = variant(&v);
DEFINE_SET_FIELD
	std::vector<int> r = value.as_list_int();
	ASSERT_LOG(r.size() == 4, "Four values must be supplied to the rect attribute");
	set_loc(r[0], r[1]);
	set_dim(r[2], r[3]);
DEFINE_FIELD(2, tooltip, "string|null")
	if(tooltip_) {
		value = variant(tooltip_->text);
	} else {
		value = variant();
	}

DEFINE_FIELD(3, visible, "bool")
	value = variant::from_bool(visible_);
DEFINE_SET_FIELD
	visible_ = value.as_bool();

DEFINE_FIELD(4, id, "string")
	value = variant(id_);

DEFINE_FIELD(5, resolution, "int")
	value = variant(resolution_);

DEFINE_FIELD(6, x, "int")
	value = variant(x());
DEFINE_SET_FIELD
	set_loc(value.as_int(), y());

DEFINE_FIELD(7, y, "int")
	value = variant(y());

DEFINE_SET_FIELD
	set_loc(x(), value.as_int());

DEFINE_FIELD(8, w, "int")
	value = variant(width());
DEFINE_SET_FIELD
	w_ = value.as_int();

DEFINE_FIELD(9, width, "int")
	value = variant(width());
DEFINE_SET_FIELD
	w_ = value.as_int();

DEFINE_FIELD(10, h, "int")
	value = variant(height());
DEFINE_SET_FIELD
	h_ = value.as_int();

DEFINE_FIELD(11, height, "int")
	value = variant(height());
DEFINE_SET_FIELD
	h_ = value.as_int();

DEFINE_FIELD(12, frame_set_name, "string")
	value = variant(frame_set_name_);

DEFINE_FIELD(13, alpha, "int")
	value = variant(get_alpha());
DEFINE_SET_FIELD
	int a = value.as_int();
	set_alpha(a < 0 ? 0 : (a > 256 ? 256 : a));

DEFINE_FIELD(14, frame_pad_width, "int")
	value = variant(get_pad_width());

DEFINE_FIELD(15, frame_pad_height, "int")
	value = variant(get_pad_height());

DEFINE_FIELD(16, frame_padding, "[int]")
	std::vector<variant> v;
	v.push_back(variant(get_pad_width()));
	v.push_back(variant(get_pad_height()));
	value = variant(&v);

DEFINE_FIELD(17, children, "[builtin widget]")
	std::vector<variant> v;
	std::vector<widget_ptr> w = get_children();
	foreach(widget_ptr item, w) {
		v.push_back(variant(item.get()));
	}

	value = variant(&v);

DEFINE_FIELD(18, disabled, "bool")
	value = variant::from_bool(disabled_);
DEFINE_SET_FIELD
	disabled_ = value.as_bool();

DEFINE_FIELD(19, disabled_opacity, "int")
	value = variant(static_cast<int>(disabled_opacity_));
DEFINE_SET_FIELD
	disabled_opacity_ = value.as_int();

DEFINE_FIELD(20, reserved__, "null")
	assert(false);
	value = variant();

END_DEFINE_CALLABLE_NOBASE(widget)

bool widget::in_widget(int xloc, int yloc) const
{
	if(xloc > 32767) {xloc -= 65536;}
	if(yloc > 32767) {yloc -= 65536;}
	return xloc > x() && xloc < x() + width() &&
			yloc > y() && yloc < y() + height();
}

bool widget_sort_zorder::operator()(const widget_ptr lhs, const widget_ptr rhs) const
{
	return lhs->zorder() < rhs->zorder() || lhs->zorder() == rhs->zorder() && lhs.get() < rhs.get();
}

}
