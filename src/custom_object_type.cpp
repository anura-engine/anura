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

#include <cassert>
#include <iostream>

#include "asserts.hpp"
#include "code_editor_dialog.hpp"
#include "collision_utils.hpp"
#include "custom_object.hpp"
#include "custom_object_callable.hpp"
#include "custom_object_functions.hpp"
#include "custom_object_type.hpp"
#include "filesystem.hpp"
#include "ffl_dom.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "formula_constants.hpp"
#include "formula_function_registry.hpp"
#include "formula_profiler.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "load_level.hpp"
#include "module.hpp"
#include "object_events.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "solid_map.hpp"
#include "sound.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "variant_callable.hpp"
#include "variant_utils.hpp"

using game_logic::FormulaCallableDefinition;
using game_logic::FormulaCallableDefinitionPtr;

PREF_BOOL(auto_anchor_objects, false, "Auto-anchors an object based on x/mid_x/x2 based on what attributes are set when spawning them.");


static const std::string DefaultVertexShaderStr(
    "uniform mat4 u_anura_mvp_matrix;\n"
    "attribute vec4 a_anura_vertex;\n"
	"attribute vec2 a_anura_texcoord;\n"
	"varying vec2 v_texcoord;\n"
	"void main()\n"
	"{\n"
		"v_texcoord = a_anura_texcoord;\n"
		"gl_Position = u_anura_mvp_matrix * a_anura_vertex;\n"
	"}\n"
);

// XXX make this a static function in CustomObjectType
std::map<std::string, std::string>& prototype_file_paths() 
{
	static std::map<std::string, std::string> paths;
	return paths;
}

namespace 
{
	PREF_BOOL(strict_mode_warnings, false, "If turned on, all objects will be run in strict mode, with errors non-fatal");
	PREF_BOOL(suppress_strict_mode, false, "If turned on, turns off strict mode checking on all objects");
	PREF_BOOL(force_strict_mode, false, "If turned on, turns on strict mode checking on all objects");

	bool custom_object_strict_mode = false;

	class StrictModeScope 
	{
		bool old_value_;
	public:
		StrictModeScope() : old_value_(custom_object_strict_mode) {
			custom_object_strict_mode = true;
		}

		~StrictModeScope() {
			custom_object_strict_mode = old_value_;
		}
	};

	//A map of base object filename -> full file paths.
	//the base filename might include module prefixes.
	std::map<std::string, std::string>& object_file_paths() 
	{
		static std::map<std::string, std::string> paths;
		return paths;
	}

	//The undecorated path under which we will find object definitions.
	const std::string& object_path() 
	{
		if(preferences::load_compiled()) {
			static const std::string value =  "data/compiled/objects";
			return value;
		} else {
			static const std::string value =  "data/objects";
			return value;
		}
	}

	const std::string& prototype_path()
	{
		static const std::string value =  "data/object_prototypes";
		return value;
	}

	void load_file_paths() 
	{
		//find out the paths to all our files
		module::get_unique_filenames_under_dir(object_path(), &object_file_paths());
		module::get_unique_filenames_under_dir("data/objects", &::prototype_file_paths());
		module::get_unique_filenames_under_dir("data/object_prototypes", &::prototype_file_paths());
	}

	typedef std::map<std::string, ConstCustomObjectTypePtr> object_map;

	object_map& cache() 
	{
		static object_map instance;
		return instance;
	}

	const std::string BaseStr = "%PROTO%";

	variant merge_into_prototype(variant prototype_node, variant node)
	{
		std::map<variant, variant> result;

		//mapping of animation nodes is kinda complicated: in the
		//prototype there can be one specification of each animation.
		//in objects there can be multiple specifications. Each
		//animation in the object inherits from the specification in
		//the prototype.
		//
		//We are going to build a completely fresh/new set of animations
		//in a vector, and then wipe out all current animations and
		//replace with these from the vector.
		std::vector<variant> animations;
		std::set<std::string> animations_seen;
		for(variant anim : node["animation"].as_list()) {
			variant id = anim["id"];
			animations_seen.insert(id.as_string());
			variant proto_anim;
			for(variant candidate : prototype_node["animation"].as_list()) {
				if(candidate["id"] == id) {
					proto_anim = candidate;
					break;
				}
			}

			if(proto_anim.is_map()) {
				//the animation is in the prototype, so we merge the
				//object's definition of the animation with the
				//prototype's.
				animations.push_back(proto_anim + anim);
			} else {
				//the animation isn't in the prototype, so just add
				//what is given in the object.
				animations.push_back(anim);
			}
		}

		//now go over the prototype node and add any animations that don't
		//appear in the child.
		for(auto anim : prototype_node["animation"].as_list()) {
			if(animations_seen.count(anim["id"].as_string()) == 0) {
				animations.push_back(anim);
			}
		}

		for(auto key : prototype_node.getKeys().as_list()) {
			result[key] = prototype_node[key];
		}

		for(variant key : node.getKeys().as_list()) {
			variant proto_value = result[key];
			variant value = node[key];

			if(value.is_null()) {
				//An explicit null in the object will kill the
				//attribute entirely.
				result[key] = variant();
				continue;
			}

			if(key.as_string().size() > 3 && std::equal(key.as_string().begin(), key.as_string().begin() + 3, "on_")) {
				if(proto_value.is_string()) {
					std::string k = key.as_string();
					const std::string proto_event_key = "on_" + prototype_node["id"].as_string() + "_PROTO_" + std::string(k.begin() + 3, k.end());
					result[variant(proto_event_key)] = proto_value;
				}
			}

			if(value.is_string()) {
				const std::string& value_str = value.as_string();
				std::string::const_iterator base_itor = std::search(value_str.begin(), value_str.end(), BaseStr.begin(), BaseStr.end());
				if(base_itor != value_str.end()) {
					const variant::debug_info* info = value.get_debug_info();
					std::string base_value = "null";
					if(proto_value.is_string()) {
						base_value = proto_value.as_string();
					}
					const std::string s = std::string(value_str.begin(), base_itor) + base_value + std::string(base_itor + BaseStr.size(), value_str.end());
					value = variant(s);
					proto_value = variant();

					if(info) {
						value.setDebugInfo(*info);
					}
				}
			}

			result[key] = append_variants(proto_value, value);
		}

		std::vector<variant> functions;
		variant proto_fn = prototype_node["functions"];
		if(proto_fn.is_string()) {
			functions.push_back(proto_fn);
		} else if(proto_fn.is_list()) {
			for(variant v : proto_fn.as_list()) {
				functions.push_back(v);
			}
		}

		variant fn = node["functions"];
		if(fn.is_string()) {
			functions.push_back(fn);
		} else if(fn.is_list()) {
			for(variant v : fn.as_list()) {
				functions.push_back(v);
			}
		}

		if(!functions.empty()) {
			result[variant("functions")] = variant(&functions);
		}

		result[variant("animation")] = variant(&animations);

		//any objects which are explicitly merged.
		result[variant("tmp")] = prototype_node["tmp"] + node["tmp"];
		result[variant("vars")] = prototype_node["vars"] + node["vars"];
		result[variant("consts")] = prototype_node["consts"] + node["consts"];
		result[variant("variations")] = prototype_node["variations"] + node["variations"];
		result[variant("types")] = prototype_node["types"] + node["types"];

		const variant editor_info_a = prototype_node["editor_info"];
		const variant editor_info_b = node["editor_info"];
		result[variant("editor_info")] = editor_info_a + editor_info_b;
		if(editor_info_a.is_map() && editor_info_b.is_map() &&
		   editor_info_a["var"].is_list() && editor_info_b["var"].is_list()) {
			std::map<variant, variant> vars_map;
			std::vector<variant> items = editor_info_a["var"].as_list();
			std::vector<variant> items2 = editor_info_b["var"].as_list();
			items.insert(items.end(), items2.begin(), items2.end());
			for(const variant& v : items) {
				variant name = v["name"];
				variant enum_value;
				if(vars_map.count(name)) {
					if(vars_map[name]["enum_values"].is_list() && v["enum_values"].is_list()) {
						std::vector<variant> e = vars_map[name]["enum_values"].as_list();
						for(variant item : v["enum_values"].as_list()) {
							if(std::count(e.begin(), e.end(), item) == 0) {
								e.push_back(item);
							}
						}

						enum_value = variant(&e);
					}

					vars_map[name] = vars_map[name] + v;
					if(enum_value.is_null() == false) {
						vars_map[name].add_attr(variant("enum_values"), enum_value);
					}
				} else {
					vars_map[name] = v;
				}
			}

			std::vector<variant> v;
			for(std::map<variant,variant>::const_iterator i = vars_map.begin(); i != vars_map.end(); ++i) {
				v.push_back(i->second);
			}

			variant vars = variant(&v);
			result[variant("editor_info")].add_attr(variant("var"), vars);
		}

		variant proto_events = prototype_node["events"];
		variant node_events = node["events"];

		if(proto_events.is_null() == false && node_events.is_null() == false) {
			//merge prototype events into derived events
			std::map<variant,variant> items = node_events.as_map();
			for(auto p : proto_events.as_map()) {
				auto itor = items.find(p.first);
				if(itor == items.end()) {
					items.insert(p);
				} else {
					variant orig = itor->second;
					std::string appended = itor->second.as_string() + " ; " + p.second.as_string();
					itor->second = variant(appended);
					if(orig.get_debug_info()) {
						itor->second.setDebugInfo(*orig.get_debug_info());
					}
				}
			}

			result[variant("events")] = variant(&items);
		}

		if(proto_events.is_null() && node_events.is_null() == false) {
			//backwards compatible merging of events for when a prototype uses events: {}
			//and the derived uses on_event
			const std::map<variant,variant>& proto_attr = prototype_node.as_map();

			std::map<variant,variant> items = node_events.as_map();
			for(auto p : proto_attr) {
				const std::string& s = p.first.as_string();
				if(s.size() > 3 && std::equal(s.begin(), s.begin() + 3, "on_")) {
					std::string key(s.begin()+3, s.end());
					auto itor = items.find(variant(key));
					if(itor != items.end()) {
						variant orig = p.second;
						std::string appended = orig.as_string() + " ; " + itor->second.as_string();
						items[variant(key)] = variant(appended);

						result.erase(p.first);
					}
				}
			}

			result[variant("events")] = variant(&items);
		}

		variant proto_properties = prototype_node["properties"];
		variant node_properties = node["properties"];

		if(proto_properties.is_map()) {
			std::vector<variant> v;
			v.push_back(proto_properties);
			proto_properties = variant(&v);
		} else if(!proto_properties.is_list()) {
			ASSERT_LOG(proto_properties.is_null(), "Illegal properties: " << proto_properties.debug_location());
			std::vector<variant> v;
			proto_properties = variant(&v);
		}

		//Add a string saying what the name of the prototype is. This will be used
		//to construct the prototype's definition.
		std::vector<variant> proto_name;
		proto_name.push_back(prototype_node["id"]);
		ASSERT_LOG(proto_name.front().is_string(), "Prototype must provide an id: " << prototype_node.debug_location());
		proto_properties = proto_properties + variant(&proto_name);

		if(node_properties.is_map()) {
			std::vector<variant> v;
			v.push_back(node_properties);
			node_properties = variant(&v);
		} else if(!node_properties.is_list()) {
			ASSERT_LOG(node_properties.is_null(), "Illegal properties: " << node_properties.debug_location());
			std::vector<variant> v;
			node_properties = variant(&v);
		}

		std::map<variant, variant> base_properties;
		for(int n = 0; n != proto_properties.num_elements(); ++n) {
			for(auto p : proto_properties[n].as_map()) {
				base_properties[p.first] = p.second;
			}
		}

		std::map<variant, variant> override_properties;

		for(int n = 0; n != node_properties.num_elements(); ++n) {
			for(auto p : node_properties[n].as_map()) {
				auto itor = base_properties.find(p.first);
				if(itor != base_properties.end()) {
					override_properties[variant(prototype_node["id"].as_string() + "_" + p.first.as_string())] = itor->second;
				}
			}
		}

		variant properties = proto_properties + node_properties;
		if(override_properties.empty() == false) {
			std::vector<variant> overrides;
			overrides.push_back(variant(&override_properties));
			properties = properties + variant(&overrides);
		}

		result[variant("properties")] = properties;

		variant res(&result);
		if(node.get_debug_info()) {
			res.setDebugInfo(*node.get_debug_info());
		}

		return res;
	}

