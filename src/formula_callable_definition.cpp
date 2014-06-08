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
#include <iostream>
#include <map>
#include <vector>

#include <stdio.h>

#include "foreach.hpp"
#include "formula_callable_definition.hpp"
#include "formula_object.hpp"
#include "unit_test.hpp"

namespace game_logic
{

void FormulaCallable_definition::entry::set_variant_type(variant_type_ptr type)
{
	variant_type = type;
	if(type) {
		type_definition.reset(type->get_definition());
	}
}

FormulaCallable_definition::FormulaCallable_definition() : is_strict_(false), supports_slot_lookups_(true)
{
	int x = 4;
	ASSERT_LOG((char*)&x - (char*)this > 10000 || (char*)this - (char*)&x > 10000 , "BAD BAD");
}

FormulaCallable_definition::~FormulaCallable_definition()
{
}

namespace
{

class simple_definition : public FormulaCallable_definition
{
public:
	simple_definition() : base_(NULL)
	{}

	int get_slot(const std::string& key) const {
		int index = 0;
		foreach(const entry& e, entries_) {
			if(e.id == key) {
				return base_num_slots() + index;
			}

			++index;
		}

		if(base_) {
			int result = base_->get_slot(key);
			if(result != -1) {
				return result;
			}
		}

		return -1;
	}

	entry* get_entry(int slot) {
		if(base_ && slot < base_num_slots()) {
			return const_cast<FormulaCallable_definition*>(base_.get())->get_entry(slot);
		}

		slot -= base_num_slots();

		if(slot < 0 || slot >= entries_.size()) {
			return NULL;
		}

		return &entries_[slot];
	}

	const entry* get_entry(int slot) const {
		if(base_ && slot < base_num_slots()) {
			return base_->get_entry(slot);
		}

		slot -= base_num_slots();

		if(slot < 0 || slot >= entries_.size()) {
			return NULL;
		}

		return &entries_[slot];
	}

	int num_slots() const { return base_num_slots() + entries_.size(); }

	void add(const std::string& id) {
		entries_.push_back(entry(id));
	}

	void add(const std::string& id, variant_type_ptr type) {
		entries_.push_back(entry(id));

		if(type) {
			entries_.back().variant_type = type;
			std::string class_name;
			if(type->is_class(&class_name)) {
				entries_.back().type_definition = get_class_definition(class_name);
			}
		}
	}

	void add(const entry& e) {
		entries_.push_back(e);
	}

	void set_base(const_FormulaCallable_definition_ptr base) { base_ = base; }

	void set_default(const entry& e) {
		default_entry_.reset(new entry(e));
	}

	const entry* get_default_entry() const { return default_entry_.get(); }

private:
	int base_num_slots() const { return base_ ? base_->num_slots() : 0; }
	const_FormulaCallable_definition_ptr base_;
	std::vector<entry> entries_;

	boost::shared_ptr<entry> default_entry_;
};

class modified_definition : public FormulaCallable_definition
{
public:
	modified_definition(const_FormulaCallable_definition_ptr base, int modified_slot, const entry& modification) : base_(base), slot_(modified_slot), mod_(modification)
	{
		set_supports_slot_lookups(base_->supports_slot_lookups());
	}

	int get_slot(const std::string& key) const {
		return base_->get_slot(key);
	}

	entry* get_entry(int slot) {
		if(slot == slot_) {
			return &mod_;
		}

		return const_cast<FormulaCallable_definition*>(base_.get())->get_entry(slot);
	}

	const entry* get_entry(int slot) const {
		if(slot == slot_) {
			return &mod_;
		}

		return base_->get_entry(slot);
	}

	int num_slots() const { return base_->num_slots(); }

	const std::string* type_name() const { return base_->type_name(); }

