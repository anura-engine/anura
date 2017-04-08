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

#include "Font.hpp"
#include "WindowManager.hpp"

#include "background.hpp"
#include "button.hpp"
#include "checkbox.hpp"
#include "custom_object_type.hpp"
#include "draw_scene.hpp"
#include "editor.hpp"
#include "editor_dialogs.hpp"
#include "editor_module_properties_dialog.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "grid_widget.hpp"
#include "input.hpp"
#include "json_parser.hpp"
#include "label.hpp"
#include "load_level.hpp"
#include "module.hpp"
#include "stats.hpp"
#include "text_editor_widget.hpp"
#include "unit_test.hpp"
#include "uuid.hpp"

namespace editor_dialogs
{
	using std::placeholders::_1;

	namespace 
	{
		const unsigned char cube_img[266] = {137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 
			73, 72, 68, 82, 0, 0, 0, 16, 0, 0, 0, 16, 8, 2, 0, 0, 0, 144, 145, 
			104, 54, 0, 0, 0, 7, 116, 73, 77, 69, 7, 220, 4, 23, 9, 56, 22, 125, 
			252, 141, 55, 0, 0, 0, 23, 116, 69, 88, 116, 83, 111, 102, 116, 119, 
			97, 114, 101, 0, 71, 76, 68, 80, 78, 71, 32, 118, 101, 114, 32, 51, 
			46, 52, 113, 133, 164, 225, 0, 0, 0, 8, 116, 112, 78, 71, 71, 76, 
			68, 51, 0, 0, 0, 0, 74, 128, 41, 31, 0, 0, 0, 4, 103, 65, 77, 65, 0,
			0, 177, 143, 11, 252, 97, 5, 0, 0, 0, 6, 98, 75, 71, 68, 0, 255, 0, 
			255, 0, 255, 160, 189, 167, 147, 0, 0, 0, 101, 73, 68, 65, 84, 120, 
			156, 221, 210, 209, 17, 128, 32, 12, 3, 208, 174, 232, 32, 30, 35, 
			116, 177, 78, 226, 50, 202, 89, 225, 66, 83, 208, 111, 115, 252, 53, 
			143, 175, 72, 217, 55, 126, 210, 146, 156, 210, 234, 209, 194, 76, 
			102, 85, 12, 50, 89, 87, 153, 61, 64, 85, 207, 59, 105, 213, 79, 102,
			54, 0, 79, 96, 189, 234, 73, 0, 50, 172, 190, 128, 154, 250, 189, 81, 
			254, 5, 216, 48, 136, 243, 10, 12, 65, 156, 6, 143, 175, 131, 213, 
			248, 62, 206, 251, 2, 161, 49, 129, 1, 89, 58, 130, 187, 0, 0, 0,
			0, 73, 69, 78, 68, 174, 66, 96, 130};

		void create_module(const module::modules& mod) 
		{
			if(!mod.name_.empty()) {
				std::string mod_path = "./modules/" + mod.name_ + "/";
				// create some default directories.
				sys::get_dir(mod_path + "data");
				sys::get_dir(mod_path + "data/level");
				sys::get_dir(mod_path + "data/objects");
				sys::get_dir(mod_path + "data/object_prototypes");
				sys::get_dir(mod_path + "data/gui");
				sys::get_dir(mod_path + "images");
				sys::get_dir(mod_path + "sounds");
				sys::get_dir(mod_path + "music");
				// Create an empty titlescreen.cfg
				variant empty_lvl = json::parse_from_file("data/level/empty.cfg");
				empty_lvl.add_attr(variant("id"), variant("titlescreen.cfg"));

				std::map<variant, variant> playable_m;
				playable_m[variant("_uuid")] = variant(write_uuid(generate_uuid()));
				playable_m[variant("current_frame")] = variant("normal");
				playable_m[variant("custom")] = variant("yes");
				playable_m[variant("face_right")] = variant(1);
				playable_m[variant("is_human")] = variant(1);
				playable_m[variant("label")] = variant("_1111");
				playable_m[variant("time_in_frame")] = variant(0);
				playable_m[variant("type")] = variant("simple_playable");
				playable_m[variant("x")] = variant(0);
				playable_m[variant("y")] = variant(0);
				empty_lvl.add_attr(variant("character"), variant(&playable_m));
				sys::write_file(mod_path + "data/level/titlescreen.cfg", empty_lvl.write_json());

				// Module specifed as standalone, write out a few extra useful files.
				if(mod.included_modules_.empty()) {
					// data/fonts.cfg			-- {font:["@flatten","@include data/dialog_font.cfg","@include data/label_font.cfg"]}
					// data/gui.cfg				-- {section:["@flatten","@include data/editor-tools.cfg","@include data/gui-elements.cfg"],FramedGuiElement: ["@flatten","@include data/framed-gui-elements.cfg"]}
					// data/music.cfg			-- {}
					// data/preload.cfg			-- { preload: [], }
					// data/tiles.cfg			-- {}
					// data/gui/null.cfg		-- {}
					sys::write_file(mod_path + "data/fonts.cfg", "{font:[\"@flatten\",\"@include data/fonts-bitmap/dialog_font.cfg\",\"@include data/fonts-bitmap/label_font.cfg\"]}");
					sys::write_file(mod_path + "data/music.cfg", "{\n}");
					sys::write_file(mod_path + "data/tiles.cfg", "{\n}");
					sys::write_file(mod_path + "data/gui/null.cfg", "{\n}");
					sys::write_file(mod_path + "data/preload.cfg", "{\npreload: [\n],\n}");
					sys::write_file(mod_path + "data/gui/default.cfg", "{\n}");
					sys::write_file(mod_path + "data/objects/simple_playable.cfg", 
					"{\n"
					"\tid: \"simple_playable\",\n"
					"\tis_human: true,\n"
					"\thitpoints: 4,\n"
					"\tEditorInfo: { category: \"player\" },\n"
					"\tanimation: [\n"
					"\t\t{\n"
					"\t\tid: \"stand\",\n"
					"\t\timage: \"cube.png\",\n"
					"\t\trect: [0,0,15,15]\n"
					"\t\t}\n"
					"\t],\n"
					"}");
					sys::write_file(mod_path + "images/cube.png", std::string(reinterpret_cast<const char *>(cube_img), 266));
				}
			}
		}

