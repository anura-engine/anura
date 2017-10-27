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

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <map>
#include <string>
#include <stdio.h>
#include <stdlib.h>

#include "base64.hpp"
#include "code_editor_dialog.hpp"
#include "compress.hpp"
#include "custom_object_functions.hpp"
#include "filesystem.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "formula_object.hpp"
#include "formula_profiler.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "uuid.hpp"
#include "variant_type.hpp"
#include "variant_utils.hpp"

#ifdef USE_LUA
#include "lua_iface.hpp"
#endif

#if defined(_MSC_VER)
#define strtoll _strtoi64
#endif

PREF_BOOL(ffl_vm_opt_const_library_calls, true, "Optimize library calls");
PREF_BOOL(ffl_allow_obj_api_from_class, false, "Allow classes to have access to custom object api.");

namespace
{
game_logic::FunctionSymbolTable* getClassFunctionSymbolTable()
{
	if(g_ffl_allow_obj_api_from_class) {
		return &get_custom_object_functions_symbol_table();
	}

	return nullptr;
}

}

namespace game_logic
{
	void invalidate_class_definition(const std::string& class_name);

	namespace 
	{
		variant flatten_list_of_maps(variant v) {
			if(v.is_list() && v.num_elements() >= 1) {
				variant result = flatten_list_of_maps(v[0]);
				for(int n = 1; n < v.num_elements(); ++n) {
					result = result + flatten_list_of_maps(v[n]);
				}

				return result;
			}

			return v;
		}

		class backup_entry_scope {
			FormulaCallableDefinition::Entry backup_;
			FormulaCallableDefinition::Entry& target_;
		public:
			backup_entry_scope(FormulaCallableDefinition::Entry& e) : backup_(e), target_(e) {
			}
			~backup_entry_scope() {
				target_ = backup_;
			}
		};

		ffl::IntrusivePtr<const FormulaClass> get_class(const std::string& type);

		struct PropertyEntry {
			PropertyEntry() : variable_slot(-1) {
			}
			PropertyEntry(const std::string& class_name, const std::string& prop_name, variant node, int& state_slot) : variable_slot(-1) {
				name = prop_name;
		name_variant = variant(name);

				FormulaCallableDefinitionPtr class_def = get_class_definition(class_name);

				FormulaCallableDefinition::Entry* data_entry = class_def->getEntry(class_def->getSlot("_data"));
				FormulaCallableDefinition::Entry* value_entry = class_def->getEntry(class_def->getSlot("value"));
				FormulaCallableDefinition::Entry* prop_entry = class_def->getEntry(class_def->getSlot(prop_name));
				assert(data_entry);
				assert(value_entry);
				assert(prop_entry);

				backup_entry_scope backup1(*data_entry);
				backup_entry_scope backup2(*value_entry);

				value_entry->setVariantType(prop_entry->variant_type);
				*data_entry = *prop_entry;

				const Formula::StrictCheckScope strict_checking;
				if(node.is_string()) {
					getter = game_logic::Formula::createOptionalFormula(node, getClassFunctionSymbolTable(), get_class_definition(class_name));
					ASSERT_LOG(getter, "COULD NOT PARSE CLASS FORMULA " << class_name << "." << prop_name);

					ASSERT_LOG(getter->queryVariantType()->is_any() == false, "COULD NOT INFER TYPE FOR CLASS PROPERTY " << class_name << "." << prop_name << ". SET THIS PROPERTY EXPLICITLY");

					FormulaCallableDefinition::Entry* entry = class_def->getEntryById(prop_name);
					ASSERT_LOG(entry != nullptr, "COULD NOT FIND CLASS PROPERTY ENTRY " << class_name << "." << prop_name);

					entry->setVariantType(getter->queryVariantType());
					return;
				} else if(node.is_map()) {
					if(node["variable"].as_bool(true)) {
						variable_slot = state_slot++;
					}

					if(node["get"].is_string()) {
						getter = game_logic::Formula::createOptionalFormula(node["get"], getClassFunctionSymbolTable(), get_class_definition(class_name));
					}

					if(node["set"].is_string()) {
						setter = game_logic::Formula::createOptionalFormula(node["set"], getClassFunctionSymbolTable(), get_class_definition(class_name));
					}

					default_value = node["default"];

					if(node["initialize"].is_string()) {
						initializer = game_logic::Formula::createOptionalFormula(node["initialize"], getClassFunctionSymbolTable());
					} else if(node["init"].is_string()) {
						initializer = game_logic::Formula::createOptionalFormula(node["init"], getClassFunctionSymbolTable());
					}

					variant valid_types = node["type"];
					if(valid_types.is_null() && variable_slot != -1) {
						variant default_value = node["default"];
						if(default_value.is_null() == false) {
							valid_types = variant(variant::variant_type_to_string(default_value.type()));
						}
					}

					if(valid_types.is_null() == false) {
						get_type = parse_variant_type(valid_types);
						set_type = get_type;
					}
					valid_types = node["set_type"];
					if(valid_types.is_null() == false) {
						set_type = parse_variant_type(valid_types);
					}
				} else {
					variable_slot = state_slot++;
					default_value = node;
					set_type = get_type = get_variant_type_from_value(node);
				}
			}

			std::string name;
			game_logic::ConstFormulaPtr getter, setter, initializer;
			variant name_variant;

			variant_type_ptr get_type, set_type;
			int variable_slot;

			variant default_value;
		};

std::map<std::string, std::string>& class_path_map()
{
	static std::map<std::string, std::string> mapping;
	static bool init = false;
	if(!init) {
		init = true;
		std::map<std::string, std::string> items;
		module::get_unique_filenames_under_dir("data/classes/", &items, module::MODULE_NO_PREFIX);
		for(auto p : items) {
			std::string key = p.first;
			if(key.size() > 4 && std::equal(key.end()-4, key.end(), ".cfg")) {
				key.resize(key.size()-4);
			} else {
				continue;
			}

			if(mapping.count(key) != 0) {
				continue;
			}

			mapping[key] = p.second;
		}
	}

	return mapping;
}

		std::map<std::string, variant> class_node_map;

		std::map<std::string, variant> unit_test_class_node_map;

		void load_class_node(const std::string& type, const variant& node)
		{
			class_node_map[type] = node;
	
			const variant classes = flatten_list_of_maps(node["classes"]);
			if(classes.is_map()) {
				for(variant key : classes.getKeys().as_list()) {
					load_class_node(type + "." + key.as_string(), classes[key]);
				}
			}
		}

		void load_class_nodes(const std::string& type)
		{
			auto itor = class_path_map().find(type);
			ASSERT_LOG(itor != class_path_map().end(), "Could not find FFL class '" << type << "'");
			const std::string& path = itor->second;
			const std::string real_path = module::map_file(path);

			sys::notify_on_file_modification(real_path, std::bind(invalidate_class_definition, type));

			variant v = json::parse_from_file_or_die(path);
			ASSERT_LOG(v.is_map(), "COULD NOT PARSE FFL CLASS: " << type);

			load_class_node(type, v);
		}

		variant get_class_node(const std::string& type)
		{
			std::map<std::string, variant>::const_iterator i = class_node_map.find(type);
			if(i != class_node_map.end()) {
				return i->second;
			}

			if (unit_test_class_node_map.size()) {
				i = unit_test_class_node_map.find(type);
				if (i != unit_test_class_node_map.end()) {
					return i->second;
				}
			}

			if(std::find(type.begin(), type.end(), '.') != type.end()) {
				std::vector<std::string> v = util::split(type, '.');
				load_class_nodes(v.front());
			} else {
				load_class_nodes(type);
			}

			i = class_node_map.find(type);

			ASSERT_LOG(i != class_node_map.end(), "COULD NOT FIND CLASS: " << type);
			return i->second;
		}

		enum CLASS_BASE_FIELDS { FIELD_PRIVATE, FIELD_VALUE, FIELD_SELF, FIELD_ME, FIELD_NEW_IN_UPDATE, FIELD_ORPHANED, FIELD_CLASS, FIELD_LIB, FIELD_UUID, NUM_BASE_FIELDS };
		static const std::string BaseFields[] = {"_data", "value", "self", "me", "new_in_update", "orphaned_by_update", "_class", "lib", "_uuid"};

