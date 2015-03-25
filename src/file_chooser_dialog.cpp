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
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include "asserts.hpp"
#include "button.hpp"
#include "dialog.hpp"
#include "dropdown_widget.hpp"
#include "file_chooser_dialog.hpp"
#include "grid_widget.hpp"
#include "input.hpp"
#include "label.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "text_editor_widget.hpp"
#include "unit_test.hpp"

#if defined(_WINDOWS)
#include <direct.h>
#define getcwd	_getcwd
#endif

// XXX should convert this to use boost::filesystem

namespace sys
{
	std::string get_absolute_path(const std::string& path, const std::string& curdir="")
	{
		std::string abs_path;
		// A path is absolute if it starts with / (linux)
		// on windows a path is absolute if it starts with \\, x:\, \
		//boost::regex regexp(re_absolute_path);
		//bool path_is_absolute = boost::regex_match(path, boost::regex(re_absolute_path));
		//std::cerr << "set_default_path: path(" << path << ") is " << (path_is_absolute ? "absolute" : "relative") << std::endl;

		if(sys::is_path_absolute(path)) {
			abs_path = sys::make_conformal_path(path);
		} else {
			if(curdir.empty()) {
				std::vector<char> buf(1024);
				const char* const res = getcwd(&buf[0], buf.capacity());
				if(res != nullptr) {
					abs_path  = sys::make_conformal_path(res);
				} else {
					ASSERT_LOG(false, "getcwd failed");
				}
			} else {
				ASSERT_LOG(sys::is_path_absolute(curdir) == true, "get_absolute_path: curdir was specified but isn't absolute! " << curdir);
				abs_path  = sys::make_conformal_path(curdir);
			}

			std::vector<std::string> cur_path;
			boost::split(cur_path, path, std::bind2nd(std::equal_to<char>(), '/'));
			for(const std::string& s : cur_path) {
				if(s == ".") {
				} else if(s == "..") {
					size_t offs = abs_path.rfind('/');
					if(abs_path.length() > 1 && offs != std::string::npos) {
						abs_path.erase(offs);
					}
				} else {
					abs_path += "/" + s;
				}
			}
			abs_path = sys::make_conformal_path(abs_path);
		}
		return abs_path;
	}
}

namespace gui 
{
	FileChooserDialog::FileChooserDialog(int x, int y, int w, int h, const filter_list& filters, bool dir_only, const std::string& default_path)
		: Dialog(x,y,w,h), 
		filters_(filters), 
		file_open_dialog_(true), 
		filter_selection_(0), 
		dir_only_(dir_only), 
		use_relative_paths_(false)
	{
		if(filters_.empty()) {
			filters_.push_back(filter_pair("All files", ".*"));
		}

		relative_path_ = sys::get_absolute_path("");
		setDefaultPath(default_path);
	
		editor_ = new TextEditorWidget(400, 32);
		editor_->setFontSize(16);
		//file_text->setOnChangeHandler(std::bind(&file_chooser_dialog::change_text_attribute, this, change_entry, attr));
		editor_->setOnEnterHandler(std::bind(&FileChooserDialog::textEnter, this, editor_));
		editor_->setOnTabHandler(std::bind(&FileChooserDialog::textEnter, this, editor_));

		init();
	}

	FileChooserDialog::FileChooserDialog(variant v, game_logic::FormulaCallable* e)
		: Dialog(v, e), 
		filter_selection_(0), 
		file_open_dialog_(v["open_dialog"].as_bool(true)), 
		use_relative_paths_(v["use_relative_paths"].as_bool(false))
	{
		if(v.has_key("filters")) {
			ASSERT_LOG(v["filters"].is_list(), "Expected filters parameter to be a list");
			for(size_t n = 0; n != v["filters"].num_elements(); ++n) {
				ASSERT_LOG(v["filters"][n].is_list() && v["filters"][n].num_elements() == 2, 
					"Expected inner filter parameter to be a two element list");
				filters_.push_back(filter_pair(v["filters"][n][0].as_string(), v["filters"][n][1].as_string()));
			}
		}
		relative_path_ = sys::get_absolute_path(preferences::user_data_path());
		setDefaultPath(preferences::user_data_path());

		editor_ = new TextEditorWidget(400, 32);
		editor_->setFontSize(16);
		//file_text->setOnChangeHandler(std::bind(&file_chooser_dialog::change_text_attribute, this, change_entry, attr));
		editor_->setOnEnterHandler(std::bind(&FileChooserDialog::textEnter, this, editor_));
		editor_->setOnTabHandler(std::bind(&FileChooserDialog::textEnter, this, editor_));
		init();
	}


