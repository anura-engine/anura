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

#include <map>
#include <vector>

#include "asserts.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "stats_server.hpp"

namespace 
{
	using namespace game_logic;

	std::map<std::string, std::vector<variant> > g_raw_entries;

	class TableInfo
	{
	public:
		explicit TableInfo(const variant& v);
		const std::string& name() const { return name_; }
		bool is_global() const { return is_global_; }

		variant init_value() const { return init_value_; }
		variant calculate_key(const variant& msg, const FormulaCallable& context_callable) const;
		variant calculate_value(const variant& msg, const variant& current_value) const;
	private:
		std::string name_;
		bool is_global_;

		ConstFormulaPtr key_;
		ConstFormulaPtr value_;
		variant init_value_;
	};

	TableInfo::TableInfo(const variant& v)
	  : name_(v["name"].as_string()),
		is_global_(v["global_scope"].as_bool()),
		key_(Formula::createOptionalFormula(v["key"])),
		value_(Formula::createOptionalFormula(v["value"])),
		init_value_(v["init_value"])
	{
	}

	namespace 
	{
		class variant_callable : public FormulaCallable {
			variant var_;
			variant getValue(const std::string& key) const override {
				return var_[variant(key)];
			}
		public:
			explicit variant_callable(const variant& v) : var_(v)
			{}
		};
	}

	variant TableInfo::calculate_key(const variant& msg, const FormulaCallable& context_callable) const
	{
		if(key_) {
			variant_callable* v = new variant_callable(msg);
			variant holder(v);
			FormulaCallableWithBackup* callable = new FormulaCallableWithBackup(*v, context_callable);
			variant callable_holder(callable);
			return key_->execute(*callable);
		} else {
			return variant();
		}
	}

	variant TableInfo::calculate_value(const variant& msg, const variant& current_value) const
	{
		if(value_) {
			MapFormulaCallable* callable = new MapFormulaCallable;
			const variant scope(callable);
			callable->add("value", current_value);
			callable->add("sample", msg);
			variant result = value_->execute(*callable);
			return result;
		} else {
			if(current_value.is_int() || current_value.is_null()) {
				return variant(current_value.as_int()+1);
			} else {
				return current_value;
			}
		}
	}

	//the tables that are used for a single message type.
	struct msg_type_info 
	{
		msg_type_info() : record_all(false) {}
		std::string name;
		std::vector<TableInfo> tables;
		bool record_all;
	};

	// module id -> message id -> tables for that message.
	std::map<std::string, std::map<std::string, msg_type_info> > message_type_index;

	typedef std::map<variant, variant> table;

	variant output_table(const table& t) {
		std::vector<variant> v;
		for(table::const_iterator i = t.begin(); i != t.end(); ++i) {
			std::map<variant, variant> m;
			m[variant("key")] = i->first;
			m[variant("value")] = i->second;
			v.push_back(variant(&m));
		}

		return variant(&v);
	}

	table read_table(const variant& v) {
		table result;
		for(int n = 0; n != v.num_elements(); ++n) {
			result.insert(std::pair<variant, variant>(v[n]["key"], v[n]["value"]));
		}

		return result;
	}

	struct table_set {
		int total_count;
		std::map<std::string, table> tables;
	};

	typedef std::map<std::string, table_set> type_data_map;

	variant output_type_data_map(const type_data_map& m) {
		std::vector<variant> type_vec;
		for(type_data_map::const_iterator i = m.begin(); i != m.end(); ++i) {
			std::map<variant, variant> obj;
			obj[variant("type")] = variant(i->first);
			obj[variant("total")] = variant(i->second.total_count);

			std::vector<variant> tables;
			for(std::map<std::string, table>::const_iterator j = i->second.tables.begin(); j != i->second.tables.end(); ++j) {
				std::map<variant, variant> table_obj;
				table_obj[variant("name")] = variant(j->first);
				table_obj[variant("entries")] = output_table(j->second);
				tables.push_back(variant(&table_obj));
			}

			obj[variant("tables")] = variant(&tables);

			type_vec.push_back(variant(&obj));
		}

		return variant(&type_vec);
	}

