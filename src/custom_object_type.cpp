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
#include <cassert>
#include <iostream>

#include <boost/bind.hpp>

#include "asserts.hpp"
#include "code_editor_dialog.hpp"
#include "collision_utils.hpp"
#include "custom_object.hpp"
#include "custom_object_callable.hpp"
#include "custom_object_functions.hpp"
#include "custom_object_type.hpp"
#include "filesystem.hpp"
#include "formula.hpp"
#include "formula_constants.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "load_level.hpp"
#include "module.hpp"
#include "object_events.hpp"
#include "preferences.hpp"
#include "solid_map.hpp"
#include "sound.hpp"
#include "string_utils.hpp"
#include "surface_cache.hpp"
#include "unit_test.hpp"
#include "variant_callable.hpp"
#include "variant_utils.hpp"

using game_logic::formula_callable_definition;
using game_logic::formula_callable_definition_ptr;

std::map<std::string, std::string>& prototype_file_paths() {
	static std::map<std::string, std::string> paths;
	return paths;
}

namespace {

PREF_BOOL(strict_mode_warnings, false, "If turned on, all objects will be run in strict mode, with errors non-fatal");
PREF_BOOL(suppress_strict_mode, false, "If turned on, turns off strict mode checking on all objects");
PREF_BOOL(force_strict_mode, false, "If turned on, turns on strict mode checking on all objects");

bool custom_object_strict_mode = false;
class strict_mode_scope {
	bool old_value_;
public:
	strict_mode_scope() : old_value_(custom_object_strict_mode) {
		custom_object_strict_mode = true;
	}

