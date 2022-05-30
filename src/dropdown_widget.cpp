/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>

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

#ifndef NO_EDITOR
#include <vector>

#include "Canvas.hpp"

#include "asserts.hpp"
#include "button.hpp"
#include "controls.hpp"
#include "dropdown_widget.hpp"
#include "graphical_font_label.hpp"
#include "i18n.hpp"
#include "image_widget.hpp"
#include "input.hpp"
#include "joystick.hpp"
#include "module.hpp"

namespace gui
{
	using std::placeholders::_1;
	using std::placeholders::_2;

	namespace
	{
		const std::string dropdown_button_image = "dropdown_button";
	}

	DropdownWidget::DropdownWidget(const DropdownList& list, int width, int height, DropdownType type)
		: dropdown_height_(100),
		  list_(list),
		  current_selection_(0),
		  type_(type),
		  editor_(),
		  dropdown_menu_(),
		  labels_(),
		  label_(),
		  dropdown_image_(),
		  on_change_(),
		  on_select_(),
		  normal_image_(),
		  focus_image_(),
		  font_(module::get_default_font()),
		  change_handler_(),
		  select_handler_(),
		  normal_color_(),
		  depressed_color_(),
		  focus_color_(),
		  text_normal_color_(),
		  text_depressed_color_(),
		  text_focus_color_(),
		  in_widget_(false)
	{
		setEnvironment();
		setDim(width, height);
		editor_ = new TextEditorWidget(width, height);
		editor_->setOnUserChangeHandler(std::bind(&DropdownWidget::textChange, this));
		editor_->setOnEnterHandler(std::bind(&DropdownWidget::textEnter, this));
		editor_->setOnTabHandler(std::bind(&DropdownWidget::textEnter, this));
		dropdown_image_ = GuiSectionWidgetPtr(new GuiSectionWidget(dropdown_button_image));
		//if(type_ == DROPDOWN_COMBOBOX) {
		//	editor_->setFocus(true);
		//}
		setZOrder(1);

		init();
	}

	DropdownWidget::DropdownWidget(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e),
		  dropdown_height_(100),
		  list_(),
		  current_selection_(0),
		  type_(DropdownType::LIST),
		  editor_(),
		  dropdown_menu_(),
		  labels_(),
		  label_(),
		  dropdown_image_(),
		  on_change_(),
		  on_select_(),
		  normal_image_(),
		  focus_image_(),
		  font_(module::get_default_font()),
		  change_handler_(),
		  select_handler_(),
		  normal_color_(),
		  depressed_color_(),
		  focus_color_(),
		  text_normal_color_(),
		  text_depressed_color_(),
		  text_focus_color_(),
		  in_widget_(false)
	{
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
		if(v.has_key("font")) {
			font_ = v["font"].as_string();
		}

		if(v.has_key("color_scheme")) {
			setColorScheme(v["color_scheme"]);
		}

		if(v.has_key("button_image")) {
			dropdown_image_ = GuiSectionWidgetPtr(new GuiSectionWidget(v["button_image"].as_string()));
			if(v.has_key("focus_button_image")) {
				normal_image_ = v["button_image"].as_string();
				focus_image_ = v["focus_button_image"].as_string();
			}
		} else {
			dropdown_image_ = GuiSectionWidgetPtr(new GuiSectionWidget(dropdown_button_image));
		}

		if(v.has_key("type")) {
			std::string s = v["type"].as_string();
			if(s == "combo" || s == "combobox") {
				type_ = DropdownType::COMBOBOX;
			} else if(s == "list" || s == "listbox") {
				type_ = DropdownType::LIST;
			} else {
				ASSERT_LOG(false, "Unreognised type: " << s);
			}
		}
		if(v.has_key("text_edit")) {
			editor_ = new TextEditorWidget(v["text_edit"], e);
		} else {
			editor_ = new TextEditorWidget(width(), height());
		}
		editor_->setOnEnterHandler(std::bind(&DropdownWidget::textEnter, this));
		editor_->setOnTabHandler(std::bind(&DropdownWidget::textEnter, this));
		editor_->setOnUserChangeHandler(std::bind(&DropdownWidget::textChange, this));
		if(v.has_key("on_change")) {
			change_handler_ = getEnvironment()->createFormula(v["on_change"]);
			on_change_ = std::bind(&DropdownWidget::changeDelegate, this, _1);
		}
		if(v.has_key("on_select")) {
			select_handler_ = getEnvironment()->createFormula(v["on_select"]);
			on_select_ = std::bind(&DropdownWidget::selectDelegate, this, _1, _2);
		}
		if(v.has_key("item_list")) {
			list_ = v["item_list"].as_list_string();
		}
		if(v.has_key("default")) {
			current_selection_ = v["default"].as_int();
		}
		init();
	}

