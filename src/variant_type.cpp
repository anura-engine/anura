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
#include <vector>

#include <stdio.h>

#include "asserts.hpp"
#include "custom_object.hpp"
#include "custom_object_type.hpp"
#include "foreach.hpp"
#include "formula_function.hpp"
#include "formula_interface.hpp"
#include "formula_object.hpp"
#include "formula_tokenizer.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "variant_type.hpp"

variant_type::variant_type()
{
}

variant_type::~variant_type()
{
}

namespace {

std::map<std::string, variant> load_named_variant_info()
{
	std::map<std::string, variant> result;

	const std::string path = module::map_file("data/types.cfg");
	if(sys::file_exists(path)) {
		variant node = json::parse_from_file(path);
		foreach(const variant::map_pair& p, node.as_map()) {
			result[p.first.as_string()] = p.second;
		}
	}

	return result;
}

variant_type_ptr get_named_variant_type(const std::string& name)
{
	static std::map<std::string, variant> info = load_named_variant_info();
	static std::map<std::string, variant_type_ptr> cache;
	std::map<std::string,variant_type_ptr>::const_iterator itor = cache.find(name);
	if(itor != cache.end()) {
		return itor->second;
	}

	std::map<std::string, variant>::const_iterator info_itor = info.find(name);
	if(info_itor != info.end()) {
		//insert into the cache a null entry to symbolize we're parsing
		//this, to avoid infinite recursion.
		variant_type_ptr& ptr = cache[name];
		ptr = parse_variant_type(info_itor->second);
		return ptr;
	}

	return variant_type_ptr();

}

class variant_type_simple : public variant_type
{
public:
	variant_type_simple(const variant& original_str, const formula_tokenizer::token& tok)
	  : type_(variant::string_to_type(std::string(tok.begin, tok.end)))
	{
		ASSERT_LOG(type_ != variant::VARIANT_TYPE_INVALID, "INVALID TYPE: " << std::string(tok.begin, tok.end) << " AT:\n" << game_logic::pinpoint_location(original_str, tok.begin, tok.end));
	}

	explicit variant_type_simple(variant::TYPE type) : type_(type) {}

	bool match(const variant& v) const {
		return v.type() == type_ || type_ == variant::VARIANT_TYPE_DECIMAL && v.type() == variant::VARIANT_TYPE_INT;
	}

	bool is_type(variant::TYPE type) const {
		return type == type_;
	}

	bool is_numeric() const {
		return type_ == variant::VARIANT_TYPE_DECIMAL || type_ == variant::VARIANT_TYPE_INT;
	}

	bool is_equal(const variant_type& o) const {
		const variant_type_simple* other = dynamic_cast<const variant_type_simple*>(&o);
		if(!other) {
			return false;
		}

		return type_ == other->type_;
	}

	std::string to_string() const {
		return variant::variant_type_to_string(type_);
	}

	variant_type_ptr is_list_of() const {
		if(type_ == variant::VARIANT_TYPE_LIST) {
			return variant_type::get_any();
		} else {
			return variant_type_ptr();
		}
	}

	std::pair<variant_type_ptr, variant_type_ptr> is_map_of() const {
		if(type_ == variant::VARIANT_TYPE_MAP) {
			return std::pair<variant_type_ptr, variant_type_ptr>(variant_type::get_any(), variant_type::get_any());
		} else {
			return std::pair<variant_type_ptr, variant_type_ptr>();
		}
	}

	bool is_compatible(variant_type_ptr type) const {
		const variant_type_simple* simple_type = dynamic_cast<const variant_type_simple*>(type.get());
		if(simple_type && simple_type->type_ == type_) {
			return true;
		}

		if(type_ == variant::VARIANT_TYPE_DECIMAL) {
			if(simple_type && simple_type->type_ == variant::VARIANT_TYPE_INT) { 
				return true;
			}
		} else if(type_ == variant::VARIANT_TYPE_LIST) {
			if(type->is_list_of()) {
				return true;
			}
		} else if(type_ == variant::VARIANT_TYPE_MAP) {
			if(type->is_map_of().first) {
				return true;
			}
		} else if(type_ == variant::VARIANT_TYPE_FUNCTION) {
			if(type->is_function(NULL, NULL, NULL)) {
				return true;
			}
		} else if(type_ == variant::VARIANT_TYPE_CALLABLE) {
			if(type->is_builtin() || type->is_custom_object()) {
				return true;
			}
		}

		return false;
	}

private:
	variant::TYPE type_;
};

class variant_type_any : public variant_type
{
public:
	bool match(const variant& v) const { return true; }
	bool is_equal(const variant_type& o) const {
		const variant_type_any* other = dynamic_cast<const variant_type_any*>(&o);
		return other != NULL;
	}

	std::string to_string() const {
		return "any";
	}

	bool is_compatible(variant_type_ptr type) const {
		return true;
	}

	bool maybe_convertible_to(variant_type_ptr type) const {
		return true;
	}

	bool is_any() const { return true; }
private:
};

class variant_type_commands : public variant_type
{
public:
	bool match(const variant& v) const {
		if(v.is_null()) {
			return true;
		}

		if(v.is_callable()) {
			return v.as_callable()->is_command();
		}

		if(v.is_list()) {
			for(int n = 0; n != v.num_elements(); ++n) {
				if(!match(v[n])) {
					return false;
				}
			}

			return true;
		}

		return false;
	}
	bool is_equal(const variant_type& o) const {
		const variant_type_commands* other = dynamic_cast<const variant_type_commands*>(&o);
		return other != NULL;
	}

	std::string to_string() const {
		return "commands";
	}

	bool is_compatible(variant_type_ptr type) const {
		if(type->is_type(variant::VARIANT_TYPE_NULL)) {
			return true;
		}

		variant_type_ptr list_type = type->is_list_of();
		if(list_type) {
			return variant_types_compatible(get_commands(), list_type);
		}

		return is_equal(*type);
	}
private:
};

class variant_type_class : public variant_type
{
public:
	explicit variant_type_class(const std::string& type) : type_(type)
	{
		ASSERT_LOG(game_logic::formula_class_valid(type), "INVALID FORMULA CLASS: " << type);
	}

	bool match(const variant& v) const {
		const game_logic::formula_object* obj = v.try_convert<game_logic::formula_object>();
		if(!obj) {
			return false;
		}

		return obj->is_a(type_);
	}

	bool is_class(std::string* class_name) const {
		if(class_name) {
			*class_name = type_;
		}

		return true;
	}

	bool is_equal(const variant_type& o) const {
		const variant_type_class* other = dynamic_cast<const variant_type_class*>(&o);
		if(!other) {
			return false;
		}

		return type_ == other->type_;
	}

