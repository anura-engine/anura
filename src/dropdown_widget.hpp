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
#pragma once
#ifndef DROPDOWN_WIDGET_HPP_INCLUDED
#define DROPDOWN_WIDGET_HPP_INCLUDED
#ifndef NO_EDITOR

#include <vector>

#include "border_widget.hpp"
#include "label.hpp"
#include "grid_widget.hpp"
#include "text_editor_widget.hpp"
#include "widget.hpp"

namespace gui {

typedef std::vector<std::string> dropdown_list;

class dropdown_widget : public Widget
{
public:
	enum dropdown_type {
		DROPDOWN_LIST,
		DROPDOWN_COMBOBOX,
	};
	dropdown_widget(const dropdown_list& list, int width, int height=0, dropdown_type type=DROPDOWN_LIST);
	dropdown_widget(const variant& v, game_logic::FormulaCallable* e);
	virtual ~dropdown_widget() {}

	void setOnChangeHandler(std::function<void(const std::string&)> fn) { on_change_ = fn; }
	void setOnSelectHandler(std::function<void(int,const std::string&)> fn) { on_select_ = fn; }
	void setSelection(int selection);
	int get_max_height() const;
	void set_dropdown_height(int h);
	void setFontSize(int size) { editor_->setFontSize(size); }
	void setText(const std::string& s) { editor_->setText(s); }
protected:
	virtual void handleDraw() const override;
	virtual bool handleEvent(const SDL_Event& event, bool claimed) override;
	virtual void handleProcess() override;

	virtual void setValue(const std::string& key, const variant& v);
	virtual variant getValue(const std::string& key) const;
	void init();
	void text_enter();
	void textChange();
private:
	bool handleMousedown(const SDL_MouseButtonEvent& event, bool claimed);
	bool handleMouseup(const SDL_MouseButtonEvent& event, bool claimed);
	bool handleMouseMotion(const SDL_MouseMotionEvent& event, bool claimed);
	void execute_selection(int selection);

	int dropdown_height_;
	dropdown_list list_;
	int current_selection_;
	dropdown_type type_;
	TextEditorWidgetPtr editor_;
	grid_ptr dropdown_menu_;
	LabelPtr label_;
	WidgetPtr dropdown_image_;
	std::function<void(const std::string&)> on_change_;
	std::function<void(int, const std::string&)> on_select_;

	// delgate 
	void changeDelegate(const std::string& s);
	void selectDelegate(int selection, const std::string& s);
	// FFL formula
	game_logic::formula_ptr change_handler_;
	game_logic::formula_ptr select_handler_;
};

typedef boost::intrusive_ptr<dropdown_widget> dropdown_WidgetPtr;
typedef boost::intrusive_ptr<const dropdown_widget> const_dropdown_WidgetPtr;

}

#endif // NO_EDITOR
#endif // DROPDOWN_WIDGET_HPP_INCLUDED
