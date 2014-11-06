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
#include "geometry.hpp"
#include "grid_widget.hpp"
#include "input.hpp"
#include "label.hpp"

namespace gui {
class code_editor_widget;
class text_editor_widget;
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

	bool handle_event(const SDL_Event& event, bool claimed);
	void handle_draw_children() const;

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

	gui::code_editor_widget_ptr editor_;
	gui::text_editor_widget_ptr search_;
	gui::text_editor_widget_ptr replace_;

	gui::text_editor_widget_ptr optional_error_text_area_;

	gui::label_ptr replace_label_, status_label_, error_label_;

	gui::grid_ptr files_grid_;

	gui::widget_ptr save_button_;

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

	gui::animation_preview_widget_ptr animation_preview_;
	gui::formula_visualize_widget_ptr visualize_widget_;

	struct KnownFile {
		std::string fname;
		boost::intrusive_ptr<frame> anim;
		gui::code_editor_widget_ptr editor;
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
	gui::widget_ptr suggestions_grid_;
	int suggestions_prefix_;

	bool have_close_buttons_;

	boost::function<void()> op_fn_;
};

typedef boost::intrusive_ptr<code_editor_dialog> code_editor_dialog_ptr;

void edit_and_continue_class(const std::string& class_name, const std::string& error);
void edit_and_continue_fn(const std::string& fname, const std::string& error, boost::function<void()> fn);

void edit_and_continue_assert(const std::string& msg, boost::function<void()> fn=boost::function<void()>());

#endif // !NO_EDITOR
#endif
