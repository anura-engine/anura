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

#include <functional>
#include <iostream>
#include <string>

#include "asserts.hpp"
#include "formula_callable_definition_fwd.hpp"
#include "formula_callable_utils.hpp"
#include "nocopy.hpp"
#include "reference_counted_object.hpp"
#include "variant_type.hpp"

namespace game_logic
{
	class FormulaCallableDefinition : public reference_counted_object
	{
	public:
		struct Entry {
			explicit Entry(const std::string& id_) : id(id_), type_definition(0), access_count(0), private_counter(0) {}
			void setVariantType(variant_type_ptr type);
			std::string id;
			ConstFormulaCallableDefinitionPtr type_definition;

			variant_type_ptr variant_type;

			//if the entry accepts different types for writes vs reads
			//(i.e. using set() or add()) then record that type here.
			variant_type_ptr write_type;

			variant_type_ptr getWriteType() const { if(write_type) { return write_type; } return variant_type; }

			mutable int access_count;

			bool isPrivate() const { return private_counter > 0; }
			int private_counter;

			std::function<bool(variant*)> constant_fn;
		};

		FormulaCallableDefinition();
		virtual ~FormulaCallableDefinition();

		//subset is a definition we expect to be a subset of this definition.
		//returns its slot offset, or -1 if it's not recognized as a subset.
		int querySubsetSlotBase(const FormulaCallableDefinition* subset) const;

		virtual int getSlot(const std::string& key) const = 0;
		virtual Entry* getEntry(int slot) = 0;
		virtual const Entry* getEntry(int slot) const = 0;
		virtual int getNumSlots() const = 0;

		virtual const Entry* getDefaultEntry() const { return nullptr; }

		virtual bool getSymbolIndexForSlot(int slot, int* index) const = 0;
		virtual int getBaseSymbolIndex() const = 0;

		virtual void setHasSymbolIndexes() { has_symbol_indexes_ = true; }
		virtual bool hasSymbolIndexes() const { return has_symbol_indexes_; }

		Entry* getEntryById(const std::string& key) {
			const int slot = getSlot(key);
			if(slot < 0) { return nullptr; } else { return getEntry(slot); }
		}

		const Entry* getEntryById(const std::string& key) const {
			const int slot = getSlot(key);
			if(slot < 0) { return nullptr; } else { return getEntry(slot); }
		}

		virtual const std::string* getTypeName() const { return !type_name_.empty() ? &type_name_ : nullptr; }
		void setTypeName(const std::string& name) { type_name_ = name; }

		virtual bool isStrict() const { return is_strict_; }
		void setStrict(bool value=true) { is_strict_ = value; }

		bool supportsSlotLookups() const { return supports_slot_lookups_; }
		void setSupportsSlotLookups(bool value) { supports_slot_lookups_ = value; }
	private:

		virtual int getSubsetSlotBase(const FormulaCallableDefinition* subset) const = 0;

		bool is_strict_;
		bool supports_slot_lookups_;
		std::string type_name_;

		bool has_symbol_indexes_;
	};

	FormulaCallableDefinitionPtr modify_formula_callable_definition(ConstFormulaCallableDefinitionPtr base_def, int slot, variant_type_ptr new_type, const FormulaCallableDefinition* new_def=nullptr);

	FormulaCallableDefinitionPtr execute_command_callable_definition(const std::string* beg, const std::string* end, ConstFormulaCallableDefinitionPtr base=nullptr, variant_type_ptr* begin_types=nullptr);
	FormulaCallableDefinitionPtr execute_command_callable_definition(const FormulaCallableDefinition::Entry* begin, const FormulaCallableDefinition::Entry* end, ConstFormulaCallableDefinitionPtr base=nullptr);

	FormulaCallableDefinitionPtr create_map_formula_callable_definition(variant_type_ptr value_type);

	int register_formula_callable_definition(const std::string& id, ConstFormulaCallableDefinitionPtr def);
	int register_formula_callable_definition(const std::string& id, const std::string& base_id, ConstFormulaCallableDefinitionPtr def);
	bool registered_definition_is_a(const std::string& derived, const std::string& base);
	ConstFormulaCallableDefinitionPtr get_formula_callable_definition(const std::string& id);
	std::string modify_class_id(const std::string& id);

