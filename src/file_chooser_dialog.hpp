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

#pragma once

#include <vector>

#include "dialog.hpp"
#include "dropdown_widget.hpp"
#include "text_editor_widget.hpp"

namespace gui 
{
	typedef std::pair<std::string, std::string> filter_pair;
	typedef std::vector<filter_pair> filter_list;

	typedef std::vector<std::string> file_list;
	typedef std::vector<std::string> dir_list;
	typedef std::pair<file_list, dir_list> file_directory_list;
	typedef std::map<std::string, file_directory_list> file_directory_map;

	class FileChooserDialog : public Dialog
	{
	public:
		FileChooserDialog(int x, int y, int w, int h, const filter_list& filters=filter_list(), bool dir_only=false, const std::string& default_path=".");
		FileChooserDialog(variant value, game_logic::FormulaCallable* e);
		std::string getFileName() const { return file_name_; }
		std::string getPath();
		void setSaveasDialog() { file_open_dialog_ = false; }
		void setOpenDialog() { file_open_dialog_ = true; }
		void setDefaultPath(const std::string& path);
		void useRelativePaths(bool val=true, const std::string& rel_path="");
	protected:
		void init();
		void okButton();
		void cancelButton();
		void upButton();
		void homeButton();
		void addDirButton();
		void textEnter(const TextEditorWidgetPtr editor);
		void executeChangeDirectory(const dir_list& d, int index);
		void executeSelectFile(const file_list& f, int index);
		void executeDirNameEnter(const TextEditorWidgetPtr editor);
		void executeDirNameSelect(int row);
		void changeFilter(int selection, const std::string& s);
	private:
		DECLARE_CALLABLE(FileChooserDialog)
		std::string abs_default_path_;
		std::string current_path_;
		std::string relative_path_;
		std::string file_name_;
		filter_list filters_;
		int filter_selection_;
		bool file_open_dialog_;
		TextEditorWidgetPtr editor_;
		WidgetPtr context_menu_;
		DropdownWidgetPtr filter_widget_;
		bool dir_only_;
		bool use_relative_paths_;
	};
}
