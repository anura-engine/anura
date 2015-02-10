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

#include <iostream>

#include "Canvas.hpp"
#include "Font.hpp"

#include "button.hpp"
#include "color_picker.hpp"
#include "dropdown_widget.hpp"
#include "grid_widget.hpp"
#include "i18n.hpp"
#include "input.hpp"
#include "label.hpp"
#include "preferences.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"
#include "widget_settings_dialog.hpp"

namespace gui 
{
	namespace 
	{
		const int default_font_size = 14;
	}

	Label::Label(const std::string& text, int size, const std::string& font)
		: text_(i18n::tr(text)), border_size_(0), size_(size), down_(false),
		  fixed_width_(false), highlight_color_(KRE::Color::factory("red")),
		  highlight_on_mouseover_(false), draw_highlight_(false), font_(font),
		  color_(KRE::Color::factory("white"))
	{
		setEnvironment();
		recalculateTexture();
	}

	Label::Label(const std::string& text, const KRE::Color& color, int size, const std::string& font)
		: text_(i18n::tr(text)), color_(new KRE::Color(color)), border_size_(0),
		  size_(size), down_(false), font_(font),
		  fixed_width_(false), highlight_color_(KRE::Color::factory("red")),
		  highlight_on_mouseover_(false), draw_highlight_(false)
	{
		setEnvironment();
		recalculateTexture();
	}

	Label::Label(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e), fixed_width_(false), down_(false), 
		highlight_color_(KRE::Color::factory("red")), draw_highlight_(false),
		font_(v["font"].as_string_default())
	{
		text_ = i18n::tr(v["text"].as_string());
	
		color_ = v.has_key("color") ? KRE::ColorPtr(new KRE::Color(v["color"])) : KRE::Color::factory("white");
	
		if(v.has_key("border_color")) {
			border_color_.reset(new KRE::Color(v["border_color"]));
			if(v.has_key("border_size")) {
				border_size_ = v["border_size"].as_int();
			} else {
				border_size_ = 2;
			}
		}

		size_ = v.has_key("size") ? v["size"].as_int() : default_font_size;
		if(v.has_key("onClick")) {
			ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
			ffl_click_handler_ = getEnvironment()->createFormula(v["onClick"]);
			on_click_ = std::bind(&Label::click_delegate, this);
		}
		if(v.has_key("highlight_color")) {
			highlight_color_.reset(new KRE::Color(v["highlight_color"]));
		}
		highlight_on_mouseover_ = v["highlight_on_mouseover"].as_bool(false);
		setClaimMouseEvents(v["claim_mouse_events"].as_bool(false));
		recalculateTexture();
	}

	void Label::click_delegate()
	{
		if(getEnvironment()) {
			variant value = ffl_click_handler_->execute(*getEnvironment());
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "Label::click() called without environment!" << std::endl;
		}
	}

	void Label::setColor(const KRE::Color& color)
	{
		color_.reset(new KRE::Color(color));
		recalculateTexture();
	}

	void Label::setFontSize(int size)
	{
		size_ = size;
		recalculateTexture();
	}

	void Label::setFont(const std::string& font)
	{
		font_ = font;
		recalculateTexture();
	}

	void Label::setText(const std::string& text)
	{
		text_ = i18n::tr(text);
		reformatText();
		recalculateTexture();
	}

	std::string& Label::currentText() {
		if(fixed_width_) {
			return formatted_;
		}
		return text_;
	}

	const std::string& Label::currentText() const {
		if(fixed_width_) {
			return formatted_;
		}
		return text_;
	}

	void Label::setFixedWidth(bool fixed_width)
	{
		fixed_width_ = fixed_width;
		reformatText();
		recalculateTexture();
	}

	void Label::setDim(int w, int h) {
		if(w != width() || h != height()) {
			innerSetDim(w, h);
			reformatText();
			recalculateTexture();
		}
	}

	void Label::innerSetDim(int w, int h) {
		Widget::setDim(w, h);
	}

	void Label::reformatText()
	{
		if(fixed_width_) {
			formatted_ = text_;
		}
	}

