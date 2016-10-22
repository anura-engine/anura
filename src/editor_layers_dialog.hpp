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

#include <vector>

#include "dialog.hpp"
#include "editor.hpp"
#include "image_widget.hpp"

namespace editor_dialogs
{
	static const int LAYERS_DIALOG_WIDTH = 40;

	class EditorLayersDialog : public gui::Dialog
	{
	public:
		explicit EditorLayersDialog(editor& e);
		void init();
		void process();
	private:
		void rowSelected(int nrow);
		void rowMouseover(int nrow);
		editor& editor_;

		struct row_data {
			gui::GuiSectionWidgetPtr Checkbox;
			int layer;
			bool hidden;
		};

		std::vector<row_data> rows_;

		bool locked_;

		std::set<int> before_locked_state_;

		void findClassifications();
		std::set<std::string> all_classifications_;

		void classificationSelected(int index);
	};

	typedef ffl::IntrusivePtr<EditorLayersDialog> EditorLayersDialogPtr;
}

#endif // !NO_EDITOR
