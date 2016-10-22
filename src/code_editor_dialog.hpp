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

#pragma once

#ifndef NO_EDITOR

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

namespace gui 
{
	class code_editor_widget;
	class TextEditorWidget;
	class Button;
}

class CodeEditorDialog : public gui::Dialog
{
public:
	explicit CodeEditorDialog(const rect& r);
	void init();
	void add_optional_error_text_area(const std::string& text);
	bool jump_to_error(const std::string& text);

	void load_file(std::string fname, bool focus=true, std::function<void()>* fn=nullptr);

	bool hasKeyboardFocus() const;

	void process();

	void change_width(int amount);

	void set_close_buttons() { have_close_buttons_ = true; }

	bool has_error() const { return has_error_; }

private:
	void init_files_grid();

	bool handleEvent(const SDL_Event& event, bool claimed) override;
	void handleDrawChildren() const override;

	void undo();
	void redo();
	void changeFontSize(int amount);

	void setAnimationRect(rect r);
	void moveSolidRect(int dx, int dy);
	void setIntegerAttr(const char* attr, int value);

	void save();
	void save_and_close();

	std::string fname_;

	int invalidated_;

	bool has_error_;

	bool modified_;

	bool file_contents_set_;

	gui::CodeEditorWidgetPtr editor_;
	gui::TextEditorWidgetPtr search_;
	gui::TextEditorWidgetPtr replace_;

	gui::Button* find_next_button_;

	gui::TextEditorWidgetPtr optional_error_text_area_;

	gui::LabelPtr replace_label_, status_label_, error_label_;

	gui::GridPtr files_grid_;

	gui::WidgetPtr save_button_;

	void on_tab();

	void on_search_changed();
	void on_search_enter();
	void on_find_next();
	void on_replace_enter();

	void on_code_changed();
	void onMoveCursor();

	void on_drag(int dx, int dy);
	void on_drag_end(int x, int y);

	//As long as there is a code editor active, we are going to want to
	//recover from errors.
	assert_recover_scope assert_recovery_;

	gui::AnimationPreviewWidgetPtr animation_preview_;
	gui::FormulaVisualizeWidgetPtr visualize_widget_;

	struct KnownFile {
		std::string fname;
		ffl::IntrusivePtr<Frame> anim;
		gui::CodeEditorWidgetPtr editor;
		std::function<void()> op_fn;
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

	std::function<void()> op_fn_;
};

typedef ffl::IntrusivePtr<CodeEditorDialog> CodeEditorDialogPtr;

void edit_and_continue_class(const std::string& class_name, const std::string& error);
void edit_and_continue_fn(const std::string& fname, const std::string& error, std::function<void()> fn);

void edit_and_continue_assert(const std::string& msg, std::function<void()> fn=std::function<void()>());

#endif // !NO_EDITOR
