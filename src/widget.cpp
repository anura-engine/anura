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
#include "widget_settings_dialog.hpp"
#include "iphone_controls.hpp"

#include <iostream>

namespace gui {

widget::widget() 
	: x_(0), y_(0), w_(0), h_(0), align_h_(HALIGN_LEFT), align_v_(VALIGN_TOP),
	true_x_(0), true_y_(0), disabled_(false), disabled_opacity_(127),
	tooltip_displayed_(false), visible_(true), zorder_(0), environ_(0),
	tooltip_display_delay_(0), tooltip_ticks_(INT_MAX), resolution_(0),
	display_alpha_(256), pad_h_(0), pad_w_(0), claim_mouse_events_(true),
	draw_with_object_shader_(true), tooltip_fontsize_(18),
	swallow_all_events_(false), tab_stop_(0), has_focus_(false)
	{
		tooltip_color_.r = tooltip_color_.g = tooltip_color_.b = tooltip_color_.a = 255;
	}

widget::widget(const variant& v, game_logic::formula_callable* e) 
	: environ_(e), w_(0), h_(0), x_(0), y_(0), zorder_(0), 
	true_x_(0), true_y_(0), disabled_(false), disabled_opacity_(v["disabled_opacity"].as_int(127)),
	tooltip_displayed_(false), id_(v["id"].as_string_default()), align_h_(HALIGN_LEFT), align_v_(VALIGN_TOP),
	tooltip_display_delay_(v["tooltip_delay"].as_int(0)), tooltip_ticks_(INT_MAX),
	resolution_(v["frame_size"].as_int(0)), display_alpha_(v["alpha"].as_int(256)),
	pad_w_(0), pad_h_(0), claim_mouse_events_(v["claim_mouse_events"].as_bool(true)),
	draw_with_object_shader_(v["draw_with_object_shader"].as_bool(true)), tooltip_fontsize_(18),
	swallow_all_events_(false), tab_stop_(v["tab_stop"].as_int(0)), has_focus_(false)
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
	tooltip_color_.r = tooltip_color_.g = tooltip_color_.b = tooltip_color_.a = 255;
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
	if(v.has_key("clip_area")) {
		set_clip_area(rect(v["clip_area"]));
	}

	recalc_loc();

