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
#include "button.hpp"
#include "custom_object_functions.hpp"
#include "dropdown_widget.hpp"
#include "formula.hpp"
#include "formula_callable_visitor.hpp"
#include "grid_widget.hpp"
#include "label.hpp"
#include "slider.hpp"
#include "framed_gui_element.hpp"
#include "widget_settings_dialog.hpp"
#include "widget_factory.hpp"

namespace gui 
{
	namespace 
	{
		const int default_hpadding = 10;
		const int default_vpadding = 4;

		variant g_color_scheme;
	}

	Button::SetColorSchemeScope::SetColorSchemeScope(variant v) : backup(g_color_scheme)
	{
		g_color_scheme = v;
	}

	Button::SetColorSchemeScope::~SetColorSchemeScope()
	{
		g_color_scheme = backup;
	}

variant Button::getColorScheme()
{
	return g_color_scheme;
}

	Button::Button(const std::string& str, std::function<void()> onclick)
	  : label_(new Label(str, KRE::Color::colorWhite())),
		onclick_(onclick), button_resolution_(BUTTON_SIZE_NORMAL_RESOLUTION),
		button_style_(BUTTON_STYLE_NORMAL), hpadding_(default_hpadding), vpadding_(default_vpadding),
		down_(false)
	{
		setEnvironment();

		if(g_color_scheme.is_null() == false) {
			setColorScheme(g_color_scheme);
			return;
		}

		setup();
	}

	Button::Button(WidgetPtr label, std::function<void ()> onclick, BUTTON_STYLE button_style, BUTTON_RESOLUTION buttonResolution)
	  : label_(label), onclick_(onclick), button_resolution_(buttonResolution), button_style_(button_style),
		down_(false), hpadding_(default_hpadding), vpadding_(default_vpadding)
	
	{
		setEnvironment();
		if(g_color_scheme.is_null() == false) {
			setColorScheme(g_color_scheme);
			return;
		}

		setup();
	}

	Button::Button(const variant& v, game_logic::FormulaCallable* e) 
		: Widget(v,e), down_(false)
	{
		variant label_var = v["label"];
		if(!label_var.is_callable()) {
			label_ = label_var.is_map() ? widget_factory::create(label_var, e) : new Label(label_var.as_string_default("Button"), KRE::Color::colorWhite());
		}
		ASSERT_LOG(v.has_key("on_click"), "Button must be supplied with an onClick handler: " << v.write_json() << " " << v.debug_location());
		// create delegate for onclick
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");

		const variant on_click_value = v["on_click"];
		if(on_click_value.is_function()) {
			ASSERT_LOG(on_click_value.min_function_arguments() == 0, "onClick button function should take 0 arguments: " << v.debug_location());
			static const variant fml("fn()");
			click_handler_.reset(new game_logic::Formula(fml));

			game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
			callable->add("fn", on_click_value);

			handler_arg_.reset(callable);
		} else { 
			click_handler_ = getEnvironment()->createFormula(on_click_value);
		}

		onclick_ = std::bind(&Button::click, this);
		button_resolution_ = v["resolution"].as_string_default("normal") == "normal" ? BUTTON_SIZE_NORMAL_RESOLUTION : BUTTON_SIZE_DOUBLE_RESOLUTION;
		button_style_ = v["style"].as_string_default("default") == "default" ? BUTTON_STYLE_DEFAULT : BUTTON_STYLE_NORMAL;
		hpadding_ = v["hpad"].as_int(default_hpadding);
		vpadding_ = v["vpad"].as_int(default_vpadding);
		if(v.has_key("padding")) {
			ASSERT_LOG(v["padding"].num_elements() == 2, "Incorrect number of padding elements specifed." << v["padding"].num_elements());
			hpadding_ = v["padding"][0].as_int();
			vpadding_ = v["padding"][1].as_int();
		}

		if(v.has_key("color_scheme")) {
			variant m = v["color_scheme"];

			setColorScheme(m);
			return;
		} else if(g_color_scheme.is_null() == false) {
			setColorScheme(g_color_scheme);
			return;
		}

		setup();
	}