	std::string to_string() const {
		return "class " + type_;
	}

	bool is_compatible(variant_type_ptr type) const {
		const variant_type_class* class_type = dynamic_cast<const variant_type_class*>(type.get());
		if(class_type) {
			return game_logic::is_class_derived_from(class_type->type_, type_);
		} else if(type->is_type(variant::VARIANT_TYPE_MAP)) {
			//maps can be converted implicity to class type.
			return true;
		}

		return false;
	}

	const game_logic::formula_callable_definition* get_definition() const {
		return game_logic::get_class_definition(type_).get();
	}
private:
	std::string type_;
};

class variant_type_custom_object : public variant_type
{
public:
	explicit variant_type_custom_object(const std::string& type) : type_(type)
	{
	}

	bool match(const variant& v) const {
		const custom_object* obj = v.try_convert<custom_object>();
		if(!obj) {
			return false;
		}

		return type_ == "" || obj->is_a(type_);
	}

	bool is_equal(const variant_type& o) const {
		const variant_type_custom_object* other = dynamic_cast<const variant_type_custom_object*>(&o);
		if(!other) {
			return false;
		}

		return type_ == other->type_;
	}

	std::string to_string() const {
		if(type_ == "") {
			return "custom_obj";
		}

		return "obj " + type_;
	}

	bool is_compatible(variant_type_ptr type) const {
		const variant_type_custom_object* other = dynamic_cast<const variant_type_custom_object*>(type.get());
		if(other == NULL) {
			return false;
		}

		return type_ == "" || type_ == other->type_;
	}

	const game_logic::formula_callable_definition* get_definition() const {
		if(type_ == "") {
			return &custom_object_callable::instance();
		}

		fprintf(stderr, "LOOKUP CUSTOM OBJ DEF: %s\n", type_.c_str());
		const game_logic::formula_callable_definition* def = custom_object_type::get_definition(type_).get();
		ASSERT_LOG(def, "Could not find custom object: " << type_);
		return def;
	}

	const std::string* is_custom_object() const { return &type_; }
private:
	std::string type_;
};

class variant_type_builtin : public variant_type
{
public:
	variant_type_builtin(const std::string& type, game_logic::const_formula_callable_definition_ptr def) : type_(type), def_(def)
	{
	}

	bool match(const variant& v) const {
		const game_logic::formula_callable* obj = v.try_convert<game_logic::formula_callable>();
		if(!obj) {
			return false;
		}

		return obj->query_id() == type_;
	}

	bool is_equal(const variant_type& o) const {
		const variant_type_builtin* other = dynamic_cast<const variant_type_builtin*>(&o);
		if(!other) {
			return false;
		}

		return type_ == other->type_;
	}

	std::string to_string() const {
		return type_;
	}

	bool is_compatible(variant_type_ptr type) const {
		if(is_equal(*type)) {
			return true;
		}

		const std::string* builtin = type->is_builtin();
		if(builtin && game_logic::registered_definition_is_a(*builtin, type_)) {
			return true;
		}

		return false;
	}

	const game_logic::formula_callable_definition* get_definition() const {
		if(!def_) {
			game_logic::const_formula_callable_definition_ptr def = game_logic::get_formula_callable_definition(type_);
			ASSERT_LOG(def, "Could not find builtin type definition: " << type_);
			def_ = def;
		}
		return def_.get();
	}

	const std::string* is_builtin() const { return &type_; }
private:
	std::string type_;
	mutable game_logic::const_formula_callable_definition_ptr def_;
};

class variant_type_interface : public variant_type
{
public:
	explicit variant_type_interface(const_formula_interface_ptr i)
	  : interface_(i)
	{}

	bool match(const variant& v) const {
		return interface_->match(v);
	}

	bool is_equal(const variant_type& o) const {
		const variant_type_interface* other = dynamic_cast<const variant_type_interface*>(&o);
		return other && other->interface_ == interface_;
	}

	std::string to_string() const {
		return interface_->to_string();
	}

	bool is_compatible(variant_type_ptr type) const {
		if(type->is_map_of().first) {
			return true;
		}

		try {
			interface_->create_factory(type);
			return true;
		} catch(game_logic::formula_interface::interface_mismatch_error&) {
			return false;
		}
	}

	const game_logic::formula_callable_definition* get_definition() const {
		return interface_->get_definition().get();
	}

	const game_logic::formula_interface* is_interface() const {
		return interface_.get();
	}
private:
	const_formula_interface_ptr interface_;
};

class variant_type_union : public variant_type
{
public:
	explicit variant_type_union(const std::vector<variant_type_ptr>& v) : types_(v)
	{}
	bool match(const variant& v) const {
		foreach(const variant_type_ptr& p, types_) {
			if(p->match(v)) {
				return true;
			}
		}

		return false;
	}

	bool is_numeric() const {
		if(types_.empty()) {
			return false;
		}

		foreach(const variant_type_ptr& p, types_) {
			if(p->is_numeric() == false) {
				return false;
			}
		}

		return true;
	}

	bool is_equal(const variant_type& o) const {
		const variant_type_union* other = dynamic_cast<const variant_type_union*>(&o);
		if(!other) {
			return false;
		}

		if(types_.size() != other->types_.size()) {
			return false;
		}

		for(int n = 0; n != types_.size(); ++n) {
			if(types_[n]->is_equal(*other->types_[n]) == false) {
				return false;
			}
		}

		return true;
	}

	std::string to_string() const {
		std::string result;
		for(int n = 0; n != types_.size(); ++n) {
			if(n != 0) {
				result += "|";
			}

			result += types_[n]->to_string();
		}
		return result;
	}

	bool is_function(std::vector<variant_type_ptr>* args, variant_type_ptr* return_type, int* min_args, bool* return_type_specified) const
	{
		std::vector<std::vector<variant_type_ptr> > arg_lists(types_.size());
		std::vector<variant_type_ptr> return_types(types_.size());
		std::vector<int> min_args_list(types_.size());

		if(return_type_specified) {
			*return_type_specified = true;
		}

		int max_min_args = -1;
		int num_args = 0;
		for(int n = 0; n != types_.size(); ++n) {
			bool return_type_spec = false;
			if(!types_[n]->is_function(&arg_lists[n], &return_types[n], &min_args_list[n], &return_type_spec)) {
				return false;
			}

			if(return_type_specified && !return_type_spec) {
				*return_type_specified = false;
			}

			if(max_min_args == -1 || min_args_list[n] > max_min_args) {
				max_min_args = min_args_list[n];
			}

			if(arg_lists[n].size() > num_args) {
				num_args = arg_lists[n].size();
			}
		}

		if(args) {
			args->clear();
			for(int n = 0; n != num_args; ++n) {
				std::vector<variant_type_ptr> a;
				foreach(const std::vector<variant_type_ptr>& arg, arg_lists) {
					if(n < arg.size()) {
						a.push_back(arg[n]);
					}
				}

				args->push_back(get_union(a));
			}
		}

		if(return_type) {
			*return_type = get_union(return_types);
		}

		if(min_args) {
			*min_args = max_min_args;
		}

		return true;
	}