	void DropdownWidget::init()
	{
		if(font_ == "bitmap") {
			make_label_ = [](const std::string& label, int size){
				return WidgetPtr(new GraphicalFontLabel(label, "door_label", 2));
			};
		} else {
			make_label_ = [this](const std::string& label, int size){
				return WidgetPtr(new Label(label, size, this->font_));
			};
		}

		const int dropdown_image_size = std::max(height(), dropdown_image_->height());
		label_ = make_label_(list_.size() > 0 && current_selection_ >= 0 && current_selection_ < static_cast<int>(list_.size()) ? list_[current_selection_] : _("No items"), 16);

		label_->setLoc((width() - dropdown_image_->width() - 8 - label_->width())/2, (height() - label_->height()) / 2);
		if(text_normal_color_) {
			label_->setColor(*text_normal_color_);
		}

		dropdown_image_->setLoc(width() - dropdown_image_->width() - 4, (height() - dropdown_image_->height()) / 2);
		// go on ask me why there is a +20 in the line below.
		// because TextEditorWidget uses a magic -20 when setting the width!
		// The magic +4's are because we want the rectangles drawn around the TextEditorWidget
		// to match the ones we draw around the dropdown image.
		editor_->setDim(width() - dropdown_image_size + 20 + 4, dropdown_image_size + 4);
		editor_->setLoc(-2, -2);

		dropdown_menu_.reset(new Grid(1));

		if(normal_color_) {
			dropdown_menu_->setBgColor(*normal_color_);
		}

		if(focus_color_) {
			dropdown_menu_->setFocusColor(*focus_color_);
		}

		dropdown_menu_->setLoc(0, height()+2);
		dropdown_menu_->allowSelection(true);
		dropdown_menu_->setShowBackground(true);
		dropdown_menu_->swallowClicks(true);
		dropdown_menu_->setColWidth(0, width());
		dropdown_menu_->setMaxHeight(dropdown_height_);
		dropdown_menu_->setDim(width(), dropdown_height_);
		dropdown_menu_->mustSelect();
		dropdown_menu_->setDim(width(), 0);
		dropdown_menu_->setVpad(8);

		labels_.clear();
		for(auto& s : list_) {
			labels_.emplace_back(make_label_(i18n::tr(s), 14));
			if(text_normal_color_ != nullptr) {
				labels_.back()->setColor(*text_normal_color_);
			}
		}

		for(auto item : labels_) {
			dropdown_menu_->addCol(item);
		}
		dropdown_menu_->registerSelectionCallback(std::bind(&DropdownWidget::executeSelection, this, _1));
		dropdown_menu_->registerMouseoverCallback(std::bind(&DropdownWidget::mouseoverItem, this, _1));
		dropdown_menu_->setVisible(false);
	}

	void DropdownWidget::setFont(const std::string& font)
	{
		font_ = font;
		init();
	}

	void DropdownWidget::setLabel()
	{
		//label_ = labels_[current_selection_];
		label_ = make_label_(list_[current_selection_], 16);
		label_->setLoc((width() - dropdown_image_->width() - 8 - label_->width())/2, (height() - label_->height()) / 2);
		if(text_normal_color_) {
			label_->setColor(*text_normal_color_);
		}
	}

	void DropdownWidget::setSelection(int selection)
	{
		if(selection >= 0 || selection < static_cast<int>(list_.size())) {
			current_selection_ = selection;
			if(type_ == DropdownType::LIST) {
				setLabel();
			} else if(type_ == DropdownType::COMBOBOX) {
				editor_->setText(list_[current_selection_]);
			}
		}
	}

	void DropdownWidget::changeDelegate(const std::string& s)
	{
		if(getEnvironment()) {
			game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable(getEnvironment());
			callable->add("selection", variant(s));
			variant v(callable);
			variant value = change_handler_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("DropdownWidget::changeDelegate() called without environment!");
		}
	}

	void DropdownWidget::selectDelegate(int selection, const std::string& s)
	{
		if(getEnvironment()) {
			game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable(getEnvironment());
			if(selection == -1) {
				callable->add("selection", variant(selection));
			} else {
				callable->add("selection", variant(s));
			}
			variant v(callable);
			variant value = select_handler_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("DropdownWidget::selectDelegate() called without environment!");
		}
	}

