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

#include <iostream>

#include "border_widget.hpp"
#include "button.hpp"
#include "editor.hpp"
#include "foreach.hpp"
#include "grid_widget.hpp"
#include "HexObject.hpp"
#include "hex_tile.hpp"
#include "hex_tileset_editor_dialog.hpp"
#include "image_widget.hpp"
#include "input.hpp"
#include "label.hpp"
#include "preview_tileset_widget.hpp"
#include "raster.hpp"

namespace editor_dialogs
{

namespace {
std::set<hex_tileset_editor_dialog*>& all_tileset_editor_dialogs() {
	static std::set<hex_tileset_editor_dialog*> all;
	return all;
}
}

void hex_tileset_editor_dialog::global_tile_update()
{
	foreach(hex_tileset_editor_dialog* d, all_tileset_editor_dialogs()) {
		d->init();
	}
}

hex_tileset_editor_dialog::hex_tileset_editor_dialog(editor& e)
  : dialog(graphics::screen_width() - EDITOR_SIDEBAR_WIDTH, 160, EDITOR_SIDEBAR_WIDTH, 440), editor_(e), first_index_(-1)
{
	all_tileset_editor_dialogs().insert(this);

	set_clear_bg_amount(255);
	if(hex::HexObject::get_editor_tiles().empty() == false) {
		category_ = hex::HexObject::get_editor_tiles().front()->getgetEditorInfo().group;
	}

	init();
}

hex_tileset_editor_dialog::~hex_tileset_editor_dialog()
{
	all_tileset_editor_dialogs().erase(this);
}

void hex_tileset_editor_dialog::init()
{
	clear();
	using namespace gui;
	setPadding(20);

	ASSERT_LOG(editor_.get_hex_tileset() >= 0 
		&& size_t(editor_.get_hex_tileset()) < hex::HexObject::get_editor_tiles().size(),
		"Index of hex tileset out of bounds must be between 0 and " 
		<< hex::HexObject::get_editor_tiles().size() << ", found " << editor_.get_hex_tileset());

	button* category_button = new button(WidgetPtr(new label(category_, graphics::color_white())), std::bind(&hex_tileset_editor_dialog::show_category_menu, this));
	addWidget(WidgetPtr(category_button), 10, 10);

	grid_ptr grid(new gui::grid(3));
	int index = 0, first_index = -1;
	first_index_ = -1;
	
	foreach(const hex::TileTypePtr& t, hex::HexObject::get_editor_tiles()) {
		if(t->getgetEditorInfo().group == category_) {
			if(first_index_ == -1) {
				first_index_ = index;
			}
			ImageWidget* preview = new ImageWidget(t->getgetEditorInfo().texture, 54, 54);
			preview->setArea(t->getgetEditorInfo().image_rect);
			ButtonPtr tileset_button(new button(WidgetPtr(preview), std::bind(&hex_tileset_editor_dialog::set_tileset, this, index)));
			tileset_button->setTooltip(t->id() + "/" + t->getgetEditorInfo().name, 14);
			tileset_button->setDim(58, 58);
			grid->add_col(gui::WidgetPtr(new gui::BorderWidget(tileset_button, index == editor_.get_hex_tileset() ? graphics::color(255,255,255,255) : graphics::color(0,0,0,0))));
		}
		++index;
	}

	grid->finish_row();
	addWidget(grid);
}

void hex_tileset_editor_dialog::select_category(const std::string& category)
{
	category_ = category;
	init();

	if(first_index_ != -1) {
		set_tileset(first_index_);
	}
}

void hex_tileset_editor_dialog::close_context_menu(int index)
{
	removeWidget(context_menu_);
	context_menu_.reset();
}

void hex_tileset_editor_dialog::show_category_menu()
{
	using namespace gui;
	gui::grid* grid = new gui::grid(2);
	grid->swallow_clicks();
	grid->set_show_background(true);
	grid->set_hpad(10);
	grid->allow_selection();
	grid->register_selection_callback(std::bind(&hex_tileset_editor_dialog::close_context_menu, this, _1));

	std::set<std::string> categories;
	foreach(const hex::TileTypePtr& t, hex::HexObject::get_hex_tiles()) {
		if(categories.count(t->getgetEditorInfo().group)) {
			continue;
		}

		categories.insert(t->getgetEditorInfo().group);

		ImageWidget* preview = new ImageWidget(t->getgetEditorInfo().texture, 54, 54);
		preview->setArea(t->getgetEditorInfo().image_rect);
		grid->add_col(WidgetPtr(preview))
		     .add_col(WidgetPtr(new label(t->getgetEditorInfo().group, graphics::color_white())));
		grid->register_row_selection_callback(std::bind(&hex_tileset_editor_dialog::select_category, this, t->getgetEditorInfo().group));
	}

	int mousex, mousey;
	input::sdl_get_mouse_state(&mousex, &mousey);
	if(mousex + grid->width() > graphics::screen_width()) {
		mousex = graphics::screen_width() - grid->width();
	}

	if(mousey + grid->height() > graphics::screen_height()) {
		mousey = graphics::screen_height() - grid->height();
	}

	mousex -= x();
	mousey -= y();

	removeWidget(context_menu_);
	context_menu_.reset(grid);
	addWidget(context_menu_, mousex, mousey);
}

void hex_tileset_editor_dialog::set_tileset(int index)
{
	if(editor_.get_hex_tileset() != index) {
		editor_.set_hex_tileset(index);
		init();
	}
}

bool hex_tileset_editor_dialog::handleEvent(const SDL_Event& event, bool claimed)
{
	if(!claimed) {
		if(context_menu_) {
			gui::WidgetPtr ptr = context_menu_;
			SDL_Event ev = event;
			normalizeEvent(&ev);
			return ptr->processEvent(ev, claimed);
		}

		switch(event.type) {
		case SDL_KEYDOWN:
			if(event.key.keysym.sym == SDLK_COMMA) {
				editor_.set_hex_tileset(editor_.get_hex_tileset()-1);
				while(hex::HexObject::get_hex_tiles()[editor_.get_hex_tileset()]->getgetEditorInfo().group != category_) {
					editor_.set_hex_tileset(editor_.get_hex_tileset()-1);
				}
				set_tileset(editor_.get_hex_tileset());
				claimed = true;
			} else if(event.key.keysym.sym == SDLK_PERIOD) {
				editor_.set_hex_tileset(editor_.get_hex_tileset()+1);
				while(hex::HexObject::get_hex_tiles()[editor_.get_hex_tileset()]->getgetEditorInfo().group != category_) {
					editor_.set_hex_tileset(editor_.get_hex_tileset()+1);
				}
				set_tileset(editor_.get_hex_tileset());
				claimed = true;
			}
			break;
		}
	}

	return dialog::handleEvent(event, claimed);
}

}

#endif // !NO_EDITOR