	std::map<std::string, std::string>& object_type_inheritance()
	{
		static std::map<std::string, std::string> instance;
		return instance;
	}

	std::vector<std::pair<int,int> >& ancestry_index()
	{
		static std::vector<std::pair<int,int> > instance;
		return instance;
	}

	void add_inheritance_relationship(const std::string& child, const std::string& parent)
	{
		object_type_inheritance()[child] = parent;

		ancestry_index().clear();
		for(auto p : object_type_inheritance()) {
			const int child_id = CustomObjectType::getObjectTypeIndex(p.first);

			std::string parent = p.second;
			const int parent_id = CustomObjectType::getObjectTypeIndex(parent);
			ancestry_index().push_back(std::pair<int,int>(child_id, parent_id));
			
			auto itor = object_type_inheritance().find(parent);
			while(itor != object_type_inheritance().end()) {
				parent = itor->second;
				const int parent_id = CustomObjectType::getObjectTypeIndex(parent);
				ancestry_index().push_back(std::pair<int,int>(child_id, parent_id));
				itor = object_type_inheritance().find(parent);
			}
		}

		std::sort(ancestry_index().begin(), ancestry_index().end());

	}

	std::map<std::string, FormulaCallableDefinitionPtr>& object_type_definitions()
	{
		static std::map<std::string, FormulaCallableDefinitionPtr>* instance = new std::map<std::string, FormulaCallableDefinitionPtr>;
		return *instance;
	}

}

namespace {
std::vector<variant> g_object_validation_functions;
}

void CustomObjectType::addObjectValidationFunction(const variant& str)
{
	game_logic::FormulaPtr f(new game_logic::Formula(str));
	variant result(f->execute());

	std::string fn_type = "function(map, [string])->string|null";
	auto t = parse_variant_type(variant(fn_type));
	ASSERT_LOG(t->match(result), "Object validation function must be of type " << fn_type);

	g_object_validation_functions.push_back(result);
}

int CustomObjectType::getObjectTypeIndex(const std::string& id)
{
	static std::map<std::string, int>* m = new std::map<std::string, int>();
	auto itor = m->find(id);
	if(itor != m->end()) {
		return itor->second;
	} else {
		int result = static_cast<int>(m->size())+1;
		(*m)[id] = result;
		return result;
	}
}

bool CustomObjectType::isDerivedFrom(const std::string& base, const std::string& derived)
{
	if(derived == base) {
		return true;
	}

	auto itor = object_type_inheritance().find(derived);
	if(itor == object_type_inheritance().end()) {
		return false;
	}

	assert(itor->second != derived);

	return isDerivedFrom(base, itor->second);
}

bool CustomObjectType::isDerivedFrom(int base, int derived)
{
	if(base == derived) {
		return true;
	}

	return std::binary_search(ancestry_index().begin(), ancestry_index().end(), std::pair<int,int>(derived, base));
}

