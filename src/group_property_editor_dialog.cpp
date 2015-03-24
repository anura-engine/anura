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

#ifndef NO_EDITOR
#include <sstream>

#include "WindowManager.hpp"

#include "button.hpp"
#include "editor.hpp"
#include "editor_dialogs.hpp"
#include "entity.hpp"
#include "group_property_editor_dialog.hpp"
#include "label.hpp"

namespace editor_dialogs
{
	GroupPropertyEditorDialog::GroupPropertyEditorDialog(editor& e)
	  : gui::Dialog(KRE::WindowManager::getMainWindow()->width() - 160, 160, 160, 440), 
	  editor_(e)
	{
		group_ = e.get_level().editor_selection();
		init();
	}

	void GroupPropertyEditorDialog::init()
	{
		clear();

		using namespace gui;
		setPadding(20);

		if(group_.empty() == false) {
			addWidget(WidgetPtr(new Button(WidgetPtr(new Label("Group Objects", KRE::Color::colorWhite())), std::bind(&GroupPropertyEditorDialog::groupObjects, this))), 10, 10);
		}
	}

	void GroupPropertyEditorDialog::setGroup(const std::vector<EntityPtr>& group)
	{
		group_ = group;
		init();
	}

	void GroupPropertyEditorDialog::groupObjects()
	{
		editor_.group_selection();
	}
}

#endif // !NO_EDITOR