	const std::vector<variant_type_ptr>* is_union() const { return &types_; }

	variant_type_ptr is_list_of() const {
		std::vector<variant_type_ptr> types;
		foreach(const variant_type_ptr& type, types_) {
			types.push_back(type->is_list_of());
			if(!types.back()) {
				return variant_type_ptr();
			}
		}

		return get_union(types);
	}

	std::pair<variant_type_ptr,variant_type_ptr> is_map_of() const {
		std::vector<variant_type_ptr> key_types, value_types;
		foreach(const variant_type_ptr& type, types_) {
			key_types.push_back(type->is_map_of().first);
			value_types.push_back(type->is_map_of().second);
			if(!key_types.back()) {
				return std::pair<variant_type_ptr,variant_type_ptr>();
			}
		}

		return std::pair<variant_type_ptr,variant_type_ptr>(get_union(key_types), get_union(value_types));
	}
private:
	variant_type_ptr null_excluded() const {
		std::vector<variant_type_ptr> new_types;
		foreach(variant_type_ptr t, types_) {
			if(t->is_type(variant::VARIANT_TYPE_NULL) == false) {
				new_types.push_back(t);
			}
		}

		if(new_types.size() != types_.size()) {
			return get_union(new_types);
		} else {
			return variant_type_ptr();
		}
	}

	variant_type_ptr subtract(variant_type_ptr type) const {
		std::vector<variant_type_ptr> new_types;
		foreach(variant_type_ptr t, types_) {
			if(t->is_equal(*type) == false) {
				new_types.push_back(t);
			}
		}

		if(new_types.size() != types_.size()) {
			return get_union(new_types);
		} else {
			return variant_type_ptr();
		}
	}

	std::vector<variant_type_ptr> types_;
};

class variant_type_list : public variant_type
{
public:
	explicit variant_type_list(const variant_type_ptr& value) : value_type_(value)
	{}

	bool match(const variant& v) const {
		if(!v.is_list()) {
			return false;
		}

		for(int n = 0; n != v.num_elements(); ++n) {
			if(!value_type_->match(v[n])) {
				return false;
			}
		}

		return true;
	}

	bool is_equal(const variant_type& o) const {
		const variant_type_list* other = dynamic_cast<const variant_type_list*>(&o);
		if(!other) {
			return false;
		}

		return value_type_->is_equal(*other->value_type_);
	}

	std::string to_string() const {
		return "[" + value_type_->to_string() + "]";
	}

	variant_type_ptr is_list_of() const {
		return value_type_;
	}

	bool is_compatible(variant_type_ptr type) const {
		variant_type_ptr value_type = type->is_list_of();
		if(value_type) {
			return variant_types_compatible(value_type_, value_type);
		}

		if(type->is_type(variant::VARIANT_TYPE_LIST)) {
			return variant_types_compatible(value_type_, variant_type::get_any());
		}

		return false;

	}
private:
	variant_type_ptr value_type_;
};

class variant_type_map : public variant_type
{
public:
	variant_type_map(variant_type_ptr key, variant_type_ptr value)
	  : key_type_(key), value_type_(value)
	{}

	bool match(const variant& v) const {
		if(!v.is_map()) {
			return false;
		}

		foreach(const variant::map_pair& p, v.as_map()) {
			if(!key_type_->match(p.first) || !value_type_->match(p.second)) {
				return false;
			}
		}

		return true;
	}

	bool is_equal(const variant_type& o) const {
		const variant_type_map* other = dynamic_cast<const variant_type_map*>(&o);
		if(!other) {
			return false;
		}

		return value_type_->is_equal(*other->value_type_) &&
		       key_type_->is_equal(*other->key_type_);
	}
	std::string to_string() const {
		return "{" + key_type_->to_string() + " -> " + value_type_->to_string() + "}";
	}

	std::pair<variant_type_ptr, variant_type_ptr> is_map_of() const {
		return std::pair<variant_type_ptr, variant_type_ptr>(key_type_, value_type_);
	}

	bool is_compatible(variant_type_ptr type) const {
		std::pair<variant_type_ptr,variant_type_ptr> p = type->is_map_of();
		if(p.first && p.second) {
			return variant_types_compatible(key_type_, p.first) &&
			       variant_types_compatible(value_type_, p.second);
		}

		if(type->is_type(variant::VARIANT_TYPE_LIST)) {
			return variant_types_compatible(key_type_, variant_type::get_any()) &&
			       variant_types_compatible(value_type_, variant_type::get_any());
		}

		return false;

	}
private:
	variant_type_ptr key_type_, value_type_;
};

class variant_type_specific_map : public variant_type
{
public:
	variant_type_specific_map(const std::map<variant, variant_type_ptr>& type_map, variant_type_ptr key_type, variant_type_ptr value_type)
	  : type_map_(type_map), key_type_(key_type), value_type_(value_type)
	{
		ASSERT_LOG(type_map.empty() == false, "Specific map which is empty");
		std::vector<std::string> keys;
		std::vector<variant_type_ptr> values;
		for(std::map<variant,variant_type_ptr>::const_iterator i = type_map.begin(); i != type_map.end(); ++i) {
			keys.push_back(i->first.as_string());
			values.push_back(i->second);

			if(get_null_excluded(i->second) == i->second) {
				must_have_keys_.insert(i->first);
			}
		}
		def_ = game_logic::create_formula_callable_definition(&keys[0], &keys[0] + keys.size(), game_logic::const_formula_callable_definition_ptr(), &values[0]);
	}

	bool match(const variant& v) const {
		if(!v.is_map()) {
			return false;
		}

		foreach(const variant::map_pair& p, v.as_map()) {
			std::map<variant, variant_type_ptr>::const_iterator itor = type_map_.find(p.first);
			if(itor == type_map_.end()) {
				return false;
			}

			if(!itor->second->match(p.second)) {
				return false;
			}
		}

		foreach(const variant& k, must_have_keys_) {
			if(v.as_map().count(k) == 0) {
				return false;
			}
		}

		return true;
	}

