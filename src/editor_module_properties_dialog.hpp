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
#ifndef EDITOR_MODULE_PROPERTIES_DIALOG_HPP_INCLUDED
#define EDITOR_MODULE_PROPERTIES_DIALOG_HPP_INCLUDED
#ifndef NO_EDITOR


#include <string>
#include <vector>

#include "dialog.hpp"
#include "module.hpp"
#include "text_editor_widget.hpp"

class editor;

namespace editor_dialogs
{

class editor_module_properties_dialog : public gui::dialog
{
public:
	explicit editor_module_properties_dialog(editor& e);
	explicit editor_module_properties_dialog(editor& e, const std::string& modname);
	void init();
	const std::string on_exit();
private:
	void change_id(const gui::text_editor_widget_ptr editor);
	void change_name(const gui::text_editor_widget_ptr editor);
	void change_prefix(const gui::text_editor_widget_ptr editor);
	void change_module_includes();
	void remove_module_include(const std::string& s);
	void execute_change_module_includes(const std::vector<std::string>& choices, int index);
	void save_module_properties();
	void create_new_module();

	bool new_mod_;
	editor& editor_;
	gui::widget_ptr context_menu_;
	module::modules mod_;
	std::vector<std::string> loaded_mod_;
	std::vector<std::string> dirs_;
};

typedef boost::intrusive_ptr<editor_module_properties_dialog> editor_module_properties_dialog_ptr;

}

#endif // !NO_EDITOR
#endif
