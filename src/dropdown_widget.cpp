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

#include "kre/Canvas.hpp"

#include "asserts.hpp"
#include "controls.hpp"
#include "image_widget.hpp"
#include "joystick.hpp"
#include "dropdown_widget.hpp"
#include "input.hpp"

namespace gui 
{
	using std::placeholders::_1;
	using std::placeholders::_2;

	namespace 
	{
		const std::string dropdown_button_image = "dropdown_button";
	}

	DropdownWidget::DropdownWidget(const DropdownList& list, int width, int height, DropdownType type)
		: list_(list), 
		type_(type), 
		current_selection_(0), 
		dropdown_height_(100)
	{
		setEnvironment();
		setDim(width, height);
		editor_ = new TextEditorWidget(width, height);
		editor_->setOnUserChangeHandler(std::bind(&DropdownWidget::textChange, this));
		editor_->setOnEnterHandler(std::bind(&DropdownWidget::textEnter, this));
		editor_->setOnTabHandler(std::bind(&DropdownWidget::textEnter, this));
		dropdown_image_ = WidgetPtr(new GuiSectionWidget(dropdown_button_image));
		//if(type_ == DROPDOWN_COMBOBOX) {
		//	editor_->setFocus(true);
		//}
		setZOrder(1);

		init();
	}

	DropdownWidget::DropdownWidget(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e), 
		current_selection_(0), 
		dropdown_height_(100)
	{
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
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
		const int dropdown_image_size = std::max(height(), dropdown_image_->height());
		label_ = new Label(list_.size() > 0 ? list_[current_selection_] : "No items");
		label_->setLoc(0, (height() - label_->height()) / 2);
		dropdown_image_->setLoc(width() - height() + (height() - dropdown_image_->width()) / 2, 
			(height() - dropdown_image_->height()) / 2);
		// go on ask me why there is a +20 in the line below.
		// because TextEditorWidget uses a magic -20 when setting the width!
		// The magic +4's are because we want the rectangles drawn around the TextEditorWidget 
		// to match the ones we draw around the dropdown image.
		editor_->setDim(width() - dropdown_image_size + 20 + 4, dropdown_image_size + 4);
		editor_->setLoc(-2, -2);

		if(dropdown_menu_) {
			dropdown_menu_.reset(new Grid(1));
		} else {
			dropdown_menu_ = new Grid(1);
		}
		dropdown_menu_->setLoc(0, height()+2);
		dropdown_menu_->allowSelection(true);
		dropdown_menu_->setShowBackground(true);
		dropdown_menu_->swallowClicks(true);
		dropdown_menu_->setColWidth(0, width());
		dropdown_menu_->setMaxHeight(dropdown_height_);
		dropdown_menu_->setDim(width(), dropdown_height_);
		dropdown_menu_->mustSelect();
		for(const std::string& s : list_) {
			dropdown_menu_->addCol(WidgetPtr(new Label(s, KRE::Color::colorWhite())));
		}
		dropdown_menu_->registerSelectionCallback(std::bind(&DropdownWidget::executeSelection, this, _1));
		dropdown_menu_->setVisible(false);

	}

	void DropdownWidget::setSelection(int selection)
	{
		if(selection >= 0 || size_t(selection) < list_.size()) {
			current_selection_ = selection;
			if(type_ == DropdownType::LIST) {
				label_->setText(list_[current_selection_]);
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
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "dropdown_widget::changeDelegate() called without environment!" << std::endl;
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
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "dropdown_widget::selectDelegate() called without environment!" << std::endl;
		}
	}

	void DropdownWidget::textEnter()
	{
		DropdownList::iterator it = std::find(list_.begin(), list_.end(), editor_->text());
		if(it == list_.end()) {
			current_selection_ = -1;
		} else {
			current_selection_ = it - list_.begin();
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

		if(type_ == DropdownType::LIST) {
			label_->draw(x(), y(), getRotation(), getScale());
		} else {
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
		SDL_Event ev = event;
		switch(ev.type) {
			case SDL_MOUSEMOTION: {
				ev.motion.x -= x() & ~1;
				ev.motion.y -= y() & ~1;
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP: {
				ev.button.x -= x() & ~1;
				ev.button.y -= y() & ~1;
				break;
			}
		}

		if(claimed) {
			return claimed;
		}

		if(type_ == DropdownType::COMBOBOX && editor_) {
			if(editor_->processEvent(ev, claimed)) {
				return true;
			}
		}

		if(dropdown_menu_ && dropdown_menu_->visible()) {
			if(dropdown_menu_->processEvent(ev, claimed)) {
				return true;
			}
		}

		if(hasFocus() && dropdown_menu_) {
			if(event.type == SDL_KEYDOWN 
				&& (ev.key.keysym.sym == controls::get_keycode(controls::CONTROL_ATTACK) 
				|| ev.key.keysym.sym == controls::get_keycode(controls::CONTROL_JUMP))) {
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
		if(pointInRect(p, rect(x(), y(), width()+height(), height()))) {
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
		if(pointInRect(p, rect(x(), y(), width()+height(), height()))) {
			claimed = claimMouseEvents();
		}
		return claimed;
	}

	bool DropdownWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, bool claimed)
	{
		point p;
		int button_state = input::sdl_get_mouse_state(&p.x, &p.y);
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
		//std::cerr << "execute_selection: " << selection << std::endl;
		current_selection_ = selection;
		if(type_ == DropdownType::LIST) {
			label_->setText(list_[current_selection_]);
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

	int DropdownWidget::getMaxHeight() const
	{
		// Maximum height required, including dropdown and borders.
		return height() + (dropdown_menu_ ? dropdown_menu_->height() : dropdown_height_) + 2;
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
		
		DEFINE_FIELD(on_change, "null")
			return variant();
		DEFINE_SET_FIELD_TYPE("callable")
			obj.on_change_ = std::bind(&DropdownWidget::changeDelegate, obj, _1);
			obj.change_handler_ = obj.getEnvironment()->createFormula(value);
		
		DEFINE_FIELD(on_select, "null")
			return variant();
		DEFINE_SET_FIELD_TYPE("callable")
			obj.on_select_ = std::bind(&DropdownWidget::selectDelegate, obj, _1, _2);
			obj.select_handler_ = obj.getEnvironment()->createFormula(value);

		DEFINE_FIELD(item_list, "[string]")
			std::vector<variant> v;
			for(auto& s : obj.list_) {
				v.emplace_back(variant(s));
			}
			return variant(&v);
		DEFINE_SET_FIELD
			obj.list_ = value.as_list_string();
			obj.current_selection_ = 0;
			
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