	void FileChooserDialog::setDefaultPath(const std::string& path)
	{
		abs_default_path_ = sys::get_absolute_path(path);
		current_path_ = abs_default_path_;
	}

	void FileChooserDialog::init()
	{
		using std::placeholders::_1;
		using std::placeholders::_2;

		int current_height = 30;
		int hpad = 10;
		clear();

		file_list files;
		dir_list dirs;
		sys::get_files_in_dir(current_path_, &files, &dirs);

		std::string l = "Choose File ";
		if(dir_only_) {
			l = "Choose Directory";
		} else {
			l += file_open_dialog_ ? "To Open" : "To Save";
		}

		LabelPtr lp = new Label(l, KRE::Color::colorWhite(), 20);
		addWidget(WidgetPtr(lp), 30, current_height);
		current_height += lp->height() + hpad;

		lp = new Label("Current Path: " + current_path_, KRE::Color::colorGreen(), 16);
		addWidget(WidgetPtr(lp), 30, current_height);
		current_height += lp->height() + hpad;

		/*  Basic list of things needed after extensive review.
			List of directory names from the directory we are currently in.
			Add directory buttons (+)?
			List of files names under the current directory.
			Ok/Cancel Buttons
			Up one level button
			Text entry box for typing the file/directory path. i.e. if file exists choose it, if it's a directory
			  then make it the current_path_;
		
		*/

		GridPtr g(new Grid(3));
		g->setHpad(50);
		g->addCol(WidgetPtr(new Button(WidgetPtr(new Label("Up", KRE::Color::colorWhite())), std::bind(&FileChooserDialog::upButton, this))));
		g->addCol(WidgetPtr(new Button(WidgetPtr(new Label("Home", KRE::Color::colorWhite())), std::bind(&FileChooserDialog::homeButton, this))));
		g->addCol(WidgetPtr(new Button(WidgetPtr(new Label("Add", KRE::Color::colorWhite())), std::bind(&FileChooserDialog::addDirButton, this))));
		addWidget(g, 30, current_height);	
		current_height += g->height() + hpad;

		GridPtr container(new Grid(dir_only_ ? 1 : 2));
		container->setHpad(30);
		container->allowSelection(false);
		container->setColWidth(0, dir_only_ ? width()*2 : width()/3);
		if(dir_only_ == false) {
			container->setColWidth(1, width()/3);
		}
		container->setShowBackground(false);

		g.reset(new Grid(1));
		g->setDim(dir_only_ ? width()/2 : width()/3, height()/3);
		g->setMaxHeight(height()/3);
		g->setShowBackground(true);
		g->allowSelection();
		for(const std::string& dir : dirs) {
			g->addCol(WidgetPtr(new Label(dir, KRE::Color::colorWhite())));
		}
		g->registerSelectionCallback(std::bind(&FileChooserDialog::executeChangeDirectory, this, dirs, _1));
		container->addCol(g);

		if(dir_only_ == false) {
			g.reset(new Grid(1));
			g->setDim(width()/3, height()/3);
			g->setMaxHeight(height()/3);
			g->setShowBackground(true);
			g->allowSelection();
			std::vector<std::string> filtered_file_list;
			for(const std::string& file : files) {
				boost::regex re(filters_[filter_selection_].second, boost::regex_constants::icase);
				if(boost::regex_match(file, re)) {
					filtered_file_list.push_back(file);
					g->addCol(WidgetPtr(new Label(file, KRE::Color::colorWhite())));
				}
			}
			g->registerSelectionCallback(std::bind(&FileChooserDialog::executeSelectFile, this, filtered_file_list, _1));
			container->addCol(g);
		}
		addWidget(container, 30, current_height);
		current_height += container->height() + hpad;

		addWidget(editor_, 30, current_height);
		current_height += editor_->height() + hpad;

		if(dir_only_ == false) {
			DropdownList dl_list;
			std::transform(filters_.begin(), filters_.end(), 
				std::back_inserter(dl_list), 
				std::bind(&filter_list::value_type::first,_1));
			filter_widget_ = new DropdownWidget(dl_list, width()/2, 20);
			filter_widget_->setOnSelectHandler(std::bind(&FileChooserDialog::changeFilter, this, _1, _2));
			filter_widget_->setSelection(filter_selection_);
			addWidget(filter_widget_, 30, current_height);
			current_height += filter_widget_->getMaxHeight() + hpad;
		}

		g.reset(new Grid(2));
		g->setHpad(20);
		g->addCol(WidgetPtr(new Button(WidgetPtr(new Label("OK", KRE::Color::colorWhite())), std::bind(&FileChooserDialog::okButton, this))));
		g->addCol(WidgetPtr(new Button(WidgetPtr(new Label("Cancel", KRE::Color::colorWhite())), std::bind(&FileChooserDialog::cancelButton, this))));
		addWidget(g, 30, current_height);
		current_height += g->height() + hpad;
	}

