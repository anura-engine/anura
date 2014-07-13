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

#include "kre/WindowManager.hpp"

#include "border_widget.hpp"
#include "button.hpp"
#include "character_editor_dialog.hpp"
#include "editor.hpp"
#include "frame.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "input.hpp"
#include "label.hpp"
#include "module.hpp"
#include "text_editor_widget.hpp"

namespace editor_dialogs
{
	CharacterEditorDialog::CharacterEditorDialog(editor& e)
	  : gui::Dialog(KRE::WindowManager::getMainWindow()->width() - EDITOR_SIDEBAR_WIDTH, 160, EDITOR_SIDEBAR_WIDTH, 440), editor_(e)
	{
		setClearBgAmount(255);
		if(editor_.all_characters().empty() == false) {
			category_ = editor_.all_characters().front().category;
		}

		init();
	}

	void CharacterEditorDialog::init()
	{
		clear();
		using namespace gui;
		setPadding(20);

		if(!find_edit_) {
			find_edit_.reset(new TextEditorWidget(140));
			find_edit_->setOnChangeHandler(std::bind(&CharacterEditorDialog::init, this));
		}

		grid_ptr find_grid(new gui::grid(2));
		find_grid->add_col(WidgetPtr(new Label("Search: ", KRE::Color::colorWhite())));
		find_grid->add_col(WidgetPtr(find_edit_));
		addWidget(find_grid, 10, 10);

		const Frame& frame = *editor_.all_characters()[editor_.get_object()].preview_frame();

		Button* facing_button = new Button(
			WidgetPtr(new Label(editor_.isFacingRight() ? "right" : "left", KRE::Color::colorWhite())),
			std::bind(&editor::toggle_facing, &editor_));
		facing_button->setTooltip("f  Change Facing");
		if(find_edit_->text().empty() == false) {
			addWidget(WidgetPtr(facing_button));
			addWidget(generate_grid(""));
		} else {

			Button* category_button = new Button(WidgetPtr(new Label(category_, KRE::Color::colorWhite())), std::bind(&CharacterEditorDialog::show_category_menu, this));
			addWidget(WidgetPtr(category_button));

			addWidget(generate_grid(category_));
	
			addWidget(WidgetPtr(facing_button), category_button->x() + category_button->width() + 10, category_button->y());
		}
	}

	gui::WidgetPtr CharacterEditorDialog::generate_grid(const std::string& category)
	{
		std::cerr << "generate grid: " << category << "\n";
		using namespace gui;
		WidgetPtr& result = grids_[category];
		std::vector<gui::BorderWidgetPtr>& borders = grid_borders_[category];
		if(!result || category == "") {
			const std::string search_string = find_edit_->text();

			grid_ptr grid(new gui::grid(3));
			grid->set_max_height(height() - 50);
			int index = 0;
			for(const editor::enemy_type& c : editor_.all_characters()) {
				bool matches = c.category == category;
				if(search_string.empty() == false) {
					const std::string id = module::get_id(c.node["type"].as_string());
					const char* p = strstr(id.c_str(), search_string.c_str());
					matches = p == id.c_str() || p != NULL && *(p-1) == '_';
				}

				if(matches) {
					if(first_obj_.count(category_) == 0) {
						first_obj_[category] = index;
					}

					ImageWidget* preview = new ImageWidget(c.preview_frame()->img());
					preview->setDim(36, 36);
					preview->setArea(c.preview_frame()->area());
					ButtonPtr char_button(new Button(WidgetPtr(preview), std::bind(&CharacterEditorDialog::set_character, this, index)));
	
					std::string tooltip_str = c.node["type"].as_string();

					if(c.help.empty() == false) {
						tooltip_str += "\n" + c.help;
					}
					char_button->setTooltip(tooltip_str);
					char_button->setDim(40, 40);
					borders.push_back(new gui::BorderWidget(char_button, KRE::Color(0,0,0,0)));
					grid->add_col(gui::WidgetPtr(borders.back()));
				} else {
					borders.push_back(NULL);
				}
	
				++index;
			}

			grid->finish_row();

			result = grid;
		}

		for(int n = 0; n != borders.size(); ++n) {
			if(!borders[n]) {
				continue;
			}
			borders[n]->setColor(n == editor_.get_object() ? KRE::Color::colorWhite() : KRE::Color(0,0,0,0));
		}
		std::cerr << "done generate grid: " << category << "\n";

		return result;
	}

	void CharacterEditorDialog::show_category_menu()
	{
		using namespace gui;
		using std::placeholders::_1;
		gui::grid* grid = new gui::grid(2);
		grid->setZOrder(100);
		grid->set_max_height(height());
		grid->setShowBackground(true);
		grid->setHpad(10);
		grid->allowSelection();
		grid->registerSelectionCallback(std::bind(&CharacterEditorDialog::close_context_menu, this, _1));

		std::map<std::string, const editor::enemy_type*> categories;
		for(const editor::enemy_type& c : editor_.all_characters()) {
			std::string category = c.category;
			for(char& c : category) {
				c = tolower(c);
			}

			if(categories.count(category)) {
				continue;
			}

			categories[category] = &c;
		}

		typedef std::pair<std::string, const editor::enemy_type*> cat_pair;
		for(const cat_pair& cp : categories) {
			const editor::enemy_type& c = *cp.second;

			ImageWidget* preview = new ImageWidget(c.preview_frame()->img());
			preview->setDim(28, 28);
			preview->setArea(c.preview_frame()->area());
			grid->add_col(WidgetPtr(preview))
				 .add_col(WidgetPtr(new Label(c.category, KRE::Color::colorWhite())));
			grid->register_row_selection_callback(std::bind(&CharacterEditorDialog::select_category, this, c.category));
		}

		int mousex, mousey;
		input::sdl_get_mouse_state(&mousex, &mousey);
		unsigned wnd_w = KRE::WindowManager::getMainWindow()->width();
		unsigned wnd_h = KRE::WindowManager::getMainWindow()->height();
		if(static_cast<unsigned>(mousex + grid->width()) > wnd_w) {
			mousex = wnd_w - grid->width();
		}

		if(static_cast<unsigned>(mousey + grid->height()) > wnd_h) {
			mousey = wnd_h - grid->height();
		}

		mousex -= x();
		mousey -= y();

		removeWidget(context_menu_);
		context_menu_.reset(grid);
		addWidget(context_menu_, mousex - 20, mousey);
	}

	void CharacterEditorDialog::set_character(int index)
	{
		category_ = editor_.all_characters()[index].category;
		editor_.setObject(index);
		init();
	}

	void CharacterEditorDialog::close_context_menu(int index)
	{
		removeWidget(context_menu_);
		context_menu_.reset();
	}

	void CharacterEditorDialog::select_category(const std::string& category)
	{
		std::cerr << "SELECT CATEGORY: " << category << "\n";
		category_ = category;
		init();
		set_character(first_obj_[category_]);
	}
}
#endif // NO_EDITOR