		class FormulaClassDefinition : public FormulaCallableDefinition
		{
		public:
			FormulaClassDefinition(const std::string& class_name, const variant& var)
			  : type_name_("class " + class_name)
			{
				setStrict();

				for(int n = 0; n != NUM_BASE_FIELDS; ++n) {
					properties_[BaseFields[n]] = n;
					slots_.push_back(Entry(BaseFields[n]));
					switch(n) {
					case FIELD_PRIVATE:
					slots_.back().variant_type = variant_type::get_type(variant::VARIANT_TYPE_MAP);
					break;
					case FIELD_VALUE:
					slots_.back().variant_type = variant_type::get_any();
					break;
					case FIELD_SELF:
					case FIELD_ME:
					slots_.back().variant_type = variant_type::get_class(class_name);
					break;
					case FIELD_NEW_IN_UPDATE:
					case FIELD_ORPHANED:
					slots_.back().variant_type = variant_type::get_type(variant::VARIANT_TYPE_BOOL);
					break;
					case FIELD_CLASS:
					slots_.back().variant_type = variant_type::get_type(variant::VARIANT_TYPE_STRING);
					break;
					case FIELD_LIB:
					slots_.back().type_definition = get_library_definition().get();
					slots_.back().variant_type = variant_type::get_builtin("library");
					assert(slots_.back().variant_type);
					break;
					case FIELD_UUID:
					slots_.back().variant_type = variant_type::get_type(variant::VARIANT_TYPE_STRING);
					break;
					}
				}

				ASSERT_LOG(var["bases"].is_null() || var["base_type"].is_null(), "MULTIPLE INHERITANT NOT YET SUPPORTED");

				std::vector<variant> nodes;
				nodes.push_back(var);
				while(nodes.back()["bases"].is_list() && nodes.back()["bases"].num_elements() > 0) {
					variant nodes_v = nodes.back()["bases"];
					ASSERT_LOG(nodes_v.num_elements() == 1, "MULTIPLE INHERITANCE NOT YET SUPPORTED");

					variant new_node = get_class_node(nodes_v[0].as_string());
					ASSERT_LOG(std::count(nodes.begin(), nodes.end(), new_node) == 0, "RECURSIVE INHERITANCE DETECTED");

					nodes.push_back(new_node);
				}

				std::reverse(nodes.begin(), nodes.end());

				variant base_builtin = nodes.back()["base_type"];
				if(base_builtin.is_string()) {
					std::string builtin = base_builtin.as_string();
					auto ctor = game_logic::get_callable_constructor(builtin);
					ASSERT_LOG(ctor, "Base type does not have a constructor: " << builtin);

					auto base = game_logic::get_formula_callable_definition(builtin);

					for(int n = 0; n < base->getNumSlots(); ++n) {
						const Entry* e = base->getEntry(n);
						properties_[e->id] = static_cast<int>(slots_.size());
						slots_.push_back(*e);

					}
				}

				for(const variant& node : nodes) {
					variant properties = node["properties"];
					if(!properties.is_map()) {
						properties = node;
					}

					for(variant key : properties.getKeys().as_list()) {
						ASSERT_LOG(std::count(BaseFields, BaseFields + NUM_BASE_FIELDS, key.as_string()) == 0, "Class " << class_name << " has property '" << key.as_string() << "' which is a reserved word");
						ASSERT_LOG(key.as_string() != "", "Class " << class_name << " has property name which is empty");

						if(properties_.count(key.as_string()) == 0) {
							properties_[key.as_string()] = static_cast<int>(slots_.size());
							slots_.push_back(Entry(key.as_string()));
							if(key.as_string()[0] == '_') {
								slots_.back().private_counter++;
							}
						}

						const int slot = properties_[key.as_string()];

						variant prop_node = properties[key];
						if(prop_node.is_map()) {
							variant access = prop_node["access"];
							if(access.is_null() == false) {
								if(access.as_string() == "public") {
									slots_[slot].private_counter = 0;
								} else if(access.as_string() == "private") {
									slots_[slot].private_counter = 1;
								} else {
									ASSERT_LOG(false, "Unknown property access specifier '" << access.as_string() << "' " << access.debug_location());
								}
							}

							variant valid_types = prop_node["type"];
							if(valid_types.is_null() && prop_node["variable"].is_bool() && prop_node["variable"].as_bool()) {
								variant default_value = prop_node["default"];
								if(default_value.is_null() == false) {
									valid_types = variant(variant::variant_type_to_string(default_value.type()));
								}
							}

							if(valid_types.is_null() == false) {
								slots_[slot].variant_type = parse_variant_type(valid_types);
							}

							variant set_type = prop_node["set_type"];
							if(set_type.is_null() == false) {
								slots_[slot].write_type = parse_variant_type(set_type);
							}
						} else if(prop_node.is_string()) {
							variant_type_ptr fn_type = parse_optional_function_type(prop_node);
							if(fn_type) {
								slots_[slot].variant_type = fn_type;
							} else {
								variant_type_ptr type = parse_optional_formula_type(prop_node);
								if(type) {
									slots_[slot].variant_type = type;
								} else {
									const Formula::StrictCheckScope strict_checking(false);
									FormulaPtr f = Formula::createOptionalFormula(prop_node, getClassFunctionSymbolTable());
									if(f) {
										slots_[slot].variant_type = f->queryVariantType();
									}
								}
							}
						} else {
							slots_[slot].variant_type = get_variant_type_from_value(prop_node);
						}
					}
				}
			}

			virtual ~FormulaClassDefinition() {}

			void init() {
				for(Entry& e : slots_) {
					if(e.variant_type && !e.type_definition) {
						e.type_definition = e.variant_type->getDefinition();
					}
				}
			}

			virtual int getSlot(const std::string& key) const override {
				std::map<std::string, int>::const_iterator itor = properties_.find(key);
				if(itor != properties_.end()) {
					return itor->second;
				}
		
				return -1;
			}

			virtual Entry* getEntry(int slot) override {
				if(slot < 0 || static_cast<unsigned>(slot) >= slots_.size()) {
					return nullptr;
				}

				return &slots_[slot];
			}

			virtual const Entry* getEntry(int slot) const override {
				if(slot < 0 || static_cast<unsigned>(slot) >= slots_.size()) {
					return nullptr;
				}

				return &slots_[slot];
			}
			virtual int getNumSlots() const override {
				return static_cast<int>(slots_.size());
			}

			bool getSymbolIndexForSlot(int slot, int* index) const override {
				return false;
			}

			int getBaseSymbolIndex() const override {
				return 0;
			}

			const std::string* getTypeName() const override {
				return &type_name_;
			}

			void pushPrivateAccess() {
				for(Entry& e : slots_) {
					e.private_counter--;
				}
			}

			void popPrivateAccess() {
				for(Entry& e : slots_) {
					e.private_counter++;
				}
			}

			int getSubsetSlotBase(const FormulaCallableDefinition* subset) const override
			{
				return -1;
			}
			

		private:
			std::map<std::string, int> properties_;
			std::vector<Entry> slots_;
			std::string type_name_;
		};

		struct definition_access_private_in_scope
		{
			definition_access_private_in_scope(FormulaClassDefinition& def) : def_(def)
			{
				def_.pushPrivateAccess();
			}
			~definition_access_private_in_scope()
			{
				def_.popPrivateAccess();
			}

			FormulaClassDefinition& def_;
		};

		typedef std::map<std::string, ffl::IntrusivePtr<FormulaClassDefinition> > class_definition_map;
		class_definition_map class_definitions;

		typedef std::map<std::string, ffl::IntrusivePtr<FormulaClass> > classes_map;

		bool in_unit_test = false;
		std::vector<FormulaClass*> unit_test_queue;
	}

	FormulaCallableDefinitionPtr get_class_definition(const std::string& name)
	{
		class_definition_map::iterator itor = class_definitions.find(name);
		if(itor != class_definitions.end()) {
			return itor->second;
		}

		FormulaClassDefinition* def = new FormulaClassDefinition(name, get_class_node(name));
		class_definitions[name].reset(def);
		def->init();

		return def;
	}

	class FormulaClass : public reference_counted_object
	{
	public:
		FormulaClass(const std::string& class_name, const variant& node);
		const std::function<FormulaCallablePtr(variant)>& getBuiltinCtor() const { return builtin_ctor_; }
		int getBuiltinSlots() const { return builtin_slots_; }
		const ConstFormulaCallableDefinitionPtr& getBuiltinDef() const { return builtin_def_; }

		void setName(const std::string& name);
		const std::string& name() const { return name_; }
		const variant& nameVariant() const { return name_variant_; }
		const variant& privateData() const { return private_data_; }
#if defined(USE_LUA)
		bool has_lua() const { return !lua_node_.is_null(); }
		const variant & getLuaNode() const { return lua_node_; }
		const std::shared_ptr<lua::CompiledChunk> & getLuaInit( lua::LuaContext & ) const ;
#endif
		const std::vector<game_logic::ConstFormulaPtr>& constructor() const { return constructor_; }
		const std::map<std::string, int>& properties() const { return properties_; }
		const std::vector<PropertyEntry>& slots() const { return slots_; }
		const std::vector<const PropertyEntry*>& variableSlots() const { return variable_slots_; }
		const classes_map& subClasses() const { return sub_classes_; }