	type_data_map read_type_data_map(variant v) {
		type_data_map result;
		for(int n = 0; n != v.num_elements(); ++n) {
			const variant& obj = v[n];

			table_set ts;
			ts.total_count = obj["total"].as_int();

			const variant& tables_v = obj["tables"];
			for(int m = 0; m != tables_v.num_elements(); ++m) {
				const std::string table_name = tables_v[m]["name"].as_string();

				ts.tables[table_name] = read_table(tables_v[m]["entries"]);
			}

			result[obj["type"].as_string()] = ts;
		}

		return result;
	}

	struct version_data {
		type_data_map global_data;
		std::map<std::string, type_data_map> level_to_data;
	};

	version_data read_version_data(variant v)
	{
		version_data result;
		variant keys = v.getKeys();
		for(int n = 0; n != keys.num_elements(); ++n) {
			if(keys[n].as_string() == "_GLOBAL_") {
				result.global_data = read_type_data_map(v[keys[n]]);
			} else {
				result.level_to_data[keys[n].as_string()] = read_type_data_map(v[keys[n]]);
			}
		}

		return result;
	}

	variant write_version_data(const version_data& d)
	{
		std::map<variant, variant> result;
		result[variant("_GLOBAL_")] = output_type_data_map(d.global_data);
		for(std::map<std::string, type_data_map>::const_iterator i = d.level_to_data.begin(); i != d.level_to_data.end(); ++i) {
			result[variant(i->first)] = output_type_data_map(i->second);
		}

		return variant(&result);
	}

	//keyed by version, module, module version.
	std::map<std::vector<std::string>, version_data> data_table;

	variant write_data_table()
	{
		std::map<variant, variant> result;
		for(std::map<std::vector<std::string>, version_data>::const_iterator i = data_table.begin(); i != data_table.end(); ++i) {
			std::vector<variant> k;
			for(const std::string& s : i->first) {
				k.push_back(variant(s));
			}
			result[variant(&k)] = write_version_data(i->second);
		}

		return variant(&result);
	}

	void read_data_table(variant v)
	{
		data_table.clear();
		variant keys = v.getKeys();
		for(int n = 0; n != keys.num_elements(); ++n) {
			data_table[keys[n].as_list_string()] = read_version_data(v[keys[n]]);
		}
	}
}

void init_tables(const variant& doc)
{
	for(const variant module : doc.getKeys().as_list()) {
		init_tables_for_module(module.as_string(), doc[module]);
	}
}

namespace 
{
	std::map<variant,variant> module_definitions;
	std::map<std::string,std::string> module_errors;
}

void init_tables_for_module(const std::string& module, const variant& doc)
{
	for(int n = 0; n != doc.num_elements(); ++n) {
		variant v = doc[n];
		msg_type_info& info = message_type_index[module][v["name"].as_string()];
		info.name = v["name"].as_string();
		variant tables_v = v["tables"];
		for(int m = 0; m != tables_v.num_elements(); ++m) {
			info.tables.push_back(TableInfo(tables_v[m]));
		}

		if(v["record_all"].as_bool(false)) {
			info.record_all = true;
		}
	}

	module_definitions[variant(module)] = doc;
}

variant get_tables_definition()
{
	std::map<variant,variant> clone = module_definitions;
	return variant(&clone);
}

std::map<std::string, std::string> get_stats_errors()
{
	std::map<std::string, std::string> m = module_errors;
	for(std::map<variant,variant>::const_iterator i = module_definitions.begin(); i != module_definitions.end(); ++i) {
		if(m.count(i->first.as_string()) == 0) {
			m[i->first.as_string()] = "";
		}
	}

	return m;
}

void read_stats(const variant& doc)
{
	read_data_table(doc);
}

variant write_stats()
{
	return write_data_table();
}

std::vector<variant> g_crashes;

variant get_crashes()
{
	std::vector<variant> res = g_crashes;
	std::reverse(res.begin(), res.end());
	return variant(&res);
}