	void Label::recalculateTexture()
	{
		texture_ = KRE::Font::getInstance()->renderText(currentText(), *color_, size_, true, font_);
		innerSetDim(texture_->width(),texture_->height());

		if(border_color_) {
			border_texture_ = KRE::Font::getInstance()->renderText(currentText(), *border_color_, size_, true, font_);
		}
	}

	void Label::handleDraw() const
	{
		if(draw_highlight_ && highlight_color_) {
			KRE::Canvas::getInstance()->drawSolidRect(rect(x(), y(), width(), height()), *highlight_color_);
		}

		if(border_texture_) {
			KRE::Canvas::getInstance()->blitTexture(border_texture_, 0, rect(x() - border_size_, y()));
			KRE::Canvas::getInstance()->blitTexture(border_texture_, 0, rect(x() + border_size_, y()));
			KRE::Canvas::getInstance()->blitTexture(border_texture_, 0, rect(x(), y() - border_size_));
			KRE::Canvas::getInstance()->blitTexture(border_texture_, 0, rect(y() + border_size_));
		}
		KRE::Canvas::getInstance()->blitTexture(texture_, 0, rect(x(), y()));
	}

	void Label::setTexture(KRE::TexturePtr t) {
		texture_ = t;
	}

	bool Label::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(!on_click_ && !highlight_on_mouseover_) {
			return claimed;
		}
		if((event.type == SDL_MOUSEWHEEL) && inWidget(event.button.x, event.button.y)) {
			// skip processing if mousewheel event
			return claimed;
		}

