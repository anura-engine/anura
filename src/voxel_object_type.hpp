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

typedef boost::shared_ptr<voxel_object_type> voxel_object_type_ptr;
typedef boost::shared_ptr<const voxel_object_type> const_voxel_object_type_ptr;

class voxel_object;

class voxel_object_type
{
public:
	enum ENTRY_TYPE { ENTRY_ME, ENTRY_DATA, ENTRY_VALUE, NUM_ENTRY_TYPE };
	static game_logic::const_FormulaCallable_definition_ptr get_definition(const std::string& id);
	static const_voxel_object_type_ptr get(const std::string& id);
	static bool is_derived_from(const std::string& base, const std::string& derived);

	voxel_object_type(const std::string& id, variant node);

	const std::string& id() const { return id_; }

	int num_base_slots() const { return num_base_slots_; }
	int num_storage_slots() const { return num_storage_slots_; }

	struct property_entry {
		property_entry() : slot(-1), storage_slot(-1), persistent(true), requires_initialization(false) {}
		std::string id;
		game_logic::const_formula_ptr getter, setter, init;
		boost::shared_ptr<variant> const_value;
		variant default_value;
		variant_type_ptr type, set_type;
		int slot;
		int storage_slot;
		bool persistent;
		bool requires_initialization;
	};

	const std::map<std::string, property_entry>& properties() const { return properties_; }
	const std::vector<property_entry>& slot_properties() const { return slot_properties_; }
	const std::vector<int>& properties_with_init() const { return properties_with_init_; }
	const std::vector<int>& properties_requiring_initialization() const { return properties_requiring_initialization_; }
	const game_logic::formula* event_handler(int event_id) const;
	
	const std::string& last_initialization_property() const { return last_initialization_property_; }

	const voxel_object* prototype() const { return prototype_.get(); }

private:
	game_logic::function_symbol_table* function_symbols() const;

	std::string id_;
	int num_base_slots_;
	int num_storage_slots_;

	std::map<std::string, property_entry> properties_;
	std::vector<property_entry> slot_properties_;
	std::vector<int> properties_with_init_, properties_requiring_initialization_, properties_requiring_dynamic_initialization_;

	game_logic::FormulaCallable_definition_ptr callable_definition_;

	std::string last_initialization_property_;

	std::vector<game_logic::const_formula_ptr> event_handlers_;

	boost::intrusive_ptr<voxel_object> prototype_;
};

}
#endif