namespace {

void init_object_definition(variant node, const std::string& id_, CustomObjectCallablePtr callable_definition_, int& slot_properties_base_, bool is_strict_)
{
	object_type_definitions()[id_] = callable_definition_;

	const types_cfg_scope types_scope(node["types"]);

	std::set<std::string> properties_to_infer;

	std::map<std::string, bool> property_overridable_state;
	std::map<std::string, variant_type_ptr> property_override_type;

	std::map<std::string, CustomObjectCallablePtr> proto_definitions;
	std::string prototype_derived_from;

	slot_properties_base_ = callable_definition_->getNumSlots();
	for(variant properties_node : node["properties"].as_list()) {
		if(properties_node.is_string()) {
			if(prototype_derived_from != "") {
				assert(properties_node.as_string() != prototype_derived_from);
				add_inheritance_relationship(properties_node.as_string(), prototype_derived_from);
			}
			prototype_derived_from = properties_node.as_string();

			if(object_type_definitions().count(properties_node.as_string())) {
				continue;
			}

			auto new_callable = new CustomObjectCallable(*callable_definition_);
			new_callable->setTypeName("obj " + properties_node.as_string());

			proto_definitions[properties_node.as_string()].reset(new_callable);
			continue;
		}

		for(variant key : properties_node.getKeys().as_list()) {
			const std::string& k = key.as_string();
			ASSERT_LOG(k.empty() == false, "property is empty");
			ASSERT_LOG(properties_to_infer.count(k) == 0, "Object " << id_ << " overrides property " << k << " which is defined with no type definition in a prototype. If you want to override a property in a prototype that property must have a type definition in the prototype");
			bool is_private = is_strict_ && k[0] == '_';
			ASSERT_LOG(CustomObjectCallable::instance().getSlot(k) == -1, "Custom object property " << id_ << "." << k << " has the same name as a builtin");

			ASSERT_LOG(property_overridable_state.count(k) == 0 || property_overridable_state[k], "Variable properties are not overridable: " << id_ << "." << k);

			bool& can_override = property_overridable_state[k];
			can_override = true;

			variant value = properties_node[key];
			variant_type_ptr type, set_type;
			bool requires_initialization = false;
			if(value.is_string()) {
				type = parse_optional_function_type(value);
				if(is_strict_ && type) {
					bool return_type_specified = false;
					type->is_function(nullptr, nullptr, nullptr, &return_type_specified);
					ASSERT_LOG(return_type_specified, "Property function definition does not specify a return type for the function, which is required in strict mode for object " << id_ << "." << k);
				}
				if(!type) {
					type = parse_optional_formula_type(value);
				}

				set_type = variant_type::get_none();

			} else if(value.is_map()) {
				if(value.has_key("access")) {
					const std::string& access = value["access"].as_string();
					if(access == "public") {
						is_private = false;
					} else if(access == "private") {
						is_private = true;
					} else {
						ASSERT_LOG(false, "unknown access: " << access << " " << value["access"].debug_location());
					}
				}

				if(value.has_key("type")) {
					type = parse_variant_type(value["type"]);
				} else if(is_strict_ && value.has_key("default")) {
					type = get_variant_type_from_value(value["default"]);
				} else {
					ASSERT_LOG(!is_strict_, "Property does not have a type specifier in strict mode object " << id_ << " property " << k);
				}

				if(value.has_key("set_type")) {
					set_type = parse_variant_type(value["set_type"]);
				} else {
					set_type = type;
				}


				if(is_strict_ && type) {
					variant default_value = value["default"];
					if(!type->match(default_value)) {
						ASSERT_LOG(default_value.is_null(), "Default value for " << id_ << "." << k << " is " << default_value.write_json() << " of type " << get_variant_type_from_value(default_value)->to_string() << " does not match type " << type->to_string());

						if(value["variable"].as_bool(true) && !value["dynamic_initialization"].as_bool(false) && !value["init"].is_string()) {
							requires_initialization = true;
						}
					}
				}
			} else {
				if(is_strict_) {
					type = get_variant_type_from_value(value);

					if(k[0] != '_') {
						set_type = variant_type::get_none();
					} else {
						set_type = type;
					}
				}
			}

			if(!type && is_strict_) {
				if(property_override_type.count(k) || !is_strict_) {
					type = property_override_type[k];
				} else {
					properties_to_infer.insert(k);
				}
			} else if(property_override_type.count(k)) {
				ASSERT_LOG(!is_strict_ || property_override_type[k], "Type mis-match for object property " << id_ << "." << k << " derived object gives a type while base object does not");

				if(is_strict_ || (property_override_type[k] && type)) {
					ASSERT_LOG(variant_types_compatible(property_override_type[k], type), "Type mis-match for object property " << id_ << "." << k << " has a different type than the definition in the prototype type: " << type->to_string() << " prototype defines as " << property_override_type[k]->to_string());
				}
			}

			property_override_type[k] = type;

			if(is_strict_) {
				int current_slot = callable_definition_->getSlot(k);
				if(current_slot != -1) {
					const game_logic::FormulaCallableDefinition::Entry* entry = callable_definition_->getEntry(current_slot);
					ASSERT_LOG(!entry->variant_type || variant_types_compatible(entry->variant_type, type), "Type mis-match for object property " << id_ << "." << k << " has a different type than the definition in the prototype: " << type->to_string() << " prototype defines as " << entry->variant_type->to_string());
					ASSERT_LOG(!set_type || set_type->is_none() == entry->getWriteType()->is_none(), "Object property " << id_ << "." << k << " is immutable in the " << (set_type->is_none() ? "object" : "prototype") << " but not in the " << (set_type->is_none() ? "prototype" : "object"));
					ASSERT_LOG(!set_type || (set_type->is_none() && entry->getWriteType()->is_none()) || (variant_types_compatible(entry->getWriteType(), set_type)), "Type mis-match for object property " << id_ << "." << k << " has a different mutable type than the definition in the prototype. The property can be mutated with a " << set_type->to_string() << " while prototype allows mutation as " << entry->getWriteType()->to_string());
				}
			}

			callable_definition_->addProperty(k, type, set_type, requires_initialization, is_private);
		}
	}

	for(auto p : proto_definitions) {
		object_type_definitions()[p.first] = p.second;
	}

	object_type_definitions()[id_] = callable_definition_;

	while(is_strict_ && !properties_to_infer.empty()) {
		const int num_items = static_cast<int>(properties_to_infer.size());
		for(variant properties_node : node["properties"].as_list()) {
			if(properties_node.is_string()) {
				continue;
			}

			for(variant key : properties_node.getKeys().as_list()) {
				const std::string& k = key.as_string();
				if(properties_to_infer.count(k) == 0) {
					continue;
				}

				variant value = properties_node[key];
				assert(value.is_string());

				for(int n = 0; n != callable_definition_->getNumSlots(); ++n) {
					callable_definition_->getEntry(n)->access_count = 0;
				}

				game_logic::FormulaPtr f = game_logic::Formula::createOptionalFormula(value, &get_custom_object_functions_symbol_table(), callable_definition_);
				bool inferred = true;
				for(int n = 0; n != callable_definition_->getNumSlots(); ++n) {
					const game_logic::FormulaCallableDefinition::Entry* entry = callable_definition_->getEntry(n);
					if(entry->access_count) {
						if(properties_to_infer.count(entry->id)) {
							inferred = false;
						}
					}
				}

				if(inferred) {
					FormulaCallableDefinition::Entry* entry = callable_definition_->getEntryById(k);
					assert(entry);
					entry->variant_type = f->queryVariantType();
					properties_to_infer.erase(k);
				}
			}
		}

		if(num_items == properties_to_infer.size()) {
			std::ostringstream s;
			for(const std::string& k : properties_to_infer) {
				s << k << ", ";
			}
			ASSERT_LOG(false, "Could not infer types for properties in object " << id_ << ": " << s.str() << ". Explicitly specify types for these properties.");
		}
	}

	if(prototype_derived_from != "") {
		ASSERT_LOG(id_ != prototype_derived_from, "Object " << id_ << " derives from itself");
		object_type_inheritance()[id_] = prototype_derived_from;
		add_inheritance_relationship(id_, prototype_derived_from);
	}

	callable_definition_->finalizeProperties();
	callable_definition_->setStrict(is_strict_);

	for(auto f : g_object_validation_functions) {
		std::vector<variant> protos;

		while(prototype_derived_from != "") {
			protos.push_back(variant(prototype_derived_from));
			auto it = object_type_inheritance().find(prototype_derived_from);
			if(it == object_type_inheritance().end()) {
				break;
			}
			prototype_derived_from = it->second;
		}

		std::vector<variant> args;
		args.push_back(node);
		args.push_back(variant(&protos));

		variant result = f(args);
		ASSERT_LOG(result.is_null(), "Object validation failed for object " << id_ << ": " << result.as_string());
	}
}

variant g_player_type_str;
}

void CustomObjectType::setPlayerVariantType(variant type_str)
{
	g_player_type_str = type_str;
}

FormulaCallableDefinitionPtr CustomObjectType::getDefinition(const std::string& id)
{
	std::map<std::string, FormulaCallableDefinitionPtr>::const_iterator itor = object_type_definitions().find(id);
	if(itor != object_type_definitions().end()) {
		return itor->second;
	} else {
		if(object_file_paths().empty()) {
			load_file_paths();
		}

		auto proto_path = module::find(prototype_file_paths(), id + ".cfg");
		if(proto_path != prototype_file_paths().end()) {
			ASSERT_LOG(getObjectPath(id) == nullptr, "Object " << id << " has a prototype with the same name. Objects and prototypes must have different names");
			variant node = mergePrototype(json::parse_from_file(proto_path->second));
			CustomObjectCallablePtr callableDefinition(new CustomObjectCallable);
			callableDefinition->setTypeName("obj " + id);
			int slot = -1;
			init_object_definition(node, node["id"].as_string(), callableDefinition, slot, (!g_suppress_strict_mode && node["is_strict"].as_bool(custom_object_strict_mode)) || g_force_strict_mode);
			std::map<std::string, FormulaCallableDefinitionPtr>::const_iterator itor = object_type_definitions().find(id);
			ASSERT_LOG(itor != object_type_definitions().end(), "Could not load object prototype definition " << id);
			return itor->second;
		}

		std::string::const_iterator dot_itor = std::find(id.begin(), id.end(), '.');
		std::string obj_id(id.begin(), dot_itor);

		const std::string* path = getObjectPath(obj_id + ".cfg");
		ASSERT_LOG(path != nullptr, "No definition for object " << id);

		std::map<std::string, variant> nodes;

		variant node = mergePrototype(json::parse_from_file(*path));
		nodes[obj_id] = node;
		if(node["object_type"].is_list() || node["object_type"].is_map()) {
			for(variant sub_node : node["object_type"].as_list()) {
				const std::string sub_id = obj_id + "." + sub_node["id"].as_string();
				ASSERT_LOG(nodes.count(sub_id) == 0, "Duplicate object: " << sub_id);
				nodes[sub_id] = mergePrototype(sub_node);
			}
		}

		for(auto p : nodes) {
			if(object_type_definitions().count(p.first)) {
				continue;
			}

			CustomObjectCallablePtr callableDefinition(new CustomObjectCallable);
			callableDefinition->setTypeName("obj " + p.first);
			int slot = -1;
			init_object_definition(p.second, p.first, callableDefinition, slot, (!g_suppress_strict_mode && p.second["is_strict"].as_bool(custom_object_strict_mode)) || g_force_strict_mode);
		}

		itor = object_type_definitions().find(id);
		ASSERT_LOG(itor != object_type_definitions().end(), "No definition for object " << id);
		return itor->second;
	}
}

void CustomObjectType::ReloadFilePaths() 
{
	invalidateAllObjects();
	load_file_paths();
}


//function which finds if a node has a prototype, and if so, applies the
//prototype to the node.
variant CustomObjectType::mergePrototype(variant node, std::vector<std::string>* proto_paths)
{
	if(!node.has_key("prototype")) {
		return node;
	}

	std::vector<std::string> protos = node["prototype"].as_list_string();
	if(protos.size() > 1) {
		LOG_WARN("Multiple inheritance of objects is deprecated: " << node["prototype"].debug_location());
	}

	for(const std::string& proto : protos) {
		//look up the object's prototype and merge it in
		std::map<std::string, std::string>::const_iterator path_itor = module::find(::prototype_file_paths(), proto + ".cfg");
		ASSERT_LOG(path_itor != ::prototype_file_paths().end(), "Could not find file for prototype '" << proto << "'");

		variant prototype_node = json::parse_from_file(path_itor->second);
		ASSERT_LOG(prototype_node["id"].as_string() == proto, "PROTOTYPE NODE FOR " << proto << " DOES NOT SPECIFY AN ACCURATE id FIELD");
		if(proto_paths) {
			proto_paths->push_back(path_itor->second);
		}
		prototype_node = mergePrototype(prototype_node, proto_paths);
		node = merge_into_prototype(prototype_node, node);
	}
	return node;
}

const std::string* CustomObjectType::getObjectPath(const std::string& id)
{
	if(object_file_paths().empty()) {
		load_file_paths();
	}

	std::map<std::string, std::string>::const_iterator itor = module::find(object_file_paths(), id);
	if(itor == object_file_paths().end()) {
		return nullptr;
	}

	return &itor->second;
}

ConstCustomObjectTypePtr CustomObjectType::get(const std::string& id)
{
	std::string::const_iterator dot_itor = std::find(id.begin(), id.end(), '.');
	if(dot_itor != id.end()) {
		ConstCustomObjectTypePtr parent = get(std::string(id.begin(), dot_itor));
		if(!parent) {
			return ConstCustomObjectTypePtr();
		}

		return parent->getSubObject(std::string(dot_itor+1, id.end()));
	}

	object_map::const_iterator itor = cache().find(module::get_id(id));
	if(itor != cache().end()) {
		return itor->second;
	}

	ConstCustomObjectTypePtr result(create(id));
	cache()[module::get_id(id)] = result;

	//load the object's variations here to avoid pausing the game
	//when an object starts its variation.
	result->loadVariations();

	for(auto s : result->preloadObjects()) {
		get(s);
	}

	return result;
}

