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
#include <vector>

#include "dialog.hpp"
#include "module.hpp"
#include "text_editor_widget.hpp"

class editor;

namespace editor_dialogs
{
	class EditorModulePropertiesDialog : public gui::Dialog
	{
	public:
		explicit EditorModulePropertiesDialog(editor& e);
		explicit EditorModulePropertiesDialog(editor& e, const std::string& modname);
		void init();
		const std::string onExit();
	private:
		void changeId(const gui::TextEditorWidgetPtr editor);
		void changeName(const gui::TextEditorWidgetPtr editor);
		void changePrefix(const gui::TextEditorWidgetPtr editor);
		void changeModuleIncludes();
		void removeModuleInclude(const std::string& s);
		void executeChangeModuleIncludes(const std::vector<std::string>& choices, int index);
		void saveModuleProperties();
		void createNewModule();

		bool new_mod_;
		editor& editor_;
		gui::WidgetPtr context_menu_;
		module::modules mod_;
		std::vector<std::string> loaded_mod_;
		std::vector<std::string> dirs_;
	};

	typedef ffl::IntrusivePtr<EditorModulePropertiesDialog> EditorModulePropertiesDialogPtr;
}

#endif // !NO_EDITOR
