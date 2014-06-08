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
#ifndef CODE_EDITOR_DIALOG_HPP_INCLUDED
#define CODE_EDITOR_DIALOG_HPP_INCLUDED
#ifndef NO_EDITOR

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include <string>

#include "animation_preview_widget.hpp"
#include "asserts.hpp"
#include "code_editor_widget.hpp"
#include "dialog.hpp"
#include "formula_visualize_widget.hpp"
#include "kre/Geometry.hpp"
#include "grid_widget.hpp"
#include "input.hpp"
#include "label.hpp"

namespace gui {
class code_editor_widget;
class TextEditorWidget;
}

class code_editor_dialog : public gui::dialog
{
public:
	explicit code_editor_dialog(const rect& r);
	void init();
	void add_optional_error_text_area(const std::string& text);
	bool jump_to_error(const std::string& text);

	void load_file(std::string fname, bool focus=true, boost::function<void()>* fn=NULL);

	bool has_keyboard_focus() const;

	void process();

	void change_width(int amount);

	void set_close_buttons() { have_close_buttons_ = true; }

	bool has_error() const { return has_error_; }

private:
	void init_files_grid();

	bool handleEvent(const SDL_Event& event, bool claimed);
	void handleDraw_children() const;

	void change_font_size(int amount);

	void set_animation_rect(rect r);
	void move_solid_rect(int dx, int dy);
	void set_integer_attr(const char* attr, int value);

	void save();
	void save_and_close();

	std::string fname_;

	int invalidated_;

	bool has_error_;

	bool modified_;

	bool file_contents_set_;

	gui::code_editor_WidgetPtr editor_;
	gui::TextEditorWidgetPtr search_;
	gui::TextEditorWidgetPtr replace_;

	gui::TextEditorWidgetPtr optional_error_text_area_;

	gui::LabelPtr replace_label_, status_label_, error_label_;

	gui::grid_ptr files_grid_;

	gui::WidgetPtr save_button_;

	void on_tab();

	void on_search_changed();
	void on_search_enter();
	void on_replace_enter();

	void on_code_changed();
	void on_move_cursor();

	void on_drag(int dx, int dy);
	void on_drag_end(int x, int y);

	//As long as there is a code editor active, we are going to want to
	//recover from errors.
	assert_recover_scope assert_recovery_;

	gui::animation_preview_WidgetPtr animation_preview_;
	gui::formula_visualize_WidgetPtr visualize_widget_;

	struct KnownFile {
		std::string fname;
		boost::intrusive_ptr<frame> anim;
		gui::code_editor_WidgetPtr editor;
		boost::function<void()> op_fn;
	};

	std::vector<KnownFile> files_;
	void select_file(int index);

	void select_suggestion(int index);

	struct Suggestion {
		std::string suggestion, suggestion_text, postfix;
		int postfix_index;
		bool operator==(const Suggestion& o) const { return o.suggestion == suggestion && o.postfix == postfix && o.postfix_index == postfix_index; }
		bool operator<(const Suggestion& o) const { return suggestion < o.suggestion; }
	};

	std::vector<Suggestion> suggestions_;
	gui::WidgetPtr suggestions_grid_;
	int suggestions_prefix_;

	bool have_close_buttons_;

	boost::function<void()> op_fn_;
};

typedef boost::intrusive_ptr<code_editor_dialog> code_editor_DialogPtr;

void edit_and_continue_class(const std::string& class_name, const std::string& error);
void edit_and_continue_fn(const std::string& fname, const std::string& error, boost::function<void()> fn);

void edit_and_continue_assert(const std::string& msg, boost::function<void()> fn=boost::function<void()>());

#endif // !NO_EDITOR
#endif