	bool is_equal(const variant_type& o) const {
		const variant_type_specific_map* other = dynamic_cast<const variant_type_specific_map*>(&o);
		if(!other) {
			return false;
		}

		if(type_map_.size() != other->type_map_.size()) {
			return false;
		}

		std::map<variant, variant_type_ptr>::const_iterator i1 = type_map_.begin();
		std::map<variant, variant_type_ptr>::const_iterator i2 = other->type_map_.begin();
		while(i1 != type_map_.end()) {
			if(i1->first != i2->first) {
				return false;
			}

			if(!i1->second->is_equal(*i2->second)) {
				return false;
			}

			++i1;
			++i2;
		}

		return false;
	}
	std::string to_string() const {
		std::ostringstream s;
		s << "{";
		std::map<variant, variant_type_ptr>::const_iterator i = type_map_.begin();
		while(i != type_map_.end()) {
			if(i->first.is_string()) {
				s << i->first.as_string();
			} else {
				s << i->first.write_json();
			}

			s << ": " << i->second->to_string();
			++i;

			if(i != type_map_.end()) {
				s << ", ";
			}
		}

		s << "}";
		return s.str();
	}

	std::pair<variant_type_ptr, variant_type_ptr> is_map_of() const {
		return std::pair<variant_type_ptr, variant_type_ptr>(key_type_, value_type_);
	}

	bool is_compatible(variant_type_ptr type) const {
		if(type->is_equal(*this)) {
			return true;
		}

		const variant_type_specific_map* other = dynamic_cast<const variant_type_specific_map*>(type.get());
		if(!other) {
			return false;
		}

		for(std::map<variant, variant_type_ptr>::const_iterator i = type_map_.begin(); i != type_map_.end(); ++i) {
			std::map<variant, variant_type_ptr>::const_iterator other_itor =
			      other->type_map_.find(i->first);
			if(other_itor == other->type_map_.end()) {
				if(must_have_keys_.count(i->first)) {
					return false;
				}
			} else {
				if(!variant_types_compatible(i->second, other_itor->second)) {
					return false;
				}
			}
		}

		for(std::map<variant, variant_type_ptr>::const_iterator i = other->type_map_.begin(); i != other->type_map_.end(); ++i) {
			if(type_map_.count(i->first) == 0) {
				return false;
			}
		}

		return false;
	}

	const std::map<variant, variant_type_ptr>* is_specific_map() const { return &type_map_; }

	const game_logic::formula_callable_definition* get_definition() const
	{
		return def_.get();
	}
private:
	std::map<variant, variant_type_ptr> type_map_;
	std::set<variant> must_have_keys_;
	variant_type_ptr key_type_, value_type_;
	game_logic::formula_callable_definition_ptr def_;
};

class variant_type_function : public variant_type
{
public:
	variant_type_function(const std::vector<variant_type_ptr>& args,
	                      variant_type_ptr return_type, int min_args)
	  : args_(args), return_(return_type), min_args_(min_args), return_type_specified_(true)
	{
		if(!return_) {
			return_ = get_any();
			return_type_specified_ = false;
		}
	}

	bool is_function(std::vector<variant_type_ptr>* args, variant_type_ptr* return_type, int* min_args, bool* return_type_specified) const
	{
		if(args) {
			*args = args_;
		}

		if(return_type) {
			*return_type = return_;
		}

		if(min_args) {
			*min_args = min_args_;
		}

		if(return_type_specified) {
			*return_type_specified = return_type_specified_;
		}

		return true;
	}

	bool is_equal(const variant_type& o) const {
		const variant_type_function* other = dynamic_cast<const variant_type_function*>(&o);
		if(!other) {
			return false;
		}

		if(!return_->is_equal(*other->return_) || args_.size() != other->args_.size()) {
			return false;
		}

		for(int n = 0; n != args_.size(); ++n) {
			if(!args_[n]->is_equal(*other->args_[n])) {
				return false;
			}
		}

		return true;
	}

	std::string to_string() const {
		std::string result = "function(";
		for(int n = 0; n != args_.size(); ++n) {
			if(n != 0) {
				result += ",";
			}

			result += args_[n]->to_string();
		}

		result += ") -> " + return_->to_string();
		return result;
	}

	bool match(const variant& v) const {
		if(v.is_function() == false) {
			return false;
		}

		if(v.function_return_type()->is_equal(*return_) == false) {
			return false;
		}

		if(v.max_function_arguments() != args_.size() || v.min_function_arguments() != min_args_) {
			return false;
		}

		const std::vector<variant_type_ptr>& arg_types = v.function_arg_types();
		for(int n = 0; n != arg_types.size(); ++n) {
			if(arg_types[n]->is_equal(*args_[n]) == false) {
				return false;
			}
		}

		return true;
	}

	bool is_compatible(variant_type_ptr type) const {
		std::vector<variant_type_ptr> args;
		variant_type_ptr return_type;
		int min_args = 0;
		if(type->is_function(&args, &return_type, &min_args)) {
			if(min_args != min_args_) {
				return false;
			}

			if(!variant_types_compatible(return_, return_type)) {
				return false;
			}

			if(args.size() != args_.size()) {
				return false;
			}

			for(int n = 0; n != args_.size(); ++n) {
				if(!variant_types_compatible(args_[n], args[n])) {
					return false;
				}
			}

			return true;
		}

		return false;
	}
	
private:

	std::vector<variant_type_ptr> args_;
	variant_type_ptr return_;
	int min_args_;
	bool return_type_specified_;
};

class variant_type_function_overload : public variant_type
{
public:
	variant_type_function_overload(variant_type_ptr overloaded_fn, const std::vector<variant_type_ptr>& fn) : overloaded_(overloaded_fn), fn_(fn)
	{}

	bool is_function(std::vector<variant_type_ptr>* args, variant_type_ptr* return_type, int* min_args, bool* return_type_specified) const
	{
		return overloaded_->is_function(args, return_type, min_args, return_type_specified);
	}

	bool match(const variant& v) const {
		return overloaded_->match(v);
	}

	bool is_equal(const variant_type& o) const {
		const variant_type_function_overload* f = dynamic_cast<const variant_type_function_overload*>(&o);
		if(!f) {
			return false;
		}

		if(overloaded_->is_equal(*f->overloaded_) == false) {
			return false;
		}

		if(fn_.size() != f->fn_.size()) {
			return false;
		}

		for(int n = 0; n != fn_.size(); ++n) {
			if(fn_[n]->is_equal(*f->fn_[n]) == false) {
				return false;
			}
		}

		return true;
	}

