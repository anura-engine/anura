/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <deque>

#include <boost/bind.hpp>

#include "asserts.hpp"
#include "base64.hpp"
#include "compress.hpp"
#include "custom_object_type.hpp"
#include "i18n.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "formula_constants.hpp"
#if !defined(NO_TCP)
#include "http_client.hpp"
#endif
#include "json_parser.hpp"
#include "md5.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "uri.hpp"
#include "variant_utils.hpp"

namespace module {

namespace {

// This will disappear when frogatto is moved to it's on module, then it becomes "core", "core", "core".
module::modules core = {"core", "core", "core", ""};

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

game_logic::const_formula_callable_ptr module_args;
}

const std::string get_module_name(){
	return loaded_paths().empty() ? "frogatto" : loaded_paths()[0].name_;
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
		for(int n = 1; n < v.size(); ++n) {
			s << "." << v[n];
		}

		return s.str();
	} else {
		return "";
	}
}

std::string map_file(const std::string& passed_fname)
{
	std::string fname = passed_fname;
	std::string module_id;
	if(std::find(fname.begin(), fname.end(), ':') != fname.end()) {
		module_id = get_module_id(fname);
		fname = get_id(fname);
	}

	foreach(const modules& p, loaded_paths()) {
		if(module_id.empty() == false && module_id != p.name_) {
			continue;
		}

		foreach(const std::string& base_path, p.base_path_) {
			const std::string path = sys::find_file(base_path + fname);
			if(sys::file_exists(path)) {
				return path;
			}
		}
	}
	return fname;
}