ConstCustomObjectTypePtr CustomObjectType::getOrDie(const std::string& id)
{
	const ConstCustomObjectTypePtr res = get(id);
	ASSERT_LOG(res.get() != nullptr, "UNRECOGNIZED OBJECT TYPE: '" << id << "'");

	return res;
}

ConstCustomObjectTypePtr CustomObjectType::getSubObject(const std::string& id) const
{
	std::map<std::string, ConstCustomObjectTypePtr>::const_iterator itor = sub_objects_.find(id);
	if(itor != sub_objects_.end()) {
		return itor->second;
	} else {
		return ConstCustomObjectTypePtr();
	}
}

CustomObjectTypePtr CustomObjectType::create(const std::string& id)
{
	return recreate(id, nullptr);
}

namespace 
{
	std::map<std::string, std::vector<std::string> > object_prototype_paths;
}

CustomObjectTypePtr CustomObjectType::recreate(const std::string& id,
                                             const CustomObjectType* old_type)
{
	static std::set<std::string> stable_object_id;
	auto p = stable_object_id.insert("LOAD_OBJECT " + id);
	formula_profiler::Instrument instrument(p.first->c_str());

	if(object_file_paths().empty()) {
		load_file_paths();
	}

	//find the file for the object we are loading.
	std::map<std::string, std::string>::const_iterator path_itor = module::find(object_file_paths(), id + ".cfg");
	ASSERT_LOG(path_itor != object_file_paths().end(), "Could not find file for object '" << id << "'");

//	auto proto_path = module::find(prototype_file_paths(), id + ".cfg");
//	ASSERT_LOG(proto_path == prototype_file_paths().end(), "Object " << id << " has a prototype with the same name. Objects and prototypes must have distinct names");

	try {
		std::vector<std::string> proto_paths;
		variant node = mergePrototype(json::parse_from_file(path_itor->second), &proto_paths);

		ASSERT_LOG(node["id"].as_string() == module::get_id(id), "IN " << path_itor->second << " OBJECT ID DOES NOT MATCH FILENAME");
		
		try {
			std::unique_ptr<assert_recover_scope> recover_scope;
			if(preferences::edit_and_continue()) {
				recover_scope.reset(new assert_recover_scope);
			}

			//create the object
			CustomObjectTypePtr result(new CustomObjectType(node["id"].as_string(), node, nullptr, old_type));
			object_prototype_paths[id] = proto_paths;

			return result;
		} catch(validation_failure_exception& e) {
			static bool in_edit_and_continue = false;
			if(in_edit_and_continue || preferences::edit_and_continue() == false) {
				throw e;
			}

			in_edit_and_continue = true;
			edit_and_continue_fn(path_itor->second, e.msg, [=](){ CustomObjectType::recreate(id, old_type); });
			in_edit_and_continue = false;
			return recreate(id, old_type);
		}

	} catch(json::ParseError& e) {
		ASSERT_LOG(false, "Error parsing FML for custom object '" << id << "' in '" << path_itor->second << "': '" << e.errorMessage() << "'");
	} catch(KRE::ImageLoadError& e) {
		ASSERT_LOG(false, "Error loading object '" << id << "': could not load needed image: " << e.what());
	}
	// We never get here, but this stops a compiler warning.
	return CustomObjectTypePtr();
}

void CustomObjectType::invalidateObject(const std::string& id)
{
	cache().erase(module::get_id(id));
}

void CustomObjectType::invalidateAllObjects()
{
	cache().clear();
	object_file_paths().clear();
	::prototype_file_paths().clear();
}

std::vector<std::string> CustomObjectType::getAllIds(bool prototypes)
{
	std::vector<std::string> res;
	std::map<std::string, std::string> file_paths;
	module::get_unique_filenames_under_dir(prototypes ? prototype_path() : object_path(), &file_paths);
	for(std::map<std::string, std::string>::const_iterator i = file_paths.begin(); i != file_paths.end(); ++i) {
		const std::string& fname = i->first;
		if(fname.size() < 4 || std::string(fname.end()-4, fname.end()) != ".cfg") {
			continue;
		}

		const std::string id(fname.begin(), fname.end() - 4);
		res.push_back(id);
	}

	return res;
}

const std::vector<std::string>& CustomObjectType::possibleIdsIncludingPrototypes()
{
	static std::vector<std::string> result;
	if(result.empty()) {
		auto a = getAllIds(true);
		auto b = getAllIds(false);
		result.insert(result.end(), a.begin(), a.end());
		result.insert(result.end(), b.begin(), b.end());

		for(std::string& s : result) {
			auto colon = std::find(s.begin(), s.end(), ':');
			if(colon != s.end()) {
				s.erase(s.begin(), colon+1);
			}
		}

		std::sort(result.begin(), result.end());
	}

	return result;
}