	std::string to_string() const {
		std::string result = "overload(";
		foreach(const variant_type_ptr& p, fn_) {
			result += p->to_string() + ",";
		}

		result[result.size()-1] = ')';
		return result;
	}

	bool is_compatible(variant_type_ptr type) const {
		return overloaded_->is_compatible(type);
	}

	variant_type_ptr function_return_type_with_args(const std::vector<variant_type_ptr>& parms) const
	{
		std::vector<variant_type_ptr> result_types;
		foreach(const variant_type_ptr& fn, fn_) {
			variant_type_ptr result;
			std::vector<variant_type_ptr> args;
			int min_args = 0;
			if(!fn->is_function(&args, &result, &min_args) ||
			   min_args > args.size() || parms.size() > args.size()) {
				continue;
			}

			bool maybe_match = true, definite_match = true;
			for(int n = 0; n != parms.size(); ++n) {
				if(variant_types_might_match(args[n], parms[n]) == false) {
					maybe_match = definite_match = false;
					break;
				}

				definite_match = definite_match && variant_types_compatible(args[n], parms[n]);
			}

			if(result_types.empty() && definite_match) {
				return result;
			}

			if(maybe_match) {
				result_types.push_back(result);
			}
		}

		return get_union(result_types);
	}

private:
	variant_type_ptr overloaded_;
	std::vector<variant_type_ptr> fn_;
};

}

bool variant_type::may_be_null(variant_type_ptr type)
{
	return type->is_any() || variant_type::get_null_excluded(type) != type;
}

variant_type_ptr get_variant_type_from_value(const variant& value) {
	using namespace game_logic;
	if(value.try_convert<formula_object>()) {
		return variant_type::get_class(value.try_convert<formula_object>()->get_class_name());
	} else if(value.is_list()) {
		std::vector<variant_type_ptr> types;
		foreach(const variant& item, value.as_list()) {
			variant_type_ptr new_type = get_variant_type_from_value(item);
			foreach(const variant_type_ptr& existing, types) {
				if(existing->is_equal(*new_type)) {
					new_type.reset();
					break;
				}
			}

			if(new_type) {
				types.push_back(new_type);
			}
		}

		return variant_type::get_list(variant_type::get_union(types));
	} else if(value.is_map()) {

		bool all_string_keys = true;
		foreach(const variant::map_pair& p, value.as_map()) {
			if(p.first.is_string() == false) {
				all_string_keys = false;
				break;
			}
		}

		if(all_string_keys) {
			std::map<variant, variant_type_ptr> type_map;
			foreach(const variant::map_pair& p, value.as_map()) {
				type_map[p.first] = get_variant_type_from_value(p.second);
			}

			return variant_type::get_specific_map(type_map);
		}

		std::vector<variant_type_ptr> key_types, value_types;

		foreach(const variant::map_pair& p, value.as_map()) {
			variant_type_ptr new_key_type = get_variant_type_from_value(p.first);
			variant_type_ptr new_value_type = get_variant_type_from_value(p.second);

			foreach(const variant_type_ptr& existing, key_types) {
				if(existing->is_equal(*new_key_type)) {
					new_key_type.reset();
					break;
				}
			}

			if(new_key_type) {
				key_types.push_back(new_key_type);
			}

			foreach(const variant_type_ptr& existing, value_types) {
				if(existing->is_equal(*new_value_type)) {
					new_value_type.reset();
					break;
				}
			}

			if(new_value_type) {
				value_types.push_back(new_value_type);
			}
		}

		variant_type_ptr key_type, value_type;

		if(key_types.size() == 1) {
			key_type = variant_type::get_list(key_types[0]);
		} else {
			key_type = variant_type::get_list(variant_type::get_union(key_types));
		}

		if(value_types.size() == 1) {
			value_type = variant_type::get_list(value_types[0]);
		} else {
			value_type = variant_type::get_list(variant_type::get_union(value_types));
		}

		return variant_type::get_map(key_type, value_type);
	} else if(value.is_callable() && value.as_callable()->is_command()) {
		return variant_type::get_commands();
	} else {
		return variant_type::get_type(value.type());
	}
}

std::string variant_type_is_class_or_null(variant_type_ptr type)
{
	std::string class_name;
	if(type->is_class(&class_name)) {
		return class_name;
	}

	const std::vector<variant_type_ptr>* union_vec = type->is_union();
	if(union_vec) {
		foreach(variant_type_ptr t, *union_vec) {
			bool found_class = false;
			if(class_name.empty()) {
				class_name = variant_type_is_class_or_null(t);
				if(class_name.empty() == false) {
					found_class = true;
				}
			}

			if(found_class == false && t->is_type(variant::VARIANT_TYPE_NULL) == false) {
				return "";
			}
		}
	}

	return class_name;
}

bool variant_types_compatible(variant_type_ptr to, variant_type_ptr from)
{
	if(from->is_union()) {
		foreach(variant_type_ptr from_type, *from->is_union()) {
			if(variant_types_compatible(to, from_type) == false) {
				return false;
			}
		}

		return true;
	}

	if(to->is_union()) {
		foreach(variant_type_ptr to_type, *to->is_union()) {
			if(variant_types_compatible(to_type, from)) {
				return true;
			}
		}

		return false;
	}

	return to->is_compatible(from);
}

bool variant_types_might_match(variant_type_ptr to, variant_type_ptr from)
{
	if(from->is_union()) {
		foreach(variant_type_ptr from_type, *from->is_union()) {
			if(variant_types_might_match(to, from_type)) {
				return true;
			}
		}

		return false;
	}

	if(to->is_union()) {
		foreach(variant_type_ptr to_type, *to->is_union()) {
			if(variant_types_might_match(to_type, from)) {
				return true;
			}
		}

		return false;
	}

	return to->is_compatible(from) || from->is_compatible(to) || from->maybe_convertible_to(to);
}

