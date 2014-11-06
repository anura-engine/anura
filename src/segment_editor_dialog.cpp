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
#ifndef NO_EDITOR
#include <boost/bind.hpp>

#include "border_widget.hpp"
#include "button.hpp"
#include "editor.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "frame.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "label.hpp"
#include "raster.hpp"
#include "segment_editor_dialog.hpp"

namespace editor_dialogs
{

segment_editor_dialog::segment_editor_dialog(editor& e)
  : gui::dialog(graphics::screen_width() - 160, 160, 160, 440),
    editor_(e), segment_(-1)
{}

void segment_editor_dialog::init()
{
	clear();
	using namespace gui;
	set_padding(20);

	if(segment_ < 0) {
		return;
	}

	variant start = editor_.get_level().get_var(formatter() << "segment_difficulty_start_" << segment_);
	const int start_value = start.as_int();

	add_widget(widget_ptr(new label(formatter() << "Difficulty: " << start_value, graphics::color_white())), 5, 5);

				grid_ptr buttons_grid(new grid(4));
				buttons_grid->add_col(widget_ptr(new button(widget_ptr(new label("-10", graphics::color_white())), boost::bind(&segment_editor_dialog::set_segment_start_difficulty, this, start_value - 10))));
				buttons_grid->add_col(widget_ptr(new button(widget_ptr(new label("-1", graphics::color_white())), boost::bind(&segment_editor_dialog::set_segment_start_difficulty, this,  start_value - 1))));
				buttons_grid->add_col(widget_ptr(new button(widget_ptr(new label("+1", graphics::color_white())), boost::bind(&segment_editor_dialog::set_segment_start_difficulty, this,  start_value + 1))));
				buttons_grid->add_col(widget_ptr(new button(widget_ptr(new label("+10", graphics::color_white())), boost::bind(&segment_editor_dialog::set_segment_start_difficulty, this,  start_value + 10))));
				add_widget(widget_ptr(buttons_grid));

}

void segment_editor_dialog::set_segment(int num)
{
	segment_ = num;
	init();
}

void segment_editor_dialog::set_segment_start_difficulty(int value)
{
	editor_.get_level().set_var(formatter() << "segment_difficulty_start_" << segment_, variant(value));
	init();
}

}
#endif // !NO_EDITOR

