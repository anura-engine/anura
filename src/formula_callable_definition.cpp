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

#include <iostream>
#include <map>
#include <vector>

#include <stdio.h>

#include "formula_callable_definition.hpp"
#include "formula_object.hpp"
#include "unit_test.hpp"

namespace game_logic
{
	void FormulaCallableDefinition::Entry::setVariantType(variant_type_ptr type)
	{
		variant_type = type;
		if(type) {
			type_definition.reset(type->getDefinition());
		}
	}

	FormulaCallableDefinition::FormulaCallableDefinition() : is_strict_(false), supports_slot_lookups_(true)
	{
		int x = 4;
		ASSERT_LOG((char*)&x - (char*)this > 10000 || (char*)this - (char*)&x > 10000 , "BAD BAD");
	}

	FormulaCallableDefinition::~FormulaCallableDefinition()
	{
	}

	namespace
	{
		class simple_definition : public FormulaCallableDefinition
		{
		public:
			simple_definition() : base_(nullptr)
			{}

			int getSlot(const std::string& key) const {
				int index = 0;
				for(const Entry& e : entries_) {
					if(e.id == key) {
						return base_getNumSlots() + index;
					}

					++index;
				}

				if(base_) {
					int result = base_->getSlot(key);
					if(result != -1) {
						return result;
					}
				}

				return -1;
			}

			Entry* getEntry(int slot) {
				if(base_ && slot < base_getNumSlots()) {
					return const_cast<FormulaCallableDefinition*>(base_.get())->getEntry(slot);
				}

				slot -= base_getNumSlots();

				if(slot < 0 || slot >= static_cast<int>(entries_.size())) {
					return nullptr;
				}

				return &entries_[slot];
			}

			const Entry* getEntry(int slot) const {
				if(base_ && slot < base_getNumSlots()) {
					return base_->getEntry(slot);
				}

				slot -= base_getNumSlots();

				if(slot < 0 || slot >= static_cast<int>(entries_.size())) {
					return nullptr;
				}

				return &entries_[slot];
			}

			int getNumSlots() const { return base_getNumSlots() + entries_.size(); }

			void add(const std::string& id) {
				entries_.push_back(Entry(id));
			}

			void add(const std::string& id, variant_type_ptr type) {
				entries_.push_back(Entry(id));

				if(type) {
					entries_.back().variant_type = type;
					std::string class_name;
					if(type->is_class(&class_name)) {
						entries_.back().type_definition = get_class_definition(class_name);
					}
				}
			}

			void add(const Entry& e) {
				entries_.push_back(e);
			}

			void set_base(ConstFormulaCallableDefinitionPtr base) { base_ = base; }

			void set_default(const Entry& e) {
				default_entry_.reset(new Entry(e));
			}

			const Entry* getDefaultEntry() const { return default_entry_.get(); }

		private:
			int base_getNumSlots() const { return base_ ? base_->getNumSlots() : 0; }
			ConstFormulaCallableDefinitionPtr base_;
			std::vector<Entry> entries_;

			std::shared_ptr<Entry> default_entry_;
		};

		class modified_definition : public FormulaCallableDefinition
		{
		public:
			modified_definition(ConstFormulaCallableDefinitionPtr base, int modified_slot, const Entry& modification) 
				: base_(base), slot_(modified_slot), mod_(modification)
			{
				setSupportsSlotLookups(base_->supportsSlotLookups());
			}

			int getSlot(const std::string& key) const {
				return base_->getSlot(key);
			}

			Entry* getEntry(int slot) {
				if(slot == slot_) {
					return &mod_;
				}

				return const_cast<FormulaCallableDefinition*>(base_.get())->getEntry(slot);
			}

			const Entry* getEntry(int slot) const {
				if(slot == slot_) {
					return &mod_;
				}

				return base_->getEntry(slot);
			}

			int getNumSlots() const { return base_->getNumSlots(); }

			const std::string* getTypeName() const { return base_->getTypeName(); }

			bool isStrict() const { return base_->isStrict(); }

		private:
			ConstFormulaCallableDefinitionPtr base_;
			const int slot_;
			Entry mod_;
		};
	}

	FormulaCallableDefinitionPtr modify_formula_callable_definition(ConstFormulaCallableDefinitionPtr base_def, int slot, variant_type_ptr new_type, const FormulaCallableDefinition* new_def)
	{
		const FormulaCallableDefinition::Entry* e = base_def->getEntry(slot);
		ASSERT_LOG(e, "NO DEFINITION FOUND");

		FormulaCallableDefinition::Entry new_entry(*e);

		if(new_type) {
			if(!new_entry.write_type) {
				new_entry.write_type = new_entry.variant_type;
			}

			new_entry.variant_type = new_type;
			if(!new_def) {
				new_def = new_type->getDefinition();
			}
		}

		if(new_def) {
			new_entry.type_definition = new_def;
		}

		return FormulaCallableDefinitionPtr(new modified_definition(base_def, slot, new_entry));
	}

	FormulaCallableDefinitionPtr execute_command_callable_definition(const std::string* i1, const std::string* i2, ConstFormulaCallableDefinitionPtr base, variant_type_ptr* types)
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

		return FormulaCallableDefinitionPtr(def);
	}

	FormulaCallableDefinitionPtr execute_command_callable_definition(const FormulaCallableDefinition::Entry* i1, const FormulaCallableDefinition::Entry* i2, ConstFormulaCallableDefinitionPtr base)
	{
		simple_definition* def = new simple_definition;
		def->set_base(base);
		while(i1 != i2) {
			def->add(*i1);
			++i1;
		}

		return FormulaCallableDefinitionPtr(def);
	}

	FormulaCallableDefinitionPtr create_map_formula_callable_definition(variant_type_ptr value_type)
	{
		simple_definition* def = new simple_definition;
		FormulaCallableDefinition::Entry e("");
		e.setVariantType(value_type);
		def->set_default(e);
		return FormulaCallableDefinitionPtr(def);
	}

	namespace {
	std::map<std::string, ConstFormulaCallableDefinitionPtr> registry;
	int num_definitions = 0;

	std::vector<std::function<void()> >& callable_init_routines() {
		static std::vector<std::function<void()> > v;
		return v;
	}

	std::map<std::string, std::string>& g_builtin_bases()
	{
		static std::map<std::string, std::string> instance;
		return instance;
	}
	}

	int register_formula_callable_definition(const std::string& id, ConstFormulaCallableDefinitionPtr def)
	{
		registry[id] = def;
		return ++num_definitions;
	}

	int register_formula_callable_definition(const std::string& id, const std::string& base_id, ConstFormulaCallableDefinitionPtr def)
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

	ConstFormulaCallableDefinitionPtr get_formula_callable_definition(const std::string& id)
	{
		std::map<std::string, ConstFormulaCallableDefinitionPtr>::const_iterator itor = registry.find(id);
		if(itor != registry.end()) {
			return itor->second;
		} else {
			return ConstFormulaCallableDefinitionPtr();
		}
	}

	int add_callable_definition_init(void(*fn)())
	{
		callable_init_routines().push_back(fn);
		return callable_init_routines().size();
	}

	void init_callable_definitions()
	{
		for(const std::function<void()>& fn : callable_init_routines()) {
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

		for(int n = 0; n != item->getNumSlots(); ++n) {
			auto entry = item->getEntry(n);
			std::cout << "  - " << entry->id << ": " << entry->variant_type->to_string();
			if(entry->getWriteType() != entry->variant_type) {
				std::string write_type = entry->getWriteType()->to_string();
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