std::map<std::string, std::string>::const_iterator find(const std::map<std::string, std::string>& filemap, const std::string& name) {
	foreach(const modules& p, loaded_paths()) {
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
	foreach(const modules& p, loaded_paths()) {
		foreach(const std::string& base_path, p.base_path_) {
			const std::string path = base_path + dir;
			sys::get_unique_filenames_under_dir(path, file_map, prefix == MODULE_PREFIX ? p.abbreviation_ + ":" : "");
		}
	}
}

void get_files_in_dir(const std::string& dir,
                      std::vector<std::string>* files,
                      std::vector<std::string>* dirs)
{
	foreach(const modules& p, loaded_paths()) {
		foreach(const std::string& base_path, p.base_path_) {
			const std::string path = base_path + dir;
			sys::get_files_in_dir(path, files, dirs);
		}
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

void set_module_args(game_logic::const_formula_callable_ptr callable)
{
	module_args = callable;
}

game_logic::const_formula_callable_ptr get_module_args()
{
	return module_args;
}


std::vector<variant> get_all()
{
	std::vector<variant> result;

	foreach(const std::string& path, module_dirs()) {
		std::vector<std::string> files, dirs;
		sys::get_files_in_dir(path, &files, &dirs);
		foreach(const std::string& dir, dirs) {
			std::string fname = path + "/" + dir + "/module.cfg";
			if(sys::file_exists(fname)) {
				variant v = json::parse_from_file(fname);
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
	
	foreach(const std::string& path, module_dirs()) {
		std::string fname = path + "/" + name + "/module.cfg";
		std::cerr << "LOOKING IN '" << fname << "': " << sys::file_exists(fname) << "\n";
		if(sys::file_exists(fname)) {
			variant v = json::parse_from_file(fname);
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
	foreach(const modules& m, loaded_paths()) {
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
	variant v = json::parse_from_file(fname);
	std::string def_font = "FreeSans";
	std::string def_font_cjk = "unifont";
	variant player_type;

	const std::string constants_path = make_base_module_path(name) + "data/constants.cfg";
	if(sys::file_exists(constants_path)) {
		const std::string contents = sys::read_file(constants_path);
		variant v = json::parse(contents);
		new game_logic::constants_loader(v);
	}

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
		if(v["dependencies"].is_null() == false) {
			if(v["dependencies"].is_string()) {
				load(v["dependencies"].as_string(), false);
			} else if( v["dependencies"].is_list()) {
				foreach(const std::string& modname, v["dependencies"].as_list_string()) {
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
	}
	modules m = {name, pretty_name, abbrev,
	             {make_base_module_path(name), make_user_module_path(name)},
				def_font, def_font_cjk};
	m.default_preferences = v["default_preferences"];
	loaded_paths().insert(loaded_paths().begin(), m);

	if(initial) {
		custom_object_type::set_player_variant_type(player_type);
	}
}

std::string get_default_font()
{
	return i18n::is_locale_cjk() ? loaded_paths().front().default_font_cjk : loaded_paths().front().default_font;
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
	foreach(const std::string& path, module_dirs()) {
		sys::get_files_in_dir(path + "/", &files, &dirs);
	}
}

void load_module_from_file(const std::string& modname, modules* mod_) {
	variant v = json::parse_from_file("./modules/" + modname + "/module.cfg");
	ASSERT_LOG(mod_ != NULL, "Invalid module pointer passed.");
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
			foreach(const std::string& s, v["dependencies"].as_list_string()) {
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
	foreach(const std::string& d, dirs) {
		get_files_in_module(dir + "/" + d, res, exclude_paths);
	}

	foreach(const std::string& fname, files) {
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

#if !defined(NO_TCP) || !defined(NO_MODULES)
variant build_package(const std::string& id, bool increment_version)
{
	std::vector<std::string> files;
	const std::string path = "modules/" + id;
	ASSERT_LOG(sys::dir_exists(path), "COULD NOT FIND PATH: " << path);

	variant config = json::parse(sys::read_file(path + "/module.cfg"));
	if(increment_version) {
		std::vector<int> version = config["version"].as_list_int();
		ASSERT_LOG(version.empty() == false, "Illegal version");
		version.back()++;
		config.add_attr(variant("version"), vector_to_variant(version));
		sys::write_file(path + "/module.cfg", config.write_json());
	}

	std::vector<std::string> exclude_paths;
	if(config.has_key("exclude_paths")) {
		exclude_paths = config["exclude_paths"].as_list_string();
	}

	std::map<variant, variant> manifest_file;

	get_files_in_module(path, files, exclude_paths);
	std::map<variant, variant> file_attr;
	foreach(const std::string& file, files) {
		std::cerr << "processing " << file << "...\n";
		std::string fname(file.begin() + path.size() + 1, file.end());
		std::map<variant, variant> attr;

		const std::string contents = sys::read_file(file);

		attr[variant("md5")] = variant(md5::sum(contents));
		attr[variant("size")] = variant(contents.size());

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
		attr[variant("size")] = variant(contents.size());

		std::vector<char> data(contents.begin(), contents.end());

		data = base64::b64encode(zip::compress(data));

		const std::string data_str(data.begin(), data.end());

		attr[variant("data")] = variant(data_str);

		file_attr[variant("manifest.cfg")] = variant(&attr);
	}

	const std::string module_cfg_file = path + "/module.cfg";
	variant module_cfg = json::parse(sys::read_file(module_cfg_file));
	ASSERT_LOG(module_cfg["version"].is_list(), "IN " << module_cfg_file << " THERE MUST BE A VERSION NUMBER GIVEN AS A LIST OF INTEGERS");

	fprintf(stderr, "Verifying compression...\n");

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
		std::cerr << "UPLOAD COMPLETE " << response << "\n";
	}
	*flag = true;
}

void error_upload(std::string response, bool* flag)
{
	std::cerr << "ERROR: " << response << "\n";
	*flag = true;
}

void upload_progress(int sent, int total, bool uploaded)
{
	if(!uploaded) {
		std::cerr << "SENT " << sent << "/" << total << "\n";
	} else {
		std::cerr << "RECEIVED " << sent << "/" << total << "\n";
	}
}

}

COMMAND_LINE_UTILITY(publish_module)
{
	std::string module_id;
	std::string server = "theargentlark.com";
	std::string port = "23455";
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
		} else {
			ASSERT_LOG(module_id.empty(), "UNRECOGNIZED ARGUMENT: " << module_id);
			module_id = arg;
			ASSERT_LOG(std::count_if(module_id.begin(), module_id.end(), isalnum) + std::count(module_id.begin(), module_id.end(), '_') == module_id.size(), "ILLEGAL ARGUMENT: " << module_id);
		}
	}

	ASSERT_LOG(module_id.empty() == false, "MUST SPECIFY MODULE ID");

	const variant package = build_package(module_id, increment_version);
	std::map<variant,variant> attr;
	attr[variant("type")] = variant("upload_module");
	attr[variant("module")] = package;

	const std::string msg = variant(&attr).write_json();

	bool done = false;

	sys::write_file("./upload.txt", msg);
	std::string* response = NULL;

	http_client client(server, port);
	client.send_request("POST /upload_module", msg, 
	                    boost::bind(finish_upload, _1, &done, response),
	                    boost::bind(error_upload, _1, &done),
	                    boost::bind(upload_progress, _1, _2, _3));

	while(!done) {
		client.process();
	}
}

namespace {

bool valid_path_chars(char c)
{
	return isalnum(c) || c == '.' || c == '/' || c == '_' || c == '-';
}

bool is_module_path_valid(const std::string& str)
{
	for(int n = 1; n < str.size(); ++n) {
		//don't allow consecutive . characters.
		if(str[n] == '.' && str[n-1] == '.') {
			return false;
		}
	}

	return str.empty() == false && isalnum(str[0]) && std::count_if(str.begin(), str.end(), valid_path_chars) == str.size();
}
}

client::client() : operation_(client::OPERATION_NONE),
                   client_(new http_client("theargentlark.com", "23455"))
{
	get_status();
}

client::client(const std::string& host, const std::string& port)
  : operation_(client::OPERATION_NONE), client_(new http_client(host, port))
{
	get_status();
}

void client::install_module(const std::string& module_id)
{
	data_.clear();
	operation_ = OPERATION_INSTALL;
	module_id_ = module_id;
	client_->send_request("GET /download_module?module_id=" + module_id, "",
	                      boost::bind(&client::on_response, this, _1),
	                      boost::bind(&client::on_error, this, _1),
	                      boost::bind(&client::on_progress, this, _1, _2, _3));
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
	client_->send_request("POST /rate_module", variant(&m).write_json(),
	                      boost::bind(&client::on_response, this, _1),
	                      boost::bind(&client::on_error, this, _1),
	                      boost::bind(&client::on_progress, this, _1, _2, _3));
}

void client::get_status()
{
	data_.clear();
	operation_ = OPERATION_GET_STATUS;
	client_->send_request("GET /get_summary", "",
	                      boost::bind(&client::on_response, this, _1),
	                      boost::bind(&client::on_error, this, _1),
	                      boost::bind(&client::on_progress, this, _1, _2, _3));
}

bool client::process()
{
	if(operation_ == OPERATION_NONE) {
		return false;
	}

	client_->process();
	return true;
}

variant client::get_value(const std::string& key) const
{
	if(key == "is_complete") {
		return variant(operation_ == OPERATION_NONE);
	} else if(key == "module_info") {
		return module_info_;
	} else if(key == "downloaded_modules") {
		std::vector<std::string> files, dirs;
		sys::get_files_in_dir(preferences::dlc_path(), &files, &dirs);
		std::vector<variant> result;
		foreach(const std::string& m, dirs) {
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

void client::on_response(std::string response)
{
	try {
		std::cerr << "GOT RESPONSE: " << response << "\n";
		variant doc = json::parse(response, json::JSON_NO_PREPROCESSOR);
		if(doc[variant("status")] != variant("ok")) {
			data_["error"] = doc[variant("message")];
			std::cerr << "SET ERROR: " << doc.write_json() << "\n";
		} else if(operation_ == OPERATION_INSTALL) {
			ASSERT_LOG(doc["status"].as_string() == "ok", "COULD NOT DOWNLOAD MODULE: " << doc["message"]);

			variant module_data = doc["module"];

			std::vector<char> data_buf;
			{
				const std::string data_str = module_data["data"].as_string();
				data_buf.insert(data_buf.begin(), data_str.begin(), data_str.end());
			}
			const int data_size = module_data["data_size"].as_int();
			std::cerr << "DATA: " << module_data["data"].as_string().size() << " " << data_size << "\n";

			std::vector<char> data = zip::decompress_known_size(base64::b64decode(data_buf), data_size);

			variant manifest = module_data["manifest"];
			foreach(variant path, manifest.get_keys().as_list()) {
				const std::string path_str = path.as_string();
				ASSERT_LOG(is_module_path_valid(path_str), "INVALID PATH IN MODULE: " << path_str);
			}

			foreach(variant path, manifest.get_keys().as_list()) {
				variant info = manifest[path];
				const std::string path_str = preferences::dlc_path() + "/" + module_id_ + "/" + path.as_string();
				const int begin = info["begin"].as_int();
				const int end = begin + info["size"].as_int();
				ASSERT_LOG(begin >= 0 && end >= 0 && begin <= data.size() && end <= data.size(), "INVALID PATH INDEXES FOR " << path_str << ": " << begin << "," << end << " / " << data.size());

				std::cerr << "CREATING FILE AT " << path_str << "\n";
				sys::write_file(path_str, std::string(data.begin() + begin, data.begin() + end));
			}

		} else if(operation_ == OPERATION_GET_STATUS) {
			module_info_ = doc[variant("summary")];

			std::vector<variant> needed_icons;
			foreach(variant m, module_info_.get_keys().as_list()) {
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
				client_->send_request("POST /query_globs", variant(&request).write_json(),
	                      boost::bind(&client::on_response, this, _1),
	                      boost::bind(&client::on_error, this, _1),
	                      boost::bind(&client::on_progress, this, _1, _2, _3));
				return;
			}
			std::cerr << "FINISH GET. SET STATUS\n";
		} else if(operation_ == OPERATION_GET_ICONS) {
			foreach(variant k, doc.get_keys().as_list()) {
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

void client::on_error(std::string response)
{
	data_["error"] = variant(response);
	operation_ = OPERATION_NONE;
}

void client::on_progress(int transferred, int total, bool uploaded)
{
	if(uploaded) {
		data_["kbytes_transferred"] = variant(transferred/1024);
		data_["kbytes_total"] = variant(total/1024);
	}
}

COMMAND_LINE_UTILITY(install_module)
{
	std::string module_id;
	std::string server = "theargentlark.com";
	std::string port = "23455";
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

	variant_builder request;
	request.add("type", "download_module");
	request.add("module_id", module_id);

	std::string version_str;
	std::string current_path = make_base_module_path(module_id);
	if(!current_path.empty() && !force && sys::file_exists(current_path + "/module.cfg")) {
		variant config = json::parse(sys::read_file(current_path + "/module.cfg"));
		request.add("current_version", config["version"]);
	}

	if(!current_path.empty() && !force && sys::file_exists(current_path + "/manifest.cfg")) {
		request.add("manifest", json::parse(sys::read_file(current_path + "/manifest.cfg")));
	}

	std::cerr << "Requesting module '" << module_id << "'\n";

	bool done = false;

	std::string response;

	http_client client(server, port);
	client.send_request("POST /download_module?module_id=" + module_id + version_str, request.build().write_json(), 
	                    boost::bind(finish_upload, _1, &done, &response),
	                    boost::bind(error_upload, _1, &done),
	                    boost::bind(upload_progress, _1, _2, _3));

	while(!done) {
		client.process();
	}

	variant doc;
	
	try {
		doc = json::parse(response, json::JSON_NO_PREPROCESSOR);
	} catch(json::parse_error& e) {
		sys::write_file("./download.txt", response);
		ASSERT_LOG(false, "Failed to parse: " << e.error_message());
	}

	if(doc["status"].as_string() == "no_newer_module") {
		fprintf(stderr, "You already have the newest version of this module. Use --force to force download.\n");
		return;
	}

	ASSERT_LOG(doc["status"].as_string() == "ok", "COULD NOT DOWNLOAD MODULE: " << doc["message"]);

	variant module_data = doc["module"];

	variant manifest = module_data["manifest"];
	foreach(variant path, manifest.get_keys().as_list()) {
		const std::string path_str = path.as_string();
		ASSERT_LOG(is_module_path_valid(path_str), "INVALID PATH IN MODULE: " << path_str);
	}

	foreach(variant path, manifest.get_keys().as_list()) {
		variant info = manifest[path];
		const std::string path_str = preferences::dlc_path() + "/" + module_id + "/" + path.as_string();
		std::vector<char> data_buf;
		{
			const std::string data_str = info["data"].as_string();
			data_buf.insert(data_buf.begin(), data_str.begin(), data_str.end());
		}
		const int data_size = info["size"].as_int();

		std::vector<char> data = zip::decompress_known_size(base64::b64decode(data_buf), data_size);

		std::cerr << "CREATING FILE AT " << path_str << "\n";

		std::string contents(data.begin(), data.end());
		ASSERT_LOG(variant(md5::sum(contents)) == info["md5"], "md5 sum for " << path.as_string() << " does not match");
		sys::write_file(path_str, contents);
	}
}

COMMAND_LINE_UTILITY(publish_module_stats)
{
	std::string module_id;

	std::string server = "theargentlark.com";
	std::string port = "23455";

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

	std::string* response = NULL;

	http_client client(server, port);
	client.send_request("POST /stats", msg, 
	                    boost::bind(finish_upload, _1, &done, response),
	                    boost::bind(error_upload, _1, &done),
	                    boost::bind(upload_progress, _1, _2, _3));

	while(!done) {
		client.process();
	}
}

COMMAND_LINE_UTILITY(list_modules)
{
	std::string server = "theargentlark.com";
	std::string port = "23455";

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
	                    boost::bind(finish_upload, _1, &done, &response),
	                    boost::bind(error_upload, _1, &done),
	                    boost::bind(upload_progress, _1, _2, _3));

	while(!done) {
		client.process();
	}

	std::cerr << "RESPONSE:\n\n" << response;
}
#endif // NO_TCP

}