	~strict_mode_scope() {
		custom_object_strict_mode = old_value_;
	}
};

std::map<std::string, std::string>& object_file_paths() {
	static std::map<std::string, std::string> paths;
	return paths;
}

const std::string& object_file_path() {
	if(preferences::load_compiled()) {
		static const std::string value =  "data/compiled/objects";
		return value;
	} else {
		static const std::string value =  "data/objects";
		return value;
	}
}

void load_file_paths() {

	//find out the paths to all our files
	module::get_unique_filenames_under_dir(object_file_path(), &object_file_paths());
	module::get_unique_filenames_under_dir("data/object_prototypes", &::prototype_file_paths());
}

typedef std::map<std::string, const_custom_object_type_ptr> object_map;

object_map& cache() {
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
	foreach(variant anim, node["animation"].as_list()) {
		variant id = anim["id"];
		animations_seen.insert(id.as_string());
		variant proto_anim;
		foreach(variant candidate, prototype_node["animation"].as_list()) {
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
	foreach(variant anim, prototype_node["animation"].as_list()) {
		if(animations_seen.count(anim["id"].as_string()) == 0) {
			animations.push_back(anim);
		}
	}

	foreach(variant key, prototype_node.get_keys().as_list()) {
		result[key] = prototype_node[key];
	}

	foreach(variant key, node.get_keys().as_list()) {
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
					value.set_debug_info(*info);
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
		foreach(variant v, proto_fn.as_list()) {
			functions.push_back(v);
		}
	}

	variant fn = node["functions"];
	if(fn.is_string()) {
		functions.push_back(fn);
	} else if(fn.is_list()) {
		foreach(variant v, fn.as_list()) {
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

	const variant editor_info_a = prototype_node["editor_info"];
	const variant editor_info_b = node["editor_info"];
	result[variant("editor_info")] = editor_info_a + editor_info_b;
	if(editor_info_a.is_map() && editor_info_b.is_map() &&
	   editor_info_a["var"].is_list() && editor_info_b["var"].is_list()) {
		std::map<variant, variant> vars_map;
		std::vector<variant> items = editor_info_a["var"].as_list();
		std::vector<variant> items2 = editor_info_b["var"].as_list();
		items.insert(items.end(), items2.begin(), items2.end());
		foreach(const variant& v, items) {
			variant name = v["name"];
			variant enum_value;
			if(vars_map.count(name)) {
				if(vars_map[name]["enum_values"].is_list() && v["enum_values"].is_list()) {
					std::vector<variant> e = vars_map[name]["enum_values"].as_list();
					foreach(variant item, v["enum_values"].as_list()) {
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
		res.set_debug_info(*node.get_debug_info());
	}

	return res;
}

std::map<std::string, std::string>& object_type_inheritance()
{
	static std::map<std::string, std::string> instance;
	return instance;
}

std::map<std::string, formula_callable_definition_ptr>& object_type_definitions()
{
	static std::map<std::string, formula_callable_definition_ptr>* instance = new std::map<std::string, formula_callable_definition_ptr>;
	return *instance;
}

}

bool custom_object_type::is_derived_from(const std::string& base, const std::string& derived)
{
	if(derived == base) {
		return true;
	}

	auto itor = object_type_inheritance().find(derived);
	if(itor == object_type_inheritance().end()) {
		return false;
	}

	assert(itor->second != derived);

	return is_derived_from(base, itor->second);
}

namespace {

void init_object_definition(variant node, const std::string& id_, custom_object_callable_ptr callable_definition_, int& slot_properties_base_, bool is_strict_)
{
	object_type_definitions()[id_] = callable_definition_;

	const types_cfg_scope types_scope(node["types"]);

	std::set<std::string> properties_to_infer;

	std::map<std::string, bool> property_overridable_state;
	std::map<std::string, variant_type_ptr> property_override_type;

	std::map<std::string, custom_object_callable_ptr> proto_definitions;
	std::string prototype_derived_from;

	slot_properties_base_ = callable_definition_->num_slots();
	foreach(variant properties_node, node["properties"].as_list()) {
		if(properties_node.is_string()) {
			if(prototype_derived_from != "") {
				assert(properties_node.as_string() != prototype_derived_from);
				object_type_inheritance()[properties_node.as_string()] = prototype_derived_from;
			}
			prototype_derived_from = properties_node.as_string();

			if(object_type_definitions().count(properties_node.as_string())) {
				continue;
			}

			proto_definitions[properties_node.as_string()].reset(new custom_object_callable(*callable_definition_));
			continue;
		}

		foreach(variant key, properties_node.get_keys().as_list()) {
			const std::string& k = key.as_string();
			ASSERT_LOG(k.empty() == false, "property is empty");
			ASSERT_LOG(properties_to_infer.count(k) == 0, "Object " << id_ << " overrides property " << k << " which is defined with no type definition in a prototype. If you want to override a property in a prototype that property must have a type definition in the prototype");
			bool is_private = is_strict_ && k[0] == '_';
			ASSERT_LOG(custom_object_callable::instance().get_slot(k) == -1, "Custom object property " << id_ << "." << k << " has the same name as a builtin");

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
					type->is_function(NULL, NULL, NULL, &return_type_specified);
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

				if(is_strict_ || property_override_type[k] && type) {
					ASSERT_LOG(variant_types_compatible(property_override_type[k], type), "Type mis-match for object property " << id_ << "." << k << " has a different type than the definition in the prototype type: " << type->to_string() << " prototype defines as " << property_override_type[k]->to_string());
				}
			}

			property_override_type[k] = type;

			if(is_strict_) {
				int current_slot = callable_definition_->get_slot(k);
				if(current_slot != -1) {
					const game_logic::formula_callable_definition::entry* entry = callable_definition_->get_entry(current_slot);
					ASSERT_LOG(!entry->variant_type || variant_types_compatible(entry->variant_type, type), "Type mis-match for object property " << id_ << "." << k << " has a different type than the definition in the prototype: " << type->to_string() << " prototype defines as " << entry->variant_type->to_string());
					ASSERT_LOG(!set_type || set_type->is_none() == entry->get_write_type()->is_none(), "Object property " << id_ << "." << k << " is immutable in the " << (set_type->is_none() ? "object" : "prototype") << " but not in the " << (set_type->is_none() ? "prototype" : "object"));
					ASSERT_LOG(!set_type || set_type->is_none() && entry->get_write_type()->is_none() || variant_types_compatible(entry->get_write_type(), set_type), "Type mis-match for object property " << id_ << "." << k << " has a different mutable type than the definition in the prototype. The property can be mutated with a " << set_type->to_string() << " while prototype allows mutation as " << entry->get_write_type()->to_string());
				}
			}

			callable_definition_->add_property(k, type, set_type, requires_initialization, is_private);
		}
	}

	for(auto p : proto_definitions) {
		object_type_definitions()[p.first] = p.second;
	}

	object_type_definitions()[id_] = callable_definition_;

	while(is_strict_ && !properties_to_infer.empty()) {
		const int num_items = properties_to_infer.size();
		foreach(variant properties_node, node["properties"].as_list()) {
			if(properties_node.is_string()) {
				continue;
			}

			foreach(variant key, properties_node.get_keys().as_list()) {
				const std::string& k = key.as_string();
				if(properties_to_infer.count(k) == 0) {
					continue;
				}

				variant value = properties_node[key];
				assert(value.is_string());

				for(int n = 0; n != callable_definition_->num_slots(); ++n) {
					callable_definition_->get_entry(n)->access_count = 0;
				}

				game_logic::formula_ptr f = game_logic::formula::create_optional_formula(value, &get_custom_object_functions_symbol_table(), callable_definition_);
				bool inferred = true;
				for(int n = 0; n != callable_definition_->num_slots(); ++n) {
					const game_logic::formula_callable_definition::entry* entry = callable_definition_->get_entry(n);
					if(entry->access_count) {
						if(properties_to_infer.count(entry->id)) {
							inferred = false;
						}
					}
				}

				if(inferred) {
					formula_callable_definition::entry* entry = callable_definition_->get_entry_by_id(k);
					assert(entry);
					entry->variant_type = f->query_variant_type();
					properties_to_infer.erase(k);
				}
			}
		}

		if(num_items == properties_to_infer.size()) {
			std::ostringstream s;
			foreach(const std::string& k, properties_to_infer) {
				s << k << ", ";
			}
			ASSERT_LOG(false, "Could not infer properties in object " << id_ << ": " << s.str());
		}
	}

	if(prototype_derived_from != "") {
		ASSERT_LOG(id_ != prototype_derived_from, "Object " << id_ << " derives from itself");
		object_type_inheritance()[id_] = prototype_derived_from;
	}

	callable_definition_->finalize_properties();
	callable_definition_->set_strict(is_strict_);
}

variant g_player_type_str;
}

void custom_object_type::set_player_variant_type(variant type_str)
{
	g_player_type_str = type_str;
}

formula_callable_definition_ptr custom_object_type::get_definition(const std::string& id)
{
	std::map<std::string, formula_callable_definition_ptr>::const_iterator itor = object_type_definitions().find(id);
	if(itor != object_type_definitions().end()) {
		return itor->second;
	} else {
		if(object_file_path().empty()) {
			load_file_paths();
		}

		auto proto_path = module::find(prototype_file_paths(), id + ".cfg");
		if(proto_path != prototype_file_paths().end()) {
			ASSERT_LOG(get_object_path(id) == NULL, "Object " << id << " has a prototype with the same name. Objects and prototypes must have different names");
			variant node = merge_prototype(json::parse_from_file(proto_path->second));
			custom_object_callable_ptr callable_definition(new custom_object_callable);
			callable_definition->set_type_name("obj " + id);
			int slot = -1;
			init_object_definition(node, node["id"].as_string(), callable_definition, slot, !g_suppress_strict_mode && node["is_strict"].as_bool(custom_object_strict_mode) || g_force_strict_mode);
			std::map<std::string, formula_callable_definition_ptr>::const_iterator itor = object_type_definitions().find(id);
			ASSERT_LOG(itor != object_type_definitions().end(), "Could not load object prototype definition " << id);
			return itor->second;
		}

		std::string::const_iterator dot_itor = std::find(id.begin(), id.end(), '.');
		std::string obj_id(id.begin(), dot_itor);

		const std::string* path = get_object_path(obj_id + ".cfg");
		ASSERT_LOG(path != NULL, "No definition for object " << id);

		std::map<std::string, variant> nodes;

		variant node = merge_prototype(json::parse_from_file(*path));
		nodes[obj_id] = node;
		if(node["object_type"].is_list() || node["object_type"].is_map()) {
			for(variant sub_node : node["object_type"].as_list()) {
				const std::string sub_id = obj_id + "." + sub_node["id"].as_string();
				ASSERT_LOG(nodes.count(sub_id) == 0, "Duplicate object: " << sub_id);
				nodes[sub_id] = merge_prototype(sub_node);
			}
		}

		for(auto p : nodes) {
			if(object_type_definitions().count(p.first)) {
				continue;
			}

			custom_object_callable_ptr callable_definition(new custom_object_callable);
			callable_definition->set_type_name("obj " + p.first);
			int slot = -1;
			init_object_definition(p.second, p.first, callable_definition, slot, !g_suppress_strict_mode && p.second["is_strict"].as_bool(custom_object_strict_mode) || g_force_strict_mode);
		}

		itor = object_type_definitions().find(id);
		ASSERT_LOG(itor != object_type_definitions().end(), "No definition for object " << id);
		return itor->second;
	}
}

void custom_object_type::reload_file_paths() 
{
	invalidate_all_objects();
	load_file_paths();
}


//function which finds if a node has a prototype, and if so, applies the
//prototype to the node.
variant custom_object_type::merge_prototype(variant node, std::vector<std::string>* proto_paths)
{
	if(!node.has_key("prototype")) {
		return node;
	}

	std::vector<std::string> protos = node["prototype"].as_list_string();
	if(protos.size() > 1) {
		std::cerr << "WARNING: Multiple inheritance of objects is deprecated: " << node["prototype"].debug_location() << "\n";
	}

	foreach(const std::string& proto, protos) {
		//look up the object's prototype and merge it in
		std::map<std::string, std::string>::const_iterator path_itor = module::find(::prototype_file_paths(), proto + ".cfg");
		ASSERT_LOG(path_itor != ::prototype_file_paths().end(), "Could not find file for prototype '" << proto << "'");

		variant prototype_node = json::parse_from_file(path_itor->second);
		ASSERT_LOG(prototype_node["id"].as_string() == proto, "PROTOTYPE NODE FOR " << proto << " DOES NOT SPECIFY AN ACCURATE id FIELD");
		if(proto_paths) {
			proto_paths->push_back(path_itor->second);
		}
		prototype_node = merge_prototype(prototype_node, proto_paths);
		node = merge_into_prototype(prototype_node, node);
	}
	return node;
}

const std::string* custom_object_type::get_object_path(const std::string& id)
{
	if(object_file_paths().empty()) {
		load_file_paths();
	}

	std::map<std::string, std::string>::const_iterator itor = module::find(object_file_paths(), id);
	if(itor == object_file_paths().end()) {
		return NULL;
	}

	return &itor->second;
}

const_custom_object_type_ptr custom_object_type::get(const std::string& id)
{
	std::string::const_iterator dot_itor = std::find(id.begin(), id.end(), '.');
	if(dot_itor != id.end()) {
		const_custom_object_type_ptr parent = get(std::string(id.begin(), dot_itor));
		if(!parent) {
			return const_custom_object_type_ptr();
		}

		return parent->get_sub_object(std::string(dot_itor+1, id.end()));
	}

	object_map::const_iterator itor = cache().find(module::get_id(id));
	if(itor != cache().end()) {
		return itor->second;
	}

	const_custom_object_type_ptr result(create(id));
	cache()[module::get_id(id)] = result;

	//load the object's variations here to avoid pausing the game
	//when an object starts its variation.
	result->load_variations();

	return result;
}

const_custom_object_type_ptr custom_object_type::get_or_die(const std::string& id)
{
	const const_custom_object_type_ptr res = get(id);
	ASSERT_LOG(res.get() != NULL, "UNRECOGNIZED OBJECT TYPE: '" << id << "'");

	return res;
}

const_custom_object_type_ptr custom_object_type::get_sub_object(const std::string& id) const
{
	std::map<std::string, const_custom_object_type_ptr>::const_iterator itor = sub_objects_.find(id);
	if(itor != sub_objects_.end()) {
		return itor->second;
	} else {
		return const_custom_object_type_ptr();
	}
}

custom_object_type_ptr custom_object_type::create(const std::string& id)
{
	return recreate(id, NULL);
}

namespace {
std::map<std::string, std::vector<std::string> > object_prototype_paths;
}

custom_object_type_ptr custom_object_type::recreate(const std::string& id,
                                             const custom_object_type* old_type)
{
	if(object_file_paths().empty()) {
		load_file_paths();
	}

	//find the file for the object we are loading.
	std::map<std::string, std::string>::const_iterator path_itor = module::find(object_file_paths(), id + ".cfg");
	ASSERT_LOG(path_itor != object_file_paths().end(), "Could not find file for object '" << id << "'");

	auto proto_path = module::find(prototype_file_paths(), id + ".cfg");
	ASSERT_LOG(proto_path == prototype_file_paths().end(), "Object " << id << " has a prototype with the same name. Objects and prototypes must have distinct names");

	try {
		std::vector<std::string> proto_paths;
		variant node = merge_prototype(json::parse_from_file(path_itor->second), &proto_paths);

		ASSERT_LOG(node["id"].as_string() == module::get_id(id), "IN " << path_itor->second << " OBJECT ID DOES NOT MATCH FILENAME");
		
		try {
			boost::scoped_ptr<assert_recover_scope> recover_scope;
			if(preferences::edit_and_continue()) {
				recover_scope.reset(new assert_recover_scope);
			}

			//create the object
			custom_object_type_ptr result(new custom_object_type(node["id"].as_string(), node, NULL, old_type));
			object_prototype_paths[id] = proto_paths;

			return result;
		} catch(validation_failure_exception& e) {
			static bool in_edit_and_continue = false;
			if(in_edit_and_continue) {
				throw e;
			}

			in_edit_and_continue = true;
			edit_and_continue_fn(path_itor->second, e.msg, boost::bind(&custom_object_type::recreate, id, old_type));
			in_edit_and_continue = false;
			return recreate(id, old_type);
		}

	} catch(json::parse_error& e) {
		ASSERT_LOG(false, "Error parsing FML for custom object '" << id << "' in '" << path_itor->second << "': '" << e.error_message() << "'");
	} catch(graphics::load_image_error&) {
		ASSERT_LOG(false, "Error loading object '" << id << "': could not load needed image");
	}
	// We never get here, but this stops a compiler warning.
	return custom_object_type_ptr();
}

void custom_object_type::invalidate_object(const std::string& id)
{
	cache().erase(module::get_id(id));
}

void custom_object_type::invalidate_all_objects()
{
	cache().clear();
	object_file_paths().clear();
	::prototype_file_paths().clear();
}

std::vector<std::string> custom_object_type::get_all_ids()
{
	std::vector<std::string> res;
	std::map<std::string, std::string> file_paths;
	module::get_unique_filenames_under_dir(object_file_path(), &file_paths);
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

std::map<std::string,custom_object_type::EditorSummary> custom_object_type::get_editor_categories()
{
	const std::string path = std::string(preferences::user_data_path()) + "/editor_cache.cfg";
	variant cache, proto_cache;
	if(sys::file_exists(path)) {
		try {
			cache = json::parse(sys::read_file(path), json::JSON_NO_PREPROCESSOR);
			proto_cache = cache["prototype_info"];
		} catch(...) {
		}
	}

	std::map<std::string, bool> proto_status;

	std::map<variant, variant> items, proto_info;
	foreach(const std::string& id, get_all_ids()) {
		variant info;
		const std::string* path = get_object_path(id + ".cfg");
		if(path == NULL) {
			fprintf(stderr, "NO FILE FOR OBJECT '%s'\n", id.c_str());
		}

		const int mod_time = static_cast<int>(sys::file_mod_time(*path));
		if(cache.is_map() && cache.has_key(id) && cache[id]["mod"].as_int() == mod_time) {
			info = cache[id];
			foreach(const std::string& p, info["prototype_paths"].as_list_string()) {
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
			variant node = merge_prototype(json::parse_from_file(*path), &proto_paths);
			std::map<variant,variant> summary;
			summary[variant("mod")] = variant(mod_time);
			std::vector<variant> proto_paths_v;
			foreach(const std::string& s, proto_paths) {
				proto_paths_v.push_back(variant(s));
			}

			summary[variant("prototype_paths")] = variant(&proto_paths_v);

			if(node["animation"].is_list()) {
				summary[variant("animation")] = node["animation"][0];
			} else if(node["animation"].is_map()) {
				summary[variant("animation")] = node["animation"];
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
	for(std::map<variant,variant>::const_iterator i = items.begin(); i != items.end(); ++i) {
		if(i->second.has_key("category")) {
			EditorSummary& summary = m[i->first.as_string()];
			summary.category = i->second["category"].as_string();
			if(i->second["help"].is_string()) {
				summary.help = i->second["help"].as_string();
			}
			summary.first_frame = i->second["animation"];
		}
	}

	items[variant("prototype_info")] = variant(&proto_info);

	const variant result = variant(&items);
	sys::write_file(path, result.write_json());

	return m;
}

std::vector<const_custom_object_type_ptr> custom_object_type::get_all()
{
	std::vector<const_custom_object_type_ptr> res;
	foreach(const std::string& id, get_all_ids()) {
		res.push_back(get(id));
	}

	return res;
}

#ifndef NO_EDITOR
namespace {
std::set<std::string> listening_for_files, files_updated;

void on_object_file_updated(std::string path)
{
	files_updated.insert(path);
}
}

int custom_object_type::reload_modified_code()
{
	static int prev_nitems = 0;
	const int nitems = cache().size();
	if(prev_nitems == nitems && files_updated.empty()) {
		return 0;
	}

	prev_nitems = nitems;

	std::set<std::string> error_paths;

	int result = 0;
	for(object_map::iterator i = cache().begin(); i != cache().end(); ++i) {

		const std::string* path = get_object_path(i->first + ".cfg");

		if(!path) {
			continue;
		}

		if(listening_for_files.count(*path) == 0) {
			sys::notify_on_file_modification(*path, boost::bind(on_object_file_updated, *path));
			listening_for_files.insert(*path);
		}

		if(files_updated.count(*path)) {
			try {
				reload_object(i->first);
				++result;
			} catch(...) {
				error_paths.insert(*path);
			}
		}
	}

	files_updated = error_paths;

	return result;
}
#endif // NO_EDITOR

void custom_object_type::set_file_contents(const std::string& file_path, const std::string& contents)
{
	json::set_file_contents(file_path, contents);
	for(object_map::iterator i = cache().begin(); i != cache().end(); ++i) {
		const std::vector<std::string>& proto_paths = object_prototype_paths[i->first];
		const std::string* path = get_object_path(i->first + ".cfg");
		if(path && *path == file_path || std::count(proto_paths.begin(), proto_paths.end(), file_path)) {
			reload_object(i->first);
		}
	}
}

namespace {
int g_num_object_reloads = 0;
}

void custom_object_type::reload_object(const std::string& type)
{
	object_map::iterator itor = cache().find(module::get_id(type));
	ASSERT_LOG(itor != cache().end(), "COULD NOT RELOAD OBJECT " << type);
	
	const_custom_object_type_ptr old_obj = itor->second;

	custom_object_type_ptr new_obj;
	
	const int begin = SDL_GetTicks();
	{
		const assert_recover_scope scope;
		new_obj = recreate(type, old_obj.get());
	}

	if(!new_obj) {
		return;
	}

	const int start = SDL_GetTicks();
	foreach(custom_object* obj, custom_object::get_all(old_obj->id())) {
		assert(obj);
		obj->update_type(old_obj, new_obj);
	}

	for(std::map<std::string, const_custom_object_type_ptr>::const_iterator i = old_obj->sub_objects_.begin(); i != old_obj->sub_objects_.end(); ++i) {
		std::map<std::string, const_custom_object_type_ptr>::const_iterator j = new_obj->sub_objects_.find(i->first);
		if(j != new_obj->sub_objects_.end() && i->second != j->second) {
			foreach(custom_object* obj, custom_object::get_all(i->second->id())) {
				obj->update_type(i->second, j->second);
			}
		}
	}

	const int end = SDL_GetTicks();
	std::cerr << "UPDATED " << custom_object::get_all(old_obj->id()).size() << " OBJECTS IN " << (end - start) << "ms\n";

	itor->second = new_obj;

	++g_num_object_reloads;
}

int custom_object_type::num_object_reloads()
{
	return g_num_object_reloads;
}

void custom_object_type::init_event_handlers(variant node,
                                             event_handler_map& handlers,
											 game_logic::function_symbol_table* symbols,
											 const event_handler_map* base_handlers) const
{
	const custom_object_callable_expose_private_scope expose_scope(*callable_definition_);
	const game_logic::formula::strict_check_scope strict_checking(is_strict_ || g_strict_mode_warnings, g_strict_mode_warnings);

	if(symbols == NULL) {
		symbols = &get_custom_object_functions_symbol_table();
	}

	foreach(const variant_pair& value, node.as_map()) {
		const std::string& key = value.first.as_string();
		if(key.size() > 3 && std::equal(key.begin(), key.begin() + 3, "on_")) {
			const std::string event(key.begin() + 3, key.end());
			const int event_id = get_object_event_id(event);
			if(handlers.size() <= event_id) {
				handlers.resize(event_id+1);
			}

			if(base_handlers && base_handlers->size() > event_id && (*base_handlers)[event_id] && (*base_handlers)[event_id]->str() == value.second.as_string()) {
				handlers[event_id] = (*base_handlers)[event_id];
			} else {
				boost::scoped_ptr<custom_object_callable_modify_scope> modify_scope;
				const variant_type_ptr arg_type = get_object_event_arg_type(event_id);
				if(arg_type) {
					modify_scope.reset(new custom_object_callable_modify_scope(*callable_definition_, CUSTOM_OBJECT_ARG, arg_type));
				}
				handlers[event_id] = game_logic::formula::create_optional_formula(value.second, symbols, callable_definition_);
			}
		}
	}
}

namespace {

std::vector<std::string> custom_object_type_stack;
struct custom_object_type_init_scope {
	explicit custom_object_type_init_scope(const std::string& id) {
		custom_object_type_stack.push_back(id);
	}

	~custom_object_type_init_scope() {
		custom_object_type_stack.pop_back();
	}
};

}

void init_level_definition();

custom_object_type::custom_object_type(const std::string& id, variant node, const custom_object_type* base_type, const custom_object_type* old_type)
  : id_(id),
	hitpoints_(node["hitpoints"].as_int(1)),
	timer_frequency_(node["timer_frequency"].as_int(-1)),
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
	solid_(solid_info::create(node)),
	platform_(solid_info::create_platform(node)),
	solid_platform_(node["solid_platform"].as_bool(false)),
	has_solid_(solid_ || use_image_for_collisions_),
	solid_dimensions_(has_solid_ || platform_ ? 0xFFFFFFFF : 0),
	collide_dimensions_(0xFFFFFFFF),
	weak_solid_dimensions_(has_solid_ || platform_ || node["has_platform"].as_bool(false) ? 0xFFFFFFFF : 0),
	weak_collide_dimensions_(0xFFFFFFFF),
	activation_border_(node["activation_border"].as_int(100)),
	editor_force_standing_(node["editor_force_standing"].as_bool(false)),
	hidden_in_game_(node["hidden_in_game"].as_bool(false)),
	stateless_(node["stateless"].as_bool(false)),
	platform_offsets_(node["platform_offsets"].as_list_int_optional()),
	slot_properties_base_(-1), 
	use_absolute_screen_coordinates_(node["use_absolute_screen_coordinates"].as_bool(false)),
	mouseover_delay_(node["mouseover_delay"].as_int(0)),
	is_strict_(!g_suppress_strict_mode && node["is_strict"].as_bool(custom_object_strict_mode) || g_force_strict_mode),
	is_shadow_(node["is_shadow"].as_bool(false)),
	true_z_(node["truez"].as_bool(false)), tx_(node["tx"].as_decimal().as_float()), 
	ty_(node["ty"].as_decimal().as_float()), tz_(node["tz"].as_decimal().as_float())
{
	if(g_player_type_str.is_null() == false) {
		//if a playable object type has been set, register what the type of
		//the player is before we construct our object.
		variant type = g_player_type_str;
		g_player_type_str = variant();
		level::set_player_variant_type(type);
	}

	if(editor_force_standing_) {
		ASSERT_LOG(has_feet_, "OBject type " << id_ << " has editor_force_standing set but has no feet. has_feet must be true for an object forced to standing");
	}
	boost::scoped_ptr<strict_mode_scope> strict_scope;
	if(is_strict_) {
		strict_scope.reset(new strict_mode_scope);
	}

	const game_logic::formula::strict_check_scope strict_checking(false);

	const custom_object_type_init_scope init_scope(id);
	const bool is_recursive_call = std::count(custom_object_type_stack.begin(), custom_object_type_stack.end(), id) > 0;

	callable_definition_.reset(new custom_object_callable);
	callable_definition_->set_type_name("obj " + id);

	custom_object_callable::instance();

	editor_entity_info* editor_info = NULL;

#ifndef NO_EDITOR
	if(node.has_key("editor_info")) {
		editor_info = new editor_entity_info(node["editor_info"]);
		editor_info_.reset(editor_info);
	}
#endif // !NO_EDITOR

	if(node.has_key("preload_sounds")) {
		//Pre-load any sounds that should be present when we create
		//this object type.
		foreach(std::string sound, util::split(node["preload_sounds"].as_string())) {
			sound::preload(sound);
		}
	}

	const bool is_variation = base_type != NULL;

	//make it so any formula has these constants defined.
	const game_logic::constants_loader scope_consts(node["consts"]);

	//if some constants change from base to variation, then we have to
	//re-parse all formulas.
	if(scope_consts.same_as_base() == false) {
		base_type = NULL;
	}

	if(node.has_key("solid_dimensions")) {
		weak_solid_dimensions_ = solid_dimensions_ = 0;
		foreach(std::string key, node["solid_dimensions"].as_list_string()) {
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
		foreach(std::string key, node["collide_dimensions"].as_list_string()) {
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

	foreach(variant anim, node["animation"].as_list()) {
		boost::intrusive_ptr<frame> f;
		try {
			f.reset(new frame(anim));
		} catch(frame::error&) {
			ASSERT_LOG(false, "ERROR LOADING FRAME IN OBJECT '" << id_ << "'");
		}

		if(use_image_for_collisions_) {
			f->set_image_as_solid();
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
		if(!default_frame_) {
			default_frame_ = f;
		}
	}

	ASSERT_LOG(default_frame_, "OBJECT " << id_ << " NO ANIMATIONS FOR OBJECT: " << node.write_json() << "'");

	std::vector<variant> available_frames;
	for(frame_map::const_iterator i = frames_.begin(); i != frames_.end(); ++i) {
		available_frames.push_back(variant(i->first));
	}

	available_frames_ = variant(&available_frames);

	mass_ = node["mass"].as_int(default_frame_->collide_w() * default_frame_->collide_h());
	
	foreach(variant child, node["child"].as_list()) {
		const std::string& child_id = child["child_id"].as_string();
		children_[child_id] = child;
	}

	assert(default_frame_);

	next_animation_formula_ = game_logic::formula::create_optional_formula(node["next_animation"], function_symbols());

	foreach(variant particle_node, node["particle_system"].as_list()) {
		particle_factories_[particle_node["id"].as_string()] = particle_system_factory::create_factory(particle_node);
	}

	if(!is_variation && !is_recursive_call) {
		//only initialize sub objects up front if it's not a recursive call
		//doing it this way means that dependencies between sub objects and
		//parent objects won't result in infinite recursion.
		init_sub_objects(node, old_type);
	}

	if(node.has_key("parallax_scale_x") || node.has_key("parallax_scale_y")) {
		parallax_scale_millis_.reset(new std::pair<int, int>(node["parallax_scale_x"].as_int(1000), node["parallax_scale_y"].as_int(1000)));
	}
	
	variant vars = node["vars"];
	if(vars.is_null() == false) {
		std::vector<std::string> var_str;
		foreach(variant key, vars.get_keys().as_list()) {
			variables_[key.as_string()] = vars[key];
			var_str.push_back(key.as_string());
		}

		if(!var_str. empty()) {
			game_logic::formula_callable_definition::entry* entry = callable_definition_->get_entry(CUSTOM_OBJECT_VARS);
			ASSERT_LOG(entry != NULL, "CANNOT FIND VARS ENTRY IN OBJECT");
			game_logic::formula_callable_definition_ptr def = game_logic::create_formula_callable_definition(&var_str[0], &var_str[0] + var_str.size());
			def->set_strict(is_strict_);
			entry->type_definition = def;
		}
	}

	variant tmp_vars = node["tmp"];
	if(tmp_vars.is_null() == false) {
		std::vector<std::string> var_str;
		foreach(variant key, tmp_vars.get_keys().as_list()) {
			tmp_variables_[key.as_string()] = tmp_vars[key];
			var_str.push_back(key.as_string());
		}

		if(!var_str.empty()) {
			game_logic::formula_callable_definition::entry* entry = callable_definition_->get_entry(CUSTOM_OBJECT_TMP);
			ASSERT_LOG(entry != NULL, "CANNOT FIND TMP ENTRY IN OBJECT");
			game_logic::formula_callable_definition_ptr def = game_logic::create_formula_callable_definition(&var_str[0], &var_str[0] + var_str.size());
			def->set_strict(is_strict_);
			entry->type_definition = def;
		}
	}

	//std::cerr << "TMP_VARIABLES: '" << id_ << "' -> " << tmp_variables_.size() << "\n";

	consts_.reset(new game_logic::map_formula_callable);
	variant consts = node["consts"];
	if(consts.is_null() == false) {
		foreach(variant key, consts.get_keys().as_list()) {
			consts_->add(key.as_string(), consts[key]);
		}
	}

	if(node.has_key("tags")) {
		const std::vector<std::string> tags = util::split(node["tags"].as_string());
		foreach(const std::string& tag, tags) {
			tags_[tag] = variant(1);
		}
	}

	//START OF FIRST PARSE OF PROPERTIES.
	//Here we get the types of properties and parse them into
	//callable_definition_. While we're in our first parse we want to make
	//sure we do not have to query other custom_object_type definitions,
	//because if we do we could end with infinite recursion.

	init_object_definition(node, id_, callable_definition_, slot_properties_base_, is_strict_);

	const types_cfg_scope types_scope(node["types"]);

	//END OF FIRST PARSE.
	//We've now constructed our definition of the object, and we can
	//safely query other object type definitions
/*
	for(int n = 0; n != callable_definition_->num_slots(); ++n) {
		if(callable_definition_->get_entry(n)->variant_type) {
			//fprintf(stderr, "  %s: %s\n", callable_definition_->get_entry(n)->id.c_str(), callable_definition_->get_entry(n)->variant_type->to_string().c_str());
		} else {
			//fprintf(stderr, "  %s: (none)\n", callable_definition_->get_entry(n)->id.c_str());
		}
	}
	//fprintf(stderr, "}\n");
*/

	callable_definition_->set_object_type(variant_type::get_custom_object(id_));

	if(!is_variation && is_recursive_call) {
		//we initialize sub objects here if we are in a recursive call,
		//to make sure that it's after we've set our definition. This will
		//avoid infinite recursion.
		init_sub_objects(node, old_type);
	}

	std::map<std::string, int> property_to_slot;

	int storage_slot = 0;

	foreach(variant properties_node, node["properties"].as_list()) {
		if(properties_node.is_string()) {
			continue;
		}

		const custom_object_callable_expose_private_scope expose_scope(*callable_definition_);
		foreach(variant key, properties_node.get_keys().as_list()) {
			const game_logic::formula::strict_check_scope strict_checking(is_strict_ || g_strict_mode_warnings, g_strict_mode_warnings);
			const std::string& k = key.as_string();
			bool dynamic_initialization = false;
			variant value = properties_node[key];
			property_entry& entry = properties_[k];
			entry.id = k;
			if(value.is_string()) {
				entry.getter = game_logic::formula::create_optional_formula(value, function_symbols(), callable_definition_);
			} else if(value.is_map()) {
				if(value.has_key("type")) {
					entry.type = parse_variant_type(value["type"]);
					entry.set_type = entry.type;
				}

				if(value.has_key("set_type")) {
					entry.set_type = parse_variant_type(value["set_type"]);
				}

				game_logic::const_formula_callable_definition_ptr property_def = callable_definition_;
				if(entry.type) {
					property_def = modify_formula_callable_definition(property_def, CUSTOM_OBJECT_DATA, entry.type);
				}

				game_logic::const_formula_callable_definition_ptr setter_def = property_def;
				if(entry.set_type) {
					setter_def = modify_formula_callable_definition(setter_def, CUSTOM_OBJECT_VALUE, entry.set_type);
				}

				entry.getter = game_logic::formula::create_optional_formula(value["get"], function_symbols(), property_def);
				entry.setter = game_logic::formula::create_optional_formula(value["set"], function_symbols(), setter_def);
				if(value["init"].is_null() == false) {
					entry.init = game_logic::formula::create_optional_formula(value["init"], function_symbols(), game_logic::const_formula_callable_definition_ptr(&custom_object_callable::instance()));
					assert(entry.init);
					if(is_strict_) {
						assert(entry.type);
						ASSERT_LOG(variant_types_compatible(entry.type, entry.init->query_variant_type()), "Initializer for " << id_ << "." << k << " does not have a matching type. Evaluates to " << entry.init->query_variant_type()->to_string() << " expected " << entry.type->to_string());
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
					const game_logic::formula::strict_check_scope strict_checking(false);
					variant editor_info_var = value["editor_info"];
					static const variant name_key("name");
					editor_info_var = editor_info_var.add_attr(name_key, variant(k));
					editor_variable_info info(editor_info_var);
					info.set_is_property();

					ASSERT_LOG(editor_info, "Object type " << id_ << " must have editor_info section since some of its properties have editor_info sections");

					editor_info->add_property(info);
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
				if(entry.getter->evaluates_to_constant(v)) {
					entry.getter.reset();
					entry.const_value.reset(new variant(v));
				}
			}

			int nslot = slot_properties_.size();
			if(property_to_slot.count(k)) {
				nslot = property_to_slot[k];
			} else {
				property_to_slot[k] = nslot;
			}

			if(entry.init) {
				properties_with_init_.push_back(nslot);
			}

			entry.requires_initialization = entry.storage_slot >= 0 && entry.type && !entry.type->match(entry.default_value) && !dynamic_initialization && !entry.init;
			if(entry.requires_initialization) {
				if(entry.setter) {
					ASSERT_LOG(last_initialization_property_ == "", "Object " << id_ << " has multiple properties which require initialization and which have custom setters. This isn't allowed because we wouldn't know which property to initialize first. Properties: " << last_initialization_property_ << ", " << entry.id);
					last_initialization_property_ = entry.id;
				}
				properties_requiring_initialization_.push_back(nslot);
			}

			if(dynamic_initialization) {
				properties_requiring_dynamic_initialization_.push_back(nslot);
			}

			if(nslot == slot_properties_.size()) {
				slot_properties_.push_back(entry);
			} else {
				assert(nslot >= 0 && nslot < slot_properties_.size());
				slot_properties_[nslot] = entry;
			}
		}
	}

	variant variations = node["variations"];
	if(variations.is_null() == false) {
		foreach(const variant_pair& v, variations.as_map()) {
			variations_[v.first.as_string()] = game_logic::formula::create_optional_formula(v.second, &get_custom_object_functions_symbol_table());
		}
		
		node_ = node;
	}

	game_logic::register_formula_callable_definition("object_type", callable_definition_);

#if defined(USE_SHADERS)
	if(node.has_key("shader")) {
		shader_.reset(new gles2::shader_program(node["shader"]));
	}

	if(node.has_key("effects")) {
		effects_.clear();
		for(size_t n = 0; n < node["effects"].num_elements(); ++n) {
			effects_.push_back(gles2::shader_program_ptr(new gles2::shader_program(node["effects"][n])));
		}
	}
#endif

#ifdef USE_BOX2D
	if(node.has_key("body")) {
		body_.reset(new box2d::body(node["body"]));
	}
#endif

#if defined(USE_LUA)
	if(node.has_key("lua")) {
		lua_source_ = node["lua"].as_string();
	}
#endif

	if(base_type) {
		//if we're a variation, just get the functions from our base type.
		//variations can't define new functions.
		object_functions_ = base_type->object_functions_;
	} else if(node.has_key("functions")) {
		object_functions_.reset(new game_logic::function_symbol_table);
		object_functions_->set_backup(&get_custom_object_functions_symbol_table());
		const variant fn = node["functions"];
		if(fn.is_string()) {
			game_logic::formula f(fn, object_functions_.get());
		} else if(fn.is_list()) {
			for(int n = 0; n != fn.num_elements(); ++n) {
				game_logic::formula f(fn[n], object_functions_.get());
			}
		}
	}
	init_event_handlers(node, event_handlers_, function_symbols(), base_type ? &base_type->event_handlers_ : NULL);

#if defined(USE_SHADERS)
	if(node.has_key("blend_mode_source") || node.has_key("blend_mode_dest")) {
		blend_mode_.reset(new graphics::blend_mode);
		blend_mode_->sfactor = GL_ONE;
		blend_mode_->dfactor = GL_ONE;

		blend_mode_->sfactor = get_blend_mode(node["blend_mode_source"]);
		blend_mode_->dfactor = get_blend_mode(node["blend_mode_dest"]);
	}
#endif
}

custom_object_type::~custom_object_type()
{
}

namespace {
struct stack_scope {
	stack_scope(std::vector<std::string>* stack, const std::string& item)
	  : stack_(stack)
	{
		stack_->push_back(item);
	}

	~stack_scope() {
		stack_->pop_back();
	}

	std::vector<std::string>* stack_;
};
}

void custom_object_type::init_sub_objects(variant node, const custom_object_type* old_type)
{
	static std::vector<std::string> init_stack;
	foreach(variant object_node, node["object_type"].as_list()) {
		variant merged = merge_prototype(object_node);
		std::string sub_key = object_node["id"].as_string();

		const std::string init_key = id_ + "." + sub_key;
		if(std::count(init_stack.begin(), init_stack.end(), init_key)) {
			continue;
		}

		stack_scope scope(&init_stack, init_key);

		if(old_type && old_type->sub_objects_.count(sub_key) &&
		   old_type->sub_objects_.find(sub_key)->second->node_ == merged) {
			//We are recreating this object, and the sub object node
			//hasn't changed at all, so just reuse the same sub object.
			sub_objects_[sub_key] = old_type->sub_objects_.find(sub_key)->second;
		} else {
			custom_object_type* type = new custom_object_type(id_ + "." + merged["id"].as_string(), merged);
			if(old_type && type->node_.is_null()){
				type->node_ = merged;
			}
		//std::cerr << "MERGED PROTOTYPE FOR " << type->id_ << ": " << merged.write_json() << "\n";
			sub_objects_[sub_key].reset(type);
		}
	}
}

const frame& custom_object_type::default_frame() const
{
	return *default_frame_;
}

const frame& custom_object_type::get_frame(const std::string& key) const
{
	frame_map::const_iterator itor = frames_.find(key);
	if(itor == frames_.end() || itor->second.empty()) {
		if(key != "normal") {
		ASSERT_LOG(key == "normal", "UNKNOWN ANIMATION FRAME " << key << " IN " << id_);
		}
		return default_frame();
	} else {
		if(itor->second.size() == 1) {
			return *itor->second.front().get();
		} else {
			return *itor->second[rand()%itor->second.size()].get();
		}
	}
}

game_logic::const_formula_ptr custom_object_type::get_event_handler(int event) const
{
	if(event >= event_handlers_.size()) {
		return game_logic::const_formula_ptr();
	} else {
		return event_handlers_[event];
	}
}

const_particle_system_factory_ptr custom_object_type::get_particle_system_factory(const std::string& id) const
{
	std::map<std::string, const_particle_system_factory_ptr>::const_iterator i = particle_factories_.find(id);
	ASSERT_LOG(i != particle_factories_.end(), "Unknown particle system type in " << id_ << ": " << id);
	return i->second;
}

game_logic::function_symbol_table* custom_object_type::function_symbols() const
{
	if(object_functions_) {
		return object_functions_.get();
	} else {
		return &get_custom_object_functions_symbol_table();
	}
}

namespace {
void execute_variation_command(variant cmd, game_logic::formula_callable& obj)
{
	if(cmd.is_list()) {
		foreach(variant c, cmd.as_list()) {
			execute_variation_command(c, obj);
		}
	} else if(cmd.try_convert<game_logic::command_callable>()) {
		cmd.try_convert<game_logic::command_callable>()->run_command(obj);
	}
}
}

const_custom_object_type_ptr custom_object_type::get_variation(const std::vector<std::string>& variations) const
{
	ASSERT_LOG(node_.is_null() == false, "tried to set variation in object " << id_ << " which has no variations");

	const_custom_object_type_ptr& result = variations_cache_[variations];
	if(!result) {
		variant node = node_;

		boost::intrusive_ptr<game_logic::map_formula_callable> callable(new game_logic::map_formula_callable);
		callable->add("doc", variant(variant_callable::create(&node)));

		foreach(const std::string& v, variations) {
			std::map<std::string, game_logic::const_formula_ptr>::const_iterator var_itor = variations_.find(v);
			ASSERT_LOG(var_itor != variations_.end(), "COULD NOT FIND VARIATION " << v << " IN " << id_);

			variant cmd = var_itor->second->execute(*callable);

			execute_variation_command(cmd, *callable);

			//std::cerr << "VARIATION " << v << ":\n--- BEFORE ---\n" << node.write_json() << "\n--- AFTER ---\n" << node_.write_json() << "\n--- DONE ---\n";
		}

		//set our constants so the variation can decide whether it needs
		//to re-parse formulas or not.
		const game_logic::constants_loader scope_consts(node_["consts"]);

		//copy the id over from the parent object, to make sure it's
		//the same. This is important for nested objects.
		custom_object_type* obj_type = new custom_object_type(id_, node, this);
		result.reset(obj_type);
	}

	return result;
}

void custom_object_type::load_variations() const
{
	if(node_.is_null() || variations_.empty() || !node_.has_key("load_variations")) {
		return;
	}

	const std::vector<std::string> variations_to_load = util::split(node_["load_variations"].as_string());
	foreach(const std::string& v, variations_to_load) {
		get_variation(std::vector<std::string>(1, v));
	}
}

#include "texture.hpp"
#include "surface_cache.hpp"

BENCHMARK(custom_object_type_load)
{
	static std::map<std::string,std::string> file_paths;
	if(file_paths.empty()) {
		module::get_unique_filenames_under_dir("data/objects", &file_paths);
	}

	BENCHMARK_LOOP {
		for(std::map<std::string,std::string>::const_iterator i = file_paths.begin(); i != file_paths.end(); ++i) {
			if(i->first.size() > 4 && std::equal(i->first.end()-4, i->first.end(), ".cfg")) {
				custom_object_type::create(std::string(i->first.begin(), i->first.end()-4));
			}
		}
		graphics::surface_cache::clear();
		graphics::texture::clear_textures();
	}
}


BENCHMARK(custom_object_type_frogatto_load)
{
	BENCHMARK_LOOP {
		custom_object_type::create("frogatto_playable");
		graphics::texture::clear_textures();
		graphics::surface_cache::clear();
	}
}

UTILITY(object_definition)
{
	foreach(const std::string& arg, args) {
		const_custom_object_type_ptr obj = custom_object_type::get(arg);
		ASSERT_LOG(obj.get() != NULL, "NO OBJECT FOUND: " << arg);

		const std::string* fname = custom_object_type::get_object_path(arg + ".cfg");
		ASSERT_LOG(fname != NULL, "NO OBJECT FILE FOUND: " << arg);

		const variant node = custom_object_type::merge_prototype(json::parse_from_file(*fname));

		std::cout << "OBJECT " << arg << "\n---\n" << node.write_json(true) << "\n---\n";
	}
}

UTILITY(test_all_objects)
{
	custom_object_type::get_all();
}
