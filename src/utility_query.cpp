/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "formula.hpp"
#include "json_tokenizer.hpp"
#include "json_parser.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "variant_callable.hpp"
#include "variant_utils.hpp"

using namespace json;
using namespace game_logic;

namespace {

typedef std::pair<const char*, const char*> StringRange;

StringRange get_list_element_range(const char* i1, const char* i2)
{
	StringRange result(i1, i2);

	int nbracket = 0;
	Token token = get_token_full(i1, i2);
	if(token.type == Token::TYPE_COMMA) {
		token = get_token_full(i1, i2);
	}
	result.first = token.begin;
	Token prev = token;
	while(nbracket > 0 || (token.type != Token::TYPE_RSQUARE && token.type != Token::TYPE_COMMA)) {
		ASSERT_NE(token.type, Token::NUM_TYPES);
		switch(token.type) {
		case Token::TYPE_RCURLY:
		case Token::TYPE_RSQUARE:
			--nbracket;
			break;
		case Token::TYPE_LCURLY:
		case Token::TYPE_LSQUARE:
			++nbracket;
			break;
		default:
			break;
		}

		prev = token;
		token = get_token_full(i1, i2);
	}

	result.second = prev.end;

	return result;
}

struct NameValuePairLocs {
	std::string::const_iterator begin_name, end_name, begin_value, end_value, end_comma;
	bool has_comma;
};

NameValuePairLocs
find_pair_range(const std::string& contents, int line, int col, variant key) {
	ASSERT_LOG(key.get_debug_info(), "NO DEBUG INFO");

	std::string::const_iterator i1 = contents.begin();
	while(i1 != contents.end() && (line < key.get_debug_info()->line || col < key.get_debug_info()->column)) {
		if(*i1 == '\n') {
			col = 1;
			++line;
		} else {
			++col;
		}

		++i1;
	}

	NameValuePairLocs result = { i1, i1, i1, i1, i1, false };

	ASSERT_LOG(i1 != contents.end(), "COULD NOT FIND LOCATION FOR " << key << ": " << line << ", " << col << ": " << key.get_debug_info()->line << ", " << key.get_debug_info()->column << ": " << contents);

	const char* ptr = &*i1;
	const char* end_ptr = contents.c_str() + contents.size();
	const char* prev = ptr;
	int nbracket = 0;
	Token token = get_token_full(ptr, end_ptr);

	result.end_name = contents.begin() + (token.end - contents.c_str());

	bool begun_value = false;

	while(nbracket > 0 || (token.type != Token::TYPE_COMMA && token.type != Token::TYPE_RCURLY && token.type != Token::TYPE_RSQUARE)) {

		switch(token.type) {
		case Token::TYPE_RCURLY:
		case Token::TYPE_RSQUARE:
			--nbracket;
			break;
		case Token::TYPE_LCURLY:
		case Token::TYPE_LSQUARE:
			++nbracket;
			break;
		default:
			break;
		}

		ASSERT_LOG(token.type != Token::NUM_TYPES, "UNEXPECTED END");
		prev = token.end;
		token = get_token_full(ptr, end_ptr);

		if(!begun_value && token.type != Token::TYPE_COLON) {
			ASSERT_LOG(token.type != Token::NUM_TYPES, "UNEXPECTED END");
			begun_value = true;
			result.begin_value = contents.begin() + (token.begin - contents.c_str());
		}
	}

	ptr = prev;

	result.end_value = contents.begin() + (ptr - contents.c_str());
	result.has_comma = token.type == Token::TYPE_COMMA;
	if(result.has_comma) {
		result.end_comma = contents.begin() + (token.end - contents.c_str());
	} else {
		result.end_comma = result.end_value;
	}

	return result;
}

void advance_line_col(std::string::const_iterator i1, std::string::const_iterator i2, int& line, int& col)
{
	while(i1 != i2) {
		if(*i1 == '\n') {
			col = 1;
			++line;
		} else {
			++col;
		}

		++i1;
	}
}

struct Modification {
	Modification(int begin, int end, const std::string& ins)
	  : begin_pos(begin), end_pos(end), insert(ins)
	{}
	int begin_pos, end_pos;
	std::string insert;

	void apply(std::string& target) const {
		target.erase(target.begin() + begin_pos, target.begin() + end_pos);
		target.insert(target.begin() + begin_pos, insert.begin(), insert.end());
	}

