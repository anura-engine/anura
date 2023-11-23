/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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

#include "checkbox.hpp"
#include "graphical_font_label.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "widget_factory.hpp"

namespace gui
{
	using std::placeholders::_1;

	namespace
	{
		WidgetPtr create_checkbox_widget(WidgetPtr label,
			bool checked,
			BUTTON_RESOLUTION resolution,
			int hpadding=12) {
			GridPtr g(new Grid(2));
			g->setHpad(hpadding);
			g->addCol(WidgetPtr(new GuiSectionWidget(checked
				? "checkbox_ticked"
				: "checkbox_unticked", -1, -1, resolution == BUTTON_SIZE_NORMAL_RESOLUTION ? 1 : 2)));

			g->addCol(label);

			return g;
		}

		WidgetPtr create_checkbox_widget(const std::string& text, bool checked, BUTTON_RESOLUTION resolution) {
			return create_checkbox_widget(WidgetPtr(new GraphicalFontLabel(text, "door_label", 2)), checked, resolution);
		}
	}

	Checkbox::Checkbox(const std::string& label, bool checked, std::function<void(bool)> onclick, BUTTON_RESOLUTION buttonResolution)
	  : Button(create_checkbox_widget(label, checked, buttonResolution), std::bind(&Checkbox::onClick, this), BUTTON_STYLE_NORMAL,buttonResolution),
	    label_(label),
		onclick_(onclick),
		checked_(checked),
		hpadding_(12)
	{
		setEnvironment();
	}

	Checkbox::Checkbox(WidgetPtr label, bool checked, std::function<void(bool)> onclick, BUTTON_RESOLUTION buttonResolution)
	  : Button(create_checkbox_widget(label, checked, buttonResolution), std::bind(&Checkbox::onClick, this), BUTTON_STYLE_NORMAL,buttonResolution),
	    label_widget_(label),
		onclick_(onclick),
		checked_(checked),
		hpadding_(12)
	{
		setEnvironment();
	}

	Checkbox::Checkbox(const variant& v, game_logic::FormulaCallable* e)
		: Button(v,e),
		  checked_(false)
	{
		hpadding_ = v["hpad"].as_int(12);
		if(v.has_key("padding")) {
			ASSERT_LOG(v["padding"].num_elements() == 2, "Incorrect number of padding elements specified." << v["padding"].num_elements());
			hpadding_ = v["padding"][0].as_int();
		}
		checked_ = v["checked"].as_bool(false);
		variant label_var = v["label"];
		label_ = (label_var.is_map() || label_var.is_callable()) ? "" : label_var.as_string_default("Checkbox");
		label_widget_ = (label_var.is_map() || label_var.is_callable())
			? widget_factory::create(label_var, e)
			: WidgetPtr(new GraphicalFontLabel(label_, "door_label", 2));
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
		click_handler_ = getEnvironment()->createFormula(v["on_click"]);
		onclick_ = std::bind(&Checkbox::click, this, _1);
		setClickHandler(std::bind(&Checkbox::onClick, this));

		setLabel(create_checkbox_widget(label_widget_,
			checked_,
			buttonResolution(),
			hpadding_));

		if(v.has_key("width") || v.has_key("height")) {
			setDim(v["width"].as_int(width()), v["height"].as_int(height()));
		}
	}

	void Checkbox::onClick()
	{
		checked_ = !checked_;
		const int w = width();
		const int h = height();
		if(label_widget_) {
			setLabel(create_checkbox_widget(label_widget_, checked_, buttonResolution(), hpadding_));
		} else {
			setLabel(create_checkbox_widget(label_, checked_, buttonResolution()));
		}
		setDim(w, h);
		onclick_(checked_);
	}

	void Checkbox::click(bool checked)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("checked", variant::from_bool(checked));
			variant value = click_handler_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("Checkbox::click() called without environment!");
		}
	}

	WidgetPtr Checkbox::clone() const
	{
		Checkbox* cb = new Checkbox(*this);
		if(label_widget_) {
			cb->label_widget_ = label_widget_->clone();
		}
		return WidgetPtr(cb);
	}

	BEGIN_DEFINE_CALLABLE(Checkbox, Button)
		DEFINE_FIELD(label, "builtin widget")
			return variant(obj.label_widget_.get());
		DEFINE_FIELD(checked, "bool")
			return variant::from_bool(obj.checked_);
	END_DEFINE_CALLABLE(Checkbox)
}
