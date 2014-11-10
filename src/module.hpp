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

#pragma once

#include "filesystem.hpp"
#include "formula_callable.hpp"
#include "variant.hpp"

#include <map>
#include <string>
#include <vector>

// XXX split this into the base module and a new translation unit with the module server.
namespace graphics {
	class color;
}


namespace module
{
	enum BASE_PATH_TYPE { BASE_PATH_GAME, BASE_PATH_USER, NUM_PATH_TYPES };

	struct modules 
	{
		std::string name_;
		std::string pretty_name_;
		std::string abbreviation_;

		//base_path_ is in the game data directory, user_path_ is in the user's
		//preferences area and is mutable.
		std::string base_path_[NUM_PATH_TYPES];

		std::string default_font;
		std::string default_font_cjk;

	boost::intrusive_ptr<graphics::color> speech_dialog_bg_color;

		std::vector<int> version_;
		std::vector<std::string> included_modules_;

		variant default_preferences;
	};

	typedef std::map<std::string, std::string> module_file_map;
	typedef std::pair<std::string, std::string> module_file_pair;
	typedef std::map<std::string, std::string>::const_iterator module_file_map_iterator;

//sets the core module name we are using. This MUST be called before
//using any other module functions to establish the dlc path we use.
void set_core_module_name(const std::string& module_name);

	const std::string get_module_name();
	const std::string get_module_pretty_name();
	std::string get_module_version();
	std::string map_file(const std::string& fname);

	std::string get_default_font();
const boost::intrusive_ptr<graphics::color>& get_speech_dialog_bg_color();

	variant get_default_preferences();

	enum MODULE_PREFIX_BEHAVIOR { MODULE_PREFIX, MODULE_NO_PREFIX };
	void get_unique_filenames_under_dir(const std::string& dir,
										std::map<std::string, std::string>* file_map,
										MODULE_PREFIX_BEHAVIOR prefix=MODULE_PREFIX);

	void get_files_in_dir(const std::string& dir,
						  std::vector<std::string>* files,
						  std::vector<std::string>* dirs=NULL);

	void get_files_matching_wildcard(const std::string& pattern,
									 std::string* dir_out,
									 std::vector<std::string>* files);

	std::string get_id(const std::string& id);
	std::string get_module_id(const std::string& id);
	std::string make_module_id(const std::string& name);
	std::map<std::string, std::string>::const_iterator find(const std::map<std::string, std::string>& filemap, const std::string& name);
	const std::string& get_module_path(const std::string& abbrev="", BASE_PATH_TYPE type=BASE_PATH_GAME);
	std::vector<variant> getAll();
	variant get(const std::string& name);
	void load(const std::string& name, bool initial=true);
	void reload(const std::string& name);
	void get_module_list(std::vector<std::string>& dirs);
	void load_module_from_file(const std::string& modname, modules* mod_);
	void write_file(const std::string& mod_path, const std::string& data);

	variant build_package(const std::string& id);

	bool uninstall_downloaded_module(const std::string& id);

	void set_module_args(game_logic::ConstFormulaCallablePtr callable);
	game_logic::ConstFormulaCallablePtr get_module_args();

	class client : public game_logic::FormulaCallable
	{
	public:
		client();
		client(const std::string& host, const std::string& port);

		//function which downloads a module and has it ready to install but
		//doesn't install it yet.
		void prepare_install_module(const std::string& module_name, bool force=false);

		//completes installation of a module after previously calling
		//prepare_install_module(). pre-condition: module_prepared() returns true
		void complete_install_module();

		//returns true iff we called prepare_install_module() previously and now
		//the module is fully downloaded and ready to install.
		bool module_prepared() const;

		//begins download and installation of a given module.
		void install_module(const std::string& module_name, bool force=false);
		void rate_module(const std::string& module_id, int rating, const std::string& review);
		void get_status();
		bool process();
		const std::string& error() const { return error_; }
		variant getValue(const std::string& key) const;

		int nbytes_transferred() const { return nbytes_transferred_; }
		int nbytes_total() const { return nbytes_total_; }
		int nfiles_written() const { return nfiles_written_; }

		void set_install_image(bool value) { install_image_ = value; }
	private:
		enum OPERATION_TYPE { OPERATION_NONE, OPERATION_INSTALL, OPERATION_PREPARE_INSTALL, OPERATION_GET_STATUS, OPERATION_GET_ICONS, OPERATION_RATE };
		OPERATION_TYPE operation_;
		std::string module_id_;
		std::string error_;
		std::unique_ptr<class http_client> client_;

		std::map<std::string, variant> data_;
		variant module_info_;

		int nbytes_transferred_, nbytes_total_;

		int nfiles_written_;

		bool install_image_;

		//a response that is ready for installation. Only used when operation_ is
		//OPERATION_PREPARE_INSTALL
		std::string pending_response_;

		void on_response(std::string response);
		void on_error(std::string response);
		void on_progress(int sent, int total, bool uploaded);

		void perform_install(const std::string& response);
	};
}