	bool operator<(const Modification& m) const {
		return begin_pos > m.begin_pos;
	}
};

}

std::string modify_variant_text(const std::string& contents, variant original, variant v, int line, int col, std::string indent) {
	if(v == original) {
		return contents;
	}

	std::vector<Modification> mods;

	if(v.is_map() && original.is_map()) {
		std::map<variant,variant> old_map = original.as_map(), new_map = v.as_map();
		foreach(const variant_pair& item, old_map) {
			std::map<variant,variant>::const_iterator itor = new_map.find(item.first);
			if(itor != new_map.end()) {
				if(itor->second == item.second) {
					continue;
				}

				//modify value.
				NameValuePairLocs range = find_pair_range(contents, line, col, item.first);
				std::string new_contents(range.begin_value, range.end_value);
				int l = line, c = col;
				advance_line_col(contents.begin(), range.begin_value, l, c);

				new_contents = modify_variant_text(new_contents, item.second, itor->second, l, c, indent + "\t") + (range.has_comma ? "" : ",");

				mods.push_back(Modification(range.begin_value - contents.begin(), range.end_value - contents.begin(), new_contents));
			} else {
				//delete value
				NameValuePairLocs range = find_pair_range(contents, line, col, item.first);
				mods.push_back(Modification(range.begin_name - contents.begin(), range.end_comma - contents.begin(), ""));
			}
		}

		foreach(const variant_pair& item, new_map) {
			if(old_map.count(item.first)) {
				continue;
			}

			ASSERT_LOG(item.first.is_string(), "ERROR: NON-STRING VALUE ADDED: " << item.first);

			std::string name_str = item.first.as_string();
			foreach(char c, name_str) {
				if(!util::c_isalpha(c) && c != '_') {
					name_str = "\"" + name_str + "\"";
					break;
				}
			}

			const char* begin = contents.c_str();
			const char* end = contents.c_str() + contents.size();
			Token t = get_token_full(begin, end);
			ASSERT_LOG(t.type == Token::TYPE_LCURLY, "UNEXPECTED TOKEN AT START OF MAP");
			std::ostringstream s;
			s << "\n" << indent << name_str << ": ";
			item.second.write_json_pretty(s, indent + "\t");
			s << ",\n";

			mods.push_back(Modification(t.end - contents.c_str(), t.end - contents.c_str(), s.str()));
		}
	} else if(v.is_list() && original.is_list()) {
		std::vector<variant> a = original.as_list();
		std::vector<variant> b = v.as_list();
		if(!a.empty() && a.size() <= b.size()) {
			std::vector<StringRange> ranges;
			StringRange range = get_list_element_range(contents.c_str()+1, contents.c_str() + contents.size());
			ranges.push_back(range);
			while(ranges.size() < a.size()) {
				range = get_list_element_range(ranges.back().second, contents.c_str() + contents.size());
				ranges.push_back(range);
			}

			std::string element_spacing;
			if(a.size() >= 2) {
				element_spacing = std::string(ranges[0].second, ranges[1].first);
			}

			for(int n = 0; n != a.size(); ++n) {
				if(a[n] == b[n]) {
					continue;
				}

				int l = line, c = col;
				advance_line_col(contents.begin(), contents.begin() + (ranges[n].first - contents.c_str()), l, c);
				std::string str = modify_variant_text(std::string(ranges[n].first, ranges[n].second), a[n], b[n], l, c, indent + "\t");
				mods.push_back(Modification(ranges[n].first - contents.c_str(), ranges[n].second - contents.c_str(), str));
			}

			std::ostringstream s;

			indent += "\t";
			for(int n = a.size(); n < b.size(); ++n) {
				s << ",";
				if(b[n].is_list() || b[n].is_map()) {
					s << "\n" << indent;
				} else {
					s << element_spacing;
				}

				b[n].write_json_pretty(s, indent);
				
			}

			indent.resize(indent.size()-1);
			mods.push_back(Modification(ranges.back().second - contents.c_str(), ranges.back().second - contents.c_str(), s.str()));
		} else {
			std::ostringstream s;
			v.write_json_pretty(s, indent);
			return s.str();
		}
	} else {
		std::ostringstream s;
		v.write_json_pretty(s, indent);
		return s.str();
	}
	
	std::string result = contents;
	std::sort(mods.begin(), mods.end());
	foreach(const Modification& mod, mods) {
		mod.apply(result);
	}

	return result;
}

