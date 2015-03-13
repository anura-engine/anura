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
#include <functional>

#include "Canvas.hpp"
#include "WindowManager.hpp"
#include "ClipScope.hpp"

#include "asserts.hpp"
#include "i18n.hpp"
#include "profile_timer.hpp"
#include "preferences.hpp"
#include "tooltip.hpp"
#include "widget.hpp"
#include "widget_settings_dialog.hpp"

#include <iostream>

namespace gui 
{
	Widget::Widget() 
		: x_(0), y_(0), w_(0), h_(0), align_h_(HALIGN_LEFT), align_v_(VALIGN_TOP),
		true_x_(0), true_y_(0), disabled_(false), disabled_opacity_(127),
		tooltip_displayed_(false), visible_(true), zorder_(0), environ_(0),
		tooltip_display_delay_(0), tooltip_ticks_(std::numeric_limits<int>::max()), resolution_(0),
		display_alpha_(256), pad_h_(0), pad_w_(0), claim_mouse_events_(true),
		draw_with_object_shader_(true), tooltip_font_size_(18),
		swallow_all_events_(false), tab_stop_(0), has_focus_(false),
		tooltip_color_(255,255,255), rotation_(0), scale_(1.0f),  position_()
		{
		}

	Widget::Widget(const variant& v, game_logic::FormulaCallable* e) 
		: environ_(e), w_(0), h_(0), x_(0), y_(0), zorder_(0), 
		true_x_(0), true_y_(0), disabled_(false), disabled_opacity_(v["disabled_opacity"].as_int(127)),
		tooltip_displayed_(false), id_(v["id"].as_string_default()), align_h_(HALIGN_LEFT), align_v_(VALIGN_TOP),
		tooltip_display_delay_(v["tooltip_delay"].as_int(0)), tooltip_ticks_(std::numeric_limits<int>::max()),
		resolution_(v["frame_size"].as_int(0)), display_alpha_(v["alpha"].as_int(256)),
		pad_w_(0), pad_h_(0), claim_mouse_events_(v["claim_mouse_events"].as_bool(true)),
		draw_with_object_shader_(v["draw_with_object_shader"].as_bool(true)), tooltip_font_size_(18),
		swallow_all_events_(false), tab_stop_(v["tab_stop"].as_int(0)), has_focus_(false),
		tooltip_color_(255,255,255), rotation_(0), scale_(1.0f), position_()
	{
		setAlpha(display_alpha_ < 0 ? 0 : (display_alpha_ > 256 ? 256 : display_alpha_));
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
			setLoc(r[0], r[1]);
			setDim(r[2], r[3]);
		} 
		if(v.has_key("draw_area")) {
			std::vector<int> r = v["draw_area"].as_list_int();
			ASSERT_LOG(r.size() == 4, "Four values must be supplied to the rect attribute");
			setLoc(r[0], r[1]);
			setDim(r[2], r[3]);
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
			on_process_ = std::bind(&Widget::processDelegate, this);
			ffl_on_process_ = getEnvironment()->createFormula(v["on_process"]);
		}
		if(v.has_key("tooltip")) {
			if(v["tooltip"].is_string()) {
				const KRE::Color color = v.has_key("tooltip_color") ? KRE::Color(v["tooltip_color"]) : KRE::Color::colorYellow();
				setTooltip(v["tooltip"].as_string(), v["tooltip_size"].as_int(18), color, v["tooltipFont"].as_string_default());
			} else if(v["tooltip"].is_map()) {
				const KRE::Color color = v["tooltip"].has_key("color") ? KRE::Color(v["tooltip"]["color"]) : KRE::Color::colorYellow();
				setTooltip(v["tooltip"]["text"].as_string(), v["tooltip"]["size"].as_int(18), color, v["tooltip"]["font"].as_string_default());
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
			setFrameSet(v["frame"].as_string());
		}
		if(v.has_key("frame_padding")) {
			ASSERT_LOG(v["frame_padding"].is_list() && v["frame_padding"].num_elements() == 2, "'pad' must be two element list");
			setPadding(v["frame_padding"][0].as_int(), v["frame_padding"][1].as_int());
		} 
		if(v.has_key("frame_pad_width")) {
			setPadding(v["frame_pad_width"].as_int(), getPadHeight());
		}
		if(v.has_key("frame_pad_height")) {
			setPadding(getPadWidth(), v["frame_pad_height"].as_int());
		}
		if(v.has_key("clip_area")) {
			setClipArea(rect(v["clip_area"]));
		}

		recalcLoc();

		if(v.has_key("clip_to_dimensions")) {
			if(v["clip_to_dimensions"].as_bool()) {
				setClipAreaToDim();
			}
		}

		if(v.has_key("rotation")) {
			setRotation(v["rotation"].as_float());
		}
		if(v.has_key("scale")) {
			setScale(v["scale"].as_float());
		}
	}