		if(event.type == SDL_MOUSEMOTION) {
			const SDL_MouseMotionEvent& e = event.motion;
			if(highlight_on_mouseover_) {
				if(inWidget(e.x,e.y)) {
					draw_highlight_ = true;
				} else {
					draw_highlight_ = false;
				}
				claimed = claimMouseEvents();
			}
		} else if(event.type == SDL_MOUSEBUTTONDOWN) {
			const SDL_MouseButtonEvent& e = event.button;
			if(inWidget(e.x,e.y)) {
				down_ = true;
				claimed = claimMouseEvents();
			}
		} else if(event.type == SDL_MOUSEBUTTONUP) {
			down_ = false;
			const SDL_MouseButtonEvent& e = event.button;
			if(inWidget(e.x,e.y)) {
				if(on_click_) {
					on_click_();
				}
				claimed = claimMouseEvents();
			}
		}
		return claimed;
	}

	variant Label::handleWrite()
	{
		variant_builder res;
		res.add("type", "Label");
		res.add("text", text());
		if(color_ && color_->r_int() != 255 || color_->g_int() != 255 || color_->b_int() != 255 || color_->a_int() != 255) {
			res.add("color", color_->write());
		}
		if(size() != default_font_size) {
			res.add("size", size());
		}
		if(font().empty() == false) {
			res.add("font", font());
		}
		if(border_color_) {
			res.add("border_color", border_color_->write());
			if(border_size_ != 2) {
				res.add("border_size", border_size_);
			}
		}
		if(highlight_on_mouseover_) {
			res.add("highlight_on_mouseover", true);
		}
		if(claimMouseEvents()) {
			res.add("claim_mouse_events", true);
		}
		return res.build();
	}

	WidgetSettingsDialog* Label::settingsDialog(int x, int y, int w, int h)
	{
		WidgetSettingsDialog* d = Widget::settingsDialog(x,y,w,h);
	/*
		grid_ptr g(new grid(2));

		TextEditorWidgetPtr text_edit = new TextEditorWidget(150, 30);
		text_edit->setText(text());
		text_edit->setOnUserChangeHandler([=](){setText(text_edit->text());});
		g->addCol(new Label("Text:", d->getTextSize(), d->font()))
			.addCol(text_edit);

		g->addCol(new Label("Size:", d->getTextSize(), d->font())).
			addCol(new Slider(120, [&](double f){setFontSize(int(f*72.0+6.0));}, (size()-6.0)/72.0, 1));

		std::vector<std::string> fonts = font::getAvailableFonts();
		fonts.insert(fonts.begin(), "");
		DropdownWidgetPtr font_list(new DropdownWidget(fonts, 150, 28, dropdown_widget::DROPDOWN_LIST));
		font_list->setFontSize(14);
		font_list->setDropdownHeight(height());
		auto fit = std::find(fonts.begin(), fonts.end(), font());
		font_list->setSelection(fit == fonts.end() ? 0 : fit-fonts.begin());
		font_list->setOnSelectHandler([&](int n, const std::string& s){setFont(s);});
		font_list->setZOrder(19);
		g->addCol(new Label("Font:", d->getTextSize(), d->font()))
			.addCol(font_list);
		g->addCol(new Label("Color:", d->getTextSize(), d->font()))
			.addCol(new button(new Label("Choose...", d->getTextSize(), d->font()), [&](){
				int mx, my;
				input::sdl_get_mouse_state(&mx, &my);
				mx = mx + 200 > preferences::actual_screen_width() ? preferences::actual_screen_width()-200 : mx;
				my = my + 600 > preferences::actual_screen_height() ? preferences::actual_screen_height()-600 : my;
				my -= d->y();
				ColorPicker* cp = new ColorPicker(rect(0, 0, 200, 600), [&](const graphics::color& color){setColor(color.as_sdl_color());});
				cp->setPrimaryColor(graphics::color(color_));

				grid_ptr gg = new grid(1);
				gg->allowSelection();
				gg->swallowClicks();
				gg->setShowBackground(true);
				gg->allowDrawHighlight(false);
				gg->registerSelectionCallback([=](int n){if(n != 0){d->removeWidget(gg); d->init();}});
				gg->setZOrder(100);
				gg->addCol(cp);
				d->addWidget(gg, d->x()-mx-100, my);
		}));

		d->addWidget(g);*/
		return d;
	}


	BEGIN_DEFINE_CALLABLE(Label, Widget)
		DEFINE_FIELD(text, "string")
			return variant(obj.text_);
		DEFINE_SET_FIELD
			if(value.is_null()) {
				obj.setText("");
			} else {
				obj.setText(value.as_string());
			}
		DEFINE_FIELD(size, "int")
			return variant(obj.size_);
		DEFINE_SET_FIELD
			obj.setFontSize(value.as_int());
		DEFINE_FIELD(font, "string")
			return variant(obj.font_);
		DEFINE_SET_FIELD
			obj.setFont(value.as_string());
		DEFINE_FIELD(color, "string")
			return variant();
		DEFINE_SET_FIELD
			obj.setColor(KRE::Color(value));
	END_DEFINE_CALLABLE(Label)

	DialogLabel::DialogLabel(const std::string& text, const KRE::Color& color, int size)
		: Label(text, color, size), progress_(0) {

		recalculateTexture();
	}

	DialogLabel::DialogLabel(const variant& v, game_logic::FormulaCallable* e)
		: Label(v, e), progress_(0)
	{
		recalculateTexture();
	}

	void DialogLabel::setProgress(int progress)
	{
		progress_ = progress;
		recalculateTexture();
	}

	void DialogLabel::recalculateTexture()
	{
		Label::recalculateTexture();
		stages_ = currentText().size();
		int prog = progress_;
		if(prog < 0) prog = 0;
		if(prog > stages_) prog = stages_;
		std::string txt = currentText().substr(0, prog);

		if(prog > 0) {
			setTexture(KRE::Font::getInstance()->renderText(txt, color(), size(), false, font()));
		} else {
			setTexture(KRE::TexturePtr());
		}
	}

	void DialogLabel::setValue(const std::string& key, const variant& v)
	{
		if(key == "progress") {
			setProgress(v.as_int());
		}
		Label::setValue(key, v);
	}

	variant DialogLabel::getValue(const std::string& key) const
	{
		if(key == "progress") {
			return variant(progress_);
		}
		return Label::getValue(key);
	}

	LabelFactory::LabelFactory(const KRE::Color& color, int size)
	   : color_(color), size_(size)
	{}

	LabelPtr LabelFactory::create(const std::string& text) const
	{
		return LabelPtr(new Label(text,color_,size_));
	}

	LabelPtr LabelFactory::create(const std::string& text,
									const std::string& tip) const
	{
		const LabelPtr res(create(text));
		res->setTooltip(tip);
		return res;
	}
}