variant_type_ptr parse_variant_type(const variant& original_str,
                                    const formula_tokenizer::token*& i1,
                                    const formula_tokenizer::token* i2,
									bool allow_failure)
{
#define ASSERT_COND(cond, msg) if(cond) {} else if(allow_failure) { return variant_type_ptr(); } else { ASSERT_LOG(cond, msg); }

	using namespace formula_tokenizer;

	std::vector<variant_type_ptr> v;

	const token* begin_token = i1;

	for(;;) {
		ASSERT_COND(i1 != i2, "EXPECTED TYPE BUT FOUND EMPTY EXPRESSION:" << original_str.debug_location());
		if(i1->type == TOKEN_IDENTIFIER && util::c_isupper(*i1->begin) && get_named_variant_type(std::string(i1->begin, i1->end))) {
			v.push_back(get_named_variant_type(std::string(i1->begin, i1->end)));
			++i1;
		} else if(i1->type == TOKEN_IDENTIFIER && i1->equals("interface") && i1+1 != i2 && (i1+1)->equals("{")) {
			i1 += 2;

			std::map<std::string, variant_type_ptr> types;

			while(i1 != i2 && !i1->equals("}")) {
				if(i1->type == TOKEN_IDENTIFIER) {
					std::string id(i1->begin, i1->end);
					++i1;

					ASSERT_COND(i1 != i2 && i1->equals(":"), "Expected : after " << id << " in interface definition");

					++i1;
					variant_type_ptr type = parse_variant_type(original_str, i1, i2, allow_failure);
					if(!type) {
						return variant_type_ptr();
					}

					types[id] = type;

					if(i1 != i2 && i1->equals(",")) {
						++i1;
					}
				} else {
					ASSERT_COND(false, "Expected identifier or } in interface definition");
				}
			}

			if(i1 != i2) {
				++i1;
			}

			v.push_back(variant_type_ptr(new variant_type_interface(
			    const_formula_interface_ptr(new game_logic::formula_interface(types)))));
		} else if(i1->type == TOKEN_IDENTIFIER && i1->equals("function") && i1+1 != i2 && (i1+1)->equals("(")) {
			i1 += 2;

			int min_args = -1;
			std::vector<variant_type_ptr> arg_types;
			while(i1 != i2 && !i1->equals(")")) {
				arg_types.push_back(parse_variant_type(original_str, i1, i2, allow_failure));
				if(allow_failure && !arg_types.back()) {
					return variant_type_ptr();
				}

				if(i1->equals("=")) {
					++i1;
					if(i1 != i2) {
						if(min_args == -1) {
							min_args = arg_types.size();
						}
						++i1;
					}
				}

				ASSERT_COND(i1 == i2 || i1->equals(")") || i1->equals(","), "UNEXPECTED TOKENS WHEN PARSING FUNCTION:\n" << game_logic::pinpoint_location(original_str, (i1-1)->end));

				if(i1->equals(",")) {
					++i1;
				}
			}

			ASSERT_COND(i1 != i2, "UNEXPECTED END OF INPUT WHILE PARSING FUNCTION DEF:\n" << game_logic::pinpoint_location(original_str, (i1-1)->end));

			++i1;

			variant_type_ptr return_type;

			if(i1 != i2 && i1->equals("->")) {
				++i1;

				ASSERT_COND(i1 != i2, "UNEXPECTED END OF INPUT WHILE PARSING FUNCTION DEF:\n" << game_logic::pinpoint_location(original_str, (i1-1)->end));
				
				return_type = parse_variant_type(original_str, i1, i2, allow_failure);
			} else {
				return_type = variant_type::get_any();
			}

			if(min_args == -1) {
				min_args = arg_types.size();
			}

			return variant_type::get_function_type(arg_types, return_type, min_args);
		} else if(i1->type == TOKEN_IDENTIFIER && (i1->equals("custom_obj") || i1->equals("object_type"))) {
			++i1;
			v.push_back(variant_type_ptr(new variant_type_custom_object("")));
		} else if(i1->type == TOKEN_IDENTIFIER && (i1->equals("class") || i1->equals("obj"))) {
			const bool is_class = i1->equals("class");
			++i1;
			ASSERT_COND(i1 != i2, "EXPECTED TYPE NAME BUT FOUND EMPTY EXPRESSION:\n" << game_logic::pinpoint_location(original_str, (i1-1)->end));
			std::string class_name(i1->begin, i1->end);

			while(i1+1 != i2 && i1+2 != i2 && (i1+1)->equals(".")) {
				class_name += ".";
				i1 += 2;
				class_name += std::string(i1->begin, i1->end);
			}

			if(is_class) {
				v.push_back(variant_type_ptr(new variant_type_class(class_name)));
			} else {
				v.push_back(variant_type_ptr(new variant_type_custom_object(class_name)));
			}

			++i1;
		} else if(i1->type == TOKEN_IDENTIFIER && i1->equals("any")) {
			v.push_back(variant_type_ptr(new variant_type_any));
			++i1;
		} else if(i1->type == TOKEN_IDENTIFIER && i1->equals("commands")) {
			v.push_back(variant_type_ptr(new variant_type_commands));
			++i1;
		} else if(i1->type == TOKEN_IDENTIFIER && i1->equals("builtin") && i1+1 != i2) {
			++i1;

			v.push_back(variant_type_ptr(new variant_type_builtin(std::string(i1->begin, i1->end), game_logic::const_formula_callable_definition_ptr())));
			++i1;
		} else if(i1->type == TOKEN_IDENTIFIER && game_logic::get_formula_callable_definition(std::string(i1->begin, i1->end)).get()) {
			v.push_back(variant_type::get_builtin(std::string(i1->begin, i1->end)));
			++i1;
		} else if(i1->type == TOKEN_IDENTIFIER || (i1->type == TOKEN_KEYWORD && std::equal(i1->begin, i1->end, "null"))) {
			ASSERT_COND(variant::string_to_type(std::string(i1->begin, i1->end)) != variant::VARIANT_TYPE_INVALID,
			  "INVALID TOKEN WHEN PARSING TYPE: " << std::string(i1->begin, i1->end) << " AT:\n" << game_logic::pinpoint_location(original_str, i1->begin, i1->end));
			v.push_back(variant_type_ptr(new variant_type_simple(original_str, *i1)));
			++i1;
		} else if(i1->type == TOKEN_LBRACKET) {
			const token* end = i1+1;
			const bool res = token_matcher().add(TOKEN_RBRACKET).find_match(end, i2);
			ASSERT_COND(res, "ERROR PARSING MAP TYPE: " << original_str.debug_location());

			++i1;
			ASSERT_COND(i1 != end, "ERROR PARSING MAP TYPE: " << original_str.debug_location());

			if(i1->type == TOKEN_IDENTIFIER && i1 != end && (i1+1)->equals(":")) {
				//a specific map type.
				std::map<variant, variant_type_ptr> types;

				for(;;) {
					ASSERT_COND(i1->type == TOKEN_IDENTIFIER && i1+1 != end && i1+2 != end && (i1+1)->equals(":"), "ERROR PARSING MAP TYPE: " << original_str.debug_location());
					variant key(std::string(i1->begin, i1->end));
					i1 += 2;
					variant_type_ptr value_type = parse_variant_type(original_str, i1, end, allow_failure);
					ASSERT_COND(value_type, "");

					types[key] = value_type;

					if(i1 == end) {
						++i1;
						break;
					}

					ASSERT_COND(i1->equals(","), "ERROR PARSING MAP TYPE: " << original_str.debug_location());
					++i1;
				}

				v.push_back(variant_type::get_specific_map(types));
			} else {

				const variant_type_ptr key_type = parse_variant_type(original_str, i1, end, allow_failure);
				ASSERT_COND(key_type, "");
				ASSERT_COND(i1->type == TOKEN_POINTER, "ERROR PARSING MAP TYPE, NO ARROW FOUND: " << original_str.debug_location());
		
				++i1;
				ASSERT_COND(i1 != end, "ERROR PARSING MAP TYPE: " << original_str.debug_location());

				const variant_type_ptr value_type = parse_variant_type(original_str, i1, end, allow_failure);
				ASSERT_COND(value_type, "");
				ASSERT_COND(i1 == end, "ERROR PARSING MAP TYPE: " << original_str.debug_location());

				v.push_back(variant_type_ptr(new variant_type_map(key_type, value_type)));

				++i1;
			}

		} else if(i1->type == TOKEN_LSQUARE) {
			const token* end = i1+1;
			const bool res = token_matcher().add(TOKEN_RSQUARE).find_match(end, i2);
			ASSERT_COND(res, "ERROR PARSING ARRAY TYPE: " << original_str.debug_location());
	
			++i1;
			ASSERT_COND(i1 != end, "ERROR PARSING ARRAY TYPE: " << original_str.debug_location());
			
			const variant_type_ptr value_type = parse_variant_type(original_str, i1, end, allow_failure);
			ASSERT_COND(i1 == end, "ERROR PARSING ARRAY TYPE: " << original_str.debug_location());
	
			v.push_back(variant_type_ptr(new variant_type_list(value_type)));
	
			++i1;
		} else {
			ASSERT_COND(false, "UNEXPECTED TOKENS WHEN PARSING TYPE: " << std::string(i1->begin, (i2-1)->end) << " AT " << original_str.debug_location());
		}

		if(i1 != i2 && i1->type == TOKEN_PIPE) {
			++i1;
		} else {
			break;
		}
	}

	if(v.size() == 1) {
		v.front()->set_str(std::string(begin_token->begin, (i1-1)->end));
		return v.front();
	} else {
		variant_type_ptr result(new variant_type_union(v));
		result->set_str(std::string(begin_token->begin, (i1-1)->end));
		return result;
	}
#undef ASSERT_COND
}