	void DropdownWidget::textEnter()
	{
		DropdownList::iterator it = std::find(list_.begin(), list_.end(), editor_->text());
		if(it == list_.end()) {
			current_selection_ = -1;
		} else {
			current_selection_ = static_cast<int>(it - list_.begin());
		}
		if(on_select_) {
			on_select_(current_selection_, editor_->text());
		}
	}

	void DropdownWidget::textChange()
	{
		if(on_change_) {
			on_change_(editor_->text());
		}
	}

	void DropdownWidget::handleDraw() const
	{
		auto canvas = KRE::Canvas::getInstance();

		if(type_ == DropdownType::LIST) {
			canvas->drawHollowRect(rect(x()-1, y()-1, width()+2, height()+2),
				hasFocus() ? KRE::Color::colorWhite() : KRE::Color::colorGrey());
		}
		canvas->drawHollowRect(rect(x()+width()-height(), y()-1, height()+1, height()+2),
			hasFocus() ? KRE::Color::colorWhite() : KRE::Color::colorGrey());

		if(normal_color_) {
			canvas->drawSolidRect(rect(x(), y(), width()+2, height()+2), in_widget_ && focus_color_ ? *focus_color_ : *normal_color_);
		}

		if(type_ == DropdownType::LIST) {
			label_->draw(x(), y(), getRotation(), getScale());
		} else if(type_ == DropdownType::COMBOBOX) {
			editor_->draw(x(), y(), getRotation(), getScale());
		}
		if(dropdown_image_) {
			dropdown_image_->draw(x(), y(), getRotation(), getScale());
		}
		if(dropdown_menu_ && dropdown_menu_->visible()) {
			dropdown_menu_->draw(x(), y(), getRotation(), getScale());
		}
	}

	void DropdownWidget::handleProcess()
	{
		/*if(hasFocus() && dropdown_menu_) {
			if(joystick::button(0) || joystick::button(1) || joystick::button(2)) {

			}

			if(dropdown_menu_->visible()) {
			} else {
			}
		}*/
	}

	bool DropdownWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(claimed) {
			return claimed;
		}

		if(type_ == DropdownType::COMBOBOX && editor_) {
			if(editor_->processEvent(getPos(), event, claimed)) {
				return true;
			}
		}

		if(dropdown_menu_ && dropdown_menu_->visible()) {
			if(dropdown_menu_->processEvent(getPos(), event, claimed)) {
				return true;
			}
		}

		if(hasFocus() && dropdown_menu_) {
			if(event.type == SDL_KEYDOWN
				&& (event.key.keysym.sym == controls::get_keycode(controls::CONTROL_ATTACK)
				|| event.key.keysym.sym == controls::get_keycode(controls::CONTROL_JUMP))) {
				claimed = true;
				dropdown_menu_->setVisible(!dropdown_menu_->visible());
			}
		}