std::map<std::string,CustomObjectType::EditorSummary> CustomObjectType::getEditorCategories()
{
	const std::string path = std::string(preferences::user_data_path()) + "/editor_cache.cfg";
	variant cache, proto_cache;
	if(sys::file_exists(path)) {
		try {
			cache = json::parse(sys::read_file(path), json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
			proto_cache = cache["prototype_info"];
		} catch(...) {
		}
	}

	std::map<std::string, bool> proto_status;

	std::map<variant, variant> items, proto_info;
	for(const std::string& id : getAllIds()) {
		variant info;
		const std::string* path = getObjectPath(id + ".cfg");
		if(path == nullptr) {
			LOG_WARN("NO FILE FOR OBJECT '" << id << "'");
		}

		const int mod_time = static_cast<int>(sys::file_mod_time(*path));
		if(cache.is_map() && cache.has_key(id) && cache[id]["mod"].as_int() == mod_time) {
			info = cache[id];
			for(const std::string& p : info["prototype_paths"].as_list_string()) {
				if(!proto_status.count(p)) {
					const int t = static_cast<int>(sys::file_mod_time(p));
					proto_info[variant(p)] = variant(t);
					proto_status[p] = t == proto_cache[p].as_int();
				}

				if(!proto_status[p]) {
					info = variant();
					break;
				}
			}
		}

		if(info.is_null()) {
			std::vector<std::string> proto_paths;
			variant node = mergePrototype(json::parse_from_file_or_die(*path), &proto_paths);
			std::map<variant,variant> summary;
			summary[variant("mod")] = variant(mod_time);
			std::vector<variant> proto_paths_v;
			for(const std::string& s : proto_paths) {
				proto_paths_v.push_back(variant(s));
			}

			summary[variant("prototype_paths")] = variant(&proto_paths_v);

			variant anim_var = node["animation"];

			if(anim_var.is_list() && anim_var.num_elements() > 0) {
				summary[variant("animation")] = anim_var[0];
			} else if(anim_var.is_map()) {
				summary[variant("animation")] = anim_var;
			} else {
				summary[variant("animation")] = json::parse_from_file_or_die("data/default-animation.cfg");
			}

			if(node["editor_info"].is_map()) {
				summary[variant("category")] = node["editor_info"]["category"];
				if(node["editor_info"]["help"].is_string()) {
					summary[variant("help")] = node["editor_info"]["help"];
				}
			}

			info = variant(&summary);
		}

		items[variant(id)] = info;
	}

	std::map<std::string,EditorSummary> m;
	for(auto i : items) {
		if(i.second.has_key("category")) {
			EditorSummary& summary = m[i.first.as_string()];
			summary.category = i.second["category"].as_string();
			if(i.second["help"].is_string()) {
				//   Word wrap the help descriptions of objects
				// in the editor. For readability!
				summary.help = util::word_wrap(
						i.second["help"].as_string(),
						//   Must be set up to this low
						// when wanting to accomodate
						// some 13" laptops.
						//   TODO Make it variable.
						108
				);
			}
			summary.first_frame = i.second["animation"];
		}
	}

	items[variant("prototype_info")] = variant(&proto_info);

	const variant result = variant(&items);
	sys::write_file(path, result.write_json());

	return m;
}

std::vector<ConstCustomObjectTypePtr> CustomObjectType::getAll()
{
	std::vector<ConstCustomObjectTypePtr> res;
	for(const std::string& id : getAllIds()) {
		res.push_back(get(id));
	}

	return res;
}

#ifndef NO_EDITOR
namespace 
{
	std::set<std::string> listening_for_files, files_updated;

	void on_object_file_updated(std::string path)
	{
		files_updated.insert(path);
	}
}

int CustomObjectType::reloadModifiedCode()
{
	static int prev_nitems = 0;
	const int nitems = static_cast<int>(cache().size());
	if(prev_nitems == nitems && files_updated.empty()) {
		return 0;
	}

	prev_nitems = nitems;

	std::set<std::string> error_paths;

	int result = 0;
	for(auto i : cache()) {
		const std::string* path = getObjectPath(i.first + ".cfg");

		if(path == nullptr) {
			continue;
		}

		if(listening_for_files.count(*path) == 0) {
			sys::notify_on_file_modification(*path, std::bind(on_object_file_updated, *path));
			listening_for_files.insert(*path);
		}

		if(files_updated.count(*path)) {
			try {
				reloadObject(i.first);
				++result;
			} catch(...) {
				error_paths.insert(*path);
			}
		}
	}

	//TODO: do something with the error paths/error messages.
	files_updated.clear();

	return result;
}
#endif // NO_EDITOR

void CustomObjectType::setFileContents(const std::string& file_path, const std::string& contents)
{
	json::set_file_contents(file_path, contents);
	for(auto i : cache()) {
		const std::vector<std::string>& proto_paths = object_prototype_paths[i.first];
		const std::string* path = getObjectPath(i.first + ".cfg");
		if((path && *path == file_path) || std::count(proto_paths.begin(), proto_paths.end(), file_path)) {
			reloadObject(i.first);
		}
	}
}

namespace 
{
	int g_numObjectReloads = 0;
}

void CustomObjectType::reloadObject(const std::string& type)
{
	object_map::iterator itor = cache().find(module::get_id(type));
	ASSERT_LOG(itor != cache().end(), "COULD NOT RELOAD OBJECT " << type);
	
	ConstCustomObjectTypePtr old_obj = itor->second;

	CustomObjectTypePtr new_obj;
	
	{
		const assert_recover_scope scope;
		new_obj = recreate(type, old_obj.get());
	}

	if(!new_obj) {
		return;
	}

	const int start = profile::get_tick_time();
	for(CustomObject* obj : CustomObject::getAll(old_obj->id())) {
		assert(obj);
		obj->updateType(old_obj, new_obj);
	}

	for(std::map<std::string, ConstCustomObjectTypePtr>::const_iterator i = old_obj->sub_objects_.begin(); i != old_obj->sub_objects_.end(); ++i) {
		std::map<std::string, ConstCustomObjectTypePtr>::const_iterator j = new_obj->sub_objects_.find(i->first);
		if(j != new_obj->sub_objects_.end() && i->second != j->second) {
			for(CustomObject* obj : CustomObject::getAll(i->second->id())) {
				obj->updateType(i->second, j->second);
			}
		}
	}

	const int end = profile::get_tick_time();
	LOG_INFO("UPDATED " << CustomObject::getAll(old_obj->id()).size() << " OBJECTS IN " << (end - start) << "ms");

	itor->second = new_obj;

	++g_numObjectReloads;
}

int CustomObjectType::numObjectReloads()
{
	return g_numObjectReloads;
}

void CustomObjectType::initEventHandler(const std::string& event, const variant& value,
                                             event_handler_map& handlers,
											 game_logic::FunctionSymbolTable* symbols,
											 const event_handler_map* base_handlers) const
{
	const int event_id = get_object_event_id(event);
	if(handlers.size() <= static_cast<unsigned>(event_id)) {
		handlers.resize(event_id+1);
	}

	if(base_handlers && base_handlers->size() > static_cast<unsigned>(event_id) && (*base_handlers)[event_id] && (*base_handlers)[event_id]->str() == value.as_string()) {
		handlers[event_id] = (*base_handlers)[event_id];
	} else {
		std::unique_ptr<CustomObjectCallableModifyScope> modify_scope;
		const variant_type_ptr arg_type = get_object_event_arg_type(get_object_event_id_maybe_proto(event));
		if(arg_type) {
			modify_scope.reset(new CustomObjectCallableModifyScope(*callable_definition_, CUSTOM_OBJECT_ARG, arg_type));
		}
		handlers[event_id] = game_logic::Formula::createOptionalFormula(value, symbols, callable_definition_);
	}
}

void CustomObjectType::initEventHandlers(variant node,
                                             event_handler_map& handlers,
											 game_logic::FunctionSymbolTable* symbols,
											 const event_handler_map* base_handlers) const
{
	const CustomObjectCallableExposePrivateScope expose_scope(*callable_definition_);
	const game_logic::Formula::StrictCheckScope strict_checking(is_strict_ || g_strict_mode_warnings, g_strict_mode_warnings);

	if(symbols == nullptr) {
		symbols = &get_custom_object_functions_symbol_table();
	}

	variant events_node = node["events"];

	if(events_node.is_null() == false) {
		for(const variant_pair& value : events_node.as_map()) {
			std::string event = value.first.as_string();
			if(event.empty() == false && event[0] == '+') {
				event.erase(event.begin());
			} else {
				ASSERT_LOG(std::count(builtin_object_event_names().begin(), builtin_object_event_names().end(), event) > 0,
				           "In object " << node["id"].as_string() << " event " << event << " is unknown. Use + in front of an event name to define a custom event name.");
			}

			initEventHandler(event, value.second, handlers, symbols, base_handlers);
		}

	}

	for(const variant_pair& value : node.as_map()) {
		const std::string& key = value.first.as_string();
		if(key.size() > 3 && std::equal(key.begin(), key.begin() + 3, "on_")) {
			//ASSERT_LOG(events_node.is_null(), "Object " << node["id"].as_string() << " has an events node but also has " << key << ". Cannot mix old and new-style events");
			const std::string event(key.begin() + 3, key.end());
			initEventHandler(event, value.second, handlers, symbols, base_handlers);
		}
	}
}

namespace 
{	
	std::vector<std::string>& get_custom_object_type_stack()
	{
		static std::vector<std::string> res;
		return res;
	}

	struct CustomObjectTypeInitScope 
	{
		explicit CustomObjectTypeInitScope(const std::string& id) {
			get_custom_object_type_stack().push_back(id);
		}

		~CustomObjectTypeInitScope() {
			get_custom_object_type_stack().pop_back();
		}
	};
}

void init_level_definition();

CustomObjectType::CustomObjectType(const std::string& id, variant node, const CustomObjectType* base_type, const CustomObjectType* old_type)
  : id_(id),
    numeric_id_(getObjectTypeIndex(id)),
	hitpoints_(node["hitpoints"].as_int(1)),
	timerFrequency_(node["timer_frequency"].as_int(-1)),
	zorder_(node["zorder"].as_int()),
	zsub_order_(node["zsub_order"].as_int()),
	is_human_(node["is_human"].as_bool(false)),
	goes_inactive_only_when_standing_(node["goes_inactive_only_when_standing"].as_bool(false)),
	dies_on_inactive_(node["dies_on_inactive"].as_bool(false)),
	always_active_(node["always_active"].as_bool(false)),
    body_harmful_(node["body_harmful"].as_bool(true)),
    body_passthrough_(node["body_passthrough"].as_bool(false)),
    ignore_collide_(node["ignore_collide"].as_bool(false)),
    object_level_collisions_(node["object_level_collisions"].as_bool(false)),
	editor_only_(node["editor_only"].as_bool(false)),
	surface_friction_(node["surface_friction"].as_int(100)),
	surface_traction_(node["surface_traction"].as_int(100)),
	friction_(node["friction"].as_int()),
	traction_(node["traction"].as_int(1000)),
	traction_in_air_(node["traction_in_air"].as_int(0)),
	traction_in_water_(node["traction_in_water"].as_int(0)),
	respawns_(node["respawns"].as_bool(true)),
	affected_by_currents_(node["affected_by_currents"].as_bool(false)),
	is_vehicle_(node["vehicle"].as_bool(false)),	
	passenger_x_(node["passenger_x"].as_int()),
	passenger_y_(node["passenger_y"].as_int()),
	feet_width_(node["feet_width"].as_int(0)),
	use_image_for_collisions_(node["use_image_for_collisions"].as_bool(false)),
	static_object_(node["static_object"].as_bool(use_image_for_collisions_)),
	collides_with_level_(node["collides_with_level"].as_bool(true)),
	has_feet_(node["has_feet"].as_bool(true) && static_object_ == false),
	adjust_feet_on_animation_change_(node["adjust_feet_on_animation_change"].as_bool(true)),
	teleport_offset_x_(node["teleport_offset_x"].as_int()),
	teleport_offset_y_(node["teleport_offset_y"].as_int()),
	no_move_to_standing_(node["no_move_to_standing"].as_bool()),
	reverse_global_vertical_zordering_(node["reverse_global_vertical_zordering"].as_bool(false)),
	serializable_(node["serializable"].as_bool(true)),
	solid_(SolidInfo::create(node)),
	platform_(SolidInfo::createPlatform(node)),
	solid_platform_(node["solid_platform"].as_bool(false)),
	has_solid_(solid_ || use_image_for_collisions_),
	solid_dimensions_(has_solid_ || platform_ ? 0xFFFFFFFF : 0),
	collide_dimensions_(0xFFFFFFFF),
	weak_solid_dimensions_(has_solid_ || platform_ || node["has_platform"].as_bool(false) ? 0xFFFFFFFF : 0),
	weak_collide_dimensions_(0xFFFFFFFF),
	activation_border_(node["activation_border"].as_int(100)),
	editor_force_standing_(node["editor_force_standing"].as_bool(false)),
	hidden_in_game_(node["hidden_in_game"].as_bool(false)),
	auto_anchor_(node["auto_anchor"].as_bool(g_auto_anchor_objects)),
	stateless_(node["stateless"].as_bool(false)),
	platform_offsets_(node["platform_offsets"].as_list_int_optional()),
	slot_properties_base_(-1), 
	use_absolute_screen_coordinates_(node["use_absolute_screen_coordinates"].as_bool(false)),
	mouseover_delay_(node["mouseover_delay"].as_int(0)),
	mouse_drag_threshold_(node["mouse_drag_threshold"].as_int(-1)),
	is_strict_((!g_suppress_strict_mode && node["is_strict"].as_bool(custom_object_strict_mode)) || g_force_strict_mode),
	is_shadow_(node["is_shadow"].as_bool(false)),
	particle_system_desc_(node["particles"]),
	preload_objects_(node["preload_objects"].as_list_string_optional()),
	document_(nullptr),
	draw_batch_id_(node["draw_batch_id"].as_string_default(""))
{
	ObjectTypesSpawnedTracker types_spawned;

	if(g_player_type_str.is_null() == false) {
		//if a playable object type has been set, register what the type of
		//the player is before we construct our object.
		variant type = g_player_type_str;
		g_player_type_str = variant();
		Level::setPlayerVariantType(type);
	}

	Frame::buildPatterns(node);

	if(editor_force_standing_) {
		ASSERT_LOG(has_feet_, "Object type " << id_ << " has editor_force_standing set but has no feet. has_feet must be true for an object forced to standing");
	}
	std::unique_ptr<StrictModeScope> strict_scope;
	if(is_strict_) {
		strict_scope.reset(new StrictModeScope);
	}

	const game_logic::Formula::StrictCheckScope strict_checking(false);

	const CustomObjectTypeInitScope init_scope(id);
	const bool is_recursive_call = std::count(get_custom_object_type_stack().begin(), get_custom_object_type_stack().end(), id) > 0;

	callable_definition_.reset(new CustomObjectCallable);
	callable_definition_->setTypeName("obj " + id);

	CustomObjectCallable::instance();

	EditorEntityInfo* EditorInfo = nullptr;

#ifndef NO_EDITOR
	if(node.has_key("editor_info")) {
		EditorInfo = new EditorEntityInfo(node["editor_info"]);
		editor_info_.reset(EditorInfo);
	}
#endif // !NO_EDITOR

	if(node.has_key("preload_sounds")) {
		//Pre-load any sounds that should be present when we create
		//this object type.
		for(const std::string& sound : util::split(node["preload_sounds"].as_string())) {
			sound::preload(sound);
		}
	}

	const bool is_variation = base_type != nullptr;

	//make it so any formula has these constants defined.
	const game_logic::ConstantsLoader scope_consts(node["consts"]);

	//if some constants change from base to variation, then we have to
	//re-parse all formulas.
	if(scope_consts.same_as_base() == false) {
		base_type = nullptr;
	}

	if(node.has_key("solid_dimensions")) {
		weak_solid_dimensions_ = solid_dimensions_ = 0;
		for(std::string key : node["solid_dimensions"].as_list_string()) {
			if(key.empty() == false && key[0] == '~') {
				key = std::string(key.begin()+1, key.end());
				weak_solid_dimensions_ |= (1 << get_solid_dimension_id(key));
			} else {
				solid_dimensions_ |= (1 << get_solid_dimension_id(key));
			}
		}

		weak_solid_dimensions_ |= solid_dimensions_;
	}

	if(node.has_key("collide_dimensions")) {
		weak_collide_dimensions_ = collide_dimensions_ = 0;
		for(std::string key : node["collide_dimensions"].as_list_string()) {
			if(key.empty() == false && key[0] == '~') {
				key = std::string(key.begin()+1, key.end());
				weak_collide_dimensions_ |= (1 << get_solid_dimension_id(key));
			} else {
				collide_dimensions_ |= (1 << get_solid_dimension_id(key));
			}
		}

		weak_collide_dimensions_ |= collide_dimensions_;
	}

	if(node.has_key("mouseover_area")) {
		mouse_over_area_ = rect(node["mouseover_area"]);
	}

	variant anim_list = node["animation"];
	if(anim_list.is_null() || anim_list.num_elements() == 0) {
		anim_list = json::parse_from_file_or_die("data/default-animation.cfg");
	}

	for(variant anim : anim_list.as_list()) {
		ffl::IntrusivePtr<Frame> f;
		try {
			f.reset(new Frame(anim));
		} catch(Frame::Error&) {
			ASSERT_LOG(false, "ERROR LOADING FRAME IN OBJECT '" << id_ << "'");
		}

		if(use_image_for_collisions_) {
			f->setImageAsSolid();
		}

		if(f->solid()) {
			has_solid_ = true;
		}

		frames_[anim["id"].as_string()].push_back(f);
		const int duplicates = anim["duplicates"].as_int();
		if(duplicates > 1) {
			for(int n = 1; n != duplicates; ++n) {
				frames_[anim["id"].as_string()].push_back(f);
			}
		}
		if(!defaultFrame_) {
			defaultFrame_ = f;
		}
	}

	ASSERT_LOG(defaultFrame_, "OBJECT " << id_ << " NO ANIMATIONS FOR OBJECT: " << node.write_json() << "'");

	std::vector<variant> available_frames;
	for(frame_map::const_iterator i = frames_.begin(); i != frames_.end(); ++i) {
		available_frames.push_back(variant(i->first));
	}

	available_frames_ = variant(&available_frames);

	mass_ = node["mass"].as_int(defaultFrame_->collideW() * defaultFrame_->collideH());
	
	for(variant child : node["child"].as_list()) {
		const std::string& child_id = child["child_id"].as_string();
		children_[child_id] = child;
	}

	assert(defaultFrame_);

	next_animation_formula_ = game_logic::Formula::createOptionalFormula(node["next_animation"], getFunctionSymbols());

	for(variant particle_node : node["particle_system"].as_list()) {
		particle_factories_[particle_node["id"].as_string()] = ParticleSystemFactory::create_factory(particle_node);
	}

	if(!is_variation && !is_recursive_call) {
		//only initialize sub objects up front if it's not a recursive call
		//doing it this way means that dependencies between sub objects and
		//parent objects won't result in infinite recursion.
		initSubObjects(node, old_type);
	}

	if(node.has_key("parallax_scale_x") || node.has_key("parallax_scale_y")) {
		parallax_scale_millis_.reset(new std::pair<int, int>(node["parallax_scale_x"].as_int(1000), node["parallax_scale_y"].as_int(1000)));
	}
	
	variant vars = node["vars"];
	if(vars.is_null() == false) {
		std::vector<std::string> var_str;
		for(variant key : vars.getKeys().as_list()) {
			variables_[key.as_string()] = vars[key];
			var_str.push_back(key.as_string());
		}

		if(!var_str. empty()) {
			game_logic::FormulaCallableDefinition::Entry* entry = callable_definition_->getEntry(CUSTOM_OBJECT_VARS);
			ASSERT_LOG(entry != nullptr, "CANNOT FIND VARS ENTRY IN OBJECT");
			game_logic::FormulaCallableDefinitionPtr def = game_logic::execute_command_callable_definition(&var_str[0], &var_str[0] + var_str.size());
			def->setStrict(is_strict_);
			entry->type_definition = def;
		}
	}

	variant tmp_vars = node["tmp"];
	if(tmp_vars.is_null() == false) {
		std::vector<std::string> var_str;
		for(variant key : tmp_vars.getKeys().as_list()) {
			tmp_variables_[key.as_string()] = tmp_vars[key];
			var_str.push_back(key.as_string());
		}

		if(!var_str.empty()) {
			game_logic::FormulaCallableDefinition::Entry* entry = callable_definition_->getEntry(CUSTOM_OBJECT_TMP);
			ASSERT_LOG(entry != nullptr, "CANNOT FIND TMP ENTRY IN OBJECT");
			game_logic::FormulaCallableDefinitionPtr def = game_logic::execute_command_callable_definition(&var_str[0], &var_str[0] + var_str.size());
			def->setStrict(is_strict_);
			entry->type_definition = def;
		}
	}

	consts_.reset(new game_logic::MapFormulaCallable);
	variant consts = node["consts"];
	if(consts.is_null() == false) {
		for(variant key : consts.getKeys().as_list()) {
			consts_->add(key.as_string(), consts[key]);
		}
	}

	if(node.has_key("tags")) {
		const std::vector<std::string> tags = util::split(node["tags"].as_string());
		for(const std::string& tag : tags) {
			tags_[tag] = variant(1);
		}
	}

	//START OF FIRST PARSE OF PROPERTIES.
	//Here we get the types of properties and parse them into
	//callable_definition_. While we're in our first parse we want to make
	//sure we do not have to query other CustomObjectType definitions,
	//because if we do we could end with infinite recursion.

	init_object_definition(node, id_, callable_definition_, slot_properties_base_, is_strict_);

	const types_cfg_scope types_scope(node["types"]);

	//END OF FIRST PARSE.
	//We've now constructed our definition of the object, and we can
	//safely query other object type definitions
	callable_definition_->setObjectType(variant_type::get_custom_object(id_));

	if(!is_variation && is_recursive_call) {
		//we initialize sub objects here if we are in a recursive call,
		//to make sure that it's after we've set our definition. This will
		//avoid infinite recursion.
		initSubObjects(node, old_type);
	}

	std::map<std::string, int> property_to_slot;

	int storage_slot = 0;

	for(variant properties_node : node["properties"].as_list()) {
		if(properties_node.is_string()) {
			continue;
		}

		const CustomObjectCallableExposePrivateScope expose_scope(*callable_definition_);
		for(variant key : properties_node.getKeys().as_list()) {
			const game_logic::Formula::StrictCheckScope strict_checking(is_strict_ || g_strict_mode_warnings, g_strict_mode_warnings);
			const std::string& k = key.as_string();
			bool dynamic_initialization = false;
			variant value = properties_node[key];
			PropertyEntry& entry = properties_[k];
			entry.id = k;
			if(value.is_string()) {
				entry.getter = game_logic::Formula::createOptionalFormula(value, getFunctionSymbols(), callable_definition_);
			} else if(value.is_map()) {
				if(value.has_key("type")) {
					entry.type = parse_variant_type(value["type"]);
					entry.set_type = entry.type;
				}

				if(value.has_key("set_type")) {
					entry.set_type = parse_variant_type(value["set_type"]);
				}

				game_logic::ConstFormulaCallableDefinitionPtr property_def = callable_definition_;
				if(entry.type) {
					property_def = modify_formula_callable_definition(property_def, CUSTOM_OBJECT_DATA, entry.type);
				}

				game_logic::ConstFormulaCallableDefinitionPtr setter_def = property_def;
				if(entry.set_type) {
					setter_def = modify_formula_callable_definition(setter_def, CUSTOM_OBJECT_VALUE, entry.set_type);
				}

				entry.getter = game_logic::Formula::createOptionalFormula(value["get"], getFunctionSymbols(), property_def);
				entry.setter = game_logic::Formula::createOptionalFormula(value["set"], getFunctionSymbols(), setter_def);
				entry.onchange = game_logic::Formula::createOptionalFormula(value["change"], getFunctionSymbols(), setter_def);
				if(value["init"].is_null() == false) {
					entry.init = game_logic::Formula::createOptionalFormula(value["init"], getFunctionSymbols(), callable_definition_);
					assert(entry.init);
					if(is_strict_) {
						assert(entry.type);
						ASSERT_LOG(variant_types_compatible(entry.type, entry.init->queryVariantType()), "Initializer for " << id_ << "." << k << " does not have a matching type. Evaluates to " << entry.init->queryVariantType()->to_string() << " expected " << entry.type->to_string());
					}
				}
				entry.default_value = value["default"];

				if(value["variable"].as_bool(true)) {
					entry.storage_slot = storage_slot++;
					entry.persistent = value["persistent"].as_bool(true);
					dynamic_initialization = value["dynamic_initialization"].as_bool(false);
				} else {
					entry.storage_slot = -1;
					entry.persistent = false;
				}

				ASSERT_LOG(!entry.init || entry.storage_slot != -1, "Property " << id_ << "." << k << " cannot have initializer since it's not a variable");



				if(value.has_key("editor_info")) {
					entry.has_editor_info = true;
					const game_logic::Formula::StrictCheckScope strict_checking(false);
					variant editor_info_var = value["editor_info"];

					if (editor_info_var.is_string() && editor_info_var.as_string() == "enum") {
						ASSERT_LOG(entry.type->is_type(variant::VARIANT_TYPE_ENUM), "Property " << id_ << " cannot be exposed as an enum to the editor since it's not an enum type.");
						variant_builder b;

						//TODO: finish up calculating the enum value.
						b.add("type", "enum");
						editor_info_var = b.build();
						value.add_attr_mutation(variant("editor_info"), editor_info_var);
					}

					static const variant name_key("name");
					editor_info_var = editor_info_var.add_attr(name_key, variant(k));
					EditorVariableInfo info(editor_info_var);
					info.setIsProperty();

					ASSERT_LOG(EditorInfo, "Object type " << id_ << " must have EditorInfo section since some of its properties have EditorInfo sections");

					EditorInfo->addProperty(info);
				}

				if(value["weak"].as_bool(false)) {
					entry.is_weak = true;
					ASSERT_LOG(entry.type && variant_type::may_be_null(entry.type), "Object " << id_ << " property " << entry.id << " is marked as weak but has a type which is not compatible with null");
				}

			} else {
				if(is_strict_) {
					entry.set_type = entry.type = get_variant_type_from_value(value);
				}

				if(entry.getter || entry.id[0] != '_') {
					entry.getter.reset();
					entry.const_value.reset(new variant(value));
				} else {
					entry.storage_slot = storage_slot++;
					entry.persistent = true;
					entry.default_value = value;
				}
			}

			if(entry.getter) {
				variant v;
				if(entry.getter->evaluatesToConstant(v)) {
					entry.getter.reset();
					entry.const_value.reset(new variant(v));
				}
			}

			entry.slot = static_cast<int>(slot_properties_.size());
			if(property_to_slot.count(k)) {
				entry.slot = property_to_slot[k];
			} else {
				property_to_slot[k] = entry.slot;
			}

			if(entry.init) {
				properties_with_init_.push_back(entry.slot);
			}

			if(entry.setter) {
				properties_with_setter_.push_back(entry.slot);
			}

			entry.requires_initialization = entry.storage_slot >= 0 && entry.type && !entry.type->match(entry.default_value) && !dynamic_initialization && !entry.init;
			if(entry.requires_initialization) {
				if(entry.setter) {
					ASSERT_LOG(last_initialization_property_ == "", "Object " << id_ << " has multiple properties which require initialization and which have custom setters. This isn't allowed because we wouldn't know which property to initialize first. Properties: " << last_initialization_property_ << ", " << entry.id);
					last_initialization_property_ = entry.id;
				}
				properties_requiring_initialization_.push_back(entry.slot);
			}

			if(dynamic_initialization) {
				properties_requiring_dynamic_initialization_.push_back(entry.slot);
			}

			if(entry.slot == slot_properties_.size()) {
				slot_properties_.push_back(entry);
			} else {
				assert(entry.slot >= 0 && static_cast<unsigned>(entry.slot) < slot_properties_.size());
				slot_properties_[entry.slot] = entry;
			}
		}
	}

	for(const PropertyEntry& entry  : slot_properties_) {
		if(entry.storage_slot >= 0) {
			if(static_cast<int>(variable_properties_.size()) < entry.storage_slot+1) {
				variable_properties_.resize(entry.storage_slot+1);
			}

			variable_properties_[entry.storage_slot] = &entry;
		}
	}

	variant variations = node["variations"];
	if(variations.is_null() == false) {
		for(const variant_pair& v : variations.as_map()) {
			variations_[v.first.as_string()] = game_logic::Formula::createOptionalFormula(v.second, &get_custom_object_functions_symbol_table());
		}
		
		node_ = node;
	}

	game_logic::register_formula_callable_definition("object_type", callable_definition_);

#ifdef USE_BOX2D
	if(node.has_key("body")) {
		body_.reset(new box2d::body(node["body"]));
	}
#endif

#if defined(USE_LUA)
	if(node.has_key("lua")) {
		lua_node_ = node["lua"];
	}
#endif

	if(node.has_key("shader")) {
		if(node["shader"].is_string()) {
			shader_ = graphics::AnuraShaderPtr(new graphics::AnuraShader(node["shader"].as_string()));
		} else {
			variant shader_info = node["shader"];

			if(shader_info.has_key("flags")) {
				variant flags = shader_info["flags"];
				for(variant f : flags.as_list()) {
					std::string prop = f.as_string();
					auto it = properties().find(prop);
					ASSERT_LOG(it != properties().end(), "Shader flag for " << id_ << " has invalid property: " << prop);
					shader_flags_.push_back(&it->second);
					ASSERT_LOG(shader_flags_.size() <= MAX_CUSTOM_OBJECT_SHADER_FLAGS, "Object " << id_ << " has too many shader flags. Only " << MAX_CUSTOM_OBJECT_SHADER_FLAGS << " allowed");
				}

				shader_node_ = shader_info;
			}

			const std::string shader_name = write_uuid(generate_uuid());
			shader_info = shader_info.add_attr(variant("name"), variant(shader_name));

			if(shader_info.has_key("fragment")) {

				if(shader_info.has_key("vertex") == false) {

					static variant DefaultVertexShader(DefaultVertexShaderStr);
					shader_info = shader_info.add_attr(variant("vertex"), DefaultVertexShader);
				}
			}
			
			KRE::ShaderProgram::loadFromVariant(shader_info);
			shader_ = graphics::AnuraShaderPtr(new graphics::AnuraShader(shader_name));

		}
		//LOG_DEBUG("Added shader '" << shader_->getName() << "' for CustomObjectType '" << id_ << "'");
	}

	if(node.has_key("effects")) {
		if(node["effects"].is_list()) {
			for(int n = 0; n != node["effects"].num_elements(); ++n) {
				const variant& eff = node["effects"][n];
				if(eff.is_string()) {
					effects_shaders_.emplace_back(new graphics::AnuraShader(eff.as_string()));
				} else {
					effects_shaders_.emplace_back(new graphics::AnuraShader(eff["name"].as_string(), eff));
				}
			}
		} else if(node["effects"].is_string()) {
			effects_shaders_.emplace_back(new graphics::AnuraShader(node["effects"].as_string()));
		} else if(node["effects"].is_map()) {
			effects_shaders_.emplace_back(new graphics::AnuraShader(node["effects"]["name"].as_string(), node["effects"]));
		}
	}

	if(node.has_key("xhtml")) {
		document_.reset(new xhtml::DocumentObject(node));
	}

	if(base_type) {
		//if we're a variation, just get the functions from our base type.
		//variations can't define new functions.
		object_functions_ = base_type->object_functions_;
	} else if(node.has_key("functions")) {
		object_functions_.reset(new game_logic::FunctionSymbolTable);
		object_functions_->setBackup(&get_custom_object_functions_symbol_table());
		const variant fn = node["functions"];
		if(fn.is_string()) {
			game_logic::Formula f(fn, object_functions_.get());
		} else if(fn.is_list()) {
			for(int n = 0; n != fn.num_elements(); ++n) {
				game_logic::Formula f(fn[n], object_functions_.get());
			}
		}
	}
	initEventHandlers(node, event_handlers_, getFunctionSymbols(), base_type ? &base_type->event_handlers_ : nullptr);

	for(const std::string& t : types_spawned.spawned) {
		if(std::find(preload_objects_.begin(), preload_objects_.end(), t) == preload_objects_.end()) {
			preload_objects_.push_back(t);
		}
	}
}

CustomObjectType::~CustomObjectType()
{
}

#ifdef USE_LUA
const std::shared_ptr<lua::CompiledChunk> & CustomObjectType::getLuaInit(lua::LuaContext & ctx) const {
	if (lua_compiled_) {
		return lua_compiled_;
	}
	if (lua_node_.has_key("init")) {
		lua_compiled_.reset(ctx.compileChunk(lua_node_.has_key("debug_name") ? lua_node_["debug_name"].as_string() : ("custom object " + id() + " lua"), lua_node_["init"].as_string()));
	}
	return lua_compiled_;
}
#endif

namespace 
{
	struct StackScope {
		StackScope(std::vector<std::string>* stack, const std::string& item)
		  : stack_(stack)
		{
			stack_->push_back(item);
		}

		~StackScope() {
			stack_->pop_back();
		}

		std::vector<std::string>* stack_;
	};
}

void CustomObjectType::initSubObjects(variant node, const CustomObjectType* old_type)
{
	static std::vector<std::string> init_stack;
	for(variant object_node : node["object_type"].as_list()) {
		variant merged = mergePrototype(object_node);
		std::string sub_key = object_node["id"].as_string();

		const std::string init_key = id_ + "." + sub_key;
		if(std::count(init_stack.begin(), init_stack.end(), init_key)) {
			continue;
		}

		StackScope scope(&init_stack, init_key);

		if(old_type && old_type->sub_objects_.count(sub_key) &&
		   old_type->sub_objects_.find(sub_key)->second->node_ == merged) {
			//We are recreating this object, and the sub object node
			//hasn't changed at all, so just reuse the same sub object.
			sub_objects_[sub_key] = old_type->sub_objects_.find(sub_key)->second;
		} else {
			CustomObjectType* type = new CustomObjectType(id_ + "." + merged["id"].as_string(), merged);
			if(old_type && type->node_.is_null()){
				type->node_ = merged;
			}
			//LOG_DEBUG("MERGED PROTOTYPE FOR " << type->id_ << ": " << merged.write_json());
			sub_objects_[sub_key].reset(type);
		}
	}
}

const Frame& CustomObjectType::defaultFrame() const
{
	return *defaultFrame_;
}

const Frame& CustomObjectType::getFrame(const std::string& key) const
{
	frame_map::const_iterator itor = frames_.find(key);
	if(itor == frames_.end() || itor->second.empty()) {
		if(key != "normal") {
		ASSERT_LOG(key == "normal", "UNKNOWN ANIMATION FRAME " << key << " IN " << id_);
		}
		return defaultFrame();
	} else {
		if(itor->second.size() == 1) {
			return *itor->second.front().get();
		} else {
			return *itor->second[rand()%itor->second.size()].get();
		}
	}
}

bool CustomObjectType::hasFrame(const std::string& key) const
{
	return frames_.count(key) != 0;
}

game_logic::ConstFormulaPtr CustomObjectType::getEventHandler(int event) const
{
	if(static_cast<unsigned>(event) >= event_handlers_.size()) {
		return game_logic::ConstFormulaPtr();
	} else {
		return event_handlers_[event];
	}
}

ConstParticleSystemFactoryPtr CustomObjectType::getParticleSystemFactory(const std::string& id) const
{
	std::map<std::string, ConstParticleSystemFactoryPtr>::const_iterator i = particle_factories_.find(id);
	ASSERT_LOG(i != particle_factories_.end(), "Unknown particle system type in " << id_ << ": " << id);
	return i->second;
}

game_logic::FunctionSymbolTable* CustomObjectType::getFunctionSymbols() const
{
	if(object_functions_) {
		return object_functions_.get();
	} else {
		return &get_custom_object_functions_symbol_table();
	}
}

namespace 
{
	void execute_variation_command(variant cmd, game_logic::FormulaCallable& obj)
	{
		if(cmd.is_list()) {
			for(variant c : cmd.as_list()) {
				execute_variation_command(c, obj);
			}
		} else if(cmd.try_convert<game_logic::CommandCallable>()) {
			cmd.try_convert<game_logic::CommandCallable>()->runCommand(obj);
		}
	}
}

ConstCustomObjectTypePtr CustomObjectType::getVariation(const std::vector<std::string>& variations) const
{
	ASSERT_LOG(node_.is_null() == false, "tried to set variation in object " << id_ << " which has no variations");

	ConstCustomObjectTypePtr& result = variations_cache_[variations];
	if(!result) {
		variant node = node_;

		ffl::IntrusivePtr<game_logic::MapFormulaCallable> callable(new game_logic::MapFormulaCallable);
		callable->add("doc", variant(variant_callable::create(&node)));

		for(const std::string& v : variations) {
			std::map<std::string, game_logic::ConstFormulaPtr>::const_iterator var_itor = variations_.find(v);
			ASSERT_LOG(var_itor != variations_.end(), "COULD NOT FIND VARIATION " << v << " IN " << id_);

			variant cmd = var_itor->second->execute(*callable);

			execute_variation_command(cmd, *callable);

			//LOG_DEBUG("VARIATION " << v << ":\n--- BEFORE ---\n" << node.write_json() << "\n--- AFTER ---\n" << node_.write_json() << "\n--- DONE ---");
		}

		//set our constants so the variation can decide whether it needs
		//to re-parse formulas or not.
		const game_logic::ConstantsLoader scope_consts(node_["consts"]);

		//copy the id over from the parent object, to make sure it's
		//the same. This is important for nested objects.
		CustomObjectType* obj_type = new CustomObjectType(id_, node, this);
		result.reset(obj_type);
	}

	return result;
}

void CustomObjectType::loadVariations() const
{
	if(node_.is_null() || variations_.empty() || !node_.has_key("load_variations")) {
		return;
	}

	const std::vector<std::string> variations_to_load = util::split(node_["load_variations"].as_string());
	for(const std::string& v : variations_to_load) {
		getVariation(std::vector<std::string>(1, v));
	}
}

graphics::AnuraShaderPtr CustomObjectType::getShaderWithParms(unsigned int flags) const
{
	if(flags == 0) {
		return shader_;
	}

	if(flags >= shader_variants_.size()) {
		shader_variants_.resize(flags+1);
	}

	if(shader_variants_[flags].get() == nullptr) {
		variant shader_info = shader_node_;

		const std::string shader_name = write_uuid(generate_uuid());
		shader_info = shader_info.add_attr(variant("name"), variant(shader_name));

		std::string frag = shader_info["fragment"].as_string();
		std::string header = "";
		for(unsigned int n = 0; n < shader_flags_.size(); ++n) {
			if(flags&(1 << n)) {
				header += "#define FLAG_" + shader_flags_[n]->id + "\n";
			}
		}

		frag = header + frag;

		shader_info = shader_info.add_attr(variant("fragment"), variant(frag));

		if(shader_info.has_key("vertex") == false) {
			static variant DefaultVertexShader(DefaultVertexShaderStr);
			shader_info = shader_info.add_attr(variant("vertex"), DefaultVertexShader);
		}
	
		KRE::ShaderProgram::loadFromVariant(shader_info);
		shader_variants_[flags] = graphics::AnuraShaderPtr(new graphics::AnuraShader(shader_name));
	}

	return shader_variants_[flags];
}

using namespace game_logic;

class CustomObjectInterface : public game_logic::FormulaCallable
{
public:
private:
	DECLARE_CALLABLE(CustomObjectInterface);
};

BEGIN_DEFINE_CALLABLE_NOBASE(CustomObjectInterface)
DEFINE_FIELD(objects, "[{name: string, refcount: int}]")
	std::vector<variant> res;
	for(const auto& p : cache()) {
		variant_builder b;
		b.add("name", p.first);
		b.add("refcount", static_cast<int>(p.second.use_count()));
		res.push_back(b.build());
	}
	return variant(&res);

BEGIN_DEFINE_FN(preload, "(string) ->commands")
	std::string str(FN_ARG(0).as_string());
	return variant(new game_logic::FnCommandCallable("object::preload", [str]() {
		CustomObjectType::get(str);
	}));
END_DEFINE_FN

BEGIN_DEFINE_FN(unload, "(string) ->commands")
	std::string str(FN_ARG(0).as_string());
	return variant(new game_logic::FnCommandCallable("object::unload", [str]() {
		cache().erase(str);
	}));
END_DEFINE_FN

END_DEFINE_CALLABLE(CustomObjectInterface)

const std::string FunctionModule = "core";

FUNCTION_DEF(anura_objects, 0, 0, "anura_objects()")
	return variant(new CustomObjectInterface);
RETURN_TYPE("builtin custom_object_interface")
END_FUNCTION_DEF(anura_objects)

#include "Texture.hpp"

BENCHMARK(CustomObjectTypeLoad)
{
	static std::map<std::string,std::string> file_paths;
	if(file_paths.empty()) {
		module::get_unique_filenames_under_dir("data/objects", &file_paths);
	}

	BENCHMARK_LOOP {
		for(std::map<std::string,std::string>::const_iterator i = file_paths.begin(); i != file_paths.end(); ++i) {
			if(i->first.size() > 4 && std::equal(i->first.end()-4, i->first.end(), ".cfg")) {
				CustomObjectType::create(std::string(i->first.begin(), i->first.end()-4));
			}
		}
		KRE::Surface::clearSurfaceCache();
		KRE::Texture::clearTextures();		
	}
}


BENCHMARK(CustomObjectTypeFrogattoLoad)
{
	BENCHMARK_LOOP {
		CustomObjectType::create("frogatto_playable");
		KRE::Surface::clearSurfaceCache();
		KRE::Texture::clearTextures();		
	}
}

UTILITY(object_definition)
{
	for(const std::string& arg : args) {
		ConstCustomObjectTypePtr obj = CustomObjectType::get(arg);
		ASSERT_LOG(obj.get() != nullptr, "NO OBJECT FOUND: " << arg);

		auto dot = std::find(arg.begin(), arg.end(), '.');

		std::string baseobj = arg;
		if(dot != arg.end()) {
			baseobj = std::string(arg.begin(), dot);
		}
		
		const std::string* fname = CustomObjectType::getObjectPath(baseobj + ".cfg");

		variant json = json::parse_from_file_or_die(*fname);
		if(dot != arg.end()) {
			std::string subtype(dot+1, arg.end());
			variant items = json[variant("object_type")];
			int n = 0;
			for(; n != items.num_elements(); ++n) {
				if(items[n][variant("id")].as_string() == subtype) {
					break;
				}
			}

			ASSERT_LOG(n != items.num_elements(), "Could not find object " << arg);

			json = items[n];
		}

		const variant node = CustomObjectType::mergePrototype(json);

		std::cout << "OBJECT " << arg << "\n---\n" << node.write_json(true) << "\n---\n";
	}
}

UTILITY(test_all_objects)
{
	CustomObjectType::getAll();
}
