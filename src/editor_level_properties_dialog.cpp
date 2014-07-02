/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef NO_EDITOR
#include "graphics.hpp"

#include <algorithm>
#include <iostream>

#include "background.hpp"
#include "button.hpp"
#include "checkbox.hpp"
#include "editor.hpp"
#include "editor_dialogs.hpp"
#include "editor_level_properties_dialog.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "grid_widget.hpp"
#include "input.hpp"
#include "label.hpp"
#include "raster.hpp"
#include "stats.hpp"
#include "text_editor_widget.hpp"

namespace editor_dialogs
{

namespace {

void set_segmented_level_width(editor_level_properties_dialog* d, editor* e, bool value)
{
	foreach(LevelPtr lvl, e->get_level_list()) {
		if(value) {
			//make sure the segment width is divisible by the tile size.
			int width = lvl->boundaries().w();
			while(width%32) {
				++width;
			}
			lvl->set_segment_width(width);
			lvl->set_boundaries(rect(lvl->boundaries().x(), lvl->boundaries().y(),
			                         width, lvl->boundaries().h()));
		} else {
			lvl->set_segment_width(0);
		}
	}

	d->init();
}

void set_segmented_level_height(editor_level_properties_dialog* d, editor* e, bool value)
{
	foreach(LevelPtr lvl, e->get_level_list()) {
		if(value) {
			//make sure the segment height is divisible by the tile size.
			int height = lvl->boundaries().h();
			while(height%32) {
				++height;
			}
			lvl->set_segment_height(height);
			lvl->set_boundaries(rect(lvl->boundaries().x(), lvl->boundaries().y(),
			                         lvl->boundaries().w(), height));
		} else {
			lvl->set_segment_height(0);
		}
	}
	
	d->init();
}

}

editor_level_properties_dialog::editor_level_properties_dialog(editor& e)
  : dialog(preferences::virtual_screen_width()/2 - 300, preferences::virtual_screen_height()/2 - 220, 600, 440), editor_(e)
{
	set_clear_bg_amount(255);
	init();
}

void editor_level_properties_dialog::init()
{
	set_clear_bg_amount(255);
	set_background_frame("empty_window");
	set_draw_background_fn([]() {
		draw_scene(level::current(), last_draw_position());
	});

	using namespace gui;
	clear();

	addWidget(WidgetPtr(new label("Level Properties", graphics::color_white(), 48)), 10, 10);

	TextEditorWidget* change_title_entry(new TextEditorWidget(200, 30));
	change_title_entry->setText(editor_.get_level().title());
	change_title_entry->setOnChangeHandler(std::bind(&editor_level_properties_dialog::change_title, this, change_title_entry));
	change_title_entry->setOnEnterHandler(std::bind(&dialog::close, this));

	grid_ptr g(new grid(2));
	g->add_col(WidgetPtr(new label("Change Title", graphics::color_white(), 36)))
	  .add_col(WidgetPtr(change_title_entry));

	addWidget(g);

	std::string background_id = editor_.get_level().get_background_id();
	if(background_id.empty()) {
		background_id = "(no background)";
	}
	g.reset(new grid(2));
	g->add_col(WidgetPtr(new label("Background", graphics::color_white())))
	  .add_col(WidgetPtr(new button(WidgetPtr(new label(background_id, graphics::color_white())), std::bind(&editor_level_properties_dialog::change_background, this))));
	addWidget(g);

	g.reset(new grid(3));
	g->set_hpad(10);
	g->add_col(WidgetPtr(new label("Next Level", graphics::color_white())));
	g->add_col(WidgetPtr(new label(editor_.get_level().next_level(), graphics::color_white())));
	g->add_col(WidgetPtr(new button(WidgetPtr(new label("Set", graphics::color_white())), std::bind(&editor_level_properties_dialog::change_next_level, this))));

	g->add_col(WidgetPtr(new label("Previous Level", graphics::color_white())));
	g->add_col(WidgetPtr(new label(editor_.get_level().previous_level(), graphics::color_white())));
	g->add_col(WidgetPtr(new button(WidgetPtr(new label("Set", graphics::color_white())), std::bind(&editor_level_properties_dialog::change_previous_level, this))));
	addWidget(g);

	Checkbox* hz_segmented_checkbox = new Checkbox("Horizontally Segmented Level", editor_.get_level().segment_width() != 0, std::bind(set_segmented_level_width, this, &editor_, _1));
	WidgetPtr hz_checkbox(hz_segmented_checkbox);
	addWidget(hz_checkbox);

	Checkbox* vt_segmented_checkbox = new Checkbox("Vertically Segmented Level", editor_.get_level().segment_height() != 0, std::bind(set_segmented_level_height, this, &editor_, _1));
	WidgetPtr vt_checkbox(vt_segmented_checkbox);
	addWidget(vt_checkbox);

	if(editor_.get_level().segment_height() != 0) {
		removeWidget(hz_checkbox);
	}

	if(editor_.get_level().segment_width() != 0) {
		removeWidget(vt_checkbox);
	}

	add_ok_and_cancel_buttons();
}

void editor_level_properties_dialog::change_title(const gui::TextEditorWidgetPtr editor)
{
	std::string title = editor->text();

	foreach(LevelPtr lvl, editor_.get_level_list()) {
		lvl->set_title(title);
	}
}

void editor_level_properties_dialog::change_background()
{
	using namespace gui;
	std::vector<std::string> backgrounds = background::get_available_backgrounds();
	if(backgrounds.empty()) {
		return;
	}

	std::sort(backgrounds.begin(), backgrounds.end());

	gui::grid* grid = new gui::grid(1);
	grid->setZOrder(100);
	grid->set_hpad(40);
	grid->set_show_background(true);
	grid->allow_selection();
	grid->swallow_clicks();
	grid->register_selection_callback(std::bind(&editor_level_properties_dialog::execute_change_background, this, backgrounds, _1));
	foreach(const std::string& bg, backgrounds) {
		grid->add_col(WidgetPtr(new label(bg, graphics::color_white())));
	}

	int mousex, mousey;
	input::sdl_get_mouse_state(&mousex, &mousey);

	mousex -= x();
	mousey -= y();

	removeWidget(context_menu_);
	context_menu_.reset(grid);
	addWidget(context_menu_, mousex, mousey);
}

void editor_level_properties_dialog::execute_change_background(const std::vector<std::string>& choices, int index)
{
	if(context_menu_) {
		removeWidget(context_menu_);
		context_menu_.reset();
	}

	if(index < 0 || index >= choices.size()) {
		return;
	}

	foreach(LevelPtr lvl, editor_.get_level_list()) {
		lvl->set_background_by_id(choices[index]);
	}

	init();
}

void editor_level_properties_dialog::change_next_level()
{
	std::string result = show_choose_level_dialog("Next Level");
	if(result.empty() == false) {
		foreach(LevelPtr lvl, editor_.get_level_list()) {
			lvl->set_next_level(result);
		}
	}

	init();
}

void editor_level_properties_dialog::change_previous_level()
{
	std::string result = show_choose_level_dialog("Previous Level");
	if(result.empty() == false) {
		foreach(LevelPtr lvl, editor_.get_level_list()) {
			lvl->set_previous_level(result);
		}
	}

	init();
}

}
#endif // !NO_EDITOR

