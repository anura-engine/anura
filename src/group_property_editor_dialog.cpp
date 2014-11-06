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
#include <boost/function.hpp>
#include <sstream>

#include "button.hpp"
#include "editor_dialogs.hpp"
#include "foreach.hpp"
#include "grid_widget.hpp"
#include "group_property_editor_dialog.hpp"
#include "image_widget.hpp"
#include "label.hpp"
#include "load_level.hpp"
#include "raster.hpp"

namespace editor_dialogs
{

group_property_editor_dialog::group_property_editor_dialog(editor& e)
  : gui::dialog(graphics::screen_width() - 160, 160, 160, 440), editor_(e)
{
	group_ = e.get_level().editor_selection();
	init();
}

void group_property_editor_dialog::init()
{
	clear();

	using namespace gui;
	set_padding(20);

	if(group_.empty() == false) {
		std::cerr << "ADD BUTTON\n";
		add_widget(widget_ptr(new button(widget_ptr(new label("Group Objects", graphics::color_white())), boost::bind(&group_property_editor_dialog::group_objects, this))), 10, 10);
	}
}

void group_property_editor_dialog::set_group(const std::vector<entity_ptr>& group)
{
	group_ = group;
	init();
}

void group_property_editor_dialog::group_objects()
{
	editor_.group_selection();
}

}
#endif // !NO_EDITOR