		bool isA(const std::string& name) const;

		int getNstateSlots() const { return nstate_slots_; }

		void build_nested_classes();
		void run_unit_tests();

		bool is_library_only() const { return is_library_only_; }

		void update_class(FormulaClass* new_class) {
			if(new_class == this) {
				return;
			}

			new_class->previous_version_.reset(this);

			for(int i = 0; i < slots_.size() && i < new_class->slots_.size(); ++i) {
				if(slots_[i].name == new_class->slots_[i].name &&
				   slots_[i].variable_slot == new_class->slots_[i].variable_slot) {
					slots_[i] = new_class->slots_[i];
				}
			}

			if(previous_version_) {
				previous_version_->update_class(this);
			}
		}

	private:
		void build_nested_classes(variant obj);

		std::function<FormulaCallablePtr(variant)> builtin_ctor_;
		ConstFormulaCallableDefinitionPtr builtin_def_;
		int builtin_slots_;

		std::string name_;
		variant name_variant_;
		variant private_data_;
		std::vector<game_logic::ConstFormulaPtr> constructor_;
		std::map<std::string, int> properties_;

		std::vector<PropertyEntry> slots_;

		std::vector<const PropertyEntry*> variable_slots_;
	
		classes_map sub_classes_;

		variant unit_test_;

		std::vector<ffl::IntrusivePtr<const FormulaClass> > bases_;

		variant nested_classes_;

		ffl::IntrusivePtr<FormulaClass> previous_version_;

#if defined(USE_LUA)
		// For lua integration
		variant lua_node_;
		mutable std::shared_ptr<lua::CompiledChunk> lua_compiled_;
#endif

		int nstate_slots_;
		
		bool is_library_only_;
	};

	bool is_class_derived_from(const std::string& derived, const std::string& base)
	{
		if(derived == base) {
			return true;
		}

		variant v = get_class_node(derived);
		if(v.is_map()) {
			variant bases = v["bases"];
			if(bases.is_list()) {
				for(const variant& b : bases.as_list()) {
					if(is_class_derived_from(b.as_string(), base)) {
						return true;
					}
				}
			}
		}

		return false;
	}

	struct DefinitionConstantFunctionResetter {
		FormulaCallableDefinitionPtr def_;
		DefinitionConstantFunctionResetter(FormulaCallableDefinitionPtr def) : def_(def)
		{}

		~DefinitionConstantFunctionResetter()
		{
			reset();
		}

		void reset() {
			if(def_) {

				for(int n = 0; n != def_->getNumSlots(); ++n) {
					auto entry = def_->getEntry(n);
					if(entry == nullptr) {
						continue;
					}
	
					entry->constant_fn = std::function<bool(variant*)>();
				}
				
				def_.reset();
			}
		}
	};

	FormulaClass::FormulaClass(const std::string& class_name, const variant& node)
	  : builtin_slots_(0), name_(class_name), name_variant_(class_name), nstate_slots_(0), is_library_only_(false)
	{
		if(node["base_type"].is_string()) {
			std::string builtin = node["base_type"].as_string();
			builtin_ctor_ = game_logic::get_callable_constructor(builtin);
			builtin_def_ = game_logic::get_formula_callable_definition(builtin);
			builtin_slots_ = builtin_def_->getNumSlots();
		}

		variant bases_v = node["bases"];
		if(bases_v.is_null() == false) {
			for(int n = 0; n != bases_v.num_elements(); ++n) {
				bases_.push_back(get_class(bases_v[n].as_string()));
			}
		}

		std::map<variant, variant> m;
		private_data_ = variant(&m);

		for(ffl::IntrusivePtr<const FormulaClass> base : bases_) {
			merge_variant_over(&private_data_, base->private_data_);
		}

		ASSERT_LOG(bases_.size() <= 1, "Multiple inheritance of classes not currently supported");

		for(ffl::IntrusivePtr<const FormulaClass> base : bases_) {
			slots_ = base->slots();
			properties_ = base->properties();
			nstate_slots_ = base->nstate_slots_;
			builtin_ctor_ = base->builtin_ctor_;
			builtin_def_ = base->builtin_def_;
			builtin_slots_ = base->builtin_slots_;
		}

		variant properties = node["properties"];
		if(!properties.is_map()) {
			properties = node;
		}

		is_library_only_ = node["is_library"].as_bool(false);

		FormulaCallableDefinitionPtr class_def = get_class_definition(class_name);
		assert(class_def);

		FormulaClassDefinition* class_definition = dynamic_cast<FormulaClassDefinition*>(class_def.get());
		assert(class_definition);

		std::vector<std::string> entries_loading;

		std::map<std::string, PropertyEntry> preloaded_entries;

		DefinitionConstantFunctionResetter resetter(class_def);

		if(g_ffl_vm_opt_const_library_calls && is_library_only_) {
			for(int n = 0; n != class_def->getNumSlots(); ++n) {
				auto entry = class_def->getEntry(n);
				if(entry == nullptr || entry->id.empty()) {
					continue;
				}

				entry->constant_fn = [n,&class_def,&preloaded_entries,&entries_loading,&class_name,&properties](variant* value) {
					auto entry = class_def->getEntry(n);

					const std::string& id = entry->id;
					if(std::count(entries_loading.begin(), entries_loading.end(), id)) {
						return false;
					}

					int dummy_slot = 0;
					const variant prop_node = properties[variant(id)];

					if(prop_node.is_string() == false) {
						return false;
					}

					PropertyEntry* e = nullptr;

					auto itor = preloaded_entries.find(id);
					if(itor != preloaded_entries.end()) {
						e = &itor->second;
					} else {

						entries_loading.push_back(id);
						PropertyEntry entry(class_name, id, prop_node, dummy_slot);
						e = &preloaded_entries[id];
						*e = entry;
						entries_loading.pop_back();
					}

					if(e->getter && e->getter->evaluatesToConstant(*value)) {
						return true;
					}


					return false;
				};
			}
		}

		const definition_access_private_in_scope expose_scope(*class_definition);

		for(variant key : properties.getKeys().as_list()) {
			entries_loading.push_back(key.as_string());

			const variant prop_node = properties[key];
			PropertyEntry entry;
			
			auto itor = preloaded_entries.find(key.as_string());
			if(itor != preloaded_entries.end()) {
				entry = itor->second;
			} else {
				entry = PropertyEntry(class_name, key.as_string(), prop_node, nstate_slots_);
				if(is_library_only_) {
					preloaded_entries[key.as_string()] = entry;
				}
			}

			if(properties_.count(key.as_string()) == 0) {
				properties_[key.as_string()] = static_cast<int>(slots_.size());
				slots_.push_back(PropertyEntry());
			}

			slots_[properties_[key.as_string()]] = entry;

			entries_loading.pop_back();
		}

		resetter.reset();

		for(const PropertyEntry& entry : slots_) {
			if(entry.variable_slot >= 0) {
				if(variable_slots_.size() < entry.variable_slot+1) {
					variable_slots_.resize(entry.variable_slot+1);
				}

				variable_slots_[entry.variable_slot] = &entry;
			}
		}

		ASSERT_LOG(variable_slots_.size() == nstate_slots_, "MISMATCH: " << variable_slots_.size() << " VS " << nstate_slots_);

		nested_classes_ = node["classes"];

		if(node["constructor"].is_string()) {
			const Formula::StrictCheckScope strict_checking;

			constructor_.push_back(game_logic::Formula::createOptionalFormula(node["constructor"], getClassFunctionSymbolTable(), class_def));
		}

#if defined(USE_LUA)
		if(node.has_key("lua")) {
			lua_node_ = node["lua"];
		}
#endif

		unit_test_ = node["test"];
	}

	void FormulaClass::build_nested_classes()
	{
		build_nested_classes(nested_classes_);
		nested_classes_ = variant();
	}

	void FormulaClass::build_nested_classes(variant classes)
	{
		if(classes.is_list()) {
			for(const variant& v : classes.as_list()) {
				build_nested_classes(v);
			}
		} else if(classes.is_map()) {
			for(variant key : classes.getKeys().as_list()) {
				const variant class_node = classes[key];
				sub_classes_[key.as_string()].reset(new FormulaClass(name_ + "." + key.as_string(), class_node));
			}
		}
	}

