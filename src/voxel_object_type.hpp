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

/* XXX - needs re-write
#include <string>

#include "intrusive_ptr.hpp"

#include "formula_callable_definition.hpp"
#include "variant.hpp"
#include "voxel_object.hpp"

namespace voxel
{
	class voxel_object_type;

	typedef std::shared_ptr<voxel_object_type> voxel_object_type_ptr;
	typedef std::shared_ptr<const voxel_object_type> const_voxel_object_type_ptr;

	class voxel_object;

	class voxel_object_type
	{
	public:
		enum ENTRY_TYPE { ENTRY_ME, ENTRY_DATA, ENTRY_VALUE, NUM_ENTRY_TYPE };
		static game_logic::ConstFormulaCallableDefinitionPtr getDefinition(const std::string& id);
		static const_voxel_object_type_ptr get(const std::string& id);
		static bool isDerivedFrom(const std::string& base, const std::string& derived);

		voxel_object_type(const std::string& id, variant node);

		const std::string& id() const { return id_; }

		int num_base_slots() const { return num_base_slots_; }
		int num_storage_slots() const { return num_storage_slots_; }

		struct PropertyEntry {
			PropertyEntry() : slot(-1), storage_slot(-1), persistent(true), requires_initialization(false) {}
			std::string id;
			game_logic::ConstFormulaPtr getter, setter, init;
			std::shared_ptr<variant> const_value;
			variant default_value;
			variant_type_ptr type, set_type;
			int slot;
			int storage_slot;
			bool persistent;
			bool requires_initialization;
		};

		const std::map<std::string, PropertyEntry>& properties() const { return properties_; }
		const std::vector<PropertyEntry>& getSlotProperties() const { return slot_properties_; }
		const std::vector<int>& getPropertiesWithInit() const { return properties_with_init_; }
		const std::vector<int>& getPropertiesRequiringInitialization() const { return properties_requiring_initialization_; }
		const game_logic::Formula* event_handler(int event_id) const;
	
		const std::string& getLastInitializationProperty() const { return last_initialization_property_; }

		const voxel_object* prototype() const { return prototype_.get(); }

	private:
		game_logic::FunctionSymbolTable* getFunctionSymbols() const;

		std::string id_;
		int num_base_slots_;
		int num_storage_slots_;

		std::map<std::string, PropertyEntry> properties_;
		std::vector<PropertyEntry> slot_properties_;
		std::vector<int> properties_with_init_, properties_requiring_initialization_, properties_requiring_dynamic_initialization_;

		game_logic::FormulaCallableDefinitionPtr callable_definition_;

		std::string last_initialization_property_;

		std::vector<game_logic::ConstFormulaPtr> event_handlers_;

		ffl::IntrusivePtr<voxel_object> prototype_;
	};

}
*/