	void Button::setColorScheme(const variant& m)
	{
		if(m.is_null()) {
			return;
		}

		if(m.has_key("normal")) {
			normal_color_ = KRE::Color(m["normal"]);
		}
		if(m.has_key("depressed")) {
			depressed_color_ = KRE::Color(m["depressed"]);
		}
		if(m.has_key("focus")) {
			focus_color_ = KRE::Color(m["focus"]);
		}

		if(m.has_key("text_normal")) {
			text_normal_color_ = KRE::Color(m["text_normal"]);
		}
		if(m.has_key("text_depressed")) {
			text_depressed_color_ = KRE::Color(m["text_depressed"]);
		}
		if(m.has_key("text_focus")) {
			text_focus_color_ = KRE::Color(m["text_focus"]);
		}

		setup();
	}

	void Button::click()
	{
		if(handler_arg_) {
			variant value = click_handler_->execute(*handler_arg_);
			getEnvironment()->createFormula(value);
		} else if(getEnvironment()) {
			variant value = click_handler_->execute(*getEnvironment());
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "Button::click() called without environment!" << std::endl;
		}
	}

	void Button::setup()
	{
		if(button_style_ == BUTTON_STYLE_DEFAULT){
			normal_button_image_set_ = FramedGuiElement::get("default_button");
			depressed_button_image_set_ = FramedGuiElement::get("default_button_pressed");
			focus_button_image_set_ = FramedGuiElement::get("default_button_focus");
		}else{
			normal_button_image_set_ = FramedGuiElement::get("regular_button");
			depressed_button_image_set_ = FramedGuiElement::get("regular_button_pressed");
			focus_button_image_set_ = FramedGuiElement::get("regular_button_focus");
		}
		current_button_image_set_ = normal_button_image_set_;
	
		setLabel(label_);
	}

	void Button::setFocus(bool f)
	{
		Widget::setFocus();
		current_button_image_set_ = f ? (down_ ? depressed_button_image_set_ : focus_button_image_set_) : normal_button_image_set_;
	}

	void Button::doExecute()
	{ 
		if(onclick_) { 
			onclick_();
		} 
	}

	void Button::setLabel(WidgetPtr label)
	{
		label_ = label;
		if(width() == 0 && height() == 0) {
			setDim(label_->width()+hpadding_*2,label_->height()+vpadding_*2);
		}
	}

	void Button::handleDraw() const
	{
		label_->setLoc(width()/2 - label_->width()/2,height()/2 - label_->height()/2);

		const KRE::Color& col = current_button_image_set_ == normal_button_image_set_ 
			? normal_color_ 
			: (current_button_image_set_ == focus_button_image_set_ ? focus_color_ : depressed_color_);

		current_button_image_set_->blit(x(),y(),width(),height(), button_resolution_ != 0, col);

		const KRE::Color& text_col = current_button_image_set_ == normal_button_image_set_ 
			? text_normal_color_ 
			: (current_button_image_set_ == focus_button_image_set_ ? text_focus_color_ : text_depressed_color_);

		KRE::Canvas::ColorManager cm(text_col);
		label_->draw(x(),y(),getRotation(),getScale());
	}

	void Button::handleProcess()
	{
		Widget::handleProcess();
		label_->process();
	}

	bool Button::handleEvent(const SDL_Event& event, bool claimed)
	{
		if((event.type == SDL_MOUSEWHEEL) && inWidget(event.button.x, event.button.y)) {
			// skip processing if mousewheel event
			return claimed;
		}

		if(claimed) {
			current_button_image_set_ = normal_button_image_set_;
			down_ = false;
			return claimed;
		}

		if(event.type == SDL_MOUSEMOTION) {
			const SDL_MouseMotionEvent& e = event.motion;
			if(inWidget(e.x,e.y)) {
				current_button_image_set_ = down_ ? depressed_button_image_set_ : focus_button_image_set_;
			} else {
				current_button_image_set_ = normal_button_image_set_;
			}
		} else if(event.type == SDL_MOUSEBUTTONDOWN) {
			const SDL_MouseButtonEvent& e = event.button;
			if(inWidget(e.x,e.y)) {
			if(clipArea()) {
				std::cerr << *clipArea() << "\n";
			} else {
				std::cerr << "(null)\n";
			}
				current_button_image_set_ = depressed_button_image_set_;
				down_ = true;
				claimed = claimMouseEvents();
			}
		} else if(event.type == SDL_MOUSEBUTTONUP) {
			down_ = false;
			const SDL_MouseButtonEvent& e = event.button;
			if(current_button_image_set_ == depressed_button_image_set_) {
				if(inWidget(e.x,e.y)) {
					current_button_image_set_ = focus_button_image_set_;
					onclick_();
					claimed = claimMouseEvents();
				} else {
					current_button_image_set_ = normal_button_image_set_;
				}
			}
		}
		return claimed;
	}