	void FormulaClass::setName(const std::string& name)
	{
		name_ = name;
		name_variant_ = variant(name);
		for(classes_map::iterator i = sub_classes_.begin(); i != sub_classes_.end(); ++i) {
			i->second->setName(name + "." + i->first);
		}
	}
	bool FormulaClass::isA(const std::string& name) const
	{
		if(name == name_) {
			return true;
		}

		typedef ffl::IntrusivePtr<const FormulaClass> Ptr;
		for(const Ptr& base : bases_) {
			if(base->isA(name)) {
				return true;
			}
		}

		return false;
	}

	void FormulaClass::run_unit_tests()
	{
		const Formula::StrictCheckScope strict_checking(false);
		const Formula::NonStaticContext NonStaticContext;

		if(unit_test_.is_null()) {
			return;
		}

		if(in_unit_test) {
			unit_test_queue.push_back(this);
			return;
		}

		variant unit_test = unit_test_;
		unit_test_ = variant();

		in_unit_test = true;

		ffl::IntrusivePtr<game_logic::MapFormulaCallable> callable(new game_logic::MapFormulaCallable);
		std::map<variant,variant> attr;
		callable->add("vars", variant(&attr));
		callable->add("lib", variant(game_logic::get_library_object().get()));

		for(int n = 0; n != unit_test.num_elements(); ++n) {
			variant test = unit_test[n];
			game_logic::FormulaPtr cmd = game_logic::Formula::createOptionalFormula(test["command"], getClassFunctionSymbolTable());
			if(cmd) {
				variant v = cmd->execute(*callable);
				callable->executeCommand(v);
			}

			game_logic::FormulaPtr predicate = game_logic::Formula::createOptionalFormula(test["assert"]);
			if(predicate) {
				game_logic::FormulaPtr message = game_logic::Formula::createOptionalFormula(test["message"]);

				std::string msg;
				if(message) {
					msg += ": " + message->execute(*callable).write_json();
				}

				ASSERT_LOG(predicate->execute(*callable).as_bool(), "UNIT TEST FAILURE FOR CLASS " << name_ << " TEST " << n << " FAILED: " << test["assert"].write_json() << msg << "\n");
			}

		}

		in_unit_test = false;

		for(classes_map::iterator i = sub_classes_.begin(); i != sub_classes_.end(); ++i) {
			i->second->run_unit_tests();
		}

		if(unit_test_queue.empty() == false) {
			FormulaClass* c = unit_test_queue.back();
			unit_test_queue.pop_back();
			c->run_unit_tests();
		}
	}

#ifdef USE_LUA
	const std::shared_ptr<lua::CompiledChunk> & FormulaClass::getLuaInit(lua::LuaContext & ctx) const {
		if (lua_compiled_) {
			return lua_compiled_;
		}
		if (lua_node_.has_key("init")) {
			lua_compiled_.reset(ctx.compileChunk(lua_node_.has_key("debug_name") ? lua_node_["debug_name"].as_string() : ("class " + name() + " lua"), lua_node_["init"].as_string()));
		}
		return lua_compiled_;
	}
#endif

	namespace
	{
		struct private_data_scope 
		{
			explicit private_data_scope(int* r, int new_value) : r_(*r), old_value_(*r) {
				r_ = new_value;
			}

			~private_data_scope() {
				r_ = old_value_;
			}
		private:
			int& r_;
			int old_value_;
		};

		classes_map classes_, backup_classes_;
		std::set<std::string> known_classes;

		void record_classes(const std::string& name, const variant& node)
		{
			known_classes.insert(name);

			const variant classes = flatten_list_of_maps(node["classes"]);
			if(classes.is_map()) {
				for(variant key : classes.getKeys().as_list()) {
					const variant class_node = classes[key];
					record_classes(name + "." + key.as_string(), class_node);
				}
			}
		}

		ffl::IntrusivePtr<FormulaClass> build_class(const std::string& type)
		{
			const variant v = get_class_node(type);

			record_classes(type, v);

			ffl::IntrusivePtr<FormulaClass> result(new FormulaClass(type, v));
			result->setName(type);
			return result;
		}

		ffl::IntrusivePtr<const FormulaClass> get_class(const std::string& type)
		{
			if(std::find(type.begin(), type.end(), '.') != type.end()) {
				std::vector<std::string> v = util::split(type, '.');
				ffl::IntrusivePtr<const FormulaClass> c = get_class(v.front());
				for(unsigned n = 1; n < v.size(); ++n) {
					classes_map::const_iterator itor = c->subClasses().find(v[n]);
					ASSERT_LOG(itor != c->subClasses().end(), "COULD NOT FIND FFL CLASS: " << type);
					c = itor->second.get();
				}

				return c;
			}

			classes_map::const_iterator itor = classes_.find(type);
			if(itor != classes_.end()) {
				return itor->second;
			}

			ffl::IntrusivePtr<FormulaClass> result;
	
			if(!backup_classes_.empty() && backup_classes_.count(type)) {
				try {
					result = build_class(type);
				} catch(...) {
					result = backup_classes_[type];
					LOG_ERROR("ERROR LOADING NEW CLASS");
				}
			} else {
				if(preferences::edit_and_continue()) {
					assert_recover_scope recover_scope;
					try {
						result = build_class(type);
					} catch(validation_failure_exception& e) {
						edit_and_continue_class(type, e.msg);
					}

					if(!result) {
						return get_class(type);
					}
				} else {
					result = build_class(type);
				}
			}

			classes_[type] = result;
			result->build_nested_classes();
			result->run_unit_tests();
			return ffl::IntrusivePtr<const FormulaClass>(result.get());
		}
	}

	void FormulaObject::visitVariantObjects(const variant& node, const std::function<void (FormulaObject*)>& fn)
	{
		std::vector<FormulaObject*> seen;
		visitVariantsInternal(node, fn, &seen);
	}

	void FormulaObject::visitVariantsInternal(const variant& node, const std::function<void (FormulaObject*)>& fn, std::vector<FormulaObject*>* seen)
	{
		std::vector<FormulaObject*> seen_buf;
		if(!seen) {
			seen = &seen_buf;
		}

		if(node.try_convert<FormulaObject>()) {
			FormulaObject* obj = node.try_convert<FormulaObject>();
			if(std::count(seen->begin(), seen->end(), obj)) {
				return;
			}

			ConstWmlSerializableFormulaCallablePtr ptr(obj);
			fn(obj);

			seen->push_back(obj);

			for(const variant& v : obj->variables_) {
				visitVariantsInternal(v, fn, seen);
			}

			seen->pop_back();
			return;
		}
	
		if(node.is_list()) {
			for(const variant& item : node.as_list()) {
				FormulaObject::visitVariantsInternal(item, fn, seen);
			}
		} else if(node.is_map()) {
			for(const variant_pair& item : node.as_map()) {
				FormulaObject::visitVariantsInternal(item.second, fn, seen);
			}
		}
	}

	void FormulaObject::visitVariants(const variant& node, const std::function<void (variant)>& fn)
	{
		std::vector<FormulaObject*> seen;
		visitVariantsInternal(node, fn, &seen);
	}

	void FormulaObject::visitVariantsInternal(const variant& node, const std::function<void (variant)>& fn, std::vector<FormulaObject*>* seen)
	{
		std::vector<FormulaObject*> seen_buf;
		if(!seen) {
			seen = &seen_buf;
		}

		if(node.try_convert<FormulaObject>()) {
			FormulaObject* obj = node.try_convert<FormulaObject>();
			if(std::count(seen->begin(), seen->end(), obj)) {
				return;
			}

			ConstWmlSerializableFormulaCallablePtr ptr(obj);
			fn(node);

			seen->push_back(obj);

			for(const variant& v : obj->variables_) {
				visitVariantsInternal(v, fn, seen);
			}

			seen->pop_back();
			return;
		}
	
		fn(node);

		if(node.is_list()) {
			for(const variant& item : node.as_list()) {
				FormulaObject::visitVariantsInternal(item, fn, seen);
			}
		} else if(node.is_map()) {
			for(const variant_pair& item : node.as_map()) {
				FormulaObject::visitVariantsInternal(item.second, fn, seen);
			}
		}
	}