	if(v.has_key("clip_to_dimensions")) {
		if(v["clip_to_dimensions"].as_bool()) {
			set_clip_area_to_dim();
		}
	}
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
	tooltip_text_ = str;
	tooltip_fontsize_ = fontsize;
	tooltip_color_ = color;
	tooltip_font_ = font;
	if(tooltip_displayed_ && tooltip_ != NULL) {
		if(tooltip_->text == str 
			&& tooltip_fontsize_ == fontsize 
			&& tooltip_color_.r == color.r && tooltip_color_.g == color.g && tooltip_color_.b == color.b && tooltip_color_.a == color.a 
			&& tooltip_font_ == font) {
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

	const bool must_swallow = swallow_all_events_ && event.type != SDL_QUIT;

	return handle_event(event, claimed) || must_swallow;
}

void widget::draw() const
{
	if(visible_) {
		color_save_context color_saver;

		GLint src = 0;
		GLint dst = 0;
#if !defined(USE_SHADERS)
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

		if(clip_area_) {
//			const graphics::clip_scope clipping_scope(clip_area_->sdl_rect());
			handle_draw();
		} else {
			handle_draw();
		}
#if !defined(USE_SHADERS)
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

const rect* widget::clip_area() const
{
	return clip_area_.get();
}

void widget::set_clip_area(const rect& area)
{
	clip_area_.reset(new rect(area));
}

void widget::set_clip_area_to_dim()
{
	set_clip_area(rect(x(), y(), width(), height()));
}

void widget::clear_clip_area()
{
	clip_area_.reset();
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


widget_settings_dialog* widget::settings_dialog(int x, int y, int w, int h)
{
	return new widget_settings_dialog(x,y,w,h,this);
}

dialog_ptr widget::get_settings_dialog(int x, int y, int w, int h)
{

	return settings_dialog(x,y,w,h);
}

BEGIN_DEFINE_CALLABLE_NOBASE(widget)

DEFINE_FIELD(draw_area, "[int]")
	std::vector<variant> v;
	v.push_back(variant(obj.x_));
	v.push_back(variant(obj.y_));
	v.push_back(variant(obj.w_));
	v.push_back(variant(obj.h_));
	return variant(&v);
DEFINE_SET_FIELD
	std::vector<int> r = value.as_list_int();
	ASSERT_LOG(r.size() == 4, "Four values must be supplied to the draw_area attribute");
	obj.set_loc(r[0], r[1]);
	obj.set_dim(r[2], r[3]);

DEFINE_FIELD(rect, "[int]")
	std::vector<variant> v;
	v.push_back(variant(obj.x_));
	v.push_back(variant(obj.y_));
	v.push_back(variant(obj.w_));
	v.push_back(variant(obj.h_));
	return variant(&v);
DEFINE_SET_FIELD
	std::vector<int> r = value.as_list_int();
	ASSERT_LOG(r.size() == 4, "Four values must be supplied to the rect attribute");
	obj.set_loc(r[0], r[1]);
	obj.set_dim(r[2], r[3]);
DEFINE_FIELD(tooltip, "string|null")
	if(obj.tooltip_) {
		return variant(obj.tooltip_->text);
	} else {
		return variant();
	}

DEFINE_FIELD(visible, "bool")
	return variant::from_bool(obj.visible_);
DEFINE_SET_FIELD
	obj.visible_ = value.as_bool();

DEFINE_FIELD(id, "string")
	return variant(obj.id_);

DEFINE_FIELD(resolution, "int")
	return variant(obj.resolution_);

DEFINE_FIELD(x, "int")
	return variant(obj.x());
DEFINE_SET_FIELD
	obj.set_loc(value.as_int(), obj.y());

DEFINE_FIELD(y, "int")
	return variant(obj.y());

DEFINE_SET_FIELD
	obj.set_loc(obj.x(), value.as_int());

DEFINE_FIELD(w, "int")
	return variant(obj.width());
DEFINE_SET_FIELD
	obj.w_ = value.as_int();

DEFINE_FIELD(width, "int")
	return variant(obj.width());
DEFINE_SET_FIELD
	obj.w_ = value.as_int();

DEFINE_FIELD(h, "int")
	return variant(obj.height());
DEFINE_SET_FIELD
	obj.h_ = value.as_int();

DEFINE_FIELD(height, "int")
	return variant(obj.height());
DEFINE_SET_FIELD
	obj.h_ = value.as_int();

DEFINE_FIELD(frame_set_name, "string")
	return variant(obj.frame_set_name_);

DEFINE_FIELD(alpha, "int")
	return variant(obj.get_alpha());
DEFINE_SET_FIELD
	int a = value.as_int();
	obj.set_alpha(a < 0 ? 0 : (a > 256 ? 256 : a));

DEFINE_FIELD(frame_pad_width, "int")
	return variant(obj.get_pad_width());

DEFINE_FIELD(frame_pad_height, "int")
	return variant(obj.get_pad_height());

DEFINE_FIELD(frame_padding, "[int]")
	std::vector<variant> v;
	v.push_back(variant(obj.get_pad_width()));
	v.push_back(variant(obj.get_pad_height()));
	return variant(&v);

DEFINE_FIELD(children, "[builtin widget]")
	std::vector<variant> v;
	std::vector<widget_ptr> w = obj.get_children();
	foreach(widget_ptr item, w) {
		v.push_back(variant(item.get()));
	}

	return variant(&v);

DEFINE_FIELD(disabled, "bool")
	return variant::from_bool(obj.disabled_);
DEFINE_SET_FIELD
	obj.disabled_ = value.as_bool();

DEFINE_FIELD(disabled_opacity, "int")
	return variant(static_cast<int>(obj.disabled_opacity_));
DEFINE_SET_FIELD
	obj.disabled_opacity_ = value.as_int();

DEFINE_FIELD(clip_area, "[int]|null")
	if(obj.clip_area_) {
		return obj.clip_area_->write();
	} else {
		return variant();
	}
DEFINE_SET_FIELD_TYPE("[int]")
	obj.set_clip_area(rect(value));

DEFINE_FIELD(clip_to_dimensions, "bool")
	if(obj.clip_area() && obj.clip_area()->x() == obj.x() && obj.clip_area()->y() == obj.y() 
		&& obj.clip_area()->w() == obj.width() && obj.clip_area()->h() == obj.height()) {
		return variant::from_bool(true);
	} else {
		return variant::from_bool(false);
	}
DEFINE_SET_FIELD
	if(value.as_bool()) { 
		obj.set_clip_area_to_dim();
	} else if(obj.clip_area()->x() == obj.x() && obj.clip_area()->y() == obj.y() 
		&& obj.clip_area()->w() == obj.width() && obj.clip_area()->h() == obj.height()) {
		obj.clear_clip_area();
	}
END_DEFINE_CALLABLE(widget)

bool widget::in_widget(int xloc, int yloc) const
{
	if(xloc > 32767) {xloc -= 65536;}
	if(yloc > 32767) {yloc -= 65536;}
	if(clip_area_ && !point_in_rect(point(xloc, yloc), *clip_area_)) {
		return false;
	}

	return xloc > x() && xloc < x() + width() &&
			yloc > y() && yloc < y() + height();
}

void widget::set_frame_set(const std::string& frame) 
{ 
	if(!frame.empty()) {
		frame_set_ = framed_gui_element::get(frame); 
	} else {
		frame_set_.reset();
	}
	frame_set_name_ = frame; 
}

void widget::set_tooltip_text(const std::string& str)
{
	set_tooltip(str, tooltip_fontsize_, tooltip_color_, tooltip_font_);
}

void widget::set_tooltip_fontsize(int fontsize)
{
	set_tooltip(tooltip_text_, fontsize, tooltip_color_, tooltip_font_);
}

void widget::set_tooltip_color(const graphics::color& color)
{
	set_tooltip(tooltip_text_, tooltip_fontsize_, color.as_sdl_color(), tooltip_font_);
}

void widget::set_tooltip_font(const std::string& font)
{
	set_tooltip(tooltip_text_, tooltip_fontsize_, tooltip_color_, font);
}

variant widget::write()
{
	variant v = widget::handle_write();
	merge_variant_over(&v, handle_write());	
	return v;
}

variant widget::handle_write()
{
	variant_builder res;
	res.add("rect", x());
	res.add("rect", y());
	res.add("rect", width());
	res.add("rect", height());
	if(zorder() != 0) {
		res.add("zorder", zorder());
	}
	if(ffl_on_process_) {
		res.add("on_process", ffl_on_process_->str());
	}
	if(tooltip_text().empty() == false 
		|| tooltip_color().r != 255
		|| tooltip_color().g != 255
		|| tooltip_color().b != 255
		|| tooltip_color().a != 255
		|| tooltip_font().empty() == false
		|| tooltip_fontsize() != 18) {
		variant_builder tt;
		tt.add("color", graphics::color(tooltip_color()).write());
		tt.add("text", tooltip_text());
		tt.add("font", tooltip_font());
		tt.add("size", tooltip_fontsize());
		res.add("tooltip", tt.build());
	}
	if(!visible_) {
		res.add("visible", false);
	}
	if(halign() != HALIGN_LEFT) {
		res.add("align_h", halign() == HALIGN_RIGHT ? "right" : "center");
	}
	if(valign() != VALIGN_TOP) {
		res.add("align_v", valign() == VALIGN_BOTTOM ? "bottom" : "center");
	}
	if(disabled()) {
		res.add("enabled", false);
	}
	if(!frame_set_name().empty()) {
		res.add("frame", frame_set_name());
	}
	if(get_pad_width() != 0 || get_pad_height() != 0) {
		res.add("frame_padding", get_pad_width());
		res.add("frame_padding", get_pad_height());
	}
	if(get_tooltip_delay() != 0) {
		res.add("tooltip_delay", get_tooltip_delay());
	}
	if(disabled_opacity() != 127) {
		res.add("disabled_opacity", disabled_opacity());
	}
	if(id().empty() == false) {
		res.add("id", id());
	}
	if(get_frame_resolution() != 0) {
		res.add("frame_size", get_frame_resolution());
	}
	if(draw_with_object_shader_ == false) {
		res.add("draw_with_object_shader", false);
	}
	if(claim_mouse_events_ == false) {
		res.add("claim_mouse_events", false);
	}
	if(get_alpha() != 256) {
		res.add("alpha", get_alpha());
	}
	if(clip_area() != NULL) {
		if(clip_area()->x() == x() && clip_area()->y() == y() 
			&& clip_area()->w() == width() && clip_area()->h() == height()) {
			res.add("clip_to_dimensions", true);
		} else {
			res.add("clip_area", clip_area()->x());
			res.add("clip_area", clip_area()->y());
			res.add("clip_area", clip_area()->w());
			res.add("clip_area", clip_area()->h());
		}
	}
	return res.build();
}

bool widget_sort_zorder::operator()(const widget_ptr& lhs, const widget_ptr& rhs) const
{
	return lhs->zorder() < rhs->zorder() 
		|| lhs->zorder() == rhs->zorder() && lhs->y() < rhs->y() 
		|| lhs->zorder() == rhs->zorder() && lhs->y() == rhs->y() && lhs->x() < rhs->x() 
		|| lhs->zorder() == rhs->zorder() && lhs->y() == rhs->y() && lhs->x() == rhs->x() && lhs.get() < rhs.get();
}

bool widget_sort_tab_order::operator()(const int lhs, const int rhs) const
{
	return lhs < rhs;
}

}