	int add_callable_definition_init(void(*fn)());
	void init_callable_definitions();

	int register_formula_callable_constructor(std::string id, std::function<FormulaCallablePtr(variant)> fn);
	std::function<FormulaCallablePtr(variant)> get_callable_constructor(const std::string& id);
}

typedef std::function<variant(const game_logic::FormulaCallable&)> GetterFn;
typedef std::function<void(game_logic::FormulaCallable&, const variant&)> SetterFn;

struct CallablePropertyEntry {
	std::string id;
	variant_type_ptr type, set_type;
	GetterFn get;
	SetterFn set;
};

#define DEFINE_CALLABLE_CONSTRUCTOR(classname, arg) \
FormulaCallablePtr do_construct_ffl_##classname(variant arg) {

#define END_DEFINE_CALLABLE_CONSTRUCTOR(classname) \
} \
const int g_dummy_ffl_construct_var_##classname = register_formula_callable_constructor(#classname, do_construct_ffl_##classname);

#define DECLARE_CALLABLE(classname) \
public: \
	virtual variant getValue(const std::string& key) const override; \
	virtual variant getValueBySlot(int slot) const override;  \
	virtual void setValue(const std::string& key, const variant& value) override; \
	virtual void setValueBySlot(int slot, const variant& value) override; \
	virtual std::string getObjectId() const override { return game_logic::modify_class_id(#classname); } \
public: \
	static void init_callable_type(std::vector<CallablePropertyEntry>& v, std::map<std::string, int>& properties); \
	enum { classname##_DECLARE_CALLABLE_does_not_match_name_of_class = 0 }; \
private: 

#define BEGIN_DEFINE_CALLABLE_NOBASE(classname) \
int classname##_num_base_slots = classname::classname##_DECLARE_CALLABLE_does_not_match_name_of_class; \
const char* classname##_base_str_name = ""; \
std::vector<CallablePropertyEntry> classname##_fields; \
std::map<std::string, int> classname##_properties; \
void classname::init_callable_type(std::vector<CallablePropertyEntry>& fields, std::map<std::string, int>& properties) { \
	typedef classname this_type; \
	{ {

#define BEGIN_DEFINE_CALLABLE(classname, base_type) \
int classname##_num_base_slots = classname::classname##_DECLARE_CALLABLE_does_not_match_name_of_class; \
const char* classname##_base_str_name = #base_type; \
std::vector<CallablePropertyEntry> classname##_fields; \
std::map<std::string, int> classname##_properties; \
void classname::init_callable_type(std::vector<CallablePropertyEntry>& fields, std::map<std::string, int>& properties) { \
	typedef classname this_type; \
	base_type::init_callable_type(fields, properties); \
	classname##_num_base_slots = static_cast<int>(fields.size()); \
	{ {

#define DEFINE_FIELD(fieldname, type_str) }; } { \
	int field_index = static_cast<int>(fields.size()); \
	if(properties.count(#fieldname)) { \
		field_index = properties[#fieldname]; \
	} else { \
		fields.resize(fields.size()+1); \
		properties[#fieldname] = field_index; \
	} \
	CallablePropertyEntry& entry = fields[field_index]; \
	entry.id = #fieldname; \
	entry.type = parse_variant_type(variant(type_str)); \
	entry.get = [](const game_logic::FormulaCallable& obj_instance) ->variant { \
		const this_type& obj = *dynamic_cast<const this_type*>(&obj_instance);

#define BEGIN_DEFINE_FN(fieldname, type_str) }; } { \
	int field_index = static_cast<int>(fields.size()); \
	if(properties.count(#fieldname)) { \
		field_index = properties[#fieldname]; \
	} else { \
		fields.resize(fields.size()+1); \
		properties[#fieldname] = field_index; \
	} \
	CallablePropertyEntry& entry = fields[field_index]; \
	entry.id = #fieldname; \
	entry.type = parse_variant_type(variant("function" type_str)); \
	entry.get = [](const game_logic::FormulaCallable& obj_instance) ->variant { \
		static VariantFunctionTypeInfoPtr type_info; \
		if(!type_info) { \
			int min_args = 0; \
			type_info.reset(new VariantFunctionTypeInfo); \
			variant_type_ptr type = parse_variant_type(variant("function" type_str)); \
			type->is_function(&type_info->variant_types, &type_info->return_type, &min_args, nullptr); \
			type_info->num_unneeded_args = static_cast<int>(type_info->variant_types.size()) - min_args; \
			type_info->arg_names.resize(type_info->variant_types.size()); \
		} \
		ffl::IntrusivePtr<const FormulaCallable> ref(&obj_instance); \
		return variant([=](const game_logic::FormulaCallable& args) ->variant { \
			const this_type& obj = *dynamic_cast<const this_type*>(ref.get());

#define FN_ARG(n) args.queryValueBySlot(n)
#define NUM_FN_ARGS reinterpret_cast<const game_logic::SlotFormulaCallable*>(&args)->getNumArgs()

#define END_DEFINE_FN }, type_info);

#define DEFINE_SET_FIELD_TYPE(type) }; \
	entry.set_type = parse_variant_type(variant(type)); \
	entry.set = [](game_logic::FormulaCallable& obj_instance, const variant& value) ->void { \
		this_type& obj = *dynamic_cast<this_type*>(&obj_instance);

#define DEFINE_SET_FIELD }; \
	entry.set_type = entry.type; \
	entry.set = [](game_logic::FormulaCallable& obj_instance, const variant& value) ->void { this_type& obj = *dynamic_cast<this_type*>(&obj_instance);

#define END_DEFINE_CALLABLE_BASE_PTR(classname, base_ptr) }; } \
	std::vector<std::string> field_names;\
	std::vector<variant_type_ptr> types, set_types; \
	for(int n = 0; n != static_cast<int>(fields.size()); ++n) { \
		field_names.push_back(fields[n].id); \
		types.push_back(fields[n].type); \
		set_types.push_back(fields[n].set_type); \
	} \
	const std::string* field_names_begin = nullptr, *field_names_end = nullptr; \
	variant_type_ptr* types_begin = nullptr; \
	if(!field_names.empty()) { field_names_begin = &field_names[0]; field_names_end = &field_names[0] + field_names.size(); } \
	if(!types.empty()) { types_begin = &types[0]; } \
	game_logic::FormulaCallableDefinitionPtr def = game_logic::execute_command_callable_definition(field_names_begin, field_names_end, game_logic::FormulaCallableDefinitionPtr(), types_begin); \
	for(int n = 0; n != static_cast<int>(fields.size()); ++n) { \
		if(set_types[n]) { \
			def->getEntry(n)->write_type = set_types[n]; \
		} else { \
			def->getEntry(n)->write_type = variant_type::get_type(variant::VARIANT_TYPE_NULL); \
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
variant classname::getValue(const std::string& key) const { \
	std::map<std::string, int>::const_iterator itor = classname##_properties.find(key); \
	if(itor != classname##_properties.end()) { \
		return getValueBySlot(itor->second); \
	} else { \
		return getValueDefault(key); \
	} \
} \
void classname::setValue(const std::string& key, const variant& value) { \
	std::map<std::string, int>::const_iterator itor = classname##_properties.find(key); \
	if(itor != classname##_properties.end()) { \
		setValueBySlot(itor->second, value); \
	} else {\
		setValueDefault(key, value); \
	} \
} \
variant classname::getValueBySlot(int slot) const { \
	ASSERT_LOG(slot >= 0 && slot < static_cast<int>(classname##_fields.size()), "Illegal slot when accessing " << #classname << ": " << slot << "/" << classname##_fields.size()); \
	const game_logic::FormulaCallable* callable = this; \
	if(slot < classname##_num_base_slots) callable = base_ptr; \
	return classname##_fields[slot].get(*callable); \
} \
void classname::setValueBySlot(int slot, const variant& value) { \
	ASSERT_LOG(slot >= 0 && slot < static_cast<int>(classname##_fields.size()) && classname##_fields[slot].set, "Illegal slot when writing to " << #classname << ": " << slot << "/" << classname##_fields.size()); \
	game_logic::FormulaCallable* callable = this; \
	if(slot < classname##_num_base_slots) callable = base_ptr; \
	classname##_fields[slot].set(*callable, value); \
}

#define END_DEFINE_CALLABLE(classname) END_DEFINE_CALLABLE_BASE_PTR(classname, this)
