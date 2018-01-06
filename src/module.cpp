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

#include <deque>

#include <boost/filesystem/operations.hpp>

#include "asserts.hpp"
#include "base64.hpp"
#include "compress.hpp"
#include "custom_object_type.hpp"
#include "i18n.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula_constants.hpp"
#include "http_client.hpp"
#include "json_parser.hpp"
#include "md5.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "uri.hpp"
#include "variant_utils.hpp"

namespace module 
{
	using std::placeholders::_1;
	using std::placeholders::_2;
	using std::placeholders::_3;

	namespace 
	{
		PREF_STRING(module_server, "theargentlark.com", "server to use to get modules from");
		PREF_STRING(module_port, "23455", "server port to get modules from");

		PREF_STRING(module_chunk_server, "", "server to use to get modules chunk from (defaults to module_server)");

		PREF_STRING(module_chunk_port, "", "server port to get modules chunk from (defaults to module_port)");
		PREF_STRING(module_chunk_query, "POST /download_chunk?chunk_id=", "request to download a module chunk");
		PREF_BOOL(module_chunk_deflate, false, "If true, module chunks are assumed compressed and will be deflated");

		bool module_chunk_query_is_get() {
			return g_module_chunk_query.size() > 3 && std::equal(g_module_chunk_query.begin(), g_module_chunk_query.begin()+3, "GET");
		}

		// The base files are referred to as core.
		module::modules core = {"core", "core", "core", {""}};

		std::vector<module::modules>& loaded_paths() {
			static std::vector<module::modules> result(1, core);
			return result;
		}

		const std::vector<std::string>& module_dirs() {
			static std::vector<std::string> result;
			if(result.empty()) {
				result.push_back("modules");
				result.push_back(preferences::dlc_path());
			}
			return result;
		}

		game_logic::ConstFormulaCallablePtr module_args;

	std::string core_module_name;
	}

	void set_core_module_name(const std::string& module_name)
	{
		core_module_name = module_name;
	}

	const std::string get_module_name(){
	ASSERT_LOG(core_module_name.empty() == false, "Do not have a module name set");
	return core_module_name;
	}

	const std::string get_module_pretty_name() {
		return loaded_paths().empty() ? "Frogatto" :  loaded_paths()[0].pretty_name_;
	}

	std::string get_module_version() {
		if(!loaded_paths().empty()) {
			const std::vector<int>& v = loaded_paths()[0].version_;
			if(v.empty()) {
				return "";
			}

			std::ostringstream s;
			s << v[0];
			for(unsigned n = 1; n < v.size(); ++n) {
				s << "." << v[n];
			}

			return s.str();
		} else {
			return "";
		}
	}

	std::string map_file(const std::string& passed_fname)
	{
		if(sys::is_path_absolute(passed_fname)) {
			return passed_fname;
		}

		std::string fname = passed_fname;
		std::string module_id;
		if(std::find(fname.begin(), fname.end(), ':') != fname.end()) {
			module_id = get_module_id(fname);
			fname = get_id(fname);
		}

		for(const modules& p : loaded_paths()) {
			if(module_id.empty() == false && module_id != p.name_) {
				continue;
			}

			for(const std::string& base_path : p.base_path_) {
				const std::string path = sys::find_file(base_path + fname);
				if(sys::file_exists(path)) {
					return path;
				}
			}
		}
		return fname;
	}

	std::string map_write_path(const std::string& fname, BASE_PATH_TYPE path_type)
	{
		if(sys::is_path_absolute(fname)) {
			return fname;
		}

		std::string module_id = get_module_name();
		std::string file = fname;
		if(std::find(fname.begin(), fname.end(), ':') != fname.end()) {
			module_id = get_module_id(fname);
			file = get_id(fname);
		}

		for(const modules& p : loaded_paths()) {
			if(module_id != p.name_) {
				continue;
			}

			std::string base_path = p.base_path_[path_type];
			std::string result = base_path + file;
			return result;
		}

		return file;
	}

	std::map<std::string, std::string>::const_iterator find(const std::map<std::string, std::string>& filemap, const std::string& name) {
		for(const modules& p : loaded_paths()) {
			std::map<std::string, std::string>::const_iterator itor = filemap.find(p.abbreviation_ + ":" + name);
			if(itor != filemap.end()) {
				return itor;
			}
			itor = filemap.find(name);
			if(itor != filemap.end()) {
				return itor;
			}
		}
		return filemap.end();
	}

	void get_unique_filenames_under_dir(const std::string& dir,
										std::map<std::string, std::string>* file_map,
										MODULE_PREFIX_BEHAVIOR prefix)
	{
		auto paths = loaded_paths();
		std::reverse(paths.begin(), paths.end());
		for(const modules& p : paths) {
			for(const std::string& base_path : p.base_path_) {
				const std::string path = base_path + dir;
				sys::get_unique_filenames_under_dir(path, file_map, prefix == MODULE_PREFIX ? p.abbreviation_ + ":" : "");
			}
		}
	}

	void get_all_filenames_under_dir(const std::string& dir,
										std::multimap<std::string, std::string>* file_map,
										MODULE_PREFIX_BEHAVIOR prefix)
	{
		auto paths = loaded_paths();
		std::reverse(paths.begin(), paths.end());
		for(const modules& p : paths) {
			for(const std::string& base_path : p.base_path_) {
				const std::string path = base_path + dir;
				sys::get_all_filenames_under_dir(path, file_map, prefix == MODULE_PREFIX ? p.abbreviation_ + ":" : "");
			}
		}
	}

	void get_files_in_dir(const std::string& dir,
						  std::vector<std::string>* files,
						  std::vector<std::string>* dirs)
	{
		for(const modules& p : loaded_paths()) {
			for(const std::string& base_path : p.base_path_) {
				const std::string path = base_path + dir;
				sys::get_files_in_dir(path, files, dirs);
			}
		}
	}

	void get_files_matching_wildcard(const std::string& pattern,
									 std::string* dir_out,
									 std::vector<std::string>* files)
	{
		ASSERT_LOG(pattern.empty() == false, "Empty pattern in wildcard search");
		std::string::const_iterator i = pattern.end()-1;
		while(i != pattern.begin() && *i != '/') {
			--i;
		}

		if(*i == '/') {
			++i;
		}

		const std::string dir(pattern.begin(), i);
		const std::string pattern_str(i, pattern.end());
		module::get_files_in_dir(dir, files);
		files->erase(std::remove_if(files->begin(), files->end(), [&pattern_str](const std::string& fname) { return util::wildcard_pattern_match(pattern_str, fname) == false; }), files->end());

		if(dir_out) {
			*dir_out = dir;
		}
	}

	std::string get_id(const std::string& id) {
		size_t cpos = id.find(':');
		if(cpos != std::string::npos) {
			return id.substr(cpos+1);
		}
		return id;
	}

	std::string get_module_id(const std::string& id) {
		size_t cpos = id.find(':');
		if(cpos != std::string::npos) {
			return id.substr(0, cpos);
		}
		return std::string();
	}

	std::string make_module_id(const std::string& name) {
		// convert string with path to module:filename syntax
		// e.g. vgi:wip/test1x.cfg -> vgi:test1x.cfg; test1.cfg -> vgi:test1.cfg 
		// (assuming vgi is default module loaded).
		std::string conv_name;
		std::string nn = name;
		size_t cpos = name.find(':');
		std::string modname = loaded_paths().front().abbreviation_;
		if(cpos != std::string::npos) {
			modname = name.substr(0, cpos);
			nn = name.substr(cpos+1);
		}
		size_t spos = nn.rfind('/');
		if(spos == std::string::npos) {
			spos = nn.rfind('\\');
		}
		if(spos != std::string::npos) {
			conv_name = modname + ":" + nn.substr(spos+1);
		} else {
			conv_name = modname + ":" + nn;
		}
		return conv_name;
	}

	void set_module_args(game_logic::ConstFormulaCallablePtr callable)
	{
		module_args = callable;
	}

