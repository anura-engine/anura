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
#include <boost/intrusive_ptr.hpp>
#include <boost/function.hpp>

#include "border_widget.hpp"
#include "label.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "text_editor_widget.hpp"
#include "widget.hpp"

namespace gui {

typedef std::vector<std::string> dropdown_list;

class dropdown_widget : public widget
{
public:
	enum dropdown_type {
		DROPDOWN_LIST,
		DROPDOWN_COMBOBOX,
	};
	dropdown_widget(const dropdown_list& list, int width, int height=0, dropdown_type type=DROPDOWN_LIST);
	dropdown_widget(const variant& v, game_logic::formula_callable* e);
	virtual ~dropdown_widget() {}

	void set_on_change_handler(boost::function<void(const std::string&)> fn) { on_change_ = fn; }
	void set_on_select_handler(boost::function<void(int,const std::string&)> fn) { on_select_ = fn; }
	void set_selection(int selection);
	int get_max_height() const;
	void set_dropdown_height(int h);
	void set_font_size(int size) { editor_->set_font_size(size); }
	void set_text(const std::string& s) { editor_->set_text(s); }
protected:
	virtual void handle_draw() const;
	virtual bool handle_event(const SDL_Event& event, bool claimed);
	virtual void handle_process();

	void init();
	void text_enter();
	void text_change();

	DECLARE_CALLABLE(dropdown_widget);
private:
	bool handle_mousedown(const SDL_MouseButtonEvent& event, bool claimed);
	bool handle_mouseup(const SDL_MouseButtonEvent& event, bool claimed);
	bool handle_mousemotion(const SDL_MouseMotionEvent& event, bool claimed);
	void execute_selection(int selection);
	void mouseover_item(int selection);

	int dropdown_height_;
	dropdown_list list_;
	int current_selection_;
	dropdown_type type_;
	text_editor_widget_ptr editor_;
	grid_ptr dropdown_menu_;
	std::vector<label_ptr> labels_;
	label_ptr label_;
	gui_section_widget_ptr dropdown_image_;
	std::string normal_image_, focus_image_;
	std::string font_;
	boost::function<void(const std::string&)> on_change_;
	boost::function<void(int, const std::string&)> on_select_;

	// delgate 
	void change_delegate(const std::string& s);
	void select_delegate(int selection, const std::string& s);

	void set_color_scheme(const variant& v);

	// FFL formula
	game_logic::formula_ptr change_handler_;
	game_logic::formula_ptr select_handler_;

	boost::scoped_ptr<graphics::color> normal_color_, depressed_color_, focus_color_;
	boost::scoped_ptr<graphics::color> text_normal_color_, text_depressed_color_, text_focus_color_;

	bool in_widget_;
};

typedef boost::intrusive_ptr<dropdown_widget> dropdown_widget_ptr;
typedef boost::intrusive_ptr<const dropdown_widget> const_dropdown_widget_ptr;

}

#endif // NO_EDITOR
#endif // DROPDOWN_WIDGET_HPP_INCLUDED