		if(event.type == SDL_MOUSEMOTION) {
			return handleMouseMotion(event.motion, claimed);
		} else if(event.type == SDL_MOUSEBUTTONDOWN) {
			return handleMousedown(event.button, claimed);
		} else if(event.type == SDL_MOUSEBUTTONUP) {
			return handleMouseup(event.button, claimed);
		}
		return claimed;
	}

	bool DropdownWidget::handleMousedown(const SDL_MouseButtonEvent& event, bool claimed)
	{
		point p(event.x, event.y);
		//int button_state = input::sdl_get_mouse_state(&p.x, &p.y);
		if(inWidget(p.x, p.y)) {
			claimed = claimMouseEvents();
			if(dropdown_menu_) {
				dropdown_menu_->setVisible(!dropdown_menu_->visible());
			}
		}
		return claimed;
	}

	void DropdownWidget::setDropdownHeight(int h)
	{
		dropdown_height_ = h;
		if(dropdown_menu_) {
			dropdown_menu_->setMaxHeight(dropdown_height_);
		}
	}

	bool DropdownWidget::handleMouseup(const SDL_MouseButtonEvent& event, bool claimed)
	{
		point p(event.x, event.y);
		//int button_state = input::sdl_get_mouse_state(&p.x, &p.y);
		if(inWidget(p.x, p.y)) {
			claimed = claimMouseEvents();
		}
		return claimed;
	}

	bool DropdownWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, bool claimed)
	{
		point p;
		input::sdl_get_mouse_state(&p.x, &p.y);
		if(in_widget_ != inWidget(event.x, event.y)) {
			in_widget_ = !in_widget_;
			if(!in_widget_ && text_normal_color_) {
				label_->setColor(*text_normal_color_);
			}

			if(in_widget_ && text_focus_color_) {
				label_->setColor(*text_focus_color_);
			}

			if(normal_image_.empty() == false) {
				if(in_widget_) {
					dropdown_image_->setGuiSection(focus_image_);
				} else {
					dropdown_image_->setGuiSection(normal_image_);
				}
			}
		}
		return claimed;
	}

	void DropdownWidget::executeSelection(int selection)
	{
		if(dropdown_menu_) {
			dropdown_menu_->setVisible(false);
		}
		if(selection < 0 || size_t(selection) >= list_.size()) {
			return;
		}
		current_selection_ = selection;
		if(type_ == DropdownType::LIST) {
			setLabel();
		} else if(type_ == DropdownType::COMBOBOX) {
			editor_->setText(list_[current_selection_]);
		}
		if(on_select_) {
			if(type_ == DropdownType::LIST) {
				on_select_(current_selection_, list_[current_selection_]);
			} else if(type_ == DropdownType::COMBOBOX) {
				on_select_(current_selection_, editor_->text());
			}
		}
	}

	void DropdownWidget::mouseoverItem(int selection)
	{
		if(text_normal_color_ && text_focus_color_) {
			for(int index = 0; index < static_cast<int>(labels_.size()); ++index) {
				labels_[index]->setColor(index == selection ? *text_focus_color_ : *text_normal_color_);
			}
		}
	}

	int DropdownWidget::getMaxHeight() const
	{
		// Maximum height required, including dropdown and borders.
		return height() + (dropdown_menu_ ? dropdown_menu_->height() : dropdown_height_) + 2;
	}

	void DropdownWidget::setColorScheme(const variant& m)
	{
		if(m.is_null()) {
			return;
		}

		if(m.has_key("normal")) {
			normal_color_.reset(new KRE::Color(m["normal"]));
		}
		if(m.has_key("depressed")) {
			depressed_color_.reset(new KRE::Color(m["depressed"]));
		}
		if(m.has_key("focus")) {
			focus_color_.reset(new KRE::Color(m["focus"]));
		}

		if(m.has_key("text_normal")) {
			text_normal_color_.reset(new KRE::Color(m["text_normal"]));
		}
		if(m.has_key("text_depressed")) {
			text_depressed_color_.reset(new KRE::Color(m["text_depressed"]));
		}
		if(m.has_key("text_focus")) {
			text_focus_color_.reset(new KRE::Color(m["text_focus"]));
		}
	}

	WidgetPtr DropdownWidget::clone() const
	{
		DropdownWidget* d = new DropdownWidget(*this);
		if(editor_ != nullptr) {
			d->editor_ = boost::dynamic_pointer_cast<TextEditorWidget>(editor_->clone());
		}
		if(label_ != nullptr) {
			d->label_ = label_->clone();
		}
		d->dropdown_menu_.reset();
		for(const auto& label : labels_) {
			if(label != nullptr) {
				d->labels_.emplace_back(label->clone());
			}
		}
		d->in_widget_ = false;
		// XXX should we clone dropdown_image_ ?
		return WidgetPtr(d);
	}

	BEGIN_DEFINE_CALLABLE(DropdownWidget, Widget)
		DEFINE_FIELD(selection, "int")
			return variant(obj.current_selection_);
		DEFINE_SET_FIELD
			obj.current_selection_ = value.as_int();

		DEFINE_FIELD(selected_item, "string|null")
			if(obj.current_selection_ < 0 || static_cast<size_t>(obj.current_selection_) > obj.list_.size()) {
				return variant();
			}
			return variant(obj.list_[obj.current_selection_]);

		DEFINE_FIELD(on_select, "null")
			return variant();
		DEFINE_SET_FIELD_TYPE("builtin callable")
			obj.on_select_ = std::bind(&DropdownWidget::selectDelegate, obj, _1, _2);
			obj.select_handler_ = obj.getEnvironment()->createFormula(value);

		DEFINE_FIELD(item_list, "[string]")
			std::vector<variant> v;
			for(auto& s : obj.list_) {
				v.emplace_back(s);
			}
			return variant(&v);
		DEFINE_FIELD(type, "string")
			if(obj.type_ == DropdownType::LIST) {
				return variant("list");
			}
			return variant("combobox");

		DEFINE_SET_FIELD
			const std::string& s = value.as_string();
			if(s == "list") {
				obj.type_ = DropdownType::LIST;
			} else if(s == "combobox") {
				obj.type_ = DropdownType::COMBOBOX;
			} else {
				ASSERT_LOG(false, "Unrecognised type: " << s);
			}
	END_DEFINE_CALLABLE(DropdownWidget)
}

#endif
