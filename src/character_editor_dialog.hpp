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

#include <map>
#include <string>

#include "dialog.hpp"
#include "text_editor_widget.hpp"
#include "widget.hpp"

class editor;

namespace gui 
{
	class BorderWidget;
}

namespace editor_dialogs
{
	//editor dialog which displays the details of an object and allows editing it.
	class CharacterEditorDialog : public gui::Dialog
	{
	public:
		explicit CharacterEditorDialog(editor& e);
		void init();
		void set_character(int index);
		void select_category(const std::string& str);
	private:
		void show_category_menu();

		void close_context_menu(int index);
		editor& editor_;
		std::string category_;
		gui::WidgetPtr context_menu_;

		gui::WidgetPtr generate_grid(const std::string& category);
		std::map<std::string, gui::WidgetPtr> grids_;

		//the borders around each object, we set the currently selected one
		//to white, and all the others to transparent.
		std::map<std::string, std::vector<gui::BorderWidgetPtr> > grid_borders_;

		//the first object in each category
		std::map<std::string, int> first_obj_;

		ffl::IntrusivePtr<gui::TextEditorWidget> find_edit_;
	};

	typedef ffl::IntrusivePtr<CharacterEditorDialog> CharacterEditorDialogPtr;
}

#endif // NO_EDITOR