		void write_module_properties(const module::modules& mod) 
		{
			if(!mod.name_.empty()) {
				std::map<variant,variant> m;
				m[variant("id")] = variant(mod.name_);
				if(mod.pretty_name_.empty() == false) {
					m[variant("name")] = variant(mod.pretty_name_);
				}
				if(mod.abbreviation_.empty() == false) {
					m[variant("abbreviation")] = variant(mod.abbreviation_);
				}
				if(mod.included_modules_.empty() == false) {
					std::vector<variant> v;
					for(const std::string& s : mod.included_modules_) {
						v.push_back(variant(s));
					}
					m[variant("dependencies")] = variant(&v);
				}
				m[variant("min_engine_version")] = preferences::version_decimal();
				variant new_module(&m);
			std::string mod_path = "./modules/" + mod.name_ + "/";
			// create the module file.
			sys::write_file(mod_path + "module.cfg", new_module.write_json());
			}
		}
	}

	EditorModulePropertiesDialog::EditorModulePropertiesDialog(editor& e)
	  : Dialog(KRE::WindowManager::getMainWindow()->logicalWidth()/2 - 300, KRE::WindowManager::getMainWindow()->logicalHeight()/2 - 220, 600, 440), 
	  editor_(e), 
	  new_mod_(true)
	{
		init();
	}

	EditorModulePropertiesDialog::EditorModulePropertiesDialog(editor& e, const std::string& modname)
		: Dialog(KRE::WindowManager::getMainWindow()->logicalWidth()/2 - 300, KRE::WindowManager::getMainWindow()->logicalHeight()/2 - 220, 600, 440), 
		editor_(e), 
		new_mod_(false)
	{
		if(!modname.empty()) {
			module::load_module_from_file(modname, &mod_);
			LOG_INFO("MOD: " << modname << ":" << mod_.name_);
		}
		init();
	}

	void EditorModulePropertiesDialog::init()
	{
		setClearBgAmount(255);
		setBackgroundFrame("empty_window");
		setDrawBackgroundFn(::draw_last_scene);

		dirs_.clear();
		module::get_module_list(dirs_);

		using namespace gui;
		clear();

		addWidget(WidgetPtr(new Label("Module Properties", KRE::Color::colorWhite(), 48)), 10, 10);

		GridPtr g(new Grid(2));
		g->setMaxHeight(320);
		if(new_mod_) {
			TextEditorWidgetPtr change_id_entry(new TextEditorWidget(200, 30));
			change_id_entry->setOnChangeHandler(std::bind(&EditorModulePropertiesDialog::changeId, this, change_id_entry));
			change_id_entry->setOnEnterHandler(std::bind(&Dialog::close, this));

			g->addCol(WidgetPtr(new Label("Identifier:  ", KRE::Color::colorWhite(), 36)))
				.addCol(WidgetPtr(change_id_entry));
		} else {
			g->addCol(WidgetPtr(new Label("Identifier: ", KRE::Color::colorWhite(), 36)))
				.addCol(WidgetPtr(new Label(mod_.name_, KRE::Color::colorWhite(), 36)));
		}

		TextEditorWidgetPtr change_name_entry(new TextEditorWidget(200, 30));
		change_name_entry->setText(mod_.pretty_name_);
		change_name_entry->setOnChangeHandler(std::bind(&EditorModulePropertiesDialog::changeName, this, change_name_entry));
		change_name_entry->setOnEnterHandler(std::bind(&Dialog::close, this));

		g->addCol(WidgetPtr(new Label("Name:", KRE::Color::colorWhite(), 36)))
		  .addCol(WidgetPtr(change_name_entry));

		TextEditorWidgetPtr change_abbrev_entry(new TextEditorWidget(200, 30));
		change_abbrev_entry->setText(mod_.abbreviation_);
		change_abbrev_entry->setOnChangeHandler(std::bind(&EditorModulePropertiesDialog::changePrefix, this, change_abbrev_entry));
		change_abbrev_entry->setOnEnterHandler(std::bind(&Dialog::close, this));

		g->addCol(WidgetPtr(new Label("Prefix:", KRE::Color::colorWhite(), 36)))
		  .addCol(WidgetPtr(change_abbrev_entry));

		g->addCol(WidgetPtr(new Label("Modules  ", KRE::Color::colorWhite(), 36)))
			.addCol(WidgetPtr(new Button(WidgetPtr(new Label("Add", KRE::Color::colorWhite())), std::bind(&EditorModulePropertiesDialog::changeModuleIncludes, this))));
		for(const std::string& s : mod_.included_modules_) {
			g->addCol(WidgetPtr(new Label(s, KRE::Color::colorWhite(), 36)))
				.addCol(WidgetPtr(new Button(WidgetPtr(new Label("Remove", KRE::Color::colorWhite())), std::bind(&EditorModulePropertiesDialog::removeModuleInclude, this, s))));
		}
		addWidget(g);

		addOkAndCancelButtons();
	}