	void FormulaObject::update(FormulaObject& updated)
	{
		std::vector<ffl::IntrusivePtr<FormulaObject> > objects;
		std::map<boost::uuids::uuid, FormulaObject*> src, dst;
		{
		formula_profiler::Instrument instrument("UPDATE_A");
		visitVariantObjects(variant(this), [&dst,&objects](FormulaObject* obj) {
			dst[obj->uuid()] = obj;
			objects.push_back(obj);
		});
		visitVariantObjects(variant(&updated), [&src,&objects](FormulaObject* obj) {
			src[obj->uuid()] = obj;
			objects.push_back(obj);
		});
		}

		std::map<FormulaObject*, FormulaObject*> mapping;

		{
		formula_profiler::Instrument instrument("UPDATE_B");
		for(auto& i : src) {
			auto j = dst.find(i.first);
			if(j != dst.end()) {
				mapping[i.second] = j->second;
			}
		}
		}

		{
		formula_profiler::Instrument instrument("UPDATE_C");
		std::set<FormulaObject*> seen;
		for(auto& i : src) {
			variant v(i.second);
			mapObjectIntoDifferentTree(v, mapping, seen);
		}
		}

		{
		formula_profiler::Instrument instrument("UPDATE_D");
		for(auto& i : mapping) {
			*i.second = *i.first;
		}

		for(auto& i : dst) {
			if(src.count(i.first) == 0) {
				i.second->orphaned_ = true;
				i.second->new_in_update_ = false;
			}
		}

		for(auto i = src.begin(); i != src.end(); ++i) {
			i->second->new_in_update_ = dst.count(i->first) == 0;
		}
		}
	}

variant FormulaObject::generateDiff(variant before, variant b)
{
	variant a = deepClone(before);

	std::vector<ffl::IntrusivePtr<FormulaObject> > objects;

	std::map<boost::uuids::uuid, FormulaObject*> src, dst;
	visitVariants(b, [&dst,&objects](variant v) {
		FormulaObject* obj = v.try_convert<FormulaObject>();
		if(obj) {
			dst[obj->uuid()] = obj;
			objects.push_back(obj);
		}});

	visitVariants(a, [&src,&objects](variant v) {
		FormulaObject* obj = v.try_convert<FormulaObject>();
		if(obj) {
			src[obj->uuid()] = obj;
			objects.push_back(obj);
		}});

	std::map<FormulaObject*, FormulaObject*> mapping;

	for(auto i = src.begin(); i != src.end(); ++i) {
		auto j = dst.find(i->first);
		if(j != dst.end()) {
			mapping[i->second] = j->second;
		}
	}

	std::vector<variant> deltas;

	std::set<FormulaObject*> seen;
	for(auto i = src.begin(); i != src.end(); ++i) {
		variant v(i->second);
		mapObjectIntoDifferentTree(v, mapping, seen);
		auto j = dst.find(i->first);
		if(j != dst.end() && i->second->variables_ != j->second->variables_) {
			std::map<variant, variant> node_delta;
			node_delta[variant("_uuid")] = variant(write_uuid(i->second->uuid()));
			if(i->second->variables_.size() < j->second->variables_.size()) {
				i->second->variables_.resize(j->second->variables_.size());
			}

			if(j->second->variables_.size() < i->second->variables_.size()) {
				j->second->variables_.resize(i->second->variables_.size());
			}

			for(int n = 0; n != j->second->variables_.size(); ++n) {
				if(i->second->variables_[n] != j->second->variables_[n]) {
					for(const PropertyEntry& e : i->second->class_->slots()) {
						if(e.variable_slot == n) {
							node_delta[e.name_variant] = j->second->variables_[n];
							break;
						}
					}
				}
			}

			deltas.push_back(variant(&node_delta));
		}
	}

	std::vector<variant> new_objects;
	for(auto i = dst.begin(); i != dst.end(); ++i) {
		if(src.find(i->first) == src.end()) {
			new_objects.push_back(i->second->serializeToWml());
		}
	}


	variant_builder builder;
	builder.add("deltas", variant(&deltas));
	builder.add("objects", variant(&new_objects));

	std::string res_doc = builder.build().write_json();
	std::vector<char> data(res_doc.begin(), res_doc.end());
	std::vector<char> compressed = base64::b64encode(zip::compress(data));

	variant_builder result;
	result.add("delta", std::string(compressed.begin(), compressed.end()));
	result.add("size", static_cast<int>(res_doc.size()));
	return result.build();
}

void FormulaObject::applyDiff(variant delta)
{
	std::map<boost::uuids::uuid, FormulaObject*> objects;
	visitVariants(variant(this), [&objects](variant v) {
		FormulaObject* obj = v.try_convert<FormulaObject>();
		if(obj) {
			objects[obj->uuid()] = obj;
		}});

	const std::string& data_str = delta["delta"].as_string();
	std::vector<char> data_buf(data_str.begin(), data_str.end());
	const int data_size = delta["size"].as_int();

	std::vector<char> data = zip::decompress_known_size(base64::b64decode(data_buf), data_size);

	const game_logic::wmlFormulaCallableReadScope read_scope;

	for(auto p : objects) {
		game_logic::wmlFormulaCallableReadScope::registerSerializedObject(p.second->uuid(), p.second);
	}

	variant v = json::parse(std::string(data.begin(), data.end()));
	for(variant obj_node : v["objects"].as_list()) {
		game_logic::WmlSerializableFormulaCallablePtr obj = obj_node.try_convert<game_logic::WmlSerializableFormulaCallable>();
		ASSERT_LOG(obj.get() != NULL, "ILLEGAL OBJECT FOUND IN SERIALIZATION");

		game_logic::wmlFormulaCallableReadScope::registerSerializedObject(obj->uuid(), obj);
	}

	for(variant d : v["deltas"].as_list()) {
		boost::uuids::uuid id = read_uuid(d["_uuid"].as_string());
		auto obj_itor = objects.find(id);
		ASSERT_LOG(obj_itor != objects.end(), "Could not find expected object id when applying delta: " << d.write_json());
		for(auto p : d.as_map()) {
			const std::string& attr = p.first.as_string();
			if(attr == "_uuid") {
				continue;
			}

			auto prop_itor = obj_itor->second->class_->properties().find(attr);
			ASSERT_LOG(prop_itor != obj_itor->second->class_->properties().end(), "Unknown property '" << attr << "' in delta: " << d.write_json());

			obj_itor->second->variables_[obj_itor->second->class_->slots()[prop_itor->second].variable_slot] = p.second;
		}
	}
}

void FormulaObject::mapObjectIntoDifferentTree(variant& v, const std::map<FormulaObject*, FormulaObject*>& mapping, std::set<FormulaObject*>& seen)
	{
		if(v.try_convert<FormulaObject>()) {
			FormulaObject* obj = v.try_convert<FormulaObject>();
			auto itor = mapping.find(obj);
			if(itor != mapping.end()) {
				v = variant(itor->second);
			}

			if(seen.count(obj)) {
				return;
			}

			seen.insert(obj);

			for(variant& v : obj->variables_) {
				mapObjectIntoDifferentTree(v, mapping, seen);
			}

			return;
		}

		if(v.is_list()) {
			std::vector<variant> result;
			for(const variant& item : v.as_list()) {
				result.push_back(item);
				FormulaObject::mapObjectIntoDifferentTree(result.back(), mapping, seen);
			}

			v = variant(&result);
		} else if(v.is_map()) {
			std::map<variant, variant> result;
			for(const variant_pair& item : v.as_map()) {
				variant key = item.first;
				variant value = item.second;
				FormulaObject::mapObjectIntoDifferentTree(key, mapping, seen);
				FormulaObject::mapObjectIntoDifferentTree(value, mapping, seen);
				result[key] = value;
			}

			v = variant(&result);
		}
	}

	variant FormulaObject::deepClone(variant v)
	{
		std::map<FormulaObject*,FormulaObject*> mapping;
		return deepClone(v, mapping);
	}

	variant FormulaObject::deepClone(variant v, std::map<FormulaObject*,FormulaObject*>& mapping)
	{
		if(v.is_callable()) {
			FormulaObject* obj = v.try_convert<FormulaObject>();
			if(obj) {
				std::map<FormulaObject*,FormulaObject*>::iterator itor = mapping.find(obj);
				if(itor != mapping.end()) {
					return variant(itor->second);
				}

				ffl::IntrusivePtr<FormulaObject> duplicate = obj->clone();
				mapping[obj] = duplicate.get();

				for(int n = 0; n != duplicate->variables_.size(); ++n) {
					duplicate->variables_[n] = deepClone(duplicate->variables_[n], mapping);
				}

				return variant(duplicate.get());
			} else {
				return v;
			}
		} else if(v.is_list()) {
			std::vector<variant> items;
			for(int n = 0; n != v.num_elements(); ++n) {
				items.push_back(deepClone(v[n], mapping));
			}

			return variant(&items);
		} else if(v.is_map()) {
			std::map<variant, variant> m;
			for(const variant::map_pair& p : v.as_map()) {
				m[deepClone(p.first, mapping)] = deepClone(p.second, mapping);
			}

			return variant(&m);
		} else {
			return v;
		}
	}