	game_logic::ConstFormulaCallablePtr get_module_args()
	{
		return module_args;
	}


	std::vector<variant> getAll()
	{
		std::vector<variant> result;

		for(const std::string& path : module_dirs()) {
			std::vector<std::string> files, dirs;
			sys::get_files_in_dir(path, &files, &dirs);
			for(const std::string& dir : dirs) {
				std::string fname = path + "/" + dir + "/module.cfg";
				if(sys::file_exists(fname)) {
					variant v = json::parse_from_file_or_die(fname);
					v.add_attr(variant("id"), variant(dir));
					result.push_back(v);
				}
			}
		}

		return result;
	}

	variant get(const std::string& mod_file_name)
	{
		std::string name(mod_file_name);
		if(name.size() > 4 && name.substr(name.size()-4) == ".cfg") {
			name = name.substr(0, name.size()-4);
		}
	
		for(const std::string& path : module_dirs()) {
			std::string fname = path + "/" + name + "/module.cfg";
			LOG_INFO("LOOKING IN '" << fname << "': " << sys::file_exists(fname));
			if(sys::file_exists(fname)) {
				variant v = json::parse_from_file_or_die(fname);
				v.add_attr(variant("id"), variant(fname));
				return v;
			}
		}

		return variant();
	}

	const std::string& get_module_path(const std::string& abbrev, BASE_PATH_TYPE type) {
		if(abbrev == "") {
			// No abbreviation returns path of first loaded module.
			return loaded_paths().front().base_path_[type];
		}
		for(const modules& m : loaded_paths()) {
			if(m.abbreviation_ == abbrev || m.name_ == abbrev) {
				return m.base_path_[type];
			}
		}
		// If not found we return the path of the default module.
		// XXX may change this behaviour, depending on how it's seen in practice.
		return loaded_paths().front().base_path_[type];
	}

	const std::string make_base_module_path(const std::string& name) {
		std::string result;
		variant best_version;
		for(int i = 0; i != module_dirs().size(); ++i) {
			const std::string& path = module_dirs()[i];
			std::string full_path = path + "/" + name + "/";
			if(sys::file_exists(full_path + "module.cfg")) {
				variant config = json::parse(sys::read_file(full_path + "module.cfg"));
				variant version = config["version"];
				if(best_version.is_null() || version > best_version) {
					best_version = version;
					result = full_path;
				}

			}
		}

		if(result.empty() == false) {
			return result;
		}

		std::string path = module_dirs().back() + "/" + name + "/";
		sys::get_dir(path);
		return path;
	}

	const std::string make_user_module_path(const std::string& name) {
		preferences::expand_data_paths();
		const std::string user_data = preferences::user_data_path();
		return user_data + "/user_module_data/" + name + "/";
	}