	Widget::~Widget()
	{
		if(tooltip_displayed_) {
			gui::remove_tooltip(tooltip_);
		}
	}

	void Widget::recalcLoc()
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

	void Widget::processDelegate()
	{
		if(getEnvironment()) {
			variant value = ffl_on_process_->execute(*getEnvironment());
			getEnvironment()->executeCommand(value);
		} else {
			std::cerr << "Widget::processDelegate() called without environment!" << std::endl;
		}
	}

	void Widget::handleProcess()
	{
		if(!tooltip_displayed_ && profile::get_tick_time() > tooltip_ticks_ && tooltip_ != nullptr) {
			gui::set_tooltip(tooltip_);
			tooltip_displayed_ = true;
		}

		if(on_process_) {
			on_process_();
		}
	}

	void Widget::process() {
		handleProcess();
	}

	void Widget::normalizeEvent(SDL_Event* event, bool translate_coords)
	{
		unsigned wnd_id = event->type == SDL_MOUSEMOTION ? event->motion.windowID : event->button.windowID;
		auto wnd = KRE::WindowManager::getWindowFromID(wnd_id);

		int tx, ty;
		switch(event->type) {
		case SDL_MOUSEMOTION:
			tx = event->motion.x; 
			ty = event->motion.y;
			wnd->mapMousePosition(&tx, &ty);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			tx = event->button.x; 
			ty = event->button.y;
			wnd->mapMousePosition(&tx, &ty);
			break;
		default:
			break;
		}
	}

	void Widget::setTooltip(const std::string& str, int fontsize, const KRE::Color& color, const std::string& font)
	{
		tooltipText_ = str;
		tooltip_font_size_ = fontsize;
		tooltip_color_ = color;
		tooltip_font_ = font;
		if(tooltip_displayed_ && tooltip_ != nullptr) {
			if(tooltip_->text == str 
				&& tooltip_font_size_ == fontsize 
				&& tooltip_color_.r() == color.r() && tooltip_color_.g() == color.g() && tooltip_color_.b() == color.b() && tooltip_color_.a() == color.a() 
				&& tooltip_font_ == font) {
				return;
			}
			gui::remove_tooltip(tooltip_);
			tooltip_displayed_ = false;
		}
		tooltip_.reset(new gui::TooltipItem(std::string(i18n::tr(str)), fontsize, color, font));
	}

	bool Widget::processEvent(const point& p, const SDL_Event& event, bool claimed)
	{
		position_ = point(p.x+x(), p.y+y());
		if(disabled_) {
			tooltip_ticks_ = std::numeric_limits<int>::max();
			return claimed;
		}
		if(!claimed) {
			if(tooltip_ && event.type == SDL_MOUSEMOTION) {
				if(event.motion.x >= x() && event.motion.x <= x()+width() &&
					event.motion.y >= y() && event.motion.y <= y()+height()) {
					if(!tooltip_displayed_) {
						if(tooltip_display_delay_ == 0 || profile::get_tick_time() > tooltip_ticks_) {
							gui::set_tooltip(tooltip_);
							tooltip_displayed_ = true;
						} else if(tooltip_ticks_ == std::numeric_limits<int>::max()) {
							tooltip_ticks_ = profile::get_tick_time() + tooltip_display_delay_;
						}
					}
				} else {
					tooltip_ticks_ = std::numeric_limits<int>::max();
					if(tooltip_displayed_) {
						gui::remove_tooltip(tooltip_);
						tooltip_displayed_ = false;
					}
				}
			}
		} else {
			tooltip_ticks_ = std::numeric_limits<int>::max();
		}

		const bool must_swallow = swallow_all_events_ && event.type != SDL_QUIT;

		return handleEvent(event, claimed) || must_swallow;
	}

	void Widget::draw(int xt, int yt, float rotate, float scale) const
	{
		if(visible_) {
			KRE::Canvas::ModelManager mm(xt, yt, rotate, scale);
			KRE::Canvas::ColorManager cm(KRE::Color(255,255,255,disabled() ? disabledOpacity() : getAlpha()));
			if(frame_set_ != nullptr) {
				frame_set_->blit(x() - getPadWidth() - frame_set_->cornerHeight(),
					y() - getPadHeight() - frame_set_->cornerHeight(), 
					width() + getPadWidth()*2 + 2*frame_set_->cornerHeight(), 
					height() + getPadHeight()*2 + 2*frame_set_->cornerHeight(), resolution_ != 0);
			}

			if(clip_area_) {
				auto cs = KRE::ClipScope::create(*clip_area_);
				handleDraw();
			} else {
				handleDraw();
			}
		}
	}