	bool is_strict() const { return base_->is_strict(); }

private:
	const_FormulaCallable_definition_ptr base_;
	const int slot_;
	entry mod_;
};

}

FormulaCallable_definition_ptr modify_FormulaCallable_definition(const_FormulaCallable_definition_ptr base_def, int slot, variant_type_ptr new_type, const FormulaCallable_definition* new_def)
{
	const FormulaCallable_definition::entry* e = base_def->get_entry(slot);
	ASSERT_LOG(e, "NO DEFINITION FOUND");

	FormulaCallable_definition::entry new_entry(*e);

	if(new_type) {
		if(!new_entry.write_type) {
			new_entry.write_type = new_entry.variant_type;
		}

		new_entry.variant_type = new_type;
		if(!new_def) {
			new_def = new_type->get_definition();
		}
	}

	if(new_def) {
		new_entry.type_definition = new_def;
	}

	return FormulaCallable_definition_ptr(new modified_definition(base_def, slot, new_entry));
}

FormulaCallable_definition_ptr executeCommand_callable_definition(const std::string* i1, const std::string* i2, const_FormulaCallable_definition_ptr base, variant_type_ptr* types)
{
	simple_definition* def = new simple_definition;
	def->set_base(base);
	while(i1 != i2) {

		if(types) {
			def->add(*i1, *types++);
		} else {
			def->add(*i1);
		}
		++i1;
	}

	return FormulaCallable_definition_ptr(def);
}

FormulaCallable_definition_ptr executeCommand_callable_definition(const FormulaCallable_definition::entry* i1, const FormulaCallable_definition::entry* i2, const_FormulaCallable_definition_ptr base)
{
	simple_definition* def = new simple_definition;
	def->set_base(base);
	while(i1 != i2) {
		def->add(*i1);
		++i1;
	}

	return FormulaCallable_definition_ptr(def);
}

FormulaCallable_definition_ptr create_map_FormulaCallable_definition(variant_type_ptr value_type)
{
	simple_definition* def = new simple_definition;
	FormulaCallable_definition::entry e("");
	e.set_variant_type(value_type);
	def->set_default(e);
	return FormulaCallable_definition_ptr(def);
}

namespace {
std::map<std::string, const_FormulaCallable_definition_ptr> registry;
int num_definitions = 0;

std::vector<boost::function<void()> >& callable_init_routines() {
	static std::vector<boost::function<void()> > v;
	return v;
}

std::map<std::string, std::string>& g_builtin_bases()
{
	static std::map<std::string, std::string> instance;
	return instance;
}
}

int register_FormulaCallable_definition(const std::string& id, const_FormulaCallable_definition_ptr def)
{
	registry[id] = def;
	return ++num_definitions;
}

int register_FormulaCallable_definition(const std::string& id, const std::string& base_id, const_FormulaCallable_definition_ptr def)
{
	if(base_id != "") {
		g_builtin_bases()[id] = base_id;
	}
	return register_FormulaCallable_definition(id, def);
}

bool registered_definition_is_a(const std::string& derived, const std::string& base)
{
	if(derived == base) {
		return true;
	}

	const std::map<std::string, std::string>::const_iterator itor = g_builtin_bases().find(derived);
	if(itor == g_builtin_bases().end()) {
		return false;
	}

	if(itor->second == base) {
		return true;
	}

	return registered_definition_is_a(itor->second, base);
}

const_FormulaCallable_definition_ptr get_FormulaCallable_definition(const std::string& id)
{
	std::map<std::string, const_FormulaCallable_definition_ptr>::const_iterator itor = registry.find(id);
	if(itor != registry.end()) {
		return itor->second;
	} else {
		return const_FormulaCallable_definition_ptr();
	}
}

int add_callable_definition_init(void(*fn)())
{
	callable_init_routines().push_back(fn);
	return callable_init_routines().size();
}

void init_callable_definitions()
{
	foreach(const boost::function<void()>& fn, callable_init_routines()) {
		fn();
	}

	callable_init_routines().clear();
}

}

COMMAND_LINE_UTILITY(document_builtins)
{
	using namespace game_logic;
	
	for(auto i = registry.begin(); i != registry.end(); ++i) {
		auto item = i->second;
		std::cout << i->first << " ::";
		auto derived_from = g_builtin_bases().find(i->first);
		if(derived_from != g_builtin_bases().end()) {
			std::cout << " " << derived_from->second;
		}

		std::cout << "\n";

		for(int n = 0; n != item->num_slots(); ++n) {
			auto entry = item->get_entry(n);
			std::cout << "  - " << entry->id << ": " << entry->variant_type->to_string();
			if(entry->get_write_type() != entry->variant_type) {
				std::string write_type = entry->get_write_type()->to_string();
				if(write_type == "null") {
					std::cout << " (read-only)";
				} else {
					std::cout << " (write: " << write_type << ")";
				}
			}
			std::cout << "\n";
		}
	}

	std::cout << "\n";
}
