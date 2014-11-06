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
#include <map>
#include <vector>

#include <stdio.h>

#include "foreach.hpp"
#include "formula_callable_definition.hpp"
#include "formula_object.hpp"
#include "unit_test.hpp"

namespace game_logic
{

void formula_callable_definition::entry::set_variant_type(variant_type_ptr type)
{
	variant_type = type;
	if(type) {
		type_definition.reset(type->get_definition());
	}
}

formula_callable_definition::formula_callable_definition() : is_strict_(false), supports_slot_lookups_(true)
{
	int x = 4;
	ASSERT_LOG((char*)&x - (char*)this > 10000 || (char*)this - (char*)&x > 10000 , "BAD BAD");
}

formula_callable_definition::~formula_callable_definition()
{
}

namespace
{

class simple_definition : public formula_callable_definition
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
			return const_cast<formula_callable_definition*>(base_.get())->get_entry(slot);
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

	void set_base(const_formula_callable_definition_ptr base) { base_ = base; }

	void set_default(const entry& e) {
		default_entry_.reset(new entry(e));
	}

	const entry* get_default_entry() const { return default_entry_.get(); }

private:
	int base_num_slots() const { return base_ ? base_->num_slots() : 0; }
	const_formula_callable_definition_ptr base_;
	std::vector<entry> entries_;

	boost::shared_ptr<entry> default_entry_;
};

class modified_definition : public formula_callable_definition
{
public:
	modified_definition(const_formula_callable_definition_ptr base, int modified_slot, const entry& modification) : base_(base), slot_(modified_slot), mod_(modification)
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

		return const_cast<formula_callable_definition*>(base_.get())->get_entry(slot);
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
	const_formula_callable_definition_ptr base_;
	const int slot_;
	entry mod_;
};

}

formula_callable_definition_ptr modify_formula_callable_definition(const_formula_callable_definition_ptr base_def, int slot, variant_type_ptr new_type, const formula_callable_definition* new_def)
{
	const formula_callable_definition::entry* e = base_def->get_entry(slot);
	ASSERT_LOG(e, "NO DEFINITION FOUND");

	formula_callable_definition::entry new_entry(*e);

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

	return formula_callable_definition_ptr(new modified_definition(base_def, slot, new_entry));
}

formula_callable_definition_ptr create_formula_callable_definition(const std::string* i1, const std::string* i2, const_formula_callable_definition_ptr base, variant_type_ptr* types)
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

	return formula_callable_definition_ptr(def);
}

formula_callable_definition_ptr create_formula_callable_definition(const formula_callable_definition::entry* i1, const formula_callable_definition::entry* i2, const_formula_callable_definition_ptr base)
{
	simple_definition* def = new simple_definition;
	def->set_base(base);
	while(i1 != i2) {
		def->add(*i1);
		++i1;
	}

	return formula_callable_definition_ptr(def);
}

formula_callable_definition_ptr create_map_formula_callable_definition(variant_type_ptr value_type)
{
	simple_definition* def = new simple_definition;
	formula_callable_definition::entry e("");
	e.set_variant_type(value_type);
	def->set_default(e);
	return formula_callable_definition_ptr(def);
}

namespace {
std::map<std::string, const_formula_callable_definition_ptr> registry;
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

int register_formula_callable_definition(const std::string& id, const_formula_callable_definition_ptr def)
{
	registry[id] = def;
	return ++num_definitions;
}

int register_formula_callable_definition(const std::string& id, const std::string& base_id, const_formula_callable_definition_ptr def)
{
	if(base_id != "") {
		g_builtin_bases()[id] = base_id;
	}
	return register_formula_callable_definition(id, def);
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

const_formula_callable_definition_ptr get_formula_callable_definition(const std::string& id)
{
	std::map<std::string, const_formula_callable_definition_ptr>::const_iterator itor = registry.find(id);
	if(itor != registry.end()) {
		return itor->second;
	} else {
		return const_formula_callable_definition_ptr();
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