	void FormulaObject::deepDestroy(variant v)
	{
		std::set<FormulaObject*> seen;
		deepDestroy(v, seen);
	}

	void FormulaObject::deepDestroy(variant v, std::set<FormulaObject*>& seen)
	{
		if(v.is_callable()) {
			FormulaObject* obj = v.try_convert<FormulaObject>();
			if(obj) {
				if(seen.insert(obj).second == false) {
					return;
				}

				for(variant& var : obj->variables_) {
					deepDestroy(var, seen);
					var = variant();
				}
			}

		} else if(v.is_list()) {
			for(int n = 0; n != v.num_elements(); ++n) {
				deepDestroy(v[n], seen);
			}
		} else if(v.is_map()) {
			for(auto p : v.as_map()) {
				deepDestroy(p.second, seen);
			}
		}
	}

	void FormulaObject::reloadClasses()
	{
		classes_.clear();
	}

	void FormulaObject::loadAllClasses()
	{
		for(auto p : class_path_map()) {
			variant node = json::parse_from_file(p.second);
			if(node["server_only"].as_bool(false) == false) {
				get_class(p.first);
			}
		}
	}

	void FormulaObject::tryLoadClass(const std::string& name)
	{
		build_class(name);
	}

	ffl::IntrusivePtr<FormulaObject> FormulaObject::create(const std::string& type, variant args)
	{
		const Formula::StrictCheckScope strict_checking;
		ffl::IntrusivePtr<FormulaObject> res(new FormulaObject(type, args));
		res->callConstructors(args);
		res->validate();
		return res;
	}

	FormulaObject::FormulaObject(const std::string& type, variant args)
	  : new_in_update_(true), orphaned_(false),
		class_(get_class(type)), private_data_(-1)
	{
		ASSERT_LOG(class_->is_library_only() == false || args.is_null(), "Creating instance of library class is illegal: " << type);

	}

	void FormulaObject::surrenderReferences(GarbageCollector* collector)
	{
		collector->surrenderVariant(&tmp_value_, "TMP");

		const std::vector<const PropertyEntry*>& entries = class_->variableSlots();

		int index = 0;
		for(variant& v : variables_) {
			collector->surrenderVariant(&v, entries[index] ? entries[index]->name.c_str() : nullptr);
			++index;
		}
	}

	std::string FormulaObject::debugObjectName() const
	{
		return "class " + class_->name();
	}

	std::string FormulaObject::write_id() const
	{
		std::string result = write_uuid(uuid());
		result.resize(15);
		return result;
	}

	bool FormulaObject::isA(const std::string& class_name) const
	{
		return class_->isA(class_name);
	}

	const std::string& FormulaObject::getClassName() const
	{
		return class_->name();
	}

	void FormulaObject::callConstructors(variant args)
	{
		if(class_->getBuiltinCtor()) {
			builtin_base_ = class_->getBuiltinCtor()(args);
		}

		variables_.resize(class_->getNstateSlots());
		for(const PropertyEntry& slot : class_->slots()) {
			if(slot.variable_slot != -1) {
				if(slot.initializer) {
					variables_[slot.variable_slot] = slot.initializer->execute(*this);
				} else {
					variables_[slot.variable_slot] = deep_copy_variant(slot.default_value);
				}
			}
		}

#if defined(USE_LUA)
		init_lua();
#endif

		if(args.is_map()) {
			ConstFormulaCallableDefinitionPtr def = get_class_definition(class_->name());
			for(const variant& key : args.getKeys().as_list()) {
				std::map<std::string, int>::const_iterator itor = class_->properties().find(key.as_string());
				if(itor != class_->properties().end() && class_->slots()[itor->second].setter.get() == nullptr && class_->slots()[itor->second].variable_slot == -1) {
					if(property_overrides_.size() <= static_cast<unsigned>(itor->second)) {
						property_overrides_.resize(itor->second+1);
					}

					//A read-only property. Set the formula to what is passed in.
					FormulaPtr f(new Formula(args[key], getClassFunctionSymbolTable(), def));
					const FormulaCallableDefinition::Entry* entry = def->getEntryById(key.as_string());
					ASSERT_LOG(entry, "COULD NOT FIND ENTRY IN CLASS DEFINITION: " << key.as_string());
					if(entry->variant_type) {
						ASSERT_LOG(variant_types_compatible(entry->variant_type, f->queryVariantType()), "ERROR: property override in instance of class " << class_->name() << " has mis-matched type for property " << key.as_string() << ": " << entry->variant_type->to_string() << " doesn't match " << f->queryVariantType()->to_string() << " at " << args[key].debug_location());
					}
					property_overrides_[itor->second] = f;
				} else {
					setValue(key.as_string(), args[key]);
				}
			}
		}

		for(const game_logic::ConstFormulaPtr f : class_->constructor()) {
			executeCommand(f->execute(*this));
		}
	}

	FormulaObject::FormulaObject(variant data)
	  : WmlSerializableFormulaCallable(data["_uuid"].is_string() ? read_uuid(data["_uuid"].as_string()) : generate_uuid()), new_in_update_(true), orphaned_(false),
		class_(get_class(data["@class"].as_string())), private_data_(-1)
	{
		if(class_->getBuiltinCtor()) {
			builtin_base_ = class_->getBuiltinCtor()(data);
		}

		variables_.resize(class_->getNstateSlots());

		if(data.is_map() && data["state"].is_map()) {
		const std::map<variant,variant>& state_map = data["state"].as_map();

		for(const PropertyEntry& entry : class_->slots()) {
			if(entry.variable_slot == -1) {
				continue;
			}

			auto itor = state_map.find(entry.name_variant);
			if(itor != state_map.end()) {
				variables_[entry.variable_slot] = itor->second;
			} else {
				variables_[entry.variable_slot] = entry.default_value;
			}
		}
/*
			for(auto& p : state.as_map()) {
				std::map<std::string, int>::const_iterator itor = class_->properties().find(p.first.as_string());
				ASSERT_LOG(itor != class_->properties().end(), "No property " << p.first.as_string() << " in class " << class_->name());

				const PropertyEntry& entry = class_->slots()[itor->second];
				ASSERT_NE(entry.variable_slot, -1);

				variables_[entry.variable_slot] = p.second;
			}
		*/
		}

		if(data.is_map() && data["property_overrides"].is_map()) {
			for(const std::pair<const variant,variant>& p : data["property_overrides"].as_map()) {
				const std::string& key = p.first.as_string();
				std::map<std::string, int>::const_iterator itor = class_->properties().find(key);
				ASSERT_LOG(itor != class_->properties().end(), "UNKNOWN PROPERTY ACCESS " << key << " IN CLASS " << class_->name() << "\nFORMULA LOCATION: " << get_call_stack());

				if(property_overrides_.size() <= itor->second) {
					property_overrides_.resize(itor->second+1);
				}

				property_overrides_[itor->second] = FormulaPtr(new Formula(p.second, getClassFunctionSymbolTable()));
			}
		}

#if defined(USE_LUA)
		init_lua();
#endif
	}

	FormulaObject::~FormulaObject()
	{}

	ffl::IntrusivePtr<FormulaObject> FormulaObject::clone() const
	{
		ffl::IntrusivePtr<FormulaObject> result(new FormulaObject(*this));

		return result;
	}

	variant FormulaObject::serializeToWml() const
	{
		std::map<variant, variant> result;
		result[variant("@class")] = variant(class_->name());
		result[variant("_uuid")] = variant(write_uuid(uuid()));

		std::map<variant,variant> state;
		for(const PropertyEntry& slot : class_->slots()) {
			const int nstate_slot = slot.variable_slot;
			if(nstate_slot != -1 && static_cast<unsigned>(nstate_slot) < variables_.size() && variables_[nstate_slot] != slot.default_value) {
				state[variant(slot.name)] = variables_[nstate_slot];
			}
		}

		result[variant("state")] = variant(&state);

		if(property_overrides_.empty() == false) {
			std::map<variant,variant> properties;
			for(int n = 0; n < property_overrides_.size(); ++n) {
				if(!property_overrides_[n]) {
					continue;
				}

				const PropertyEntry& entry = class_->slots()[n];

				auto debug_info = property_overrides_[n]->strVal().get_debug_info();

				if(debug_info && debug_info->filename) {
					std::ostringstream s;
					s << "@str_with_debug " << *debug_info->filename << ":" << debug_info->line << "|" << property_overrides_[n]->str();
					properties[entry.name_variant] = variant(s.str());
				} else {
					properties[entry.name_variant] = variant(property_overrides_[n]->str());
				}
			}

			result[variant("property_overrides")] = variant(&properties);
		}

		return variant(&result);
	}

