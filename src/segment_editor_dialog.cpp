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

#include "WindowManager.hpp"

#include "border_widget.hpp"
#include "button.hpp"
#include "editor.hpp"
#include "formatter.hpp"
#include "frame.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "label.hpp"
#include "segment_editor_dialog.hpp"

namespace editor_dialogs
{
	SegmentEditorDialog::SegmentEditorDialog(editor& e)
		: gui::Dialog(KRE::WindowManager::getMainWindow()->width() - 160, 160, 160, 440),
		editor_(e),
		segment_(-1)
	{
	}

	void SegmentEditorDialog::init()
	{
		clear();
		using namespace gui;
		setPadding(20);

		if(segment_ < 0) {
			return;
		}

		variant start = editor_.get_level().get_var(formatter() << "segment_difficulty_start_" << segment_);
		const int start_value = start.as_int();

		addWidget(WidgetPtr(new Label(formatter() << "Difficulty: " << start_value, KRE::Color::colorWhite())), 5, 5);

		GridPtr buttons_grid(new Grid(4));
		buttons_grid->addCol(WidgetPtr(new Button(WidgetPtr(new Label("-10", KRE::Color::colorWhite())), std::bind(&SegmentEditorDialog::setSegmentStartDifficulty, this, start_value - 10))));
		buttons_grid->addCol(WidgetPtr(new Button(WidgetPtr(new Label("-1", KRE::Color::colorWhite())), std::bind(&SegmentEditorDialog::setSegmentStartDifficulty, this,  start_value - 1))));
		buttons_grid->addCol(WidgetPtr(new Button(WidgetPtr(new Label("+1", KRE::Color::colorWhite())), std::bind(&SegmentEditorDialog::setSegmentStartDifficulty, this,  start_value + 1))));
		buttons_grid->addCol(WidgetPtr(new Button(WidgetPtr(new Label("+10", KRE::Color::colorWhite())), std::bind(&SegmentEditorDialog::setSegmentStartDifficulty, this,  start_value + 10))));
		addWidget(WidgetPtr(buttons_grid));
	}

	void SegmentEditorDialog::setSegment(int num)
	{
		segment_ = num;
		init();
	}

	void SegmentEditorDialog::setSegmentStartDifficulty(int value)
	{
		editor_.get_level().set_var(formatter() << "segment_difficulty_start_" << segment_, variant(value));
		init();
	}
}

#endif // !NO_EDITOR

