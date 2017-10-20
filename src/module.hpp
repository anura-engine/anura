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

#include <boost/shared_ptr.hpp>

#include "filesystem.hpp"
#include "formula_callable.hpp"
#include "variant.hpp"

#include "Color.hpp"

#include <map>
#include <string>
#include <vector>

// XXX split this into the base module and a new translation unit with the module server.
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

		KRE::ColorPtr speech_dialog_bg_color;

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

	//tries to find a file path in all possible modules
	std::string map_file(const std::string& fname);

	//maps a filename, which might have an encoded module id, otherwise uses get_module_name().
	//doesn't depend on any existing files.
	std::string map_write_path(const std::string& fname, BASE_PATH_TYPE path_type=BASE_PATH_GAME);

	std::string get_default_font();
	const KRE::ColorPtr& get_speech_dialog_bg_color();

	variant get_default_preferences();

	enum MODULE_PREFIX_BEHAVIOR { MODULE_PREFIX, MODULE_NO_PREFIX };
	void get_unique_filenames_under_dir(const std::string& dir,
										std::map<std::string, std::string>* file_map,
										MODULE_PREFIX_BEHAVIOR prefix=MODULE_PREFIX);
	void get_all_filenames_under_dir(const std::string& dir,
										std::multimap<std::string, std::string>* file_map,
										MODULE_PREFIX_BEHAVIOR prefix=MODULE_PREFIX);

	void get_files_in_dir(const std::string& dir,
						  std::vector<std::string>* files,
						  std::vector<std::string>* dirs=nullptr);

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
		virtual ~client();

		virtual bool isHighPriorityChunk(const variant& chunk_id, variant& chunk) { return false; }
		virtual void onChunkReceived(variant& chunk) {}

		//function which downloads a module and has it ready to install but
		//doesn't install it yet.
		void prepare_install_module(const std::string& module_name, bool force=false);

		//returns true iff we called prepare_install_module() previously and now
		//the module is fully downloaded and ready to install.
		bool module_prepared() const;

		//begins download and installation of a given module.
		bool install_module(const std::string& module_name, bool force=false);
		void rate_module(const std::string& module_id, int rating, const std::string& review);
		void get_status();
		bool process();
		const std::string& error() const { return error_; }
		bool out_of_date() const { return out_of_date_; }
		variant getValue(const std::string& key) const override;

		int nbytes_transferred() const { return nbytes_transferred_; }
		int nbytes_total() const { return nbytes_total_; }
		int nfiles_written() const { return nfiles_written_; }

		void set_install_image(bool value) { install_image_ = value; }
		void set_install_path_override(const std::string& path) { install_path_override_ = path; }

		bool is_new_install() const { return is_new_install_; }

		std::string module_path() const;

		void set_show_progress_fn(std::function<void(std::string)> fn) { show_progress_fn_ = fn; }

		bool is_pending_install() const { return operation_ == OPERATION_PENDING_INSTALL; }

		void complete_install();

		void set_module_description(const std::string& str) { module_description_ = str; }
	private:
		
		bool install_module_confirmed_out_of_date(const std::string& module_name);

		enum OPERATION_TYPE { OPERATION_NONE, OPERATION_PENDING_INSTALL, OPERATION_INSTALL, OPERATION_QUERY_VERSION_FOR_INSTALL, OPERATION_PREPARE_INSTALL, OPERATION_GET_CHUNKS, OPERATION_GET_STATUS, OPERATION_GET_ICONS, OPERATION_RATE };
		OPERATION_TYPE operation_;
		bool force_install_;
		std::string module_id_;
		std::string error_;
		std::string host_, port_;
		bool out_of_date_;
		std::unique_ptr<class http_client> client_;

		std::map<std::string, variant> data_;
		variant module_info_;

		int nbytes_transferred_, nbytes_total_;

		int nfiles_written_;

		std::string get_module_path(const std::string& module_id) const;

		bool install_image_;
		std::string install_path_override_;

		//a response that is ready for installation. Only used when operation_ is
		//OPERATION_PREPARE_INSTALL
		std::string pending_response_;

		bool is_new_install_;

		void on_response(std::string response);
		void on_error(std::string response, std::string url, std::string request);
		void on_progress(int sent, int total, bool uploaded);

		void on_chunk_error(std::string response, std::string url, std::string request, variant chunk, boost::shared_ptr<class http_client> client);
		int nchunk_errors_;

		void perform_install(const variant& doc);
		void perform_install_from_doc(variant doc);

		variant doc_pending_chunks_;
		std::vector<variant> chunks_to_get_;
		std::vector<boost::shared_ptr<class http_client> > chunk_clients_;

		void on_chunk_response(std::string chunk_url, variant node, boost::shared_ptr<class http_client> client, std::string response);
		void on_chunk_progress(std::string chunk_url, size_t received, size_t total, bool response);

		std::map<std::string, size_t> chunk_progress_;

		std::function<void(std::string)> show_progress_fn_;

		void show_progress(const std::string& msg) { if(show_progress_fn_) { show_progress_fn_(msg); } }

		std::string module_description_;
	};
}