	REGISTER_SERIALIZABLE_CALLABLE(FormulaObject, "@class");

	bool FormulaObject::getConstantValue(const std::string& id, variant* result) const
	{
		std::map<std::string, int>::const_iterator itor = class_->properties().find(id);
		if(itor == class_->properties().end()) {
			return false;
		}

		ConstFormulaPtr getter;

		if(static_cast<unsigned>(itor->second) < property_overrides_.size()) {
			getter = property_overrides_[itor->second];
		}

		if(!getter) {
			const PropertyEntry& entry = class_->slots()[itor->second];
			getter = entry.getter;
		}

		if(getter && getter->evaluatesToConstant(*result)) {
			return true;
		}

		return false;
	}

	variant FormulaObject::getValue(const std::string& key) const
	{
		{
			if(key == "_data") {
				ASSERT_NE(private_data_, -1);
				return variables_[private_data_];
			} else if(key == "value") {
				return tmp_value_;
			}
		}

		if(key == "self" || key == "me") {
			return variant(this);
		}

		if(key == "_class") {
			return class_->nameVariant();
		}

		if(key == "lib") {
			return variant(get_library_object().get());
		}

		if(key == "_uuid") {
			return variant(write_uuid(uuid()));
		}

		auto def = class_->getBuiltinDef();
		if(def) {
			const int slot = def->getSlot(key);
			if(slot >= 0) {
				return builtin_base_->queryValueBySlot(slot);
			}
		}

		std::map<std::string, int>::const_iterator itor = class_->properties().find(key);
		ASSERT_LOG(itor != class_->properties().end(), "UNKNOWN PROPERTY ACCESS " << key << " IN CLASS " << class_->name() << "\nFORMULA LOCATION: " << get_call_stack());

		if(static_cast<unsigned>(itor->second) < property_overrides_.size() && property_overrides_[itor->second]) {
			return property_overrides_[itor->second]->execute(*this);
		}

		const PropertyEntry& entry = class_->slots()[itor->second];

		if(entry.getter) {
			private_data_scope scope(&private_data_, entry.variable_slot);
			return entry.getter->execute(*this);
		} else if(entry.variable_slot != -1) {
			return variables_[entry.variable_slot];
		} else {
			ASSERT_LOG(false, "ILLEGAL READ PROPERTY ACCESS OF NON-READABLE VARIABLE " << key << " IN CLASS " << class_->name());
		}
	}

	variant FormulaObject::getValueBySlot(int slot) const
	{
		switch(slot) {
			case FIELD_PRIVATE: {
				ASSERT_NE(private_data_, -1);
				return variables_[private_data_];
			}
			case FIELD_VALUE: return tmp_value_;
			case FIELD_SELF:
			case FIELD_ME: return variant(this);
			case FIELD_NEW_IN_UPDATE: return variant::from_bool(new_in_update_);
			case FIELD_ORPHANED: return variant::from_bool(orphaned_);
			case FIELD_CLASS: return class_->nameVariant();
			case FIELD_LIB: return variant(get_library_object().get());
			case FIELD_UUID: return variant(write_uuid(uuid()));
			default: break;
		}

		slot -= NUM_BASE_FIELDS;

		if(slot < class_->getBuiltinSlots()) {
			return builtin_base_->queryValueBySlot(slot);
		}

		slot -= class_->getBuiltinSlots();

		ASSERT_LOG(slot >= 0 && static_cast<unsigned>(slot) < class_->slots().size(), "ILLEGAL VALUE QUERY TO FORMULA OBJECT: " << slot << " IN " << class_->name());


		if(static_cast<unsigned>(slot) < property_overrides_.size() && property_overrides_[slot]) {
			return property_overrides_[slot]->execute(*this);
		}
	
		const PropertyEntry& entry = class_->slots()[slot];

		if(entry.getter) {
			private_data_scope scope(&private_data_, entry.variable_slot);
			return entry.getter->execute(*this);
		} else if(entry.variable_slot != -1) {
			return variables_[entry.variable_slot];
		} else {
			ASSERT_LOG(false, "ILLEGAL READ PROPERTY ACCESS OF NON-READABLE VARIABLE IN CLASS " << class_->name());
		}
	}

	void FormulaObject::setValue(const std::string& key, const variant& value)
	{
		if(private_data_ != -1 && key == "_data") {
			variables_[private_data_] = value;
			return;
		}

		auto def = class_->getBuiltinDef();
		if(def) {
			const int slot = def->getSlot(key);
			if(slot >= 0) {
				builtin_base_->mutateValueBySlot(slot, value);
				return;
			}
		}

		std::map<std::string, int>::const_iterator itor = class_->properties().find(key);
		ASSERT_LOG(itor != class_->properties().end(), "UNKNOWN PROPERTY ACCESS " << key << " IN CLASS " << class_->name());

		setValueBySlot(itor->second+class_->getBuiltinSlots()+NUM_BASE_FIELDS, value);
		return;
	}

	void FormulaObject::setValueBySlot(int slot, const variant& value)
	{
		if(slot < NUM_BASE_FIELDS) {
			switch(slot) {
			case FIELD_PRIVATE:
				ASSERT_NE(private_data_, -1);
				variables_[private_data_] = value;
				return;
			default:
				ASSERT_LOG(false, "TRIED TO SET ILLEGAL KEY IN CLASS: " << BaseFields[slot]);
			}
		}

		slot -= NUM_BASE_FIELDS;

		if(slot < class_->getBuiltinSlots()) {
			builtin_base_->mutateValueBySlot(slot, value);
			return;
		}

		slot -= class_->getBuiltinSlots();

		ASSERT_LOG(slot >= 0 && static_cast<unsigned>(slot) < class_->slots().size(), "ILLEGAL VALUE SET TO FORMULA OBJECT: " << slot << " IN " << class_->name());

		const PropertyEntry& entry = class_->slots()[slot];

		if(entry.set_type) {
			if(!entry.set_type->match(value)) {
				ASSERT_LOG(false, "ILLEGAL WRITE PROPERTY ACCESS: SETTING VARIABLE " << entry.name << " OF TYPE " << entry.set_type->to_string() << " IN CLASS " << class_->name() << " TO INVALID TYPE " << variant::variant_type_to_string(value.type()) << ": " << value.write_json());
			}
		}

		if(entry.setter) {
			tmp_value_ = value;
			private_data_scope scope(&private_data_, entry.variable_slot);
			executeCommand(entry.setter->execute(*this));
		} else if(entry.variable_slot != -1) {
			variables_[entry.variable_slot] = value;
		} else {
			ASSERT_LOG(false, "ILLEGAL WRITE PROPERTY ACCESS OF NON-WRITABLE VARIABLE " << entry.name << " IN CLASS " << class_->name());
		}

		if(entry.get_type && (entry.getter || entry.setter)) {
			//now that we've set the value, retrieve it and ensure it matches
			//the type we expect.
			variant var;

			FormulaPtr override;
			if(static_cast<unsigned>(slot) < property_overrides_.size()) {
				override = property_overrides_[slot];
			}
			if(override) {
				private_data_scope scope(&private_data_, entry.variable_slot);
				var = override->execute(*this);
			} else if(entry.getter) {
				private_data_scope scope(&private_data_, entry.variable_slot);
				var = entry.getter->execute(*this);
			} else {
				ASSERT_NE(entry.variable_slot, -1);
				var = variables_[entry.variable_slot];
			}

			ASSERT_LOG(entry.get_type->match(var), "AFTER WRITE TO " << entry.name << " IN CLASS " << class_->name() << " TYPE IS INVALID. EXPECTED " << entry.get_type->str() << " BUT FOUND " << var.write_json());
		}
	}

	variant_type_ptr FormulaObject::getPropertySetType(const std::string & key) const
	{
		std::map<std::string, int>::const_iterator itor = class_->properties().find(key);
		ASSERT_LOG(itor != class_->properties().end(), "UNKNOWN PROPERTY ACCESS " << key << " IN CLASS " << class_->name());

		return class_->slots()[itor->second].set_type;
	}

