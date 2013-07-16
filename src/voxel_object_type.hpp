#pragma once

#include <string>

#include <boost/shared_ptr.hpp>

#include "formula_callable_definition.hpp"
#include "variant.hpp"

namespace voxel
{

class voxel_object_type;

typedef boost::shared_ptr<voxel_object_type> voxel_object_type_ptr;
typedef boost::shared_ptr<const voxel_object_type> const_voxel_object_type_ptr;

class voxel_object_type
{
public:
	static game_logic::formula_callable_definition_ptr get_definition(const std::string& id);
	static voxel_object_type_ptr get(const std::string& id);

	voxel_object_type(const std::string& id, variant node);

	struct property_entry {
		property_entry() : storage_slot(-1), persistent(true), requires_initialization(false) {}
		std::string id;
		game_logic::const_formula_ptr getter, setter, init;
		boost::shared_ptr<variant> const_value;
		variant default_value;
		variant_type_ptr type, set_type;
		int storage_slot;
		bool persistent;
		bool requires_initialization;
	};

	const std::map<std::string, property_entry>& properties() const { return properties_; }
	const std::vector<property_entry>& slot_properties() const { return slot_properties_; }
	const std::vector<int>& properties_with_init() const { return properties_with_init_; }
	const std::vector<int>& properties_requiring_initialization() const { return properties_requiring_initialization_; }

private:
	game_logic::function_symbol_table* function_symbols() const;

	std::string id_;

	std::map<std::string, property_entry> properties_;
	std::vector<property_entry> slot_properties_;
	std::vector<int> properties_with_init_, properties_requiring_initialization_, properties_requiring_dynamic_initialization_;

	game_logic::const_formula_callable_definition_ptr callable_definition_;

	std::string last_initialization_property_;
};

}