	WidgetPtr Button::getWidgetById(const std::string& id)
	{
		if(label_ && label_->getWidgetById(id)) {
			return label_;
		}
		return Widget::getWidgetById(id);
	}

	ConstWidgetPtr Button::getWidgetById(const std::string& id) const
	{
		if(label_ && label_->getWidgetById(id)) {
			return label_;
		}
		return Widget::getWidgetById(id);
	}

	std::vector<WidgetPtr> Button::getChildren() const
	{
		std::vector<WidgetPtr> result;
		result.push_back(label_);
		return result;
	}

	BEGIN_DEFINE_CALLABLE(Button, Widget)
		DEFINE_FIELD(label, "builtin widget")
			return variant(obj.label_.get());
	END_DEFINE_CALLABLE(Button)

	void Button::visitValues(game_logic::FormulaCallableVisitor& visitor)
	{
		if(handler_arg_) {
			visitor.visit(&handler_arg_);
		}
	}

	void Button::setHPadding(int hpad)
	{
		hpadding_ = hpad;
		setup();
	}

	void Button::setVPadding(int vpad)
	{
		vpadding_ = vpad;
		setup();
	}


	WidgetSettingsDialog* Button::settingsDialog(int x, int y, int w, int h)
	{
		WidgetSettingsDialog* d = Widget::settingsDialog(x,y,w,h);
	/*
		GridPtr g(new Grid(2));
		g->addCol(new label("H Pad:", d->getTextSize(), d->font()));
		g->addCol(new Slider(120, [&](float f){this->setDim(0,0); this->setHPadding(static_cast<int>(f*100.0f));}, hpadding_/100.0f, 1));
		g->addCol(new label("V Pad:", d->getTextSize(), d->font()));
		g->addCol(new Slider(120, [&](float f){this->setDim(0,0); this->setVPadding(static_cast<int>(f*100.0f));}, vpadding_/100.0f, 1));

		std::vector<std::string> v;
		v.push_back("normal");
		v.push_back("double");
		DropdownWidgetPtr resolution(new DropdownWidget(v, 150, 28, dropdown_widget::DROPDOWN_LIST));
		resolution->setFontSize(14);
		resolution->setDropdownHeight(h);
		resolution->setSelection(button_resolution_ == BUTTON_SIZE_NORMAL_RESOLUTION ? 0 : 1);
		resolution->setOnSelectHandler([&](int n, const std::string& s){
			this->button_resolution_ = s == "normal" ? BUTTON_SIZE_NORMAL_RESOLUTION : BUTTON_SIZE_DOUBLE_RESOLUTION;
			this->setup();
		});
		resolution->setZOrder(11);
		g->addCol(new label("Resolution:", d->getTextSize(), d->font()));
		g->addCol(resolution);

		v.clear();
		v.push_back("default");
		v.push_back("normal");
		DropdownWidgetPtr style(new DropdownWidget(v, 150, 28, dropdown_widget::DROPDOWN_LIST));
		style->setFontSize(14);
		style->setDropdownHeight(h);
		style->setSelection(button_style_ == BUTTON_STYLE_DEFAULT ? 0 : 1);
		style->setOnSelectHandler([&](int n, const std::string& s){
			this->button_style_ = s == "normal" ? BUTTON_STYLE_NORMAL : BUTTON_STYLE_DEFAULT;
			this->setup();
		});
		style->setZOrder(10);
		g->addCol(new label("Style:", d->getTextSize(), d->font()));
		g->addCol(style);

		// label: widget
		// onClick: function
		// *** resolution: string/dropdown (normal/double)
		// *** style: string/dropdown (default/formal)
		// *** hpad: int
		// *** vpad: int
		d->addWidget(g);
		*/
		return d;
	}

	variant Button::handleWrite()
	{
		variant_builder res;
		res.add("type", "button");
		if(hpadding_ != default_hpadding && vpadding_ != default_vpadding) {
			res.add("padding", hpadding_);
			res.add("padding", vpadding_);
		}
		res.add("resolution", button_resolution_ == BUTTON_SIZE_NORMAL_RESOLUTION ? "normal" : "double");
		res.add("style", button_style_ == BUTTON_STYLE_DEFAULT ? "default" : "normal");
		if(click_handler_) {
			res.add("on_click", click_handler_->str());
		} else {
			res.add("on_click", "def()");
		}
		res.add("label", label_->write());
		return res.build();
	}
}
