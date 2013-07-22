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
#ifndef FORMULA_CALLABLE_DEFINITION_HPP_INCLUDED
#define FORMULA_CALLABLE_DEFINITION_HPP_INCLUDED

#include <functional>
#include <iostream>
#include <string>
#include <boost/function.hpp>

#include "asserts.hpp"
#include "formula_callable_definition_fwd.hpp"
#include "formula_callable_utils.hpp"
#include "reference_counted_object.hpp"
#include "variant_type.hpp"

namespace game_logic
{

class formula_callable_definition : public reference_counted_object
{
public:
	struct entry {
		explicit entry(const std::string& id_) : id(id_), type_definition(0), access_count(0), private_counter(0) {}
		void set_variant_type(variant_type_ptr type);
		std::string id;
		const_formula_callable_definition_ptr type_definition;

		variant_type_ptr variant_type;

		//if the entry accepts different types for writes vs reads
		//(i.e. using set() or add()) then record that type here.
		variant_type_ptr write_type;

		variant_type_ptr get_write_type() const { if(write_type) { return write_type; } return variant_type; }

		mutable int access_count;

		bool is_private() const { return private_counter > 0; }
		int private_counter;
	};

	formula_callable_definition();
	virtual ~formula_callable_definition();

	virtual int get_slot(const std::string& key) const = 0;
	virtual entry* get_entry(int slot) = 0;
	virtual const entry* get_entry(int slot) const = 0;
	virtual int num_slots() const = 0;

	virtual const entry* get_default_entry() const { return NULL; }

	entry* get_entry_by_id(const std::string& key) {
		const int slot = get_slot(key);
		if(slot < 0) { return NULL; } else { return get_entry(slot); }
	}

	const entry* get_entry_by_id(const std::string& key) const {
		const int slot = get_slot(key);
		if(slot < 0) { return NULL; } else { return get_entry(slot); }
	}

	virtual const std::string* type_name() const { return !type_name_.empty() ? &type_name_ : NULL; }
	void set_type_name(const std::string& name) { type_name_ = name; }

	virtual bool is_strict() const { return is_strict_; }
	void set_strict(bool value=true) { is_strict_ = value; }

	bool supports_slot_lookups() const { return supports_slot_lookups_; }
	void set_supports_slot_lookups(bool value) { supports_slot_lookups_ = value; }
private:
	bool is_strict_;
	bool supports_slot_lookups_;
	std::string type_name_;
};

formula_callable_definition_ptr modify_formula_callable_definition(const_formula_callable_definition_ptr base_def, int slot, variant_type_ptr new_type, const formula_callable_definition* new_def=NULL);

formula_callable_definition_ptr create_formula_callable_definition(const std::string* beg, const std::string* end, const_formula_callable_definition_ptr base=NULL, variant_type_ptr* begin_types=NULL);
formula_callable_definition_ptr create_formula_callable_definition(const formula_callable_definition::entry* begin, const formula_callable_definition::entry* end, const_formula_callable_definition_ptr base=NULL);

formula_callable_definition_ptr create_map_formula_callable_definition(variant_type_ptr value_type);

int register_formula_callable_definition(const std::string& id, const_formula_callable_definition_ptr def);
int register_formula_callable_definition(const std::string& id, const std::string& base_id, const_formula_callable_definition_ptr def);
bool registered_definition_is_a(const std::string& derived, const std::string& base);
const_formula_callable_definition_ptr get_formula_callable_definition(const std::string& id);

int add_callable_definition_init(void(*fn)());
void init_callable_definitions();

}

typedef std::function<variant(const game_logic::formula_callable&)> GetterFn;
typedef std::function<void(game_logic::formula_callable&, const variant&)> SetterFn;

struct callable_property_entry {
	std::string id;
	variant_type_ptr type, set_type;
	GetterFn get;
	SetterFn set;
};