	void EditorModulePropertiesDialog::changeId(const gui::TextEditorWidgetPtr editor)
	{
		if(std::find(dirs_.begin(), dirs_.end(), editor->text()) == dirs_.end()) {
			mod_.name_ = editor->text();
		}
	}

	void EditorModulePropertiesDialog::changeName(const gui::TextEditorWidgetPtr editor)
	{
		mod_.pretty_name_ = editor->text();
	}

	void EditorModulePropertiesDialog::changePrefix(const gui::TextEditorWidgetPtr editor)
	{
		mod_.abbreviation_ = editor->text();
	}

	void EditorModulePropertiesDialog::changeModuleIncludes()
	{
		using namespace gui;
		Dialog d(0, 0, KRE::WindowManager::getMainWindow()->width(), KRE::WindowManager::getMainWindow()->height());
		d.addWidget(WidgetPtr(new Label("Change Included Modules", KRE::Color::colorWhite(), 48)));
		if(dirs_.empty()) {
			return;
		}
		std::sort(dirs_.begin(), dirs_.end());

		gui::Grid* grid = new gui::Grid(1);
		grid->setHpad(40);
		grid->setShowBackground(true);
		grid->allowSelection();
		grid->swallowClicks();
		std::vector<std::string> choices;
		for(const std::string& dir : dirs_) {
			// only include modules not included already.
			if(std::find(mod_.included_modules_.begin(), mod_.included_modules_.end(), dir) == mod_.included_modules_.end() 
				&& dir != mod_.name_) {
				grid->addCol(WidgetPtr(new Label(dir, KRE::Color::colorWhite())));
				choices.push_back(dir);
			}
		}
		grid->registerSelectionCallback(std::bind(&EditorModulePropertiesDialog::executeChangeModuleIncludes, this, choices, _1));

		int mousex, mousey;
		input::sdl_get_mouse_state(&mousex, &mousey);

		mousex -= x();
		mousey -= y();

		removeWidget(context_menu_);
		context_menu_.reset(grid);
		addWidget(context_menu_, mousex, mousey);
	}

	void EditorModulePropertiesDialog::removeModuleInclude(const std::string& s)
	{
		std::vector<std::string>::iterator it = std::find(mod_.included_modules_.begin(), mod_.included_modules_.end(), s);
		if(it != mod_.included_modules_.end()) {
			mod_.included_modules_.erase(it);
		}
		init();
	}

	void EditorModulePropertiesDialog::executeChangeModuleIncludes(const std::vector<std::string>& choices, int index)
	{
		if(context_menu_) {
			removeWidget(context_menu_);
			context_menu_.reset();
		}

		if(index < 0 || size_t(index) >= choices.size()) {
			return;
		}

		mod_.included_modules_.push_back(choices[index]);

		init();
	}

	const std::string EditorModulePropertiesDialog::onExit() 
	{
		Level::setPlayerVariantType(variant());
		saveModuleProperties();
		if(new_mod_) {
			createNewModule();
		} 
		// Switch to the new_module
		module::reload(mod_.name_);
		// Reload level paths
		reload_level_paths();
		CustomObjectType::ReloadFilePaths();
		std::map<std::string,std::string> font_paths;
		module::get_unique_filenames_under_dir("data/fonts/", &font_paths);
		KRE::Font::setAvailableFonts(font_paths);
		if(mod_.abbreviation_.empty() == false) {
			return mod_.abbreviation_ + ":titlescreen.cfg";
		}
		return mod_.name_ + ":titlescreen.cfg";
	}

	void EditorModulePropertiesDialog::createNewModule() 
	{
		create_module(mod_);
	}

	void EditorModulePropertiesDialog::saveModuleProperties() 
	{
		write_module_properties(mod_);
	}
}

COMMAND_LINE_UTILITY(create_module) {
	module::modules mod;

	ASSERT_LOG(args.size() == 1, "Must provide name of module to create");

	mod.name_ = args[0];

	editor_dialogs::create_module(mod);
	editor_dialogs::write_module_properties(mod);
}

#endif // !NO_EDITOR