variant_type_ptr parse_variant_type(const variant& type)
{
	using namespace formula_tokenizer;
	const std::string& s = type.as_string();
	std::vector<token> tokens;
	std::string::const_iterator i1 = s.begin();
	std::string::const_iterator i2 = s.end();
	while(i1 != i2) {
		try {
			token tok = get_token(i1, i2);
			if(tok.type != TOKEN_WHITESPACE && tok.type != TOKEN_COMMENT) {
				tokens.push_back(tok);
			}
		} catch(token_error& e) {
			ASSERT_LOG(false, "ERROR PARSING TYPE: " << e.msg << " IN '" << s << "' AT " << type.debug_location());
		}
	}

	ASSERT_LOG(tokens.empty() == false, "ERROR PARSING TYPE: EMPTY STRING AT " << type.debug_location());

	const token* begin = &tokens[0];
	return parse_variant_type(type, begin, begin + tokens.size());
}

variant_type_ptr
parse_optional_function_type(const variant& original_str,
                             const formula_tokenizer::token*& i1,
                             const formula_tokenizer::token* i2)
{
	using namespace formula_tokenizer;

	if(i1 == i2 || !i1->equals("def")) {
		return variant_type_ptr();
	}

	++i1;
	if(i1 == i2 || i1->type != TOKEN_LPARENS) {
		return variant_type_ptr();
	}

	int optional_args = 0;

	std::vector<variant_type_ptr> args;

	++i1;
	while(i1 != i2 && i1->type != TOKEN_RPARENS) {
		if(i1->type == TOKEN_IDENTIFIER && i1+1 != i2 &&
		   ((i1+1)->type == TOKEN_COMMA ||
		    (i1+1)->type == TOKEN_RPARENS ||
			(i1+1)->equals("="))) {
			args.push_back(variant_type::get_any());
			++i1;
			if(i1->type == TOKEN_COMMA) {
				++i1;
			} else if(i1->equals("=")) {
				++optional_args;
				while(i1 != i2 && !i1->equals(",") && !i1->equals(")")) {
					++i1;
				}

				if(i1 != i2 && i1->type == TOKEN_COMMA) {
					++i1;
				}
			}
			continue;
		}

		variant_type_ptr arg_type = parse_variant_type(original_str, i1, i2);
		args.push_back(arg_type);
		ASSERT_LOG(i1 != i2, "UNEXPECTED END OF EXPRESSION: " << original_str.debug_location());
		if(i1->type == TOKEN_IDENTIFIER) {
			++i1;

			if(i1 != i2 && i1->equals("=")) {
				++optional_args;

				while(i1 != i2 && !i1->equals(",") && !i1->equals(")")) {
					++i1;
				}
			}
		}

		if(i1 != i2 && i1->type == TOKEN_RPARENS) {
			break;
		}

		ASSERT_LOG(i1 != i2 && i1->type == TOKEN_COMMA, "ERROR PARSING FUNCTION SIGNATURE: " << original_str.debug_location());

		++i1;
	}

	ASSERT_LOG(i1 != i2 && i1->type == TOKEN_RPARENS, "UNEXPECTED END OF FUNCTION SIGNATURE: " << original_str.debug_location());

	variant_type_ptr return_type;

	++i1;
	if(i1 != i2 && i1->type == TOKEN_POINTER) {
		++i1;
		return_type = parse_variant_type(original_str, i1, i2);
	}

	return variant_type::get_function_type(args, return_type, args.size() - optional_args);
}

variant_type_ptr parse_optional_function_type(const variant& type)
{
	using namespace formula_tokenizer;
	const std::string& s = type.as_string();
	std::vector<token> tokens;
	std::string::const_iterator i1 = s.begin();
	std::string::const_iterator i2 = s.end();
	while(i1 != i2) {
		try {
			token tok = get_token(i1, i2);
			if(tok.type != TOKEN_WHITESPACE && tok.type != TOKEN_COMMENT) {
				tokens.push_back(tok);
			}
		} catch(token_error& e) {
			ASSERT_LOG(false, "ERROR PARSING TYPE: " << e.msg << " IN '" << s << "' AT " << type.debug_location());
		}
	}

	ASSERT_LOG(tokens.empty() == false, "ERROR PARSING TYPE: EMPTY STRING AT " << type.debug_location());

	const token* begin = &tokens[0];
	return parse_optional_function_type(type, begin, begin + tokens.size());
}