	void load(const std::string& mod_file_name, bool initial)
	{
		std::string name(mod_file_name);
		if(name.size() > 4 && name.substr(name.size()-4) == ".cfg") {
			name = name.substr(0, name.size()-4);
		}
		std::string pretty_name = name;
		std::string abbrev = name;
		std::string fname = make_base_module_path(name) + "module.cfg";
		variant v = json::parse_from_file_or_die(fname);
		std::string def_font = "FreeSans";
		std::string def_font_cjk = "unifont";
		auto speech_dialog_bg_color = std::make_shared<KRE::Color>(85, 53, 53, 255);
		variant player_type;

		const std::string constants_path = make_base_module_path(name) + "data/constants.cfg";
		if(sys::file_exists(constants_path)) {
			const std::string contents = sys::read_file(constants_path);
			try {
				variant v = json::parse(contents, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
				new game_logic::ConstantsLoader(v);
			} catch(json::ParseError& e) {
				ASSERT_LOG(false, "Error parsing file: " << e.errorMessage());
			}
		}

		std::vector<int> module_version;

		if(v.is_map()) {
			ASSERT_LOG(v["min_engine_version"].is_null() == false, "A min_engine_version field in the module.cfg file must be specified.");
			ASSERT_LOG(v["min_engine_version"] <= preferences::version_decimal(), "The engine version being used (" << preferences::version_decimal()
				<< ") to run the module is older than required by the module (" << v["min_engine_version"] << ").");

			if(v["name"].is_null() == false) {
				pretty_name = v["name"].as_string();
			} else if(v["id"].is_null() == false) {
				pretty_name = v["id"].as_string();
			}
			if(v["abbreviation"].is_null() == false) {
				abbrev = v["abbreviation"].as_string();
			}

		if(v["custom_arguments"].is_null() == false) {
			ASSERT_LOG(v["custom_arguments"].is_map(), "custom_arguments in module.cfg is not a map. Found " << v["custom_arguments"].write_json() << " instead");

			for(auto p : v["custom_arguments"].as_map()) {
				preferences::register_module_setting(p.first.as_string(), p.second);
			}
		}

			if(v["dependencies"].is_null() == false) {
				if(v["dependencies"].is_string()) {
					load(v["dependencies"].as_string(), false);
				} else if( v["dependencies"].is_list()) {
					for(const std::string& modname : v["dependencies"].as_list_string()) {
						load(modname, false);
					}
				}
			}
			if(v.has_key("font")) {
				if(v["font"].is_string()) {
					def_font_cjk = def_font = v["font"].as_string();
				} else if(v["font"].is_list()) {
					if(v["font"].num_elements() == 1) {
						def_font_cjk = def_font = v["font"][0].as_string();
					} else if(v["font"].num_elements() == 2) {
						def_font = v["font"][0].as_string();
						def_font_cjk = v["font"][1].as_string();
					} else {
						ASSERT_LOG(false, "font tag must be either a list of one or two strings: " << v["font"].num_elements());
					}
				} else {
					ASSERT_LOG(false, "font tag must be either string or list of strings");
				}
			}
		if(v.has_key("speech_dialog_background_color")) {
			speech_dialog_bg_color = std::make_shared<KRE::Color>(v["speech_dialog_background_color"]);
		}
			if(v.has_key("build_requirements")) {
				const variant& br = v["build_requirements"];
				if(br.is_string()) {
					auto it = preferences::get_build_options().find(br.as_string());
					ASSERT_LOG(it != preferences::get_build_options().end(), 
						"Unsatisfied build requirement: " << br.as_string());
				} else if(br.is_list()) {
					std::vector<std::string> failed_reqs;
					for(int n = 0; n != br.num_elements(); ++n) {
						auto it = preferences::get_build_options().find(br[n].as_string());
						if(it == preferences::get_build_options().end()) {
							failed_reqs.push_back(br[n].as_string());
						}
					}
					if(failed_reqs.empty() == false) {
						std::stringstream str;
						for(auto fr : failed_reqs) {
							str << " " << fr;
						}
						ASSERT_LOG(false, "There are unsatisfied build requirements:" << str.str());
					}
				} else {
					ASSERT_LOG(false, "In module.cfg build_requirements must be string or list of strings: " << mod_file_name);
				}
			}

			if(v.has_key("player_type")) {
				player_type = v["player_type"];
			}

			if(v.has_key("version")) {
				module_version = v["version"].as_list_int();
			}

			if(v.has_key("validate_objects")) {
				CustomObjectType::addObjectValidationFunction(v["validate_objects"]);
			}
		}
		modules m = {name, pretty_name, abbrev,
					 {make_base_module_path(name), make_user_module_path(name)},
				def_font, def_font_cjk, speech_dialog_bg_color};
		m.default_preferences = v["default_preferences"];
		m.version_ = module_version;
		loaded_paths().insert(loaded_paths().begin(), m);

		if(initial) {
			CustomObjectType::setPlayerVariantType(player_type);
		}
	}

	std::string get_default_font()
	{
		return i18n::is_locale_cjk() ? loaded_paths().front().default_font_cjk : loaded_paths().front().default_font;
	}

	const KRE::ColorPtr& get_speech_dialog_bg_color()
	{
		return loaded_paths().front().speech_dialog_bg_color;
	}

	variant get_default_preferences()
	{
		if(loaded_paths().empty()) {
			return variant();
		} else {
			return loaded_paths().front().default_preferences;
		}
	}

	void reload(const std::string& name) {
		preferences::set_preferences_path_from_module(name);
		loaded_paths().clear();
		loaded_paths().push_back(core);
		load(name, true);
	}

	void get_module_list(std::vector<std::string>& dirs) {
		// Grab the files/directories under ./module/ for later use.
		std::vector<std::string> files;
		for(const std::string& path : module_dirs()) {
			sys::get_files_in_dir(path + "/", &files, &dirs);
		}
	}

	void load_module_from_file(const std::string& modname, modules* mod_) {
		variant v = json::parse_from_file_or_die("./modules/" + modname + "/module.cfg");
		ASSERT_LOG(mod_ != nullptr, "Invalid module pointer passed.");
		if(v.is_map()) {
			ASSERT_LOG(v["min_engine_version"].is_null() == false, "A min_engine_version field in the module.cfg file must be specified.");
			ASSERT_LOG(v["min_engine_version"] <= preferences::version_decimal(), "The engine version being used (" << preferences::version_decimal()
				<< ") to run the module is older than required by the module (" << v["min_engine_version"] << ").");

			if(v["id"].is_null() == false) {
				mod_->name_= v["id"].as_string();
			}
			if(v["name"].is_null() == false) {
				mod_->pretty_name_= v["name"].as_string();
			}
			if(v["abbreviation"].is_null() == false) {
				mod_->abbreviation_= v["abbreviation"].as_string();
			}
			if(v["dependencies"].is_string()) {
				mod_->included_modules_.push_back(v["dependencies"].as_string());
			} else if(v["dependencies"].is_list()) {
				for(const std::string& s : v["dependencies"].as_list_string()) {
					mod_->included_modules_.push_back(s);
				}
			}

			if(v["version"].is_list()) {
				mod_->version_ = v["version"].as_list_int();
			}
		}
	}

	void write_file(const std::string& mod_path, const std::string& data)
	{
		std::string path;
		std::string abbrev = get_module_id(mod_path);
		std::string rel_path = get_id(mod_path);
		// Write a file to a relative path inside a module. rel_path includes the file name.
		// e.g. module::write_file("", "data/object/experimental/bat.cfg", data);
		// If the current module was xxx, then the file would get written to the path
		// ./modules/xxx/data/object/experimental/bat.cfg	
		if(loaded_paths().empty()) {
			path = rel_path;
		} else {
			path = get_module_path(abbrev) + rel_path;
		}
		sys::write_file(path, data);
	}

	namespace {
	void get_files_in_module(const std::string& dir, std::vector<std::string>& res, const std::vector<std::string>& exclude_paths)
	{
		if(std::count(exclude_paths.begin(), exclude_paths.end(), dir)) {
			return;
		}

		if(dir.size() >= 4 && std::equal(dir.end()-4, dir.end(), ".git")) {
			return;
		}

		std::vector<std::string> files, dirs;
		sys::get_files_in_dir(dir, &files, &dirs);
		for(const std::string& d : dirs) {
			if(d == "" || d[0] == '.') {
				continue;
			}

			get_files_in_module(dir + "/" + d, res, exclude_paths);
		}

		for(const std::string& fname : files) {
			if(fname.empty() == false && fname[0] == '.') {
				continue;
			}

			res.push_back(dir + "/" + fname);
		}
	}

	bool is_valid_module_id(const std::string& id)
	{
		for(int n = 0; n != id.size(); ++n) {
			if(!isalpha(id[n]) && id[n] != '_' && id[n] != '-') {
				return false;
			}
		}

		return true;
	}
	}

	variant build_package(const std::string& id, bool increment_version, variant version_override, std::string path)
	{
		std::vector<std::string> files;
		if(path == "") {
			path = "modules/" + id;
		}

		ASSERT_LOG(sys::dir_exists(path), "COULD NOT FIND PATH: " << path);

		variant config;
	
		if(sys::file_exists(path + "/module.cfg")) {
			config = json::parse(sys::read_file(path + "/module.cfg"));
		}

		if(increment_version) {
			std::vector<int> version;
			if(version_override.is_list()) {
				version = version_override.as_list_int();
			} else {
				version = config["version"].as_list_int();
			}
			ASSERT_LOG(version.empty() == false, "Illegal version");
			version.back()++;
			config.add_attr(variant("version"), vector_to_variant(version));
			sys::write_file(path + "/module.cfg", config.write_json(true, variant::EXPANDED_LISTS));
		}

		std::vector<std::string> exclude_paths;
		if(config.has_key("exclude_paths")) {
			exclude_paths = config["exclude_paths"].as_list_string();
		}

		std::map<variant, variant> manifest_file;

		get_files_in_module(path, files, exclude_paths);
		std::map<variant, variant> file_attr;
		for(const std::string& file : files) {
			if(std::find(file.begin(), file.end(), ' ') != file.end()) {
				LOG_INFO("Ignoring file with invalid path: " << file);
				continue;
			}

			LOG_INFO("processing " << file << "...");
			std::string fname(file.begin() + path.size() + 1, file.end());
			std::map<variant, variant> attr;

			const std::string contents = sys::read_file(file);
			if(sys::is_file_executable(file)) {
				attr[variant("exe")] = variant::from_bool(true);
			}

			attr[variant("md5")] = variant(md5::sum(contents));
			attr[variant("size")] = variant(static_cast<int>(contents.size()));

			auto attr_copy = attr;

			manifest_file[variant(fname)] = variant(&attr_copy);

			std::vector<char> data(contents.begin(), contents.end());

			data = base64::b64encode(zip::compress(data));

			const std::string data_str(data.begin(), data.end());


			attr[variant("data")] = variant(data_str);

			file_attr[variant(fname)] = variant(&attr);
		}

		//now save the manifest file.
		{
			std::map<variant, variant> attr;
			const std::string contents = variant(&manifest_file).write_json();

			attr[variant("md5")] = variant(md5::sum(contents));
			attr[variant("size")] = variant(static_cast<int>(contents.size()));

			std::vector<char> data(contents.begin(), contents.end());

			data = base64::b64encode(zip::compress(data));

			const std::string data_str(data.begin(), data.end());

			attr[variant("data")] = variant(data_str);

			file_attr[variant("manifest.cfg")] = variant(&attr);
		}

		const std::string module_cfg_file = path + "/module.cfg";
		variant module_cfg = json::parse(sys::read_file(module_cfg_file));
		ASSERT_LOG(module_cfg["version"].is_list(), "IN " << module_cfg_file << " THERE MUST BE A VERSION NUMBER GIVEN AS A LIST OF INTEGERS");

		LOG_INFO("Verifying compression...");

		//this verifies that compression/decompression works but is slow.
		//	ASSERT_LOG(zip::decompress_known_size(base64::b64decode(base64::b64encode(zip::compress(data))), data.size()) == data, "COMPRESS/DECOMPRESS BROKEN");

		std::map<variant, variant> data_attr;
		data_attr[variant("id")] = variant(id);
		data_attr[variant("version")] = module_cfg["version"];
		data_attr[variant("name")] = module_cfg["name"];
		data_attr[variant("author")] = module_cfg["author"];
		data_attr[variant("description")] = module_cfg["description"];
		data_attr[variant("dependencies")] = module_cfg["dependencies"];
		data_attr[variant("manifest")] = variant(&file_attr);

		if(module_cfg.has_key("icon")) {
			const std::string icon_path = path + "/images/" + module_cfg["icon"].as_string();
			ASSERT_LOG(sys::file_exists(icon_path), "COULD NOT FIND ICON: " << icon_path);
			data_attr[variant("icon")] = variant(base64::b64encode(sys::read_file(icon_path)));
		}

		return variant(&data_attr);
	}

	bool uninstall_downloaded_module(const std::string& id)
	{
		if(!is_valid_module_id(id)) {
			ASSERT_LOG(false, "ILLEGAL MODULE ID: " << id);
			return false;
		}

		const std::string path_str = preferences::dlc_path() + "/" + id;
		sys::rmdir_recursive(path_str);
		return true;
	}

	namespace {
	void finish_upload(std::string response, bool* flag, std::string* result)
	{
		if(result) {
			*result = response;
		} else {
			LOG_INFO("UPLOAD COMPLETE " << response);
		}
		*flag = true;
	}

	void error_upload(std::string response, bool* flag)
	{
		LOG_ERROR("ERROR: " << response);
		*flag = true;
	}

	void upload_progress(int sent, int total, bool uploaded)
	{
		if(!uploaded) {
			LOG_INFO("SENT " << sent << "/" << total);
		} else {
			LOG_INFO("RECEIVED " << sent << "/" << total);
		}
	}

	}

COMMAND_LINE_UTILITY(generate_manifest)
{
	std::deque<std::string> arguments(args.begin(), args.end());
	ASSERT_LOG(arguments.size() >= 1 && arguments.size() <= 2, "Expected arguments: module_name [path override]");

	std::string module_id = arguments.front();

	std::string path_override;
	if(arguments.size() > 1) {
		path_override = arguments.back();
	}

	variant package = build_package(module_id, false, variant(), path_override);

	variant manifest = package["manifest"];
	ASSERT_LOG(manifest.is_map(), "Could not find manifest");

	for(auto p : manifest.as_map()) {
		p.second.remove_attr_mutation(variant("data"));
	}

	std::cout << manifest.write_json();
}

	COMMAND_LINE_UTILITY(replicate_module)
	{
		std::string server = g_module_server;
		std::string port = g_module_port;

		std::string src_module, dst_module;
		std::string upload_passcode;

		std::deque<std::string> arguments(args.begin(), args.end());
		while(!arguments.empty()) {
			const std::string arg = arguments.front();
			arguments.pop_front();
			if(arg == "--server") {
				ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
				server = arguments.front();
				arguments.pop_front();
			} else if(arg == "-p" || arg == "--port") {
				ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
				port = arguments.front();
				arguments.pop_front();
			} else if(arg == "--passcode") {
				ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
				upload_passcode = arguments.front();
				arguments.pop_front();
			} else {
				ASSERT_LOG(dst_module.empty(), "UNRECOGNIZED ARGUMENT: " << arg);

				if(src_module.empty()) {
					src_module = arg;
				} else {
					dst_module = arg;
				}
			}
		}

		ASSERT_LOG(dst_module.empty() == false, "Must specify source and dest modules");

		std::map<variant,variant> attr;

		attr[variant("type")] = variant("replicate_module");
		attr[variant("src_id")] = variant(src_module);
		attr[variant("dst_id")] = variant(dst_module);

		if(upload_passcode.empty() == false) {
			attr[variant("passcode")] = variant(upload_passcode);
		}

		const std::string msg = variant(&attr).write_json();

		bool done = false, error = false;
		std::string response;

		http_client client(server, port);
		client.send_request("POST /replicate_module", msg, 
							std::bind(finish_upload, _1, &done, &response),
							std::bind(error_upload, _1, &error),
							std::bind(upload_progress, _1, _2, _3));

		while(!done) {
			client.process();
			SDL_Delay(20);
			ASSERT_LOG(!error, "Error in upload");
		}

		variant response_doc(json::parse(response));
		if(response_doc["status"].as_string() != "ok") {
			ASSERT_LOG(false, "Error in replicating module: " << response);
		}
	}

	COMMAND_LINE_UTILITY(publish_module)
	{
		std::string path_override;
		std::string module_id;
		std::string module_id_override;
		std::string server = g_module_server;
		std::string port = g_module_port;
		std::string upload_passcode;
		bool increment_version = false;

		std::deque<std::string> arguments(args.begin(), args.end());
		while(!arguments.empty()) {
			const std::string arg = arguments.front();
			arguments.pop_front();
			if(arg == "--server") {
				ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
				server = arguments.front();
				arguments.pop_front();
			} else if(arg == "-p" || arg == "--port") {
				ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
				port = arguments.front();
				arguments.pop_front();
			} else if(arg == "--increment-version") {
				increment_version = true;
			} else if(arg == "--path-override") {
				ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
				path_override = arguments.front();
				arguments.pop_front();
			} else if(arg == "--module-id-override") {
				ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
				module_id_override = arguments.front();
				arguments.pop_front();
			} else if(arg == "--passcode") {
				ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
				upload_passcode = arguments.front();
				arguments.pop_front();

			} else {
				ASSERT_LOG(module_id.empty(), "UNRECOGNIZED ARGUMENT: " << arg);
				module_id = arg;
				ASSERT_LOG(std::count_if(module_id.begin(), module_id.end(), isalnum) + std::count(module_id.begin(), module_id.end(), '_') == module_id.size(), "ILLEGAL ARGUMENT: " << module_id);
			}
		}

		ASSERT_LOG(module_id.empty() == false, "MUST SPECIFY MODULE ID");

		variant version_on_server;

		if(increment_version) {
			std::map<variant,variant> attr;
			attr[variant("type")] = variant("query_module_version");
			attr[variant("module_id")] = variant(module_id);
			const std::string msg = variant(&attr).write_json();
			std::string response;
			bool done = false;
			bool error = false;

			http_client client(server, port);
			client.send_request("POST /upload_module", msg, 
								std::bind(finish_upload, _1, &done, &response),
								std::bind(error_upload, _1, &error),
								std::bind(upload_progress, _1, _2, _3));
			while(!done) {
				client.process();
				SDL_Delay(20);
				ASSERT_LOG(!error, "Error in upload");
			}

			variant response_doc(json::parse(response));
			if(response_doc["status"].as_string() != "ok") {
				ASSERT_LOG(false, "Error in querying module version " << response);
			}

			version_on_server = response_doc["version"];
		}

		variant package = build_package(module_id, increment_version, version_on_server, path_override);
		std::map<variant,variant> attr;

		attr[variant("type")] = variant("prepare_upload_module");
		attr[variant("module_id")] = variant(module_id);

		if(module_id_override != "") {
			attr[variant("module_id")] = variant(module_id_override);
			package.add_attr_mutation(variant("id"), variant(module_id_override));
		}

		{
			const std::string msg = variant(&attr).write_json();
			std::string response;
			bool done = false;
			bool error = false;

			http_client client(server, port);
			client.send_request("POST /upload_module", msg, 
								std::bind(finish_upload, _1, &done, &response),
								std::bind(error_upload, _1, &error),
								std::bind(upload_progress, _1, _2, _3));

			while(!done) {
				client.process();
				ASSERT_LOG(!error, "Error in upload");
				SDL_Delay(20);
			}

			variant response_doc(json::parse(response));
			if(response_doc["status"].as_string() != "ok") {
				ASSERT_LOG(false, "Error in acquiring lock to upload: " << response);
			}

			attr[variant("lock_id")] = response_doc["lock_id"];


			if(response_doc.has_key("manifest")) {
				variant their_manifest = response_doc["manifest"];
				variant our_manifest = package["manifest"];

				std::vector<variant> deletions_from_server;
				for(auto p : their_manifest.as_map()) {
					if(our_manifest.has_key(p.first) == false) {
						deletions_from_server.push_back(p.first);
					}
				}

				if(!deletions_from_server.empty()) {
					attr[variant("delete")] = variant(&deletions_from_server);
				}

				std::vector<variant> keys_to_delete;
				for(auto p : our_manifest.as_map()) {
					if(their_manifest.has_key(p.first) && their_manifest[p.first]["md5"] == p.second["md5"]) {
						keys_to_delete.push_back(p.first);
						LOG_INFO("File " << p.first.as_string() << " is unchanged, not uploading");
					} else if(their_manifest.has_key(p.first) == false) {
						LOG_INFO("File " << p.first.as_string() << " is new, uploading " << p.second["size"].as_int());
					} else {
						LOG_INFO("File " << p.first.as_string() << " has changed, uploading " << p.second["size"].as_int());

					}
				}

				for(auto key : keys_to_delete) {
					our_manifest.remove_attr_mutation(key);
				}

			}

		}


		attr[variant("type")] = variant("upload_module");
		attr[variant("module")] = package;

		if(upload_passcode.empty() == false) {
			attr[variant("passcode")] = variant(upload_passcode);
		}

		const std::string msg = variant(&attr).write_json();

		sys::write_file("./upload.txt", msg);

		bool done = false;

		std::string* response = nullptr;

		http_client client(server, port);
		client.send_request("POST /upload_module", msg, 
							std::bind(finish_upload, _1, &done, response),
							std::bind(error_upload, _1, &done),
							std::bind(upload_progress, _1, _2, _3));

		while(!done) {
			client.process();
			SDL_Delay(20);
		}
	}

	namespace 
	{
		bool valid_path_chars(char c)
		{
			static const char* AllowedChars = "(){}[]+./_-@";
			return isalnum(c) || strchr(AllowedChars, c);
		}

		bool is_module_path_valid(const std::string& str)
		{
			for(unsigned n = 1; n < str.size(); ++n) {
				//don't allow consecutive . characters.
				if(str[n] == '.' && str[n-1] == '.') {
					return false;
				}
			}

			static const char* AllowedChars = "(){}[]+@";

			return str.empty() == false && (isalnum(str[0]) || strchr(AllowedChars, str[0])) && std::count_if(str.begin(), str.end(), valid_path_chars) == str.size();
		}
	}

	client::client() : operation_(client::OPERATION_NONE),
	                   force_install_(false),
	                   host_(g_module_server), port_(g_module_port),
					   out_of_date_(false),
					   client_(new http_client(host_, port_)),
					   nbytes_transferred_(0),
					   nbytes_total_(-1),
					   nfiles_written_(0),
					   install_image_(false),
					   is_new_install_(true),
					   nchunk_errors_(0)
	{
		client_->set_timeout_and_retry();
	}

	client::client(const std::string& host, const std::string& port)
	  : operation_(client::OPERATION_NONE),
	    force_install_(false),
	    host_(host), port_(port), out_of_date_(false),
	    client_(new http_client(host, port)),
		nbytes_transferred_(0), nbytes_total_(0),
		nfiles_written_(0), install_image_(false),
		nchunk_errors_(0)
	{
		client_->set_timeout_and_retry();
	}

	client::~client()
	{
	}

	void client::prepare_install_module(const std::string& module_id, bool force)
	{
		install_module(module_id, force);
		operation_ = OPERATION_PREPARE_INSTALL;
	}

	bool client::module_prepared() const
	{
		return operation_ == OPERATION_PREPARE_INSTALL && pending_response_.empty() == false;
	}

namespace {
#ifdef __APPLE__
const char* InstallImagePath = "../../";
#else
const char* InstallImagePath = ".";
#endif

static const int ModuleProtocolVersion = 1;
}

	bool client::install_module(const std::string& module_id, bool force)
	{
		data_.clear();
		module_id_ = module_id;
		force_install_ = force;

		std::string current_path = module_path();

		if(!current_path.empty() && !force && sys::dir_exists(current_path + "/.git")) {
			LOG_INFO("Not installing module " << module_id << " because a git sync exists in " << current_path);
			operation_ = OPERATION_NONE;
			return false;
		}

		if(!current_path.empty() && !force && sys::file_exists(current_path + "/module.cfg")) {

			is_new_install_ = false;

			LOG_INFO("Querying version of module available on server");
			operation_ = OPERATION_QUERY_VERSION_FOR_INSTALL;

			std::string doc;
			const std::string url = "GET /module_version/" + module_id;
			client_->send_request(url, doc, 
							  std::bind(&client::on_response, this, _1),
							  std::bind(&client::on_error, this, _1, url, ""),
							  std::bind(&client::on_progress, this, _1, _2, _3));

			return true;
		} else {
			return install_module_confirmed_out_of_date(module_id);
		}
	}

	bool client::install_module_confirmed_out_of_date(const std::string& module_id)
	{
		operation_ = OPERATION_INSTALL;
		data_.clear();

		LOG_INFO("Requesting module '" << module_id << "'");

		const std::string url = "GET /module_data/" + module_id;
		client_->send_request(url, "", 
							  std::bind(&client::on_response, this, _1),
							  std::bind(&client::on_error, this, _1, url, ""),
							  std::bind(&client::on_progress, this, _1, _2, _3));
		return true;
	}

	std::string client::module_path() const
	{
		if(install_path_override_.empty() == false) {
			return install_path_override_;
		}

		return get_module_path(module_id_);
	}

	std::string client::get_module_path(const std::string& module_id) const
	{
		return install_image_ ? InstallImagePath : make_base_module_path(module_id);
	}

	void client::rate_module(const std::string& module_id, int rating, const std::string& review)
	{
		std::map<variant,variant> m;
		m[variant("type")] = variant("rate");
		m[variant("module_id")] = variant(module_id);
		m[variant("rating")] = variant(rating);
		if(review.empty() == false) {
			m[variant("review")] = variant(review);
		}
		operation_ = OPERATION_RATE;
		const std::string url = "POST /rate_module";
		const std::string doc = variant(&m).write_json();
		client_->send_request(url, doc,
							  std::bind(&client::on_response, this, _1),
							  std::bind(&client::on_error, this, _1, url, doc),
							  std::bind(&client::on_progress, this, _1, _2, _3));
	}

	void client::get_status()
	{
		data_.clear();
		operation_ = OPERATION_GET_STATUS;
		const std::string url = "GET /get_summary";
		const std::string doc = "";
		client_->send_request(url, doc,
							  std::bind(&client::on_response, this, _1),
							  std::bind(&client::on_error, this, _1, url, doc),
							  std::bind(&client::on_progress, this, _1, _2, _3));
	}

	bool client::process()
	{
		if(operation_ == OPERATION_NONE || operation_ == OPERATION_PENDING_INSTALL || (operation_ == OPERATION_PREPARE_INSTALL && module_prepared())) {
			return false;
		}

		client_->process();
		std::vector<boost::shared_ptr<http_client> > chunk_clients = chunk_clients_;
		for(auto c : chunk_clients) {
			c->process();
		}

		if(operation_ == OPERATION_NONE) {
			return false;
		}

		return true;
	}

	variant client::getValue(const std::string& key) const
	{
		if(key == "is_complete") {
			return variant(operation_ == OPERATION_NONE);
		} else if(key == "module_info") {
			return module_info_;
		} else if(key == "downloaded_modules") {
			std::vector<std::string> files, dirs;
			sys::get_files_in_dir(preferences::dlc_path(), &files, &dirs);
			std::vector<variant> result;
			for(const std::string& m : dirs) {
				result.push_back(variant(m));
			}

			return variant(&result);
		} else {
			std::map<std::string, variant>::const_iterator i = data_.find(key);
			if(i != data_.end()) {
				return i->second;
			} else {
				return variant();
			}
		} 
	}

	void client::complete_install()
	{
		ASSERT_LOG(is_pending_install(), "Trying to complete install when not pending");

		perform_install_from_doc(doc_pending_chunks_);
		doc_pending_chunks_ = variant();
	}

	void client::on_chunk_response(std::string chunk_url, variant node, boost::shared_ptr<http_client> client, std::string response)
	{
		if(g_module_chunk_deflate) {
			std::vector<char> data(response.begin(), response.end());
			auto v = zip::decompress(data);
			std::string(v.begin(), v.end()).swap(response);
		}

		//write a copy of the response for this file to the update cache.
		sys::write_file("update-cache/" + node["md5"].as_string(), response);

		auto progress_itor = chunk_progress_.find(chunk_url);
		if(progress_itor != chunk_progress_.end()) {
			nbytes_transferred_ -= progress_itor->second;
			chunk_progress_.erase(progress_itor);
		}

		nbytes_transferred_ += node["size"].as_int();

		onChunkReceived(node);

		chunk_clients_.erase(std::remove(chunk_clients_.begin(), chunk_clients_.end(), client), chunk_clients_.end());
		if(chunks_to_get_.empty()) {
			if(chunk_clients_.empty()) {
				operation_ = OPERATION_PENDING_INSTALL;
			}
		} else {
			boost::shared_ptr<http_client> new_client(new http_client(g_module_chunk_server.empty() ? host_ : g_module_chunk_server, g_module_chunk_port.empty() ? port_ : g_module_chunk_port));
			new_client->set_timeout_and_retry();

			variant chunk = chunks_to_get_.back();
			chunks_to_get_.pop_back();

			variant_builder request;
			request.add("type", "download_chunk");
			request.add("chunk_id", chunk["md5"]);

			LOG_INFO("Module request chunk: " << chunk["md5"].as_string() << "\n");

			const std::string url = g_module_chunk_query + chunk["md5"].as_string();
			const std::string doc = module_chunk_query_is_get() ? "" : request.build().write_json();
			new_client->send_request(url, doc,
						  std::bind(&client::on_chunk_response, this, url, chunk, new_client, _1),
						  std::bind(&client::on_chunk_error, this, _1, url, doc, chunk, new_client),
						  std::bind(&client::on_chunk_progress, this, url, _1, _2, _3)
			);

			chunk_clients_.push_back(new_client);
		}
	}

	void client::on_chunk_progress(std::string chunk_url, size_t received, size_t total, bool response)
	{
		auto progress_itor = chunk_progress_.find(chunk_url);
		if(progress_itor != chunk_progress_.end()) {
			nbytes_transferred_ -= progress_itor->second;
			chunk_progress_.erase(progress_itor);
		}

		nbytes_transferred_ += received;
		chunk_progress_[chunk_url] = received;
	}

	void client::on_chunk_error(std::string response, std::string url, std::string doc, variant chunk, boost::shared_ptr<http_client> client)
	{
		auto progress_itor = chunk_progress_.find(url);
		if(progress_itor != chunk_progress_.end()) {
			nbytes_transferred_ -= progress_itor->second;
			chunk_progress_.erase(progress_itor);
		}

		LOG_INFO("Chunk error: " << chunk.write_json() << " errors = " << nchunk_errors_ << "\n");

		chunk_clients_.erase(std::remove(chunk_clients_.begin(), chunk_clients_.end(), client), chunk_clients_.end());
		++nchunk_errors_;
		if (nchunk_errors_ > 128)
		{
			LOG_INFO("Failed too many chunks, aborting\n");
			on_error(response, url, doc);
		}
		else
		{
			boost::shared_ptr<http_client> new_client(new http_client(host_, port_));
			new_client->set_timeout_and_retry();

			new_client->send_request(url, doc,
				std::bind(&client::on_chunk_response, this, url, chunk, new_client, _1),
				std::bind(&client::on_chunk_error, this, _1, url, doc, chunk, new_client),
				std::bind(&client::on_chunk_progress, this, url, _1, _2, _3)
			);

			chunk_clients_.push_back(new_client);
		}
	}

	void client::on_response(std::string response)
	{
		LOG_INFO("module client response received. Mode = " << static_cast<int>(operation_));

		try {
			variant doc = json::parse(response, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
			if(doc[variant("status")] != variant("ok") && doc[variant("manifest")].is_null()) {
				if(doc[variant("out_of_date")].as_bool(false)) {
					on_error(doc[variant("message")].as_string(), "", "");
					out_of_date_ = true;
					operation_ = OPERATION_NONE;
					return;
				}

				on_error(doc[variant("status")].as_string(), "", "");

				LOG_ERROR("SET ERROR: " << doc.write_json());
			} else if(operation_ == OPERATION_QUERY_VERSION_FOR_INSTALL) {
				variant version = doc[variant("version")];
				std::string current_path = module_path();
				variant config = json::parse(sys::read_file(current_path + "/module.cfg"));
				LOG_INFO("Server has module version " << version.write_json() << " we have " << config["version"].write_json());
				if(version == config["version"]) {
					operation_ = OPERATION_NONE;
					LOG_INFO("You already have the newest version of this module. Use --force to force download.");
					return;
				} else {
					install_module_confirmed_out_of_date(module_id_);
					return;
				}

			} else if(operation_ == OPERATION_INSTALL) {

				operation_ = OPERATION_NONE;

				perform_install(doc);
				return;

			} else if(operation_ == OPERATION_PREPARE_INSTALL) {
				pending_response_ = response;

			} else if(operation_ == OPERATION_GET_STATUS) {
				module_info_ = doc[variant("summary")];

				std::vector<variant> needed_icons;
				for(variant m : module_info_.getKeys().as_list()) {
					variant icon = module_info_[m][variant("icon")];
					if(icon.is_string()) {
						const std::string icon_path = std::string(preferences::user_data_path()) + "/tmp_images/" + icon.as_string() + ".png";
						if(!sys::file_exists(icon_path)) {
							needed_icons.push_back(icon);
						}

						variant item = module_info_[m];
						item.add_attr_mutation(variant("icon"), variant("#" + icon.as_string() + ".png"));
					}
				}

				if(needed_icons.empty() == false) {
					std::map<variant, variant> request;
					request[variant("type")] = variant("query_globs");
					request[variant("keys")] = variant(&needed_icons);
					operation_ = OPERATION_GET_ICONS;
					const std::string url = "POST /query_globs";
					const std::string doc = variant(&request).write_json();
					client_->send_request(url, doc,
							  std::bind(&client::on_response, this, _1),
							  std::bind(&client::on_error, this, _1, url, doc),
							  std::bind(&client::on_progress, this, _1, _2, _3));
					return;
				}
				LOG_INFO("FINISH GET. SET STATUS");
			} else if(operation_ == OPERATION_GET_ICONS) {
				for(variant k : doc.getKeys().as_list()) {
					const std::string key = k.as_string();
					if(key.size() != 32) {
						continue;
					}

					const std::string icon_path = std::string(preferences::user_data_path()) + "/tmp_images/" + key + ".png";
					sys::write_file(icon_path, base64::b64decode(doc[k].as_string()));
				}
			} else if(operation_ == OPERATION_RATE) {
				//pass
			} else {
				ASSERT_LOG(false, "UNKNOWN MODULE CLIENT STATE");
			}
		} catch(...) {
			data_["error"] = variant("Could not parse response");
		}

		operation_ = OPERATION_NONE;
	}

	void client::perform_install(const variant& doc_ref)
	{
		variant doc = doc_ref;

		variant local_manifest;
		std::string current_path = module_path();
		if(!current_path.empty() && !force_install_ && sys::file_exists(current_path + "/module.cfg") && sys::file_exists(current_path + "/manifest.cfg")) {
			local_manifest = json::parse(sys::read_file(current_path + "/manifest.cfg"));
			LOG_INFO("Parsed local manifest");
		}

		std::vector<variant> unchanged_keys;

		static const variant md5_variant("md5");

		std::vector<variant> high_priority_chunks;

		variant manifest = doc["manifest"];

		LOG_INFO("Searching cache for existing files...");

		int last_progress_update = SDL_GetTicks();

		int nfound_in_cache = 0;


		int ncount = 0;
		for(auto p : manifest.as_map()) {
			++ncount;

			if(SDL_GetTicks() > last_progress_update+50) {
				last_progress_update = SDL_GetTicks();
				show_progress(formatter() << "Checking cache: " << ncount << "/" << manifest.as_map().size());
			}

			if(local_manifest.is_map() && local_manifest.has_key(p.first) && local_manifest[p.first][md5_variant] == p.second[md5_variant]) {
				unchanged_keys.push_back(p.first);
				continue;
			}

			bool cached = false;

			std::string cached_fname = "update-cache/" + p.second["md5"].as_string();
			if(p.second["data"].is_null() && sys::file_exists(cached_fname)) {
				std::string contents = sys::read_file(cached_fname);
				std::vector<char> data_buf(contents.begin(), contents.end());
				const int data_size = p.second["size"].as_int();

				std::vector<char> data = zip::decompress_known_size(base64::b64decode(data_buf), data_size);
				std::string data_str(data.begin(), data.end());

				if(variant(md5::sum(data_str)) == p.second["md5"]) {
					LOG_INFO("Cached data found for " << p.second["md5"].as_string());
					cached = true;
					++nfound_in_cache;
				} else {
					LOG_INFO("ERROR: CACHE INVALID FOR " << p.second["md5"].as_string());
					sys::remove_file(cached_fname);
				}

			}
			
			if(cached || p.second["data"].is_null() == false) {
				isHighPriorityChunk(p.first, p.second);
				onChunkReceived(p.second);
			} else {
				nbytes_total_ += p.second["size"].as_int();

				if(isHighPriorityChunk(p.first, p.second)) {
					high_priority_chunks.push_back(p.second);
				} else {
					chunks_to_get_.push_back(p.second);
				}
			}
		}

		LOG_INFO("Found " << nfound_in_cache << " files in cache");

		for(auto v : high_priority_chunks) {
			chunks_to_get_.push_back(v);
		}

		if(local_manifest.is_map()) {
			std::vector<variant> keys_to_delete;
			for(auto p : local_manifest.as_map()) {
				if(manifest.has_key(p.first) == false) {
					keys_to_delete.push_back(p.first);
				}
			}

			doc.add_attr_mutation(variant("delete"), variant(&keys_to_delete));
		}

		for(variant k : unchanged_keys) {
			manifest.remove_attr_mutation(k);
		}

		LOG_INFO("Getting chunks: " << chunks_to_get_.size());


		if(chunks_to_get_.empty() == false) {
			doc_pending_chunks_ = doc;

			while(chunk_clients_.size() < 8 && chunks_to_get_.empty() == false) {
				variant chunk = chunks_to_get_.back();
				chunks_to_get_.pop_back();

				variant_builder request;
				request.add("type", "download_chunk");
				request.add("chunk_id", chunk["md5"]);

				boost::shared_ptr<http_client> client(new http_client(g_module_chunk_server.empty() ? host_ : g_module_chunk_server, g_module_chunk_port.empty() ? port_ : g_module_chunk_port));
				client->set_timeout_and_retry();

				const std::string url = g_module_chunk_query + chunk["md5"].as_string();
				const std::string doc = module_chunk_query_is_get() ? "" : request.build().write_json();
				client->send_request(url, doc, 
							  std::bind(&client::on_chunk_response, this, url, chunk, client, _1),
							  std::bind(&client::on_chunk_error, this, _1, url, doc, chunk, client),
							  std::bind(&client::on_chunk_progress, this, url, _1, _2, _3)
				);
				chunk_clients_.push_back(client);
			}

			operation_ = OPERATION_GET_CHUNKS;
		} else {
			perform_install_from_doc(doc);
		}
	}

	void client::perform_install_from_doc(variant doc)
	{
		if(doc.has_key("delete")) {
			for(variant path : doc["delete"].as_list()) {
				const std::string path_str = module_path() + "/" + path.as_string();
				LOG_INFO("DELETING FILE: " << path_str);

				try {
					if(!sys::is_file_writable(path_str)) {
						sys::set_file_writable(path_str);
					}

					sys::remove_file(path_str);
				} catch(boost::filesystem::filesystem_error& e) {
					LOG_ERROR("FAILED TO DELETE FILE: " << path_str);
				}
			}
		}

		variant manifest = doc["manifest"];
		for(variant path : manifest.getKeys().as_list()) {
			const std::string path_str = path.as_string();
			ASSERT_LOG(is_module_path_valid(path_str), "INVALID PATH IN MODULE: " << path_str);
		}

		variant full_manifest;

		LOG_INFO("Install files: " << (int)manifest.getKeys().as_list().size());

		int last_draw = 0;

		show_progress(formatter() << "Installing " << module_description_ << " files: 0/" << manifest.getKeys().as_list().size());
		last_draw = SDL_GetTicks();

		int ncount = 0;

		for(variant path : manifest.getKeys().as_list()) {
			++ncount;
			const int new_time = SDL_GetTicks();
			if(new_time > last_draw+50) {
				last_draw = new_time;
				show_progress(formatter() << "Installing " << module_description_ << " files: " << ncount << "/" << manifest.getKeys().as_list().size());
			}

			variant info = manifest[path];
			std::string path_str = (install_image_ ? InstallImagePath : module_path()) + "/" + path.as_string();

			if(install_image_ && sys::file_exists(path_str)) {
				//try removing the file, and failing that, move it.
				try {
					sys::remove_file(path_str);
				} catch(...) {
					LOG_WARN("Failed to remove " << path_str);

					try {
						sys::move_file(path_str, path_str + ".tmp");
					} catch(...) {
						LOG_INFO("Failed to move: " << path_str);
						if(path.as_string() == "anura.exe") {
							//For the windows executable if we fail to move it we try
							//an alternative name.
							for(int i = 0; i != 10; ++i) {
								std::ostringstream s;
								s << "anura" << i << ".exe";
								const std::string candidate_path_str = (install_image_ ? InstallImagePath : preferences::dlc_path() + "/" + module_id_) + "/" + s.str();
								try {
									if(sys::file_exists(candidate_path_str)) {
										sys::remove_file(candidate_path_str);
									}
									path_str = candidate_path_str;
									break;
								} catch(...) {
								}
							}
						}
					}
				}
			}

			std::vector<char> data_buf;
			{
				std::string data_str;
				if(info["data"].is_null()) {
					const std::string cache_path = "update-cache/" + info["md5"].as_string();
					data_str = sys::read_file(cache_path);

					if(data_str.empty() && sys::file_exists(cache_path) == false) {
						ASSERT_LOG(false, "Could not find data for " << info["md5"].as_string());
					}
				} else {
					data_str = info["data"].as_string();
				}
				data_buf.insert(data_buf.begin(), data_str.begin(), data_str.end());
			}
			const int data_size = info["size"].as_int();

			std::vector<char> data = zip::decompress_known_size(base64::b64decode(data_buf), data_size);

			LOG_INFO("CREATING FILE AT " << path_str);

			std::string contents(data.begin(), data.end());
			ASSERT_LOG(variant(md5::sum(contents)) == info["md5"], "md5 sum for " << path.as_string() << " does not match");

			try {
				sys::write_file(path_str, contents);
			} catch(boost::filesystem::filesystem_error& e) {
				bool fixed = false;
				try {
					if(!sys::is_file_writable(path_str)) {
						sys::set_file_writable(path_str);
						sys::write_file(path_str, contents);
						fixed = true;
					}
				} catch(boost::filesystem::filesystem_error& e) {
				}

				ASSERT_LOG(fixed, "Could not write file: " << path_str);
			}

			if(info["exe"].as_bool(false)) {
				sys::set_file_executable(path_str);
			}

			if(path.as_string() == "manifest.cfg") {
				full_manifest = json::parse(contents);
			}

			++nfiles_written_;
		}

		//if we downloaded a full manifest of all files, make sure that
		//locally all the files we already had are copied appropriately.
		if(full_manifest.is_null() == false && install_image_ == false) {
			ncount = 0;
			for(variant path : full_manifest.getKeys().as_list()) {
				++ncount;
				if(manifest.has_key(path)) {
					//we just downloaded this file.
					continue;
				}

				const std::string path_str = module_path() + "/" + path.as_string();
				if(sys::file_exists(path_str)) {
					continue;
				}

				const int new_time = SDL_GetTicks();
				if(new_time > last_draw+50) {
					last_draw = new_time;
					show_progress(formatter() << "Checking files: " << ncount << "/" << manifest.getKeys().as_list().size());
				}

				variant info = manifest[path];
			
				bool found = false;
				for(auto dir : module_dirs()) {
					std::string src_path = dir + "/" + module_id_ + "/" + path.as_string();
					if(sys::file_exists(src_path)) {
						std::string contents = sys::read_file(src_path);
						if(md5::sum(contents) != full_manifest[path]["md5"].as_string()) {
							ASSERT_LOG(false, "Trying to source file from existing repo but md5 does not match the manifest: " << src_path << " -> " << path.as_string());
						}

						LOG_INFO("copy file from existing source: " << src_path << " -> " << path_str);
						sys::write_file(path_str, contents);
						found = true;
					}
				}

				ASSERT_LOG(found, "Could not find file locally even though it's in the manifest: " << path.as_string());
			}
		}

		//update the module.cfg version to be equal to the version of the module we now have.
		variant new_module_version = doc["version"];

		const std::string module_cfg_path = (install_image_ ? InstallImagePath : preferences::dlc_path() + "/" + module_id_) + "/module.cfg";

		bool wrote_version = false;
		if(sys::file_exists(module_cfg_path)) {
			try {
				variant node = json::parse(sys::read_file(module_cfg_path), json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
				node.add_attr_mutation(variant("version"), new_module_version);
				sys::write_file(module_cfg_path, node.write_json());
				wrote_version = true;
			} catch(...) {
			}
		}

		if(!wrote_version) {
			std::map<variant,variant> m;
			m[variant("version")] = new_module_version;
			variant node(&m);
			sys::write_file(module_cfg_path, node.write_json());
		}
	}

	void client::on_error(std::string response, std::string url, std::string doc)
	{
		LOG_INFO("client error: " << response << " (" << url << ")");
		error_ = response;
		data_["error"] = variant(response);
		operation_ = OPERATION_NONE;
	}

	void client::on_progress(int transferred, int total, bool uploaded)
	{
		if(uploaded) {
			data_["kbytes_transferred"] = variant(transferred/1024);
			data_["kbytes_total"] = variant(total/1024);
			nbytes_transferred_ = transferred;
			nbytes_total_ = total;
		}
	}

	COMMAND_LINE_UTILITY(install_module)
	{

		std::string module_id;
		std::string server = g_module_server;
		std::string port = g_module_port;
		bool force = false;

		std::deque<std::string> arguments(args.begin(), args.end());
		while(!arguments.empty()) {
			const std::string arg = arguments.front();
			arguments.pop_front();
			if(arg == "--server") {
				ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
				server = arguments.front();
				arguments.pop_front();
			} else if(arg == "-p" || arg == "--port") {
				ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
				port = arguments.front();
				arguments.pop_front();
			} else if(arg == "--force") {
				force = true;
			} else if(arg.empty() == false && arg[0] != '-') {
				module_id = arg;
			} else {
				ASSERT_LOG(false, "UNRECOGNIZED ARGUMENT: '" << arg << "'");
			}
		}

		ffl::IntrusivePtr<client> cl(new client(server, port));

		cl->install_module(module_id, force);
		int nbytes_transferred = 0;

		while(cl->process()) {
			if(nbytes_transferred != cl->nbytes_transferred()) {
				nbytes_transferred = cl->nbytes_transferred();
				LOG_INFO("TRANSFER: " << nbytes_transferred/1024 << "/" << cl->nbytes_total()/1024 << " kbytes");
			}
		}

		ASSERT_LOG(cl->error().empty(), "Could not download module: " << cl->error());
	}

	COMMAND_LINE_UTILITY(publish_module_stats)
	{
		std::string module_id;

		std::string server = g_module_server;
		std::string port = g_module_port;

		std::deque<std::string> arguments(args.begin(), args.end());
		while(!arguments.empty()) {
			const std::string arg = arguments.front();
			arguments.pop_front();
			if(arg == "--server") {
				ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
				uri::uri url = uri::uri::parse(arguments.front());
				arguments.pop_front();
				server = url.host();
				port = url.port();
			} else {
				ASSERT_LOG(module_id.empty(), "UNRECOGNIZED ARGUMENT: " << module_id);
				module_id = arg;
				ASSERT_LOG(std::count_if(module_id.begin(), module_id.end(), isalnum) + std::count(module_id.begin(), module_id.end(), '_') == module_id.size(), "ILLEGAL ARGUMENT: " << module_id);
			}
		}

		ASSERT_LOG(module_id.empty() == false, "MUST SPECIFY MODULE ID");

		const std::string path = "modules/" + module_id + "/stats-server.json";
		ASSERT_LOG(sys::file_exists(path), "DID NOT FIND STATS FILE DEFINITION AT " << path);

		const variant doc = json::parse(sys::read_file(path));

		std::map<variant,variant> attr;
		attr[variant("type")] = variant("upload_table_definitions");
		attr[variant("module")] = variant(module_id);
		attr[variant("definition")] = doc;

		const std::string msg = variant(&attr).write_json();

		bool done = false;

		std::string* response = nullptr;

		http_client client(server, port);
		client.send_request("POST /stats", msg, 
							std::bind(finish_upload, _1, &done, response),
							std::bind(error_upload, _1, &done),
							std::bind(upload_progress, _1, _2, _3));

		while(!done) {
			client.process();
			SDL_Delay(20);
		}
	}

	COMMAND_LINE_UTILITY(list_modules)
	{
		std::string server = g_module_server;
		std::string port = g_module_port;

		std::deque<std::string> arguments(args.begin(), args.end());
		while(!arguments.empty()) {
			const std::string arg = arguments.front();
			arguments.pop_front();
			if(arg == "--server") {
				ASSERT_LOG(arguments.empty() == false, "NEED ARGUMENT AFTER " << arg);
				uri::uri url = uri::uri::parse(arguments.front());
				arguments.pop_front();
				server = url.host();
				port = url.port();
			} else {
				ASSERT_LOG(false, "UNRECOGNIZED ARGUMENT: " << arg);
			}
		}

		bool done = false;

		std::string response;

		http_client client(server, port);
		client.send_request("GET /get_summary", "", 
							std::bind(finish_upload, _1, &done, &response),
							std::bind(error_upload, _1, &done),
							std::bind(upload_progress, _1, _2, _3));

		while(!done) {
			client.process();
			SDL_Delay(20);
		}

		LOG_INFO("RESPONSE:\n" << response);
	}

	COMMAND_LINE_UTILITY(get_module_version)
	{
		std::string server = g_module_server;
		std::string port = g_module_port;
		std::deque<std::string> arguments(args.begin(), args.end());

		ASSERT_LOG(arguments.size() == 1, "Usage: <module>");

		std::map<variant,variant> attr;
		attr[variant("type")] = variant("query_module_version");
		attr[variant("module_id")] = variant(args[0]);

		std::string msg = variant(&attr).write_json();

		bool done = false;

		std::string response;

		http_client client(server, port);
		client.send_request("POST /query_module_version", msg, 
							std::bind(finish_upload, _1, &done, &response),
							std::bind(error_upload, _1, &done),
							std::bind(upload_progress, _1, _2, _3));

		while(!done) {
			client.process();
			SDL_Delay(20);
		}

		printf("Response: %s\n", response.c_str());
	}

	COMMAND_LINE_UTILITY(set_module_label)
	{
		std::string server = g_module_server;
		std::string port = g_module_port;
		std::deque<std::string> arguments(args.begin(), args.end());

		ASSERT_LOG(arguments.size() == 3, "Usage: <module> <label> <version>");

		std::map<variant,variant> attr;
		attr[variant("type")] = variant("set_module_label");
		attr[variant("module_id")] = variant(args[0]);
		attr[variant("label")] = variant(args[1]);
		attr[variant("version")] = json::parse(args[2]);

		std::string msg = variant(&attr).write_json();

		bool done = false;

		std::string response;

		http_client client(server, port);
		client.send_request("POST /set_module_label", msg, 
							std::bind(finish_upload, _1, &done, &response),
							std::bind(error_upload, _1, &done),
							std::bind(upload_progress, _1, _2, _3));

		while(!done) {
			client.process();
			SDL_Delay(20);
		}

		printf("Response: %s\n", response.c_str());
	}

}