void process_stats(const variant& doc)
{

	if(!doc["signature"].is_string()) {
		return;
	}

	variant version = doc["version"];
	if(!version.is_string()) {
		return;
	}

	variant module = doc["module"];
	if(!module.is_string()) {
		return;
	}

	variant module_version = doc["module_version"];
	if(!module_version.is_string()) {
		return;
	}

	const std::string& version_str = version.as_string();
	const std::string& module_str = module.as_string();
	const std::string& module_version_str = module_version.as_string();
	const int user_id = doc["user_id"].as_int();

	game_logic::MapFormulaCallable* context_callable = new game_logic::MapFormulaCallable;
	context_callable->add("user_id", variant(user_id));
	context_callable->add("program_args", doc["program_args"]);
	context_callable->add("build_description", doc["build_description"]);
	context_callable->add("signature", doc["signature"]);
	variant context_holder(context_callable);

	std::vector<std::string> data_table_key(3);
	data_table_key[0] = version_str;
	data_table_key[1] = module_str;
	data_table_key[2] = module_version_str;

	version_data* data_store[2];
	data_store[0] = &data_table[data_table_key];
	data_table_key[0] = "";
	data_store[1] = &data_table[data_table_key];

	variant levels = doc["levels"];	
	if(!levels.is_list()) {
		return;
	}

	try {
	for(int n = 0; n != levels.num_elements(); ++n) {
		variant lvl = levels[n];
		variant level_id_v = lvl["level"];
		variant level_id = level_id_v;
		if(!level_id.is_string()) {
			continue;
		}

		context_callable->add("level", level_id_v);

		variant stats = lvl["stats"];
		for(int m = 0; m != stats.num_elements(); ++m) {
			variant msg = stats[m];
			if(!msg.is_map()) {
				continue;
			}

			variant type = msg["type"];
			if(!type.is_string()) {
				continue;
			}
			
			const std::string& type_str = type.as_string();
			const msg_type_info& msg_info = message_type_index[module_str][type_str];
			if(msg_info.record_all) {
				g_raw_entries[type_str].push_back(msg);
			}

			if(type_str == "crash") {
				variant m = msg;
#ifdef LINUX
				time_t t = time(nullptr);
				tm* ltime = localtime(&t);

				char tbuf[512];
				sprintf(tbuf, "%04d/%02d/%02d %02d:%02d:%02d", ltime->tm_year + 1900, ltime->tm_mon + 1, ltime->tm_mday, ltime->tm_hour, ltime->tm_min, ltime->tm_sec);

				m.add_attr_mutation(variant("timestamp"), variant(std::string(tbuf)));
#endif
				g_crashes.push_back(m);
			}

			table_set* all_ts[4];

			table_set** global_ts = &all_ts[0];
			table_set** level_ts = &all_ts[2];
			for(int i = 0; i != 2; ++i) {
				global_ts[i] = &data_store[i]->global_data[type_str];
				global_ts[i]->total_count++;

				type_data_map& data_map = data_store[i]->level_to_data[level_id.as_string()];
				level_ts[i] = &data_map[type_str];
				level_ts[i]->total_count++;
			}

			for(const TableInfo& info : msg_info.tables) {
				variant key = info.calculate_key(msg, *context_callable);
				for(int i = (info.is_global() ? 0 : 2); i != 4; ++i) {
					table_set* ts = all_ts[i];
					table& tb = ts->tables[info.name()];

					variant& val = tb[key];
					if(val.is_null()) {
						val = info.init_value();
					}

					val = info.calculate_value(msg, val);
				}
			}
		}
	}
	} catch(validation_failure_exception& e) {
		message_type_index.erase(module_str);
		LOG_ERROR("ERROR IN MODULE PROCESSING FOR " << module_str);
		module_errors[module_str] = e.msg;
	}
}

variant get_stats(const std::string& version, const std::string& module, const std::string& module_version, const std::string& lvl)
{
	std::vector<std::string> key(3);
	key[0] = version;
	key[1] = module;
	key[2] = module_version;
	version_data& ver_data = data_table[key];
	type_data_map& data = lvl.empty() ? ver_data.global_data : ver_data.level_to_data[lvl];
	return output_type_data_map(data);
}
variant get_raw_stats(const std::string& type)
{
	std::vector<variant> v = g_raw_entries[type];
	return variant(&v);
}
