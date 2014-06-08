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

#include "checkbox.hpp"
#include "graphical_font_label.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "raster.hpp"
#include "widget_factory.hpp"

namespace gui {
namespace {

WidgetPtr create_checkbox_widget(WidgetPtr label, 
	bool checked, 
	buttonResolution resolution,
	int hpadding=12) {
	grid_ptr g(new grid(2));
	g->set_hpad(hpadding);
	g->add_col(WidgetPtr(new gui_section_widget(checked ? "checkbox_ticked" : "checkbox_unticked", -1, -1, resolution == BUTTON_SIZE_NORMAL_RESOLUTION ? 1 : 2)));

	g->add_col(label);

	return g;
}

WidgetPtr create_checkbox_widget(const std::string& text, bool checked, buttonResolution resolution) {
	return create_checkbox_widget(WidgetPtr(new graphical_font_label(text, "door_label", 2)), checked, resolution);
}
}

checkbox::checkbox(const std::string& label, bool checked, boost::function<void(bool)> onclick, buttonResolution buttonResolution)
  : button(create_checkbox_widget(label, checked, buttonResolution), boost::bind(&checkbox::on_click, this), BUTTON_STYLE_NORMAL,buttonResolution), label_(label), onclick_(onclick), checked_(checked), hpadding_(12)
{
	setEnvironment();
}

checkbox::checkbox(WidgetPtr label, bool checked, boost::function<void(bool)> onclick, buttonResolution buttonResolution)
  : button(create_checkbox_widget(label, checked, buttonResolution), boost::bind(&checkbox::on_click, this), BUTTON_STYLE_NORMAL,buttonResolution), label_widget_(label), onclick_(onclick), checked_(checked), hpadding_(12)
{
	setEnvironment();
}

checkbox::checkbox(const variant& v, game_logic::FormulaCallable* e) 
	: checked_(false), button(v,e)
{
	hpadding_ = v["hpad"].as_int(12);
	if(v.has_key("padding")) {
		ASSERT_LOG(v["padding"].num_elements() == 2, "Incorrect number of padding elements specifed." << v["padding"].num_elements());
		hpadding_ = v["padding"][0].as_int();
	}
	checked_ = v["checked"].as_bool(false);
	variant label_var = v["label"];
	label_ = (label_var.is_map() || label_var.is_callable()) ? "" : label_var.as_string_default("Checkbox");
	label_widget_ = (label_var.is_map() || label_var.is_callable())
		? widget_factory::create(label_var, e) 
		: WidgetPtr(new graphical_font_label(label_, "door_label", 2));
	ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
	click_handler_ = getEnvironment()->createFormula(v["on_click"]);
	onclick_ = boost::bind(&checkbox::click, this, _1);
	setClickHandler(boost::bind(&checkbox::on_click, this));

	set_label(create_checkbox_widget(label_widget_, 
		checked_, 
		buttonResolution(),
		hpadding_));

	if(v.has_key("width") || v.has_key("height")) {
		setDim(v["width"].as_int(width()), v["height"].as_int(height()));
	}
}

void checkbox::on_click()
{
	checked_ = !checked_;
	const int w = width();
	const int h = height();
	if(label_widget_) {
		set_label(create_checkbox_widget(label_widget_, checked_, buttonResolution(), hpadding_));
	} else {
		set_label(create_checkbox_widget(label_, checked_, buttonResolution()));
	}
	setDim(w, h);
	onclick_(checked_);
}

void checkbox::click(bool checked)
{
	using namespace game_logic;
	if(getEnvironment()) {
		map_FormulaCallablePtr callable = map_FormulaCallablePtr(new map_FormulaCallable(getEnvironment()));
		callable->add("checked", variant::from_bool(checked));
		variant value = click_handler_->execute(*callable);
		getEnvironment()->createFormula(value);
	} else {
		std::cerr << "checkbox::click() called without environment!" << std::endl;
	}
}

variant checkbox::getValue(const std::string& key) const
{
	if(key == "checked") {
		return variant::from_bool(checked_);
	} else if(key == "label") {
		return variant(label_widget_.get());
	}
	return button::getValue(key);
}

}
