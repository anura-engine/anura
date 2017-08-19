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

#include "editor_layers_dialog.hpp"
#include "formatter.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "label.hpp"
#include "level.hpp"

namespace editor_dialogs
{
	using std::placeholders::_1;

	EditorLayersDialog::EditorLayersDialog(editor& e)
		: Dialog(KRE::WindowManager::getMainWindow()->width() - 200, 40, LAYERS_DIALOG_WIDTH, KRE::WindowManager::getMainWindow()->height() - 40), 
		editor_(e), 
		locked_(false)
	{
		setClearBgAmount(255);
		init();
	}

	void EditorLayersDialog::init()
	{
		clear();
		rows_.clear();

		using namespace gui;
		GridPtr g(new Grid(2));

		std::set<int> all_layers, hidden_layers;
		editor_.get_level().get_tile_layers(&all_layers, &hidden_layers);

		for(int layer : all_layers) {
			const bool hidden = hidden_layers.count(layer) != 0;
			GuiSectionWidget* section = new GuiSectionWidget(hidden ? "checkbox-empty" : "checkbox-filled");

			row_data row = { section, layer, hidden };
			rows_.push_back(row);
			g->addCol(WidgetPtr(section));
			g->addCol(WidgetPtr(new Label(formatter() << layer, KRE::Color::colorWhite())));
		}

		GuiSectionWidget* section = new GuiSectionWidget(locked_ ? "checkbox-filled" : "checkbox-empty");
		g->addCol(WidgetPtr(section));
		g->addCol(WidgetPtr(new Label("lock", KRE::Color::colorWhite())));

		g->allowSelection();
		g->registerSelectionCallback(std::bind(&EditorLayersDialog::rowSelected, this, _1));
		g->registerMouseoverCallback(std::bind(&EditorLayersDialog::rowMouseover, this, _1));
	
		addWidget(g, 0, 0);

		const int ypos = g->y() + g->height();

		findClassifications();
		g.reset(new Grid(2));
		for(const std::string& classification : all_classifications_) {
			const bool hidden = editor_.get_level().hidden_object_classifications().count(classification) != 0;
			GuiSectionWidget* section = new GuiSectionWidget(hidden ? "checkbox-empty" : "checkbox-filled");
			g->addCol(WidgetPtr(section));
			g->addCol(WidgetPtr(new Label(classification, KRE::Color::colorWhite())));
		}

		g->allowSelection();
		g->registerSelectionCallback(std::bind(&EditorLayersDialog::classificationSelected, this, _1));

		addWidget(g, 0, ypos + 80);
	}

	void EditorLayersDialog::process()
	{
		const int index = editor_.get_tileset();
		if(index < 0 || static_cast<unsigned>(index) >= editor_.all_tilesets().size()) {
			return;
		}

		if(locked_) {
			const editor::tileset& t = editor_.all_tilesets()[index];
			std::set<int> all_layers, hidden_layers;
			editor_.get_level().get_tile_layers(&all_layers, &hidden_layers);
			LOG_INFO("LOCKED.. " << hidden_layers.size());

			if(hidden_layers.size() != 1 || *hidden_layers.begin() != t.zorder) {
				LOG_INFO("CHANGING LOCK");
				for(LevelPtr lvl : editor_.get_level_list()) {
					for(int layer : all_layers) {
						lvl->hide_tile_layer(layer, true);
					}

					lvl->hide_tile_layer(t.zorder, false);
				}

				init();
			}
		}

		const std::set<std::string> classifications = all_classifications_;
		findClassifications();
		if(classifications != all_classifications_) {
			init();
		}
	}

	void EditorLayersDialog::rowSelected(int nrow)
	{
		if(static_cast<unsigned>(nrow) == rows_.size()) {
			locked_ = !locked_;
			if(locked_) {
				std::set<int> all_layers, hidden_layers;
				editor_.get_level().get_tile_layers(&all_layers, &hidden_layers);
				before_locked_state_ = hidden_layers;
			} else if(!locked_) {
				std::set<int> all_layers, hidden_layers;
				editor_.get_level().get_tile_layers(&all_layers, &hidden_layers);
				for(LevelPtr lvl : editor_.get_level_list()) {
					for(int layer : all_layers) {
						lvl->hide_tile_layer(layer, before_locked_state_.count(layer) != 0);
					}
				}
			}
			init();
			return;
		}
	
		if(nrow < 0 || static_cast<unsigned>(nrow) >= rows_.size()) {
			return;
		}

		locked_ = false;

		for(LevelPtr lvl : editor_.get_level_list()) {
			lvl->hide_tile_layer(rows_[nrow].layer, !rows_[nrow].hidden);
		}

		init();
	}

	void EditorLayersDialog::rowMouseover(int nrow)
	{
		if(nrow < 0 || static_cast<unsigned>(nrow) >= rows_.size()) {
			editor_.get_level().highlight_tile_layer(std::numeric_limits<int>::min());
			return;
		}

		editor_.get_level().highlight_tile_layer(rows_[nrow].layer);
	}

	void EditorLayersDialog::findClassifications()
	{
		all_classifications_.clear();
		for(LevelPtr lvl : editor_.get_level_list()) {
			for(EntityPtr e : lvl->get_chars()) {
				if(e->getEditorInfo() && !e->getEditorInfo()->getClassification().empty()) {
					all_classifications_.insert(e->getEditorInfo()->getClassification());
				}
			}
		}

	}

	void EditorLayersDialog::classificationSelected(int index)
	{
		if(index < 0 || static_cast<unsigned>(index) >= all_classifications_.size()) {
			return;
		}

		std::set<std::string>::const_iterator itor = all_classifications_.begin();
		std::advance(itor, index);
		for(LevelPtr lvl : editor_.get_level_list()) {
			lvl->hide_object_classification(*itor, !lvl->hidden_object_classifications().count(*itor));
		}

		init();
	}
}
#endif // !NO_EDITOR