	void FormulaObject::validate() const
	{
	#ifndef NO_FFL_TYPE_SAFETY_CHECKS
		if(preferences::type_safety_checks() == false) {
			return;
		}

		int index = 0;
		for(const PropertyEntry& entry : class_->slots()) {
			if(!entry.get_type) {
				++index;
				continue;
			}

			variant value;

			FormulaPtr override;
			if(static_cast<unsigned>(index) < property_overrides_.size()) {
				override = property_overrides_[index];
			}
			if(override) {
				private_data_scope scope(&private_data_, entry.variable_slot);
				value = override->execute(*this);
			} else if(entry.getter) {
				private_data_scope scope(&private_data_, entry.variable_slot);
				value = entry.getter->execute(*this);
			} else if(entry.variable_slot != -1) {
				private_data_scope scope(&private_data_, entry.variable_slot);
				value = variables_[entry.variable_slot];
			} else {
				++index;
				continue;
			}

			++index;

			ASSERT_LOG(entry.get_type->match(value), "OBJECT OF CLASS TYPE " << class_->name() << " HAS INVALID PROPERTY " << entry.name << ": " << value.write_json() << " EXPECTED " << entry.get_type->str() << " GIVEN TYPE " << variant::variant_type_to_string(value.type()));
		}
	#endif
	}

	void FormulaObject::getInputs(std::vector<FormulaInput>* inputs) const
	{
		for(const PropertyEntry& entry : class_->slots()) {
			FORMULA_ACCESS_TYPE type = FORMULA_ACCESS_TYPE::READ_ONLY;
			if((entry.getter && entry.setter) || entry.variable_slot != -1) {
				type = FORMULA_ACCESS_TYPE::READ_WRITE;
			} else if(entry.getter) {
				type = FORMULA_ACCESS_TYPE::READ_ONLY;
			} else if(entry.setter) {
				type = FORMULA_ACCESS_TYPE::WRITE_ONLY;
			} else {
				continue;
			}

			inputs->push_back(FormulaInput(entry.name, type));
		}
	}

#if defined(USE_LUA)
	void FormulaObject::init_lua()
	{
		if (class_->has_lua())
		{
			lua_ptr_.reset(new lua::LuaContext(*this)); // sets self object implicitly

			if (auto init_script = class_->getLuaInit(*lua_ptr_)) {
				init_script->run(*lua_ptr_);
			}
		}
	}
#endif

	bool formula_class_valid(const std::string& type)
	{
		return known_classes.count(type) != false || get_class_node(type).is_map();
	}

	void invalidate_class_definition(const std::string& name)
	{
		LOG_DEBUG("INVALIDATE CLASS: " << name);
		for(auto i = class_node_map.begin(); i != class_node_map.end(); ) {
			const std::string& class_name = i->first;
			std::string::const_iterator dot = std::find(class_name.begin(), class_name.end(), '.');
			std::string base_class(class_name.begin(), dot);
			if(base_class == name) {
				class_node_map.erase(i++);
			} else {
				++i;
			}
		}

		for(auto i = class_definitions.begin(); i != class_definitions.end(); )
		{
			const std::string& class_name = i->first;
			std::string::const_iterator dot = std::find(class_name.begin(), class_name.end(), '.');
			std::string base_class(class_name.begin(), dot);
			if(base_class == name) {
				class_definitions.erase(i++);
			} else {
				++i;
			}
		}

		classes_map removed_classes;

		for(auto i = classes_.begin(); i != classes_.end(); )
		{
			const std::string& class_name = i->first;
			std::string::const_iterator dot = std::find(class_name.begin(), class_name.end(), '.');
			std::string base_class(class_name.begin(), dot);
			if(base_class == name) {
				removed_classes.insert(*i);
				known_classes.erase(class_name);
				backup_classes_[i->first] = i->second;
				classes_.erase(i++);
			} else {
				++i;
			}
		}

		for(auto p : removed_classes) {
			auto new_class = get_class(p.first);
			p.second->update_class(const_cast<FormulaClass*>(new_class.get()));
		}
	}

	namespace 
	{
		FormulaCallableDefinitionPtr g_library_definition;
	}

	FormulaCallableDefinitionPtr get_library_definition()
	{
		if(!g_library_definition) {

			std::vector<std::string> classes;

			for(auto p : class_path_map()) {
				const std::string& class_name = p.first;
				if(std::count(classes.begin(), classes.end(), class_name) == 0) {
					variant node;
					try {
						node = json::parse_from_file(p.second);
					} catch(json::ParseError& e) {
						ASSERT_LOG(false, "Error parsing " << p.second << ": " << e.errorMessage());
					}

					if(node["server_only"].as_bool(false) == false) {
						classes.push_back(class_name);
					}
				}
			}

			std::vector<variant_type_ptr> types;
			for(const std::string& class_name : classes) {
				types.push_back(variant_type::get_class(class_name));
			}

			if(!types.empty()) {
				g_library_definition = game_logic::execute_command_callable_definition(&classes[0], &classes[0] + classes.size(), nullptr);
				game_logic::register_formula_callable_definition("library", g_library_definition);

				//first pass we have to just set the basic variant type
				//without any definitions.
				for(int n = 0; n != g_library_definition->getNumSlots(); ++n) {
					g_library_definition->getEntry(n)->variant_type = types[n];
				}

				//this time we do a full set_variant_type() which looks up the
				//definitions of the type. We can only do this after we have
				//the first pass done though so lib types can be looked up.
				for(int n = 0; n != g_library_definition->getNumSlots(); ++n) {
					g_library_definition->getEntry(n)->setVariantType(types[n]);
				}
			} else {
				g_library_definition = game_logic::execute_command_callable_definition(nullptr, nullptr, nullptr, nullptr);
			}
		}

		return g_library_definition;
	}

	namespace 
	{
		struct SlotsLoadingGuard {
			explicit SlotsLoadingGuard(std::vector<int>& v) : v_(v)
			{}

			~SlotsLoadingGuard() { v_.pop_back(); }

			std::vector<int>& v_;
		};

		class LibraryCallable : public game_logic::FormulaCallable
		{
		public:
			LibraryCallable() {
				items_.resize(get_library_definition()->getNumSlots());
			}

			bool currently_loading_library(const std::string& key) {
				FormulaCallableDefinitionPtr def = get_library_definition();
				const int slot = def->getSlot(key);
				ASSERT_LOG(slot >= 0, "Unknown library: " << key << "\n" << get_full_call_stack());
				return std::find(slots_loading_.begin(), slots_loading_.end(), slot) != slots_loading_.end();
			}
		private:
			variant getValue(const std::string& key) const override {
				FormulaCallableDefinitionPtr def = get_library_definition();
				const int slot = def->getSlot(key);
				ASSERT_LOG(slot >= 0, "Unknown library: " << key << "\n" << get_full_call_stack());
				return queryValueBySlot(slot);
			}

			variant getValueBySlot(int slot) const override {
				ASSERT_LOG(slot >= 0 && static_cast<unsigned>(slot) < items_.size(), "ILLEGAL LOOK UP IN LIBRARY: " << slot << "/" << items_.size());
				if(items_[slot].is_null()) {
					FormulaCallableDefinitionPtr def = get_library_definition();
					const FormulaCallableDefinition::Entry* entry = def->getEntry(slot);
					ASSERT_LOG(entry != nullptr, "INVALID SLOT: " << slot);
					std::string class_name;
					if(entry->variant_type->is_class(&class_name) == false) {
						ASSERT_LOG(false, "ERROR IN LIBRARY");
					}

					slots_loading_.push_back(slot);
					SlotsLoadingGuard guard(slots_loading_);
					items_[slot] = variant(FormulaObject::create(class_name).get());
				}

				return items_[slot];
			}

			void surrenderReferences(GarbageCollector* collector) override {
				for(variant& item : items_) {
					collector->surrenderVariant(&item);
				}
			}

			mutable std::vector<variant> items_;

			mutable std::vector<int> slots_loading_;
		};
	}

	namespace {
		boost::intrusive_ptr<LibraryCallable> g_library_obj;
	}

	FormulaCallablePtr get_library_object()
	{
		if(g_library_obj.get() == nullptr) {
			g_library_obj.reset(new LibraryCallable);
		}

		return g_library_obj;
	}

	bool can_load_library_instance(const std::string& id)
	{
		get_library_object();
		return !g_library_obj->currently_loading_library(id);
	}

	FormulaCallablePtr get_library_instance(const std::string& id)
	{
		return FormulaCallablePtr(get_library_object()->queryValue(id).mutable_callable());
	}

#if defined(USE_LUA)
	formula_class_unit_test_helper::formula_class_unit_test_helper()
	{
		ASSERT_LOG(unit_test_class_node_map.size() == 0, "Tried to construct multiple helpers?");
	}
	formula_class_unit_test_helper::~formula_class_unit_test_helper()
	{
		unit_test_class_node_map.clear();
	}
	void formula_class_unit_test_helper::add_class_defn(const std::string & name, const variant & node) {
		unit_test_class_node_map[name] = node;
	}
#endif

}
