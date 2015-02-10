/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <iostream>

#include "WindowManager.hpp"

#include "border_widget.hpp"
#include "button.hpp"
#include "editor.hpp"
#include "grid_widget.hpp"
#include "hex_object.hpp"
#include "hex_tile.hpp"
#include "hex_tileset_editor_dialog.hpp"
#include "image_widget.hpp"
#include "input.hpp"
#include "label.hpp"

namespace editor_dialogs
{
	using std::placeholders::_1;

	namespace 
	{
		std::set<HexTilesetEditorDialog*>& all_tileset_editor_dialogs() 
		{
			static std::set<HexTilesetEditorDialog*> all;
			return all;
		}
	}

	void HexTilesetEditorDialog::globalTileUpdate()
	{
		for(HexTilesetEditorDialog* d : all_tileset_editor_dialogs()) {
			d->init();
		}
	}

	HexTilesetEditorDialog::HexTilesetEditorDialog(editor& e)
	  : Dialog(KRE::WindowManager::getMainWindow()->width() - EDITOR_SIDEBAR_WIDTH, 160, EDITOR_SIDEBAR_WIDTH, 440), 
	  editor_(e), 
	  first_index_(-1)
	{
		all_tileset_editor_dialogs().insert(this);

		setClearBgAmount(255);
		if(hex::HexObject::getEditorTiles().empty() == false) {
			category_ = hex::HexObject::getEditorTiles().front()->getgetEditorInfo().group;
		}

		init();
	}

	HexTilesetEditorDialog::~HexTilesetEditorDialog()
	{
		all_tileset_editor_dialogs().erase(this);
	}

	void HexTilesetEditorDialog::init()
	{
		clear();
		using namespace gui;
		setPadding(20);

		ASSERT_LOG(editor_.get_hex_tileset() >= 0 
			&& size_t(editor_.get_hex_tileset()) < hex::HexObject::getEditorTiles().size(),
			"Index of hex tileset out of bounds must be between 0 and " 
			<< hex::HexObject::getEditorTiles().size() << ", found " << editor_.get_hex_tileset());

		Button* category_button = new Button(WidgetPtr(new Label(category_, KRE::Color::colorWhite())), std::bind(&HexTilesetEditorDialog::showCategoryMenu, this));
		addWidget(WidgetPtr(category_button), 10, 10);

		GridPtr grid(new gui::Grid(3));
		int index = 0, first_index = -1;
		first_index_ = -1;
	
		for(const hex::TileTypePtr& t : hex::HexObject::getEditorTiles()) {
			if(t->getgetEditorInfo().group == category_) {
				if(first_index_ == -1) {
					first_index_ = index;
				}
				ImageWidget* preview = new ImageWidget(t->getgetEditorInfo().texture, 54, 54);
				preview->setArea(t->getgetEditorInfo().image_rect);
				ButtonPtr tileset_button(new Button(WidgetPtr(preview), std::bind(&HexTilesetEditorDialog::setTileset, this, index)));
				tileset_button->setTooltip(t->id() + "/" + t->getgetEditorInfo().name, 14);
				tileset_button->setDim(58, 58);
				grid->addCol(gui::WidgetPtr(new gui::BorderWidget(tileset_button, index == editor_.get_hex_tileset() ? KRE::Color::colorWhite() : KRE::Color(0,0,0,0))));
			}
			++index;
		}

		grid->finishRow();
		addWidget(grid);
	}

	void HexTilesetEditorDialog::selectCategory(const std::string& category)
	{
		category_ = category;
		init();

		if(first_index_ != -1) {
			setTileset(first_index_);
		}
	}

	void HexTilesetEditorDialog::closeContextMenu(int index)
	{
		removeWidget(context_menu_);
		context_menu_.reset();
	}

	void HexTilesetEditorDialog::showCategoryMenu()
	{
		using namespace gui;
		Grid* grid = new Grid(2);
		grid->swallowClicks();
		grid->setShowBackground(true);
		grid->setHpad(10);
		grid->allowSelection();
		grid->registerSelectionCallback(std::bind(&HexTilesetEditorDialog::closeContextMenu, this, _1));

		std::set<std::string> categories;
		for(const hex::TileTypePtr& t : hex::HexObject::getHexTiles()) {
			if(categories.count(t->getgetEditorInfo().group)) {
				continue;
			}

			categories.insert(t->getgetEditorInfo().group);

			ImageWidget* preview = new ImageWidget(t->getgetEditorInfo().texture, 54, 54);
			preview->setArea(t->getgetEditorInfo().image_rect);
			grid->addCol(WidgetPtr(preview))
				 .addCol(WidgetPtr(new Label(t->getgetEditorInfo().group, KRE::Color::colorWhite())));
			grid->registerRowSelectionCallback(std::bind(&HexTilesetEditorDialog::selectCategory, this, t->getgetEditorInfo().group));
		}

		int mousex, mousey;
		input::sdl_get_mouse_state(&mousex, &mousey);
		if(static_cast<unsigned>(mousex + grid->width()) > KRE::WindowManager::getMainWindow()->width()) {
			mousex = KRE::WindowManager::getMainWindow()->width() - grid->width();
		}

		if(static_cast<unsigned>(mousey + grid->height()) > KRE::WindowManager::getMainWindow()->height()) {
			mousey = KRE::WindowManager::getMainWindow()->height() - grid->height();
		}

		mousex -= x();
		mousey -= y();

		removeWidget(context_menu_);
		context_menu_.reset(grid);
		addWidget(context_menu_, mousex, mousey);
	}

	void HexTilesetEditorDialog::setTileset(int index)
	{
		if(editor_.get_hex_tileset() != index) {
			editor_.set_hex_tileset(index);
			init();
		}
	}

	bool HexTilesetEditorDialog::handleEvent(const SDL_Event& event, bool claimed)
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
					while(hex::HexObject::getHexTiles()[editor_.get_hex_tileset()]->getgetEditorInfo().group != category_) {
						editor_.set_hex_tileset(editor_.get_hex_tileset()-1);
					}
					setTileset(editor_.get_hex_tileset());
					claimed = true;
				} else if(event.key.keysym.sym == SDLK_PERIOD) {
					editor_.set_hex_tileset(editor_.get_hex_tileset()+1);
					while(hex::HexObject::getHexTiles()[editor_.get_hex_tileset()]->getgetEditorInfo().group != category_) {
						editor_.set_hex_tileset(editor_.get_hex_tileset()+1);
					}
					setTileset(editor_.get_hex_tileset());
					claimed = true;
				}
				break;
			}
		}

		return Dialog::handleEvent(event, claimed);
	}
}

#endif // !NO_EDITOR