	int Widget::x() const
	{
		return x_;
	}

	int Widget::y() const
	{
		return y_;
	}

	int Widget::width() const
	{
		return w_;
	}

	int Widget::height() const
	{
		return h_;
	}

	const rect* Widget::clipArea() const
	{
		return clip_area_.get();
	}

	void Widget::setClipArea(const rect& area)
	{
		clip_area_.reset(new rect(area));
	}

	void Widget::setClipAreaToDim()
	{
		setClipArea(rect(x(), y(), width(), height()));
	}

	void Widget::clearClipArea()
	{
		clip_area_.reset();
	}

	ConstWidgetPtr Widget::getWidgetById(const std::string& id) const
	{
		if(id_ == id) {
			return ConstWidgetPtr(this);
		}
		return WidgetPtr();
	}

	WidgetPtr Widget::getWidgetById(const std::string& id)
	{
		if(id_ == id) {
			return WidgetPtr(this);
		}
		return WidgetPtr();
	}


	WidgetSettingsDialog* Widget::settingsDialog(int x, int y, int w, int h)
	{
		return new WidgetSettingsDialog(x,y,w,h,this);
	}

	DialogPtr Widget::getSettingsDialog(int x, int y, int w, int h)
	{
		return settingsDialog(x,y,w,h);
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(Widget)

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
		obj.setLoc(r[0], r[1]);
		obj.setDim(r[2], r[3]);

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
		obj.setLoc(r[0], r[1]);
		obj.setDim(r[2], r[3]);
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
		obj.setLoc(value.as_int(), obj.y());

	DEFINE_FIELD(y, "int")
		return variant(obj.y());

	DEFINE_SET_FIELD
		obj.setLoc(obj.x(), value.as_int());

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
		return variant(obj.getAlpha());
	DEFINE_SET_FIELD
		int a = value.as_int();
		obj.setAlpha(a < 0 ? 0 : (a > 256 ? 256 : a));

	DEFINE_FIELD(frame_pad_width, "int")
		return variant(obj.getPadWidth());

	DEFINE_FIELD(frame_pad_height, "int")
		return variant(obj.getPadHeight());

	DEFINE_FIELD(frame_padding, "[int]")
		std::vector<variant> v;
		v.push_back(variant(obj.getPadWidth()));
		v.push_back(variant(obj.getPadHeight()));
		return variant(&v);

	DEFINE_FIELD(children, "[builtin widget]")
		std::vector<variant> v;
		std::vector<WidgetPtr> w = obj.getChildren();
		for(auto item : w) {
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
		obj.setClipArea(rect(value));

	DEFINE_FIELD(clip_to_dimensions, "bool")
		if(obj.clipArea() && obj.clipArea()->x() == obj.x() && obj.clipArea()->y() == obj.y() 
			&& obj.clipArea()->w() == obj.width() && obj.clipArea()->h() == obj.height()) {
			return variant::from_bool(true);
		} else {
			return variant::from_bool(false);
		}
	DEFINE_SET_FIELD
		if(value.as_bool()) { 
			obj.setClipAreaToDim();
		} else if(obj.clipArea()->x() == obj.x() && obj.clipArea()->y() == obj.y() 
			&& obj.clipArea()->w() == obj.width() && obj.clipArea()->h() == obj.height()) {
			obj.clearClipArea();
		}

	DEFINE_FIELD(rotation, "decimal")
		return variant(obj.getRotation());
	DEFINE_SET_FIELD_TYPE("decimal|int")
		obj.setRotation(value.as_float());

	DEFINE_FIELD(scale, "decimal")
		return variant(obj.getScale());
	DEFINE_SET_FIELD_TYPE("decimal|int")
		obj.setScale(value.as_float());

	END_DEFINE_CALLABLE(Widget)

	bool Widget::inWidget(int xloc, int yloc) const
	{
		xloc -= getPos().x;
		yloc -= getPos().y;
		if(xloc > 32767) {xloc -= 65536;}
		if(yloc > 32767) {yloc -= 65536;}
		if(clip_area_ && !pointInRect(point(xloc, yloc), *clip_area_)) {
			return false;
		}

		return xloc > 0 && xloc < width() &&
				yloc > 0 && yloc < height();
	}

	void Widget::setFrameSet(const std::string& frame) 
	{ 
		if(!frame.empty()) {
			frame_set_ = FramedGuiElement::get(frame); 
		} else {
			frame_set_.reset();
		}
		frame_set_name_ = frame; 
	}

	void Widget::setTooltipText(const std::string& str)
	{
		setTooltip(str, tooltip_font_size_, tooltip_color_, tooltip_font_);
	}

	void Widget::setTooltipFontSize(int fontsize)
	{
		setTooltip(tooltipText_, fontsize, tooltip_color_, tooltip_font_);
	}

	void Widget::setTooltipColor(const KRE::Color& color)
	{
		setTooltip(tooltipText_, tooltip_font_size_, color, tooltip_font_);
	}

	void Widget::setTooltipFont(const std::string& font)
	{
		setTooltip(tooltipText_, tooltip_font_size_, tooltip_color_, font);
	}

	variant Widget::write()
	{
		variant v = Widget::handleWrite();
		merge_variant_over(&v, handleWrite());	
		return v;
	}

	void Widget::setScale(float s) 
	{
		scale_ = s;
		if(scale_ < FLT_EPSILON) {
			scale_ = 1.0f;
		}
	}

	void Widget::setRotation(float r) 
	{ 
		rotation_ = r;
	}

	variant Widget::handleWrite()
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
		if(tooltipText().empty() == false 
			|| tooltipColor().r_int() != 255
			|| tooltipColor().g_int() != 255
			|| tooltipColor().b_int() != 255
			|| tooltipColor().a_int() != 255
			|| tooltipFont().empty() == false
			|| tooltipFontSize() != 18) {
			variant_builder tt;
			tt.add("color", tooltipColor().write());
			tt.add("text", tooltipText());
			tt.add("font", tooltipFont());
			tt.add("size", tooltipFontSize());
			res.add("tooltip", tt.build());
		}
		if(!visible_) {
			res.add("visible", false);
		}
		if(hAlign() != HALIGN_LEFT) {
			res.add("align_h", hAlign() == HALIGN_RIGHT ? "right" : "center");
		}
		if(vAlign() != VALIGN_TOP) {
			res.add("align_v", vAlign() == VALIGN_BOTTOM ? "bottom" : "center");
		}
		if(disabled()) {
			res.add("enabled", false);
		}
		if(!frameSetName().empty()) {
			res.add("frame", frameSetName());
		}
		if(getPadWidth() != 0 || getPadHeight() != 0) {
			res.add("frame_padding", getPadWidth());
			res.add("frame_padding", getPadHeight());
		}
		if(getTooltipDelay() != 0) {
			res.add("tooltip_delay", getTooltipDelay());
		}
		if(disabledOpacity() != 127) {
			res.add("disabled_opacity", disabledOpacity());
		}
		if(id().empty() == false) {
			res.add("id", id());
		}
		if(getFrameResolution() != 0) {
			res.add("frame_size", getFrameResolution());
		}
		if(draw_with_object_shader_ == false) {
			res.add("draw_with_object_shader", false);
		}
		if(claim_mouse_events_ == false) {
			res.add("claim_mouse_events", false);
		}
		if(getAlpha() != 256) {
			res.add("alpha", getAlpha());
		}
		if(clipArea() != nullptr) {
			if(clipArea()->x() == x() && clipArea()->y() == y() 
				&& clipArea()->w() == width() && clipArea()->h() == height()) {
				res.add("clip_to_dimensions", true);
			} else {
				res.add("clip_area", clipArea()->x());
				res.add("clip_area", clipArea()->y());
				res.add("clip_area", clipArea()->w());
				res.add("clip_area", clipArea()->h());
			}
		}
		if(getRotation() != 0) {
			res.add("rotation", getRotation());
		}
		if(getScale() != 1.0f) {
			res.add("scale", getScale());
		}
		return res.build();
	}

	bool WidgetSortZOrder::operator()(const WidgetPtr& lhs, const WidgetPtr& rhs) const
	{
		return lhs->zorder() < rhs->zorder() 
			|| lhs->zorder() == rhs->zorder() && lhs->y() < rhs->y() 
			|| lhs->zorder() == rhs->zorder() && lhs->y() == rhs->y() && lhs->x() < rhs->x() 
			|| lhs->zorder() == rhs->zorder() && lhs->y() == rhs->y() && lhs->x() == rhs->x() && lhs.get() < rhs.get();
	}

	bool widgetSortTabOrder::operator()(const int lhs, const int rhs) const
	{
		return lhs < rhs;
	}
}
