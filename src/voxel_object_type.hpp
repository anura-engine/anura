#pragma once
#if defined(USE_ISOMAP)

#include <string>

#include <boost/intrusive_ptr.hpp>
#include <boost/shared_ptr.hpp>

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
		game_logic::const_formula_ptr getter, setter, init;
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
	const game_logic::formula* event_handler(int event_id) const;
	
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

	std::vector<game_logic::const_formula_ptr> event_handlers_;

	boost::intrusive_ptr<voxel_object> prototype_;
};

}
#endif
