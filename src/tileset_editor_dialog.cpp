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
#include "grid_widget.hpp"
#include "input.hpp"
#include "label.hpp"
#include "preview_tileset_widget.hpp"
#include "tileset_editor_dialog.hpp"

PREF_INT(editor_tileset_button_size, 44, "Size of tileset editing buttons in the editor");

namespace editor_dialogs
{
	namespace 
	{
		std::set<TilesetEditorDialog*>& all_tileset_editor_dialogs() {
			static std::set<TilesetEditorDialog*>* all = new std::set<TilesetEditorDialog*>;
			return *all;
		}
	}

	void TilesetEditorDialog::globalTileUpdate()
	{
		for(TilesetEditorDialog* d : all_tileset_editor_dialogs()) {
			d->init();
		}
	}

	TilesetEditorDialog::TilesetEditorDialog(editor& e)
	  : Dialog(KRE::WindowManager::getMainWindow()->width() - EDITOR_SIDEBAR_WIDTH, 160, EDITOR_SIDEBAR_WIDTH, 440), 
	    editor_(e), 
		first_index_(-1)
	{
		all_tileset_editor_dialogs().insert(this);

		setClearBgAmount(255);
		if(editor_.all_tilesets().empty() == false) {
			category_ = editor_.all_tilesets().front().category;
		}

		init();
	}

	TilesetEditorDialog::~TilesetEditorDialog()
	{
		all_tileset_editor_dialogs().erase(this);
	}

	void TilesetEditorDialog::init()
	{
		clear();
		using namespace gui;
		setPadding(20);

		assert(editor_.get_tileset() >= 0 && static_cast<unsigned>(editor_.get_tileset()) < editor_.all_tilesets().size());

		Button* category_button = new Button(WidgetPtr(new Label(category_, KRE::Color::colorWhite())), std::bind(&TilesetEditorDialog::showCategoryMenu, this));
		addWidget(WidgetPtr(category_button), 10, 10);

		GridPtr grid(new Grid(3));
		int index = 0, first_index = -1;
		first_index_ = -1;
		for(const editor::tileset& t : editor_.all_tilesets()) {
			if(t.category == category_) {
				if(first_index_ == -1) {
					first_index_ = index;
				}
				PreviewTilesetWidget* preview = new PreviewTilesetWidget(*t.preview());
				preview->setDim(g_editor_tileset_button_size - 4, g_editor_tileset_button_size - 4);
				ButtonPtr tileset_button(new Button(WidgetPtr(preview), std::bind(&TilesetEditorDialog::setTileset, this, index)));
				tileset_button->setDim(g_editor_tileset_button_size, g_editor_tileset_button_size);
				grid->addCol(gui::WidgetPtr(new gui::BorderWidget(tileset_button, index == editor_.get_tileset() ? KRE::Color::colorWhite() : KRE::Color::colorBlack())));
			}

			++index;
		}

		grid->finishRow();
		addWidget(grid);
	}

	void TilesetEditorDialog::selectCategory(const std::string& category)
	{
		category_ = category;
		init();

		if(first_index_ != -1) {
			setTileset(first_index_);
		}
	}

	void TilesetEditorDialog::closeContextMenu(int index)
	{
		removeWidget(context_menu_);
		context_menu_.reset();
	}

	void TilesetEditorDialog::showCategoryMenu()
	{
		using namespace gui;
		Grid* grid = new Grid(2);
		grid->swallowClicks();
		grid->setShowBackground(true);
		grid->setHpad(10);
		grid->allowSelection();
		grid->registerSelectionCallback(std::bind(&TilesetEditorDialog::closeContextMenu, this, std::placeholders::_1));

		std::set<std::string> categories;
		for(const editor::tileset& t : editor_.all_tilesets()) {
			if(categories.count(t.category)) {
				continue;
			}

			categories.insert(t.category);

			PreviewTilesetWidget* preview = new PreviewTilesetWidget(*t.preview());
			preview->setDim(48, 48);
			grid->addCol(WidgetPtr(preview))
				 .addCol(WidgetPtr(new Label(t.category, KRE::Color::colorWhite())));
			grid->registerRowSelectionCallback(std::bind(&TilesetEditorDialog::selectCategory, this, t.category));
		}

		int mousex, mousey;
		input::sdl_get_mouse_state(&mousex, &mousey);
		if(mousex + grid->width() > KRE::WindowManager::getMainWindow()->width()) {
			mousex = KRE::WindowManager::getMainWindow()->width() - grid->width();
		}

		if(mousey + grid->height() > KRE::WindowManager::getMainWindow()->height()) {
			mousey = KRE::WindowManager::getMainWindow()->height() - grid->height();
		}

		mousex -= x();
		mousey -= y();

		removeWidget(context_menu_);
		context_menu_.reset(grid);
		context_menu_->setZOrder(1000);
		addWidget(context_menu_, mousex, mousey);
	}

	void TilesetEditorDialog::setTileset(int index)
	{
		if(editor_.get_tileset() != index) {
			editor_.set_tileset(index);
			init();
		}
	}

	bool TilesetEditorDialog::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(!claimed) {
			if(context_menu_) {
				gui::WidgetPtr ptr = context_menu_;
				SDL_Event ev = event;
				//normalizeEvent(&ev);
				return ptr->processEvent(getPos(), ev, claimed);
			}

			switch(event.type) {
			case SDL_KEYDOWN:
				if(event.key.keysym.sym == SDLK_COMMA) {
					editor_.set_tileset(editor_.get_tileset()-1);
					while(editor_.all_tilesets()[editor_.get_tileset()].category != category_) {
						editor_.set_tileset(editor_.get_tileset()-1);
					}
					setTileset(editor_.get_tileset());
					claimed = true;
				} else if(event.key.keysym.sym == SDLK_PERIOD) {
					editor_.set_tileset(editor_.get_tileset()+1);
					while(editor_.all_tilesets()[editor_.get_tileset()].category != category_) {
						editor_.set_tileset(editor_.get_tileset()+1);
					}
					setTileset(editor_.get_tileset());
					claimed = true;
				}
				break;
			}
		}

		return Dialog::handleEvent(event, claimed);
	}
}

#endif // !NO_EDITOR