	void FileChooserDialog::changeFilter(int selection, const std::string& s)
	{
		if(selection >= 0) {
			filter_selection_ = selection;
		}
		init();
	}

	void FileChooserDialog::executeChangeDirectory(const dir_list& d, int index)
	{
		if(index < 0 || size_t(index) >= d.size()) {
			return;
		}
		if(d[index] == "."){
			return;
		}
		if(d[index] == ".."){
			upButton();
		}
		current_path_ = current_path_ + "/" + d[index];
		if(dir_only_) {
			editor_->setText(getPath());
		} else {
			editor_->setText("");
		}
		init();
	}

	void FileChooserDialog::okButton() 
	{
		close();
	}

	void FileChooserDialog::cancelButton() 
	{
		cancel();
		close();
	}

	void FileChooserDialog::homeButton()
	{
		current_path_ = relative_path_;
		if(dir_only_) {
			editor_->setText(getPath());
		} else {
			editor_->setText("");
		}
		init();
	}

	void FileChooserDialog::upButton()
	{
		size_t offs = current_path_.rfind('/');
		if(current_path_.length() > 1 && offs != std::string::npos) {
			current_path_.erase(offs);
			if(dir_only_) {
				editor_->setText(getPath());
			} else {
				editor_->setText("");
			}
		}
		init();
	}

	void FileChooserDialog::addDirButton()
	{
		using std::placeholders::_1;

		gui::Grid* grid = new gui::Grid(1);
		grid->setShowBackground(true);
		grid->allowSelection(true);
		grid->swallowClicks(false);
		grid->allowDrawHighlight(false);
		TextEditorWidgetPtr dir_name_editor = new TextEditorWidget(200, 28);
		dir_name_editor->setFontSize(14);
		dir_name_editor->setOnEnterHandler(std::bind(&FileChooserDialog::executeDirNameEnter, this, dir_name_editor));
		dir_name_editor->setOnTabHandler(std::bind(&FileChooserDialog::executeDirNameEnter, this, dir_name_editor));
		dir_name_editor->setFocus(true);
		grid->addCol(dir_name_editor);
		grid->registerSelectionCallback(std::bind(&FileChooserDialog::executeDirNameSelect, this, _1));

		int mousex, mousey;
		input::sdl_get_mouse_state(&mousex, &mousey);

		mousex -= x();
		mousey -= y();

		removeWidget(context_menu_);
		context_menu_.reset(grid);
		addWidget(context_menu_, mousex, mousey);
	}

	void FileChooserDialog::executeDirNameSelect(int row)
	{
		if(row == -1 && context_menu_) {
			removeWidget(context_menu_);
			context_menu_.reset();
		}
	}

