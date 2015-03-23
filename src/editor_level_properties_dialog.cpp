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

#include <algorithm>
#include <iostream>

#include "WindowManager.hpp"

#include "background.hpp"
#include "button.hpp"
#include "checkbox.hpp"
#include "draw_scene.hpp"
#include "editor.hpp"
#include "editor_dialogs.hpp"
#include "editor_level_properties_dialog.hpp"
#include "formatter.hpp"
#include "grid_widget.hpp"
#include "input.hpp"
#include "label.hpp"
#include "stats.hpp"
#include "text_editor_widget.hpp"

namespace editor_dialogs
{
	using std::placeholders::_1;

	namespace 
	{
		void set_segmented_level_width(EditorLevelPropertiesDialog* d, editor* e, bool value)
		{
			for(LevelPtr lvl : e->get_level_list()) {
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

		void set_segmented_level_height(EditorLevelPropertiesDialog* d, editor* e, bool value)
		{
			for(LevelPtr lvl : e->get_level_list()) {
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

	EditorLevelPropertiesDialog::EditorLevelPropertiesDialog(editor& e)
	  : Dialog(KRE::WindowManager::getMainWindow()->width()/2 - 300, KRE::WindowManager::getMainWindow()->height()/2 - 220, 600, 440), 
	  editor_(e)
	{
		setClearBgAmount(255);
		init();
	}

	void EditorLevelPropertiesDialog::init()
	{
		setClearBgAmount(255);
		setBackgroundFrame("empty_window");
		setDrawBackgroundFn(::draw_last_scene);

		using namespace gui;
		clear();

		addWidget(WidgetPtr(new Label("Level Properties", KRE::Color::colorWhite(), 48)), 10, 10);

		TextEditorWidget* change_title_entry(new TextEditorWidget(200, 30));
		change_title_entry->setText(editor_.get_level().title());
		change_title_entry->setOnChangeHandler(std::bind(&EditorLevelPropertiesDialog::changeTitle, this, change_title_entry));
		change_title_entry->setOnEnterHandler(std::bind(&Dialog::close, this));

		GridPtr g(new Grid(2));
		g->addCol(WidgetPtr(new Label("Change Title", KRE::Color::colorWhite(), 36)))
		  .addCol(WidgetPtr(change_title_entry));

		addWidget(g);

		std::string background_id = editor_.get_level().get_background_id();
		if(background_id.empty()) {
			background_id = "(no background)";
		}
		g.reset(new Grid(2));
		g->addCol(WidgetPtr(new Label("Background", KRE::Color::colorWhite())))
		  .addCol(WidgetPtr(new Button(WidgetPtr(new Label(background_id, KRE::Color::colorWhite())), 
			std::bind(&EditorLevelPropertiesDialog::changeBackground, this))));
		addWidget(g);

		g.reset(new Grid(3));
		g->setHpad(10);
		g->addCol(WidgetPtr(new Label("Next Level", KRE::Color::colorWhite())));
		g->addCol(WidgetPtr(new Label(editor_.get_level().next_level(), KRE::Color::colorWhite())));
		g->addCol(WidgetPtr(new Button(WidgetPtr(new Label("Set", KRE::Color::colorWhite())), std::bind(&EditorLevelPropertiesDialog::changeNextLevel, this))));

		g->addCol(WidgetPtr(new Label("Previous Level", KRE::Color::colorWhite())));
		g->addCol(WidgetPtr(new Label(editor_.get_level().previous_level(), KRE::Color::colorWhite())));
		g->addCol(WidgetPtr(new Button(WidgetPtr(new Label("Set", KRE::Color::colorWhite())), std::bind(&EditorLevelPropertiesDialog::changePreviousLevel, this))));
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

		addOkAndCancelButtons();
	}

	void EditorLevelPropertiesDialog::changeTitle(const gui::TextEditorWidgetPtr editor)
	{
		std::string title = editor->text();

		for(LevelPtr lvl : editor_.get_level_list()) {
			lvl->set_title(title);
		}
	}

	void EditorLevelPropertiesDialog::changeBackground()
	{
		using namespace gui;
		std::vector<std::string> backgrounds = Background::getAvailableBackgrounds();
		if(backgrounds.empty()) {
			return;
		}

		std::sort(backgrounds.begin(), backgrounds.end());

		gui::Grid* grid = new gui::Grid(1);
		grid->setZOrder(100);
		grid->setHpad(40);
		grid->setShowBackground(true);
		grid->allowSelection();
		grid->swallowClicks();
		grid->registerSelectionCallback(std::bind(&EditorLevelPropertiesDialog::executeChangeBackground, this, backgrounds, _1));
		for(const std::string& bg : backgrounds) {
			grid->addCol(WidgetPtr(new Label(bg, KRE::Color::colorWhite())));
		}

		int mousex, mousey;
		input::sdl_get_mouse_state(&mousex, &mousey);

		mousex -= x();
		mousey -= y();

		removeWidget(context_menu_);
		context_menu_.reset(grid);
		addWidget(context_menu_, mousex, mousey);
	}

	void EditorLevelPropertiesDialog::executeChangeBackground(const std::vector<std::string>& choices, int index)
	{
		if(context_menu_) {
			removeWidget(context_menu_);
			context_menu_.reset();
		}

		if(index < 0 || static_cast<unsigned>(index) >= choices.size()) {
			return;
		}

		for(LevelPtr lvl : editor_.get_level_list()) {
			lvl->set_background_by_id(choices[index]);
		}

		init();
	}

	void EditorLevelPropertiesDialog::changeNextLevel()
	{
		std::string result = show_choose_level_dialog("Next Level");
		if(result.empty() == false) {
			for(LevelPtr lvl : editor_.get_level_list()) {
				lvl->set_next_level(result);
			}
		}

		init();
	}

	void EditorLevelPropertiesDialog::changePreviousLevel()
	{
		std::string result = show_choose_level_dialog("Previous Level");
		if(result.empty() == false) {
			for(LevelPtr lvl : editor_.get_level_list()) {
				lvl->set_previous_level(result);
			}
		}

		init();
	}
}

#endif // !NO_EDITOR
