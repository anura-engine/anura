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
#pragma once
#ifndef CUSTOM_OBJECT_DIALOG_HPP_INCLUDED

#include <boost/function.hpp>

#include "custom_object.hpp"
#include "custom_object_type.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "label.hpp"
#include "module.hpp"
#include "variant.hpp"
#include "scrollable_widget.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"

namespace editor_dialogs {

class custom_object_dialog : public gui::dialog
{
public:
	explicit custom_object_dialog(editor& e, int x, int y, int w, int h);
	void init();
	variant get_object() const { return object_template_; }
	void show_modal();
protected:
	void change_text_attribute(const gui::text_editor_widget_ptr editor, const std::string& s);
	void change_int_attribute_text(const gui::text_editor_widget_ptr editor, const std::string& s, gui::slider_ptr slide);
	void change_int_attribute_slider(const gui::text_editor_widget_ptr editor, const std::string& s, double d);
	void slider_drag_end(const gui::text_editor_widget_ptr editor, const std::string& s, gui::slider_ptr slide, double d);
	void change_template(int selection, const std::string& s);
	void change_prototype();
	void remove_prototype(const std::string& s);
	void execute_change_prototype(const std::vector<std::string>& choices, size_t index);
	void on_create();
	void id_change_focus(bool);

	void on_set_path();
	
	void on_edit_animations();
	void on_edit_items(const std::string& name, const std::string& attr, bool allow_functions);

	std::vector<gui::widget_ptr> get_widget_for_attribute(const std::string& attr);
private:
	module::module_file_pair template_file_;
	variant object_template_;
	custom_object_type_ptr object_;
	int selected_template_;
	std::string current_object_save_path_;

	gui::widget_ptr context_menu_;

	std::string error_text_;

	std::string image_file_, image_file_name_, rel_path_;

	//std::map<std::string, double> slider_magnitude_;
	std::map<std::string, int> slider_offset_;
	std::vector<std::string> prototypes_;	// cached prototypes.
	bool dragging_slider_;
};

}

#endif // CUSTOM_OBJECT_DIALOG_HPP_INCLUDED