variant_type_ptr
parse_optional_formula_type(const variant& original_str,
                            const formula_tokenizer::token*& i1,
                            const formula_tokenizer::token* i2)
{
	variant_type_ptr result = parse_variant_type(original_str, i1, i2, true);
	if(i1 != i2 && i1->equals("<-")) {
		return result;
	}

	return variant_type_ptr();
}

variant_type_ptr parse_optional_formula_type(const variant& type)
{
	using namespace formula_tokenizer;
	const std::string& s = type.as_string();
	std::vector<token> tokens;
	std::string::const_iterator i1 = s.begin();
	std::string::const_iterator i2 = s.end();
	while(i1 != i2) {
		try {
			token tok = get_token(i1, i2);
			if(tok.type != TOKEN_WHITESPACE && tok.type != TOKEN_COMMENT) {
				tokens.push_back(tok);
			}
		} catch(token_error& e) {
			ASSERT_LOG(false, "ERROR PARSING TYPE: " << e.msg << " IN '" << s << "' AT " << type.debug_location());
		}
	}

	ASSERT_LOG(tokens.empty() == false, "ERROR PARSING TYPE: EMPTY STRING AT " << type.debug_location());

	const token* begin = &tokens[0];
	return parse_optional_formula_type(type, begin, begin + tokens.size());
}

variant_type_ptr variant_type::get_any()
{
	static const variant_type_ptr result(new variant_type_any);
	return result;
}

variant_type_ptr variant_type::get_commands()
{
	static const variant_type_ptr result(new variant_type_commands);
	return result;
}

variant_type_ptr variant_type::get_type(variant::TYPE type)
{
	return variant_type_ptr(new variant_type_simple(type));
}

variant_type_ptr variant_type::get_union(const std::vector<variant_type_ptr>& elements)
{
	foreach(variant_type_ptr el, elements) {
		if(!el || el->is_any()) {
			return variant_type::get_any();
		}
	}

	foreach(variant_type_ptr el, elements) {
		const std::vector<variant_type_ptr>* items = el->is_union();
		if(items) {
			std::vector<variant_type_ptr> v = elements;
			v.erase(std::find(v.begin(), v.end(), el));
			v.insert(v.end(), items->begin(), items->end());
			return get_union(v);
		}
	}

	std::vector<variant_type_ptr> items;
	foreach(variant_type_ptr el, elements) {
		foreach(variant_type_ptr item, items) {
			if(el->is_equal(*item)) {
				el = variant_type_ptr();
				break;
			}
		}

		if(el) {
			items.push_back(el);
		}
	}

	if(items.size() == 1) {
		return items[0];
	}

	return variant_type_ptr(new variant_type_union(items));
}

variant_type_ptr variant_type::get_list(variant_type_ptr element_type)
{
	return variant_type_ptr(new variant_type_list(element_type));
}

variant_type_ptr variant_type::get_map(variant_type_ptr key_type, variant_type_ptr value_type)
{
	return variant_type_ptr(new variant_type_map(key_type, value_type));
}

variant_type_ptr variant_type::get_specific_map(const std::map<variant, variant_type_ptr>& type_map)
{
	std::vector<variant_type_ptr> keys, values;
	for(std::map<variant, variant_type_ptr>::const_iterator i = type_map.begin(); i != type_map.end(); ++i) {
		keys.push_back(get_variant_type_from_value(i->first));
		values.push_back(i->second);
	}

	return variant_type_ptr(new variant_type_specific_map(type_map, variant_type::get_union(keys), variant_type::get_union(values)));
}

variant_type_ptr variant_type::get_class(const std::string& class_name)
{
	return variant_type_ptr(new variant_type_class(class_name));
}

variant_type_ptr variant_type::get_custom_object(const std::string& name)
{
	return variant_type_ptr(new variant_type_custom_object(name));
}

variant_type_ptr variant_type::get_builtin(const std::string& name)
{
	game_logic::const_formula_callable_definition_ptr def = game_logic::get_formula_callable_definition(name);
	if(def) {
		return variant_type_ptr(new variant_type_builtin(name, def));
	} else {
		return variant_type_ptr();
	}
}

variant_type_ptr variant_type::get_function_type(const std::vector<variant_type_ptr>& arg_types, variant_type_ptr return_type, int min_args)
{
	return variant_type_ptr(new variant_type_function(arg_types, return_type, min_args));
}

variant_type_ptr variant_type::get_function_overload_type(variant_type_ptr overloaded_fn, const std::vector<variant_type_ptr>& fn)
{
	return variant_type_ptr(new variant_type_function_overload(overloaded_fn, fn));
}

variant_type_ptr variant_type::get_null_excluded(variant_type_ptr input)
{
	variant_type_ptr result = input->null_excluded();
	if(result) {
		return result;
	} else {
		return input;
	}
}

variant_type_ptr variant_type::get_with_exclusion(variant_type_ptr input, variant_type_ptr subtract)
{
	variant_type_ptr result = input->subtract(subtract);
	if(result) {
		return result;
	} else {
		return input;
	}
}

UNIT_TEST(variant_type) {
#define TYPES_COMPAT(a, b) CHECK_EQ(variant_types_compatible(parse_variant_type(variant(a)), parse_variant_type(variant(b))), true)
#define TYPES_INCOMPAT(a, b) CHECK_EQ(variant_types_compatible(parse_variant_type(variant(a)), parse_variant_type(variant(b))), false)

	TYPES_COMPAT("int|bool", "int");
	TYPES_COMPAT("int|bool|string", "string");
	TYPES_COMPAT("decimal", "int");
	TYPES_COMPAT("list", "[int]");
	TYPES_COMPAT("list", "[int|string]");
	TYPES_COMPAT("list", "[any]");
	TYPES_COMPAT("[any]", "[int|string]");
	TYPES_COMPAT("[any]", "list");
	TYPES_COMPAT("{int|string -> string}", "{int -> string}");
	TYPES_COMPAT("map", "{int -> string}");

	TYPES_INCOMPAT("int", "int|bool");
	TYPES_INCOMPAT("int", "decimal");
	TYPES_INCOMPAT("int", "decimal");
	TYPES_INCOMPAT("[int]", "list");
	TYPES_INCOMPAT("{int -> int}", "map");
	TYPES_INCOMPAT("{int -> int}", "{string -> int}");

#undef TYPES_COMPAT	
}