#define DECLARE_CALLABLE(classname) \
public: \
	virtual variant get_value(const std::string& key) const; \
	virtual variant get_value_by_slot(int slot) const; \
	virtual void set_value(const std::string& key, const variant& value); \
	virtual void set_value_by_slot(int slot, const variant& value); \
	virtual std::string get_object_id() const { return #classname; } \
public: \
	static void init_callable_type(std::vector<callable_property_entry>& v, std::map<std::string, int>& properties); \
private:

#define BEGIN_DEFINE_CALLABLE_NOBASE(classname) \
int classname##_num_base_slots = 0; \
const char* classname##_base_str_name = ""; \
std::vector<callable_property_entry> classname##_fields; \
std::map<std::string, int> classname##_properties; \
void classname::init_callable_type(std::vector<callable_property_entry>& fields, std::map<std::string, int>& properties) { \
	typedef classname this_type; \
	{ {

#define BEGIN_DEFINE_CALLABLE(classname, base_type) \
int classname##_num_base_slots = 0; \
const char* classname##_base_str_name = #base_type; \
std::vector<callable_property_entry> classname##_fields; \
std::map<std::string, int> classname##_properties; \
void classname::init_callable_type(std::vector<callable_property_entry>& fields, std::map<std::string, int>& properties) { \
	typedef classname this_type; \
	base_type::init_callable_type(fields, properties); \
	classname##_num_base_slots = fields.size(); \
	{ {

#define DEFINE_FIELD(fieldname, type_str) }; } { \
	int field_index = fields.size(); \
	if(properties.count(#fieldname)) { \
		field_index = properties[#fieldname]; \
	} else { \
		fields.resize(fields.size()+1); \
		properties[#fieldname] = field_index; \
	} \
	callable_property_entry& entry = fields[field_index]; \
	entry.id = #fieldname; \
	entry.type = parse_variant_type(variant(type_str)); \
	entry.get = [](const game_logic::formula_callable& obj_instance) ->variant { \
		const this_type& obj = *dynamic_cast<const this_type*>(&obj_instance);

#define BEGIN_DEFINE_FN(fieldname, type_str) }; } { \
	int field_index = fields.size(); \
	if(properties.count(#fieldname)) { \
		field_index = properties[#fieldname]; \
	} else { \
		fields.resize(fields.size()+1); \
		properties[#fieldname] = field_index; \
	} \
	callable_property_entry& entry = fields[field_index]; \
	entry.id = #fieldname; \
	entry.type = parse_variant_type(variant("function" type_str)); \
	entry.get = [](const game_logic::formula_callable& obj_instance) ->variant { \
		static VariantFunctionTypeInfoPtr type_info; \
		if(!type_info) { \
			int min_args = 0; \
			type_info.reset(new VariantFunctionTypeInfo); \
			variant_type_ptr type = parse_variant_type(variant("function" type_str)); \
			type->is_function(&type_info->variant_types, &type_info->return_type, &min_args, NULL); \
			type_info->num_unneeded_args = type_info->variant_types.size() - min_args; \
			type_info->arg_names.resize(type_info->variant_types.size()); \
		} \
		const this_type& obj = *dynamic_cast<const this_type*>(&obj_instance); \
		return variant([&obj](const game_logic::formula_callable& args) ->variant {

#define FN_ARG(n) args.query_value_by_slot(n)
#define NUM_FN_ARGS reinterpret_cast<const game_logic::slot_formula_callable*>(&args)->num_args()

#define END_DEFINE_FN }, type_info);

#define DEFINE_SET_FIELD_TYPE(type) }; \
	entry.set_type = parse_variant_type(variant(type)); \
	entry.set = [](game_logic::formula_callable& obj_instance, const variant& value) ->void { \
		this_type& obj = *dynamic_cast<this_type*>(&obj_instance);

#define DEFINE_SET_FIELD }; \
	entry.set_type = entry.type; \
	entry.set = [](game_logic::formula_callable& obj_instance, const variant& value) ->void { this_type& obj = *dynamic_cast<this_type*>(&obj_instance);

#define END_DEFINE_CALLABLE_BASE_PTR(classname, base_ptr) }; } \
	std::vector<std::string> field_names;\
	std::vector<variant_type_ptr> types, set_types; \
	for(int n = 0; n != fields.size(); ++n) { \
		field_names.push_back(fields[n].id); \
		types.push_back(fields[n].type); \
		set_types.push_back(fields[n].set_type); \
	} \
	game_logic::formula_callable_definition_ptr def = game_logic::create_formula_callable_definition(&field_names[0], &field_names[0] + field_names.size(), game_logic::formula_callable_definition_ptr(), &types[0]); \
	for(int n = 0; n != fields.size(); ++n) { \
		if(set_types[n]) { \
			def->get_entry(n)->write_type = set_types[n]; \
		} else { \
			def->get_entry(n)->write_type = variant_type::get_type(variant::VARIANT_TYPE_NULL); \
		} \
	} \
	register_formula_callable_definition(#classname, classname##_base_str_name, def); \
	return; \
} \
namespace { \
void init_definition_##classname() { \
	classname::init_callable_type(classname##_fields, classname##_properties); \
} \
int dummy_var_##classname = game_logic::add_callable_definition_init(init_definition_##classname); \
} \
 \
variant classname::get_value(const std::string& key) const { \
	std::map<std::string, int>::const_iterator itor = classname##_properties.find(key); \
	if(itor != classname##_properties.end()) { \
		return get_value_by_slot(itor->second); \
	} else { \
		return variant(); \
	} \
} \
void classname::set_value(const std::string& key, const variant& value) { \
	std::map<std::string, int>::const_iterator itor = classname##_properties.find(key); \
	if(itor != classname##_properties.end()) { \
		set_value_by_slot(itor->second, value); \
	} \
} \
variant classname::get_value_by_slot(int slot) const { \
	ASSERT_LOG(slot >= 0 && slot < classname##_fields.size(), "Illegal slot when accessing " << #classname << ": " << slot << "/" << classname##_fields.size()); \
	const game_logic::formula_callable* callable = this; \
	if(slot < classname##_num_base_slots) callable = base_ptr; \
	return classname##_fields[slot].get(*callable); \
} \
void classname::set_value_by_slot(int slot, const variant& value) { \
	ASSERT_LOG(slot >= 0 && slot < classname##_fields.size() && classname##_fields[slot].set, "Illegal slot when writing to " << #classname << ": " << slot << "/" << classname##_fields.size()); \
	game_logic::formula_callable* callable = this; \
	if(slot < classname##_num_base_slots) callable = base_ptr; \
	classname##_fields[slot].set(*callable, value); \
}

#define END_DEFINE_CALLABLE(classname) END_DEFINE_CALLABLE_BASE_PTR(classname, this)

#endif