	void FileChooserDialog::executeDirNameEnter(const TextEditorWidgetPtr editor)
	{
		if(context_menu_) {
			removeWidget(context_menu_);
			context_menu_.reset();
		}

		if(editor->text().empty() == false) {
			std::string new_path = sys::get_dir(sys::get_absolute_path(editor->text(), current_path_));
			if(new_path.empty() == false) {
				current_path_ = new_path;
				if(dir_only_) {
					editor_->setText(getPath());
				} else {
					editor_->setText("");
				}
			} else {
				LOG_WARN("Failed to create directory " << editor->text() << " in " << current_path_);
			}
		}
		init();
	}

	void FileChooserDialog::textEnter(const TextEditorWidgetPtr editor)
	{
		if(dir_only_) {
			std::string path = sys::get_absolute_path(editor->text(), current_path_);
			if(sys::is_directory(path)) {
				current_path_ = path;
				editor->setText(getPath());
			} else {
				path = sys::get_absolute_path(editor->text(), relative_path_);
				if(sys::is_directory(path)) {
					current_path_ = path;
					editor->setText(getPath());
				} else {
					LOG_WARN("Invalid Path: " << path);
				}
			}
		} else if(file_open_dialog_) {
			if(sys::file_exists(editor->text())) {
				file_name_ = editor->text();
			} else if(sys::is_directory(editor->text())) {
				current_path_ = editor->text();
				editor->setText("");
			} else {
				// Not a valid file or directory name.
				// XXX
			}
		} else {
			// save as...
			if(sys::file_exists(editor->text())) {
				// XXX File exists prompt with an over-write confirmation box.
				file_name_ = editor->text();
			} else if(sys::is_directory(editor->text())) {
				current_path_ = editor->text();
				editor->setText("");
			} else {
				file_name_ = editor->text();
			}
		}
		init();
	}

	void FileChooserDialog::executeSelectFile(const file_list& f, int index)
	{
		if(index < 0 || size_t(index) >= f.size()) {
			return;
		}
		file_name_ = current_path_ + "/" + f[index];
		editor_->setText(f[index]);
		init();
	}

	std::string FileChooserDialog::getPath()
	{
		if(use_relative_paths_) {
			return sys::compute_relative_path(relative_path_, current_path_);
		} 
		return current_path_;
	}

	void FileChooserDialog::useRelativePaths(bool val, const std::string& rel_path) 
	{ 
		use_relative_paths_ = val; 
		relative_path_ = sys::get_absolute_path(rel_path);
		if(editor_) {
			editor_->setText(getPath());
		}
	}

	BEGIN_DEFINE_CALLABLE(FileChooserDialog, Dialog)
		DEFINE_FIELD(relative_file_name, "string")
			return variant(obj.getFileName());
	END_DEFINE_CALLABLE(FileChooserDialog)
}

UNIT_TEST(compute_relative_paths_test) {
	CHECK_EQ(sys::compute_relative_path("/home/tester/frogatto/images", "/home/tester/frogatto/data"), "../data");
	CHECK_EQ(sys::compute_relative_path("/", "/"), "");
	CHECK_EQ(sys::compute_relative_path("/home/tester", "/"), "../..");
	CHECK_EQ(sys::compute_relative_path("/", "/home"), "home");
	CHECK_EQ(sys::compute_relative_path("C:/Projects/frogatto", "C:/Projects"), "..");
	CHECK_EQ(sys::compute_relative_path("C:/Projects/frogatto/images/experimental", "C:/Projects/xyzzy/test1/test2"), "../../../xyzzy/test1/test2");
	CHECK_EQ(sys::compute_relative_path("C:/Projects/frogatto/", "C:/Projects/frogatto/modules/vgi/images"), "modules/vgi/images");
	CHECK_EQ(sys::compute_relative_path("C:/Projects/frogatto-build/Frogatto/Win32/Release", "C:/Projects/frogatto-build/Frogatto/Win32/Release/modules/vgi/images"), "modules/vgi/images");
	CHECK_EQ(sys::compute_relative_path("C:/Projects/frogatto-build/Frogatto/Win32/Release", "c:/windows"), "../../../../../windows");
}

#endif