namespace {

const_formula_ptr formula_;

void execute_command(variant cmd, variant obj, const std::string& fname)
{
	if(cmd.try_convert<variant_callable>()) {
		cmd = cmd.try_convert<variant_callable>()->get_value();
	}

	if(cmd.is_list()) {
		foreach(variant v, cmd.as_list()) {
			execute_command(v, obj, fname);
		}
	} else if(cmd.try_convert<game_logic::command_callable>()) {
		cmd.try_convert<game_logic::command_callable>()->run_command(*obj.try_convert<formula_callable>());
	} else if(cmd.as_bool()) {
		std::cout << cmd.write_json() << "\n";
	}
}

void process_file(const std::string& fname, std::map<std::string,std::string>& file_mappings)
{
	static const std::string Postfix = ".cfg";
	if(fname.size() <= Postfix.size() || std::string(fname.end()-Postfix.size(),fname.end()) != Postfix) {
		return;
	}

	variant original = json::parse(sys::read_file(fname), JSON_NO_PREPROCESSOR);
	variant v = original;

	variant obj = variant_callable::create(&v);

	boost::intrusive_ptr<map_formula_callable> map_callable(new map_formula_callable(obj.try_convert<formula_callable>()));
	map_callable->add("doc", v);
	map_callable->add("filename", variant(fname));

	variant result = formula_->execute(*map_callable);
	execute_command(result, obj, fname);
	if(result.as_bool()) {
		//std::cout << fname << ": " << result.write_json() << "\n";
	}

	if(original != v) {
		std::string contents = sys::read_file(fname);
		std::string new_contents = modify_variant_text(contents, original, v, 1, 1, "");
		try {
			json::parse(new_contents, JSON_NO_PREPROCESSOR);
		} catch(json::parse_error& e) {
			ASSERT_LOG(false, "ERROR: MODIFIED DOCUMENT " << fname << " COULD NOT BE PARSED. FILE NOT WRITTEN: " << e.error_message() << "\n" << new_contents);
		}

		file_mappings[fname] = new_contents;
		std::cerr << "file " << fname << " has changes\n";
	}
}

void process_dir(const std::string& dir, std::map<std::string, std::string>& file_mappings, std::vector<std::string>& error_files)
{
	std::vector<std::string> subdirs, files;
	sys::get_files_in_dir(dir, &files, &subdirs);
	foreach(const std::string& d, subdirs) {
		process_dir(dir + "/" + d, file_mappings, error_files);
	}

	foreach(const std::string& fname, files) {
		try {
			process_file(dir + "/" + fname, file_mappings);
		} catch(json::parse_error& e) {
			std::cerr << "FAILED TO PARSE " << fname << "\n";
		} catch(type_error& e) {
			std::cerr << "TYPE ERROR PARSING " << fname << "\n";
			error_files.push_back(fname);
		} catch(validation_failure_exception& e) {
			std::cerr << "ERROR PARSING " << fname << ": " << e.msg << "\n";
			error_files.push_back(fname);
		}
	}
}

}

COMMAND_LINE_UTILITY(query)
{
	if(args.size() != 2) {
		std::cerr << "USAGE: <dir> <formula>\n";
		return;
	}

	std::vector<std::string> error_files;
	std::map<std::string, std::string> file_mappings;

	formula_.reset(new formula(variant(args[1])));
	if(args[0].size() > 4 && std::string(args[0].end()-4,args[0].end()) == ".cfg") {
		process_file(args[0], file_mappings);
	} else {
		const assert_recover_scope scope;
		process_dir(args[0], file_mappings, error_files);
	}

	if(error_files.empty()) {
		std::cerr << "ALL FILES PROCESSED OKAY. APPLYING MODIFICATIONS TO " << file_mappings.size() << " FILES\n";
		for(std::map<std::string, std::string>::const_iterator i = file_mappings.begin(); i != file_mappings.end(); ++i) {
			sys::write_file(i->first, i->second);
			std::cerr << "WROTE " << i->first << "\n";
		}
	} else {
		std::cerr << "ERRORS IN " << error_files.size() << " FILES. NO CHANGES MADE\n";
	}
}
