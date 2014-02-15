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
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <map>
#include <string>
#include <stdio.h>

#include "code_editor_dialog.hpp"
#include "filesystem.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "formula_object.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "variant_type.hpp"
#include "variant_utils.hpp"


namespace game_logic
{

void invalidate_class_definition(const std::string& class_name);

namespace {

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
	formula_callable_definition::entry backup_;
	formula_callable_definition::entry& target_;
public:
	backup_entry_scope(formula_callable_definition::entry& e) : backup_(e), target_(e) {
	}
	~backup_entry_scope() {
		target_ = backup_;
	}
};

boost::intrusive_ptr<const formula_class> get_class(const std::string& type);

struct property_entry {
	property_entry() : variable_slot(-1) {
	}
	property_entry(const std::string& class_name, const std::string& prop_name, variant node, int& state_slot) : variable_slot(-1) {
		name = prop_name;

		formula_callable_definition_ptr class_def = get_class_definition(class_name);

		formula_callable_definition::entry* data_entry = class_def->get_entry(class_def->get_slot("_data"));
		formula_callable_definition::entry* value_entry = class_def->get_entry(class_def->get_slot("value"));
		formula_callable_definition::entry* prop_entry = class_def->get_entry(class_def->get_slot(prop_name));
		assert(data_entry);
		assert(value_entry);
		assert(prop_entry);

		backup_entry_scope backup1(*data_entry);
		backup_entry_scope backup2(*value_entry);

		value_entry->set_variant_type(prop_entry->variant_type);
		*data_entry = *prop_entry;

		const formula::strict_check_scope strict_checking;
		if(node.is_string()) {
			getter = game_logic::formula::create_optional_formula(node, NULL, get_class_definition(class_name));
			ASSERT_LOG(getter, "COULD NOT PARSE CLASS FORMULA " << class_name << "." << prop_name);

			ASSERT_LOG(getter->query_variant_type()->is_any() == false, "COULD NOT INFER TYPE FOR CLASS PROPERTY " << class_name << "." << prop_name << ". SET THIS PROPERTY EXPLICITLY");

			formula_callable_definition::entry* entry = class_def->get_entry_by_id(prop_name);
			ASSERT_LOG(entry != NULL, "COULD NOT FIND CLASS PROPERTY ENTRY " << class_name << "." << prop_name);

			entry->set_variant_type(getter->query_variant_type());
			return;
		} else if(node.is_map()) {
			if(node["variable"].as_bool(true)) {
				variable_slot = state_slot++;
			}

			if(node["get"].is_string()) {
				getter = game_logic::formula::create_optional_formula(node["get"], NULL, get_class_definition(class_name));
			}

			if(node["set"].is_string()) {
				setter = game_logic::formula::create_optional_formula(node["set"], NULL, get_class_definition(class_name));
			}

			default_value = node["default"];

			if(node["initialize"].is_string()) {
				initializer = game_logic::formula::create_optional_formula(node["initialize"], NULL);
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
	game_logic::const_formula_ptr getter, setter, initializer;

	variant_type_ptr get_type, set_type;
	int variable_slot;

	variant default_value;
};

std::map<std::string, variant> class_node_map;

void load_class_node(const std::string& type, const variant& node)
{
	class_node_map[type] = node;
	
	const variant classes = flatten_list_of_maps(node["classes"]);
	if(classes.is_map()) {
		foreach(variant key, classes.get_keys().as_list()) {
			load_class_node(type + "." + key.as_string(), classes[key]);
		}
	}
}

void load_class_nodes(const std::string& type)
{
	const std::string path = "data/classes/" + type + ".cfg";
	const std::string real_path = module::map_file(path);

	sys::notify_on_file_modification(real_path, boost::bind(invalidate_class_definition, type));

	const variant v = json::parse_from_file(path);
	ASSERT_LOG(v.is_map(), "COULD NOT FIND FFL CLASS: " << type);

	load_class_node(type, v);
}

variant get_class_node(const std::string& type)
{
	std::map<std::string, variant>::const_iterator i = class_node_map.find(type);
	if(i != class_node_map.end()) {
		return i->second;
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

enum CLASS_BASE_FIELDS { FIELD_PRIVATE, FIELD_VALUE, FIELD_SELF, FIELD_ME, FIELD_NEW_IN_UPDATE, FIELD_ORPHANED, FIELD_PREVIOUS, FIELD_CLASS, FIELD_LIB, NUM_BASE_FIELDS };
static const std::string BaseFields[] = {"_data", "value", "self", "me", "new_in_update", "orphaned_by_update", "previous", "_class", "lib"};

class formula_class_definition : public formula_callable_definition
{
public:
	formula_class_definition(const std::string& class_name, const variant& var)
	  : type_name_("class " + class_name)
	{
		set_strict();

		for(int n = 0; n != NUM_BASE_FIELDS; ++n) {
			properties_[BaseFields[n]] = n;
			slots_.push_back(entry(BaseFields[n]));
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
			case FIELD_PREVIOUS:
			slots_.back().variant_type = variant_type::get_class(class_name);
			break;
			case FIELD_CLASS:
			slots_.back().variant_type = variant_type::get_type(variant::VARIANT_TYPE_STRING);
			break;
			case FIELD_LIB:
			slots_.back().type_definition = get_library_definition().get();
			slots_.back().variant_type = variant_type::get_builtin("library");
			assert(slots_.back().variant_type);
			break;
			}
		}

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

		foreach(const variant& node, nodes) {
			variant properties = node["properties"];
			if(!properties.is_map()) {
				properties = node;
			}

			foreach(variant key, properties.get_keys().as_list()) {
				ASSERT_LOG(std::count(BaseFields, BaseFields + NUM_BASE_FIELDS, key.as_string()) == 0, "Class " << class_name << " has property '" << key.as_string() << "' which is a reserved word");
				ASSERT_LOG(key.as_string() != "", "Class " << class_name << " has property name which is empty");

				if(properties_.count(key.as_string()) == 0) {
					properties_[key.as_string()] = slots_.size();
					slots_.push_back(entry(key.as_string()));
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
							const formula::strict_check_scope strict_checking(false);
							formula_ptr f = formula::create_optional_formula(prop_node);
							if(f) {
								slots_[slot].variant_type = f->query_variant_type();
							}
						}
					}
				} else {
					slots_[slot].variant_type = get_variant_type_from_value(prop_node);
				}
			}
		}
	}

	virtual ~formula_class_definition() {}

	void init() {
		foreach(entry& e, slots_) {
			if(e.variant_type && !e.type_definition) {
				e.type_definition = e.variant_type->get_definition();
			}
		}
	}

	virtual int get_slot(const std::string& key) const {
		std::map<std::string, int>::const_iterator itor = properties_.find(key);
		if(itor != properties_.end()) {
			return itor->second;
		}
		
		return -1;
	}

	virtual entry* get_entry(int slot) {
		if(slot < 0 || slot >= slots_.size()) {
			return NULL;
		}

		return &slots_[slot];
	}

	virtual const entry* get_entry(int slot) const {
		if(slot < 0 || slot >= slots_.size()) {
			return NULL;
		}

		return &slots_[slot];
	}
	virtual int num_slots() const {
		return slots_.size();
	}

	const std::string* type_name() const {
		return &type_name_;
	}

	void push_private_access() {
		foreach(entry& e, slots_) {
			e.private_counter--;
		}
	}

	void pop_private_access() {
		foreach(entry& e, slots_) {
			e.private_counter++;
		}
	}

private:
	std::map<std::string, int> properties_;
	std::vector<entry> slots_;
	std::string type_name_;
};

struct definition_access_private_in_scope
{
	definition_access_private_in_scope(formula_class_definition& def) : def_(def)
	{
		def_.push_private_access();
	}
	~definition_access_private_in_scope()
	{
		def_.pop_private_access();
	}

	formula_class_definition& def_;
};

typedef std::map<std::string, boost::intrusive_ptr<formula_class_definition> > class_definition_map;
class_definition_map class_definitions;

typedef std::map<std::string, boost::intrusive_ptr<formula_class> > classes_map;

bool in_unit_test = false;
std::vector<formula_class*> unit_test_queue;

}

formula_callable_definition_ptr get_class_definition(const std::string& name)
{
	class_definition_map::iterator itor = class_definitions.find(name);
	if(itor != class_definitions.end()) {
		return itor->second;
	}

	formula_class_definition* def = new formula_class_definition(name, get_class_node(name));
	class_definitions[name].reset(def);
	def->init();

	return def;
}

class formula_class : public reference_counted_object
{
public:
	formula_class(const std::string& class_name, const variant& node);
	void set_name(const std::string& name);
	const std::string& name() const { return name_; }
	const variant& name_variant() const { return name_variant_; }
	const variant& private_data() const { return private_data_; }
	const std::vector<game_logic::const_formula_ptr>& constructor() const { return constructor_; }
	const std::map<std::string, int>& properties() const { return properties_; }
	const std::vector<property_entry>& slots() const { return slots_; }
	const classes_map& sub_classes() const { return sub_classes_; }

	bool is_a(const std::string& name) const;

	int nstate_slots() const { return nstate_slots_; }

	void build_nested_classes();
	void run_unit_tests();

private:
	void build_nested_classes(variant obj);
	std::string name_;
	variant name_variant_;
	variant private_data_;
	std::vector<game_logic::const_formula_ptr> constructor_;
	std::map<std::string, int> properties_;

	std::vector<property_entry> slots_;
	
	classes_map sub_classes_;

	variant unit_test_;

	std::vector<boost::intrusive_ptr<const formula_class> > bases_;

	variant nested_classes_;

	int nstate_slots_;
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
			foreach(const variant& b, bases.as_list()) {
				if(is_class_derived_from(b.as_string(), base)) {
					return true;
				}
			}
		}
	}

	return false;
}

formula_class::formula_class(const std::string& class_name, const variant& node)
  : name_(class_name), nstate_slots_(0)
{
	variant bases_v = node["bases"];
	if(bases_v.is_null() == false) {
		for(int n = 0; n != bases_v.num_elements(); ++n) {
			bases_.push_back(get_class(bases_v[n].as_string()));
		}
	}

	std::map<variant, variant> m;
	private_data_ = variant(&m);

	foreach(boost::intrusive_ptr<const formula_class> base, bases_) {
		merge_variant_over(&private_data_, base->private_data_);
	}

	ASSERT_LOG(bases_.size() <= 1, "Multiple inheritance of classes not currently supported");

	foreach(boost::intrusive_ptr<const formula_class> base, bases_) {
		slots_ = base->slots();
		properties_ = base->properties();
		nstate_slots_ = base->nstate_slots_;
	}

	variant properties = node["properties"];
	if(!properties.is_map()) {
		properties = node;
	}

	formula_callable_definition_ptr class_def = get_class_definition(class_name);
	assert(class_def);

	formula_class_definition* class_definition = dynamic_cast<formula_class_definition*>(class_def.get());
	assert(class_definition);

	const definition_access_private_in_scope expose_scope(*class_definition);

	foreach(variant key, properties.get_keys().as_list()) {
		const variant prop_node = properties[key];
		property_entry entry(class_name, key.as_string(), prop_node, nstate_slots_);

		if(properties_.count(key.as_string()) == 0) {
			properties_[key.as_string()] = slots_.size();
			slots_.push_back(property_entry());
		}

		slots_[properties_[key.as_string()]] = entry;
	}

	nested_classes_ = node["classes"];

	if(node["constructor"].is_string()) {
		const formula::strict_check_scope strict_checking;

		constructor_.push_back(game_logic::formula::create_optional_formula(node["constructor"], NULL, class_def));
	}

	unit_test_ = node["test"];
}

void formula_class::build_nested_classes()
{
	build_nested_classes(nested_classes_);
	nested_classes_ = variant();
}

void formula_class::build_nested_classes(variant classes)
{
	if(classes.is_list()) {
		foreach(const variant& v, classes.as_list()) {
			build_nested_classes(v);
		}
	} else if(classes.is_map()) {
		foreach(variant key, classes.get_keys().as_list()) {
			const variant class_node = classes[key];
			sub_classes_[key.as_string()].reset(new formula_class(name_ + "." + key.as_string(), class_node));
		}
	}
}

void formula_class::set_name(const std::string& name)
{
	name_ = name;
	name_variant_ = variant(name);
	for(classes_map::iterator i = sub_classes_.begin(); i != sub_classes_.end(); ++i) {
		i->second->set_name(name + "." + i->first);
	}
}
bool formula_class::is_a(const std::string& name) const
{
	if(name == name_) {
		return true;
	}

	typedef boost::intrusive_ptr<const formula_class> Ptr;
	foreach(const Ptr& base, bases_) {
		if(base->is_a(name)) {
			return true;
		}
	}

	return false;
}

void formula_class::run_unit_tests()
{
	const formula::strict_check_scope strict_checking(false);
	const formula::non_static_context non_static_context;

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

	boost::intrusive_ptr<game_logic::map_formula_callable> callable(new game_logic::map_formula_callable);
	std::map<variant,variant> attr;
	callable->add("vars", variant(&attr));
	callable->add("lib", variant(game_logic::get_library_object().get()));

	for(int n = 0; n != unit_test.num_elements(); ++n) {
		variant test = unit_test[n];
		game_logic::formula_ptr cmd = game_logic::formula::create_optional_formula(test["command"]);
		if(cmd) {
			variant v = cmd->execute(*callable);
			callable->execute_command(v);
		}

		game_logic::formula_ptr predicate = game_logic::formula::create_optional_formula(test["assert"]);
		if(predicate) {
			game_logic::formula_ptr message = game_logic::formula::create_optional_formula(test["message"]);

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
		formula_class* c = unit_test_queue.back();
		unit_test_queue.pop_back();
		c->run_unit_tests();
	}
}

namespace
{
struct private_data_scope {
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
		foreach(variant key, classes.get_keys().as_list()) {
			const variant class_node = classes[key];
			record_classes(name + "." + key.as_string(), class_node);
		}
	}
}

boost::intrusive_ptr<formula_class> build_class(const std::string& type)
{
	const variant v = get_class_node(type);

	record_classes(type, v);

	boost::intrusive_ptr<formula_class> result(new formula_class(type, v));
	result->set_name(type);
	return result;
}

boost::intrusive_ptr<const formula_class> get_class(const std::string& type)
{
	if(std::find(type.begin(), type.end(), '.') != type.end()) {
		std::vector<std::string> v = util::split(type, '.');
		boost::intrusive_ptr<const formula_class> c = get_class(v.front());
		for(int n = 1; n < v.size(); ++n) {
			classes_map::const_iterator itor = c->sub_classes().find(v[n]);
			ASSERT_LOG(itor != c->sub_classes().end(), "COULD NOT FIND FFL CLASS: " << type);
			c = itor->second.get();
		}

		return c;
	}

	classes_map::const_iterator itor = classes_.find(type);
	if(itor != classes_.end()) {
		return itor->second;
	}

	boost::intrusive_ptr<formula_class> result;
	
	if(!backup_classes_.empty() && backup_classes_.count(type)) {
		try {
			result = build_class(type);
		} catch(...) {
			result = backup_classes_[type];
			std::cerr << "ERROR LOADING NEW CLASS\n";
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
	return boost::intrusive_ptr<const formula_class>(result.get());
}

}

void formula_object::visit_variants(variant node, boost::function<void (variant)> fn, std::vector<formula_object*>* seen)
{
	std::vector<formula_object*> seen_buf;
	if(!seen) {
		seen = &seen_buf;
	}

	if(node.try_convert<formula_object>()) {
		formula_object* obj = node.try_convert<formula_object>();
		if(std::count(seen->begin(), seen->end(), obj)) {
			return;
		}

		const_wml_serializable_formula_callable_ptr ptr(obj);
		fn(node);

		seen->push_back(obj);

		foreach(const variant& v, obj->variables_) {
			visit_variants(v, fn, seen);
		}

		seen->pop_back();
		return;
	}
	
	fn(node);

	if(node.is_list()) {
		foreach(const variant& item, node.as_list()) {
			formula_object::visit_variants(item, fn, seen);
		}
	} else if(node.is_map()) {
		foreach(const variant_pair& item, node.as_map()) {
			formula_object::visit_variants(item.second, fn, seen);
		}
	}
}

void formula_object::update(formula_object& updated)
{
	std::vector<boost::intrusive_ptr<formula_object> > objects;
	std::map<boost::uuids::uuid, formula_object*> src, dst;
	visit_variants(variant(this), [&dst,&objects](variant v) {
		formula_object* obj = v.try_convert<formula_object>();
		if(obj) {
			dst[obj->id_] = obj;
			obj->previous_.reset();
			obj->previous_.reset(new formula_object(*obj));
			objects.push_back(obj);
		}});
	visit_variants(variant(&updated), [&src,&objects](variant v) {
		formula_object* obj = v.try_convert<formula_object>();
		if(obj) {
			src[obj->id_] = obj;
			objects.push_back(obj);
		}});

	std::map<formula_object*, formula_object*> mapping;

	for(auto i = src.begin(); i != src.end(); ++i) {
		auto j = dst.find(i->first);
		if(j != dst.end()) {
			mapping[i->second] = j->second;
		}
	}

	for(auto i = src.begin(); i != src.end(); ++i) {
		variant v(i->second);
		map_object_into_different_tree(v, mapping);
	}

	for(auto i = mapping.begin(); i != mapping.end(); ++i) {
		*i->second = *i->first;
	}

	for(auto i = dst.begin(); i != dst.end(); ++i) {
		if(src.count(i->first) == 0) {
			i->second->orphaned_ = true;
			i->second->new_in_update_ = false;
		}
	}

	for(auto i = src.begin(); i != src.end(); ++i) {
		i->second->new_in_update_ = dst.count(i->first) == 0;
	}
}

void formula_object::map_object_into_different_tree(variant& v, const std::map<formula_object*, formula_object*>& mapping, std::vector<formula_object*>* seen)
{
	std::vector<formula_object*> seen_buf;
	if(!seen) {
		seen = &seen_buf;
	}
	
	if(v.try_convert<formula_object>()) {
		formula_object* obj = v.try_convert<formula_object>();
		auto itor = mapping.find(obj);
		if(itor != mapping.end()) {
			v = variant(itor->second);
		}

		if(std::count(seen->begin(), seen->end(), obj)) {
			return;
		}

		seen->push_back(obj);

		foreach(variant& v, obj->variables_) {
			map_object_into_different_tree(v, mapping, seen);
		}

		seen->pop_back();
		return;
	}

	if(v.is_list()) {
		std::vector<variant> result;
		foreach(const variant& item, v.as_list()) {
			result.push_back(item);
			formula_object::map_object_into_different_tree(result.back(), mapping, seen);
		}

		v = variant(&result);
	} else if(v.is_map()) {
		std::map<variant, variant> result;
		foreach(const variant_pair& item, v.as_map()) {
			variant key = item.first;
			variant value = item.second;
			formula_object::map_object_into_different_tree(key, mapping, seen);
			formula_object::map_object_into_different_tree(value, mapping, seen);
			result[key] = value;
		}

		v = variant(&result);
	}
}

variant formula_object::deep_clone(variant v)
{
	std::map<formula_object*,formula_object*> mapping;
	return deep_clone(v, mapping);
}

variant formula_object::deep_clone(variant v, std::map<formula_object*,formula_object*>& mapping)
{
	if(v.is_callable()) {
		formula_object* obj = v.try_convert<formula_object>();
		if(obj) {
			std::map<formula_object*,formula_object*>::iterator itor = mapping.find(obj);
			if(itor != mapping.end()) {
				return variant(itor->second);
			}

			boost::intrusive_ptr<formula_object> duplicate = obj->clone();
			mapping[obj] = duplicate.get();

			for(int n = 0; n != duplicate->variables_.size(); ++n) {
				duplicate->variables_[n] = deep_clone(duplicate->variables_[n], mapping);
			}

			return variant(duplicate.get());
		} else {
			return v;
		}
	} else if(v.is_list()) {
		std::vector<variant> items;
		for(int n = 0; n != v.num_elements(); ++n) {
			items.push_back(deep_clone(v[n], mapping));
		}

		return variant(&items);
	} else if(v.is_map()) {
		std::map<variant, variant> m;
		foreach(const variant::map_pair& p, v.as_map()) {
			m[deep_clone(p.first, mapping)] = deep_clone(p.second, mapping);
		}

		return variant(&m);
	} else {
		return v;
	}
}

void formula_object::reload_classes()
{
	classes_.clear();
}

void formula_object::load_all_classes()
{
	std::vector<std::string> files;
	module::get_files_in_dir("data/classes/", &files, NULL);
	foreach(std::string f, files) {
		if(f.size() > 4 && std::equal(f.end()-4,f.end(),".cfg")) {
			f.resize(f.size()-4);
			get_class(f);
		}
	}
}

void formula_object::try_load_class(const std::string& name)
{
	build_class(name);
}

boost::intrusive_ptr<formula_object> formula_object::create(const std::string& type, variant args)
{
	const formula::strict_check_scope strict_checking;
	boost::intrusive_ptr<formula_object> res(new formula_object(type, args));
	res->call_constructors(args);
	res->validate();
	return res;
}

namespace {

boost::mt19937* twister_rng() {
	static boost::mt19937 ran;
	ran.seed(boost::posix_time::microsec_clock::local_time().time_of_day().total_milliseconds());
	return &ran;
}

boost::uuids::uuid generate_uuid() {
	static boost::uuids::basic_random_generator<boost::mt19937> gen(twister_rng());
	return gen();
}

variant write_uuid(const boost::uuids::uuid& id) {
	char result[33];
	char* ptr = result;
	for(auto num : id) {
		sprintf(ptr, "%02x", static_cast<int>(num));
		ptr += 2;
	}
	return variant(std::string(result, result+32));
}

boost::uuids::uuid read_uuid(const variant& v) {
	if(v.is_int()) {
		//backwards compatibility
		return generate_uuid();
	}

	boost::uuids::uuid result;

	const std::string& nums = v.as_string();
	const char* ptr = nums.c_str();
	ASSERT_LOG(nums.size() == 32, "Trying to deserialize bad UUID: " << v.write_json());
	for(auto itor = result.begin(); itor != result.end(); ++itor) {
		char buf[3];
		buf[0] = *ptr++;
		buf[1] = *ptr++;
		buf[2] = 0;

		*itor = strtol(buf, NULL, 16);
	}

	return result;
}

UNIT_TEST(serialize_uuid) {
	for(int i = 0; i != 8; ++i) {
		boost::uuids::uuid id = generate_uuid();
		const bool succeeded = id == read_uuid(write_uuid(id));
		CHECK_EQ(succeeded, true);
	}
}

}

formula_object::formula_object(const std::string& type, variant args)
  : id_(generate_uuid()), new_in_update_(true), orphaned_(false),
    class_(get_class(type)), private_data_(-1)
{
	variables_.resize(class_->nstate_slots());
	foreach(const property_entry& slot, class_->slots()) {
		if(slot.variable_slot != -1) {
			if(slot.initializer) {
				variables_[slot.variable_slot] = slot.initializer->execute(*this);
			} else {
				variables_[slot.variable_slot] = deep_copy_variant(slot.default_value);
			}
		}
	}
}

bool formula_object::is_a(const std::string& class_name) const
{
	return class_->is_a(class_name);
}

const std::string& formula_object::get_class_name() const
{
	return class_->name();
}

void formula_object::call_constructors(variant args)
{
	if(args.is_map()) {
		const_formula_callable_definition_ptr def = get_class_definition(class_->name());
		foreach(const variant& key, args.get_keys().as_list()) {
			std::map<std::string, int>::const_iterator itor = class_->properties().find(key.as_string());
			if(itor != class_->properties().end() && class_->slots()[itor->second].setter.get() == NULL && class_->slots()[itor->second].variable_slot == -1) {
				if(property_overrides_.size() <= itor->second) {
					property_overrides_.resize(itor->second+1);
				}

				//A read-only property. Set the formula to what is passed in.
				formula_ptr f(new formula(args[key], NULL, def));
				const formula_callable_definition::entry* entry = def->get_entry_by_id(key.as_string());
				ASSERT_LOG(entry, "COULD NOT FIND ENTRY IN CLASS DEFINITION: " << key.as_string());
				if(entry->variant_type) {
					ASSERT_LOG(variant_types_compatible(entry->variant_type, f->query_variant_type()), "ERROR: property override in instance of class " << class_->name() << " has mis-matched type for property " << key.as_string() << ": " << entry->variant_type->to_string() << " doesn't match " << f->query_variant_type()->to_string() << " at " << args[key].debug_location());
				}
				property_overrides_[itor->second] = f;
			} else {
				set_value(key.as_string(), args[key]);
			}
		}
	}

	foreach(const game_logic::const_formula_ptr f, class_->constructor()) {
		execute_command(f->execute(*this));
	}
}

formula_object::formula_object(variant data)
  : id_(read_uuid(data["id"])), new_in_update_(true), orphaned_(false),
    class_(get_class(data["@class"].as_string())), private_data_(-1)
{
	variables_.resize(class_->nstate_slots());

	if(data.is_map() && data["state"].is_map()) {
		variant state = data["state"];
		foreach(const variant::map_pair& p, state.as_map()) {
			std::map<std::string, int>::const_iterator itor = class_->properties().find(p.first.as_string());
			ASSERT_LOG(itor != class_->properties().end(), "No property " << p.first.as_string() << " in class " << class_->name());

			const property_entry& entry = class_->slots()[itor->second];
			ASSERT_NE(entry.variable_slot, -1);

			variables_[entry.variable_slot] = p.second;
		}
	}

	if(data.is_map() && data["property_overrides"].is_list()) {
		const variant overrides = data["property_overrides"];
		property_overrides_.reserve(overrides.num_elements());
		for(int n = 0; n != overrides.num_elements(); ++n) {
			if(overrides[n].is_null()) {
				property_overrides_.push_back(formula_ptr());
			} else {
				property_overrides_.push_back(formula_ptr(new formula(overrides[n])));
			}
		}
	}

	set_addr(data["_addr"].as_string());
}

formula_object::~formula_object()
{}

boost::intrusive_ptr<formula_object> formula_object::clone() const
{
	return boost::intrusive_ptr<formula_object>(new formula_object(*this));
}

variant formula_object::serialize_to_wml() const
{
	std::map<variant, variant> result;
	result[variant("@class")] = variant(class_->name());
	result[variant("id")] = write_uuid(id_);

	std::map<variant,variant> state;
	foreach(const property_entry& slot, class_->slots()) {
		const int nstate_slot = slot.variable_slot;
		if(nstate_slot != -1 && nstate_slot < variables_.size() &&
		   variables_[nstate_slot].is_null() == false) {
			state[variant(slot.name)] = variables_[nstate_slot];
		}
	}

	result[variant("state")] = variant(&state);

	if(property_overrides_.empty() == false) {
		std::vector<variant> properties;
		foreach(const formula_ptr& f, property_overrides_) {
			if(f) {
				properties.push_back(variant(f->str()));
			} else {
				properties.push_back(variant());
			}
		}
		result[variant("property_overrides")] = variant(&properties);
	}

	return variant(&result);
}

REGISTER_SERIALIZABLE_CALLABLE(formula_object, "@class");

variant formula_object::get_value(const std::string& key) const
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
		return class_->name_variant();
	}

	if(key == "lib") {
		return variant(get_library_object().get());
	}

	std::map<std::string, int>::const_iterator itor = class_->properties().find(key);
	ASSERT_LOG(itor != class_->properties().end(), "UNKNOWN PROPERTY ACCESS " << key << " IN CLASS " << class_->name() << "\nFORMULA LOCATION: " << get_call_stack());

	if(itor->second < property_overrides_.size() && property_overrides_[itor->second]) {
		return property_overrides_[itor->second]->execute(*this);
	}

	const property_entry& entry = class_->slots()[itor->second];

	if(entry.getter) {
		private_data_scope scope(&private_data_, entry.variable_slot);
		return entry.getter->execute(*this);
	} else if(entry.variable_slot != -1) {
		return variables_[entry.variable_slot];
	} else {
		ASSERT_LOG(false, "ILLEGAL READ PROPERTY ACCESS OF NON-READABLE VARIABLE " << key << " IN CLASS " << class_->name());
	}
}

variant formula_object::get_value_by_slot(int slot) const
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
		case FIELD_PREVIOUS: if(previous_) { return variant(previous_.get()); } else { return variant(this); }
		case FIELD_CLASS: return class_->name_variant();
		case FIELD_LIB: return variant(get_library_object().get());
		default: break;
	}

	slot -= NUM_BASE_FIELDS;

	ASSERT_LOG(slot >= 0 && slot < class_->slots().size(), "ILLEGAL VALUE QUERY TO FORMULA OBJECT: " << slot << " IN " << class_->name());


	if(slot < property_overrides_.size() && property_overrides_[slot]) {
		return property_overrides_[slot]->execute(*this);
	}
	
	const property_entry& entry = class_->slots()[slot];

	if(entry.getter) {
		private_data_scope scope(&private_data_, entry.variable_slot);
		return entry.getter->execute(*this);
	} else if(entry.variable_slot != -1) {
		return variables_[entry.variable_slot];
	} else {
		ASSERT_LOG(false, "ILLEGAL READ PROPERTY ACCESS OF NON-READABLE VARIABLE IN CLASS " << class_->name());
	}
}

void formula_object::set_value(const std::string& key, const variant& value)
{
	if(private_data_ != -1 && key == "_data") {
		variables_[private_data_] = value;
		return;
	}

	std::map<std::string, int>::const_iterator itor = class_->properties().find(key);
	ASSERT_LOG(itor != class_->properties().end(), "UNKNOWN PROPERTY ACCESS " << key << " IN CLASS " << class_->name());

	set_value_by_slot(itor->second+NUM_BASE_FIELDS, value);
	return;
}

void formula_object::set_value_by_slot(int slot, const variant& value)
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
	ASSERT_LOG(slot >= 0 && slot < class_->slots().size(), "ILLEGAL VALUE SET TO FORMULA OBJECT: " << slot << " IN " << class_->name());

	const property_entry& entry = class_->slots()[slot];

	if(entry.set_type) {
		if(!entry.set_type->match(value)) {
			ASSERT_LOG(false, "ILLEGAL WRITE PROPERTY ACCESS: SETTING VARIABLE " << entry.name << " IN CLASS " << class_->name() << " TO INVALID TYPE " << variant::variant_type_to_string(value.type()) << ": " << value.write_json());
		}
	}

	if(entry.setter) {
		tmp_value_ = value;
		private_data_scope scope(&private_data_, entry.variable_slot);
		execute_command(entry.setter->execute(*this));
	} else if(entry.variable_slot != -1) {
		variables_[entry.variable_slot] = value;
	} else {
		ASSERT_LOG(false, "ILLEGAL WRITE PROPERTY ACCESS OF NON-WRITABLE VARIABLE " << entry.name << " IN CLASS " << class_->name());
	}

	if(entry.get_type && (entry.getter || entry.setter)) {
		//now that we've set the value, retrieve it and ensure it matches
		//the type we expect.
		variant var;

		formula_ptr override;
		if(slot < property_overrides_.size()) {
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

void formula_object::validate() const
{
#ifndef NO_FFL_TYPE_SAFETY_CHECKS
	if(preferences::type_safety_checks() == false) {
		return;
	}

	int index = 0;
	foreach(const property_entry& entry, class_->slots()) {
		if(!entry.get_type) {
			++index;
			continue;
		}

		variant value;

		formula_ptr override;
		if(index < property_overrides_.size()) {
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

		ASSERT_LOG(entry.get_type->match(value), "OBJECT OF CLASS TYPE " << class_->name() << " HAS INVALID PROPERTY " << entry.name << ": " << value.write_json() << " EXPECTED " << entry.get_type->str());
	}
#endif
}

void formula_object::get_inputs(std::vector<formula_input>* inputs) const
{
	foreach(const property_entry& entry, class_->slots()) {
		FORMULA_ACCESS_TYPE type = FORMULA_READ_ONLY;
		if(entry.getter && entry.setter || entry.variable_slot != -1) {
			type = FORMULA_READ_WRITE;
		} else if(entry.getter) {
			type = FORMULA_READ_ONLY;
		} else if(entry.setter) {
			type = FORMULA_WRITE_ONLY;
		} else {
			continue;
		}

		inputs->push_back(formula_input(entry.name, type));
	}
}

bool formula_class_valid(const std::string& type)
{
	return known_classes.count(type) != false || get_class_node(type).is_map();
}

void invalidate_class_definition(const std::string& name)
{
	std::cerr << "INVALIDATE CLASS: " << name << "\n";
	for(std::map<std::string, variant>::iterator i = class_node_map.begin(); i != class_node_map.end(); ) {
		const std::string& class_name = i->first;
		std::string::const_iterator dot = std::find(class_name.begin(), class_name.end(), '.');
		std::string base_class(class_name.begin(), dot);
		if(base_class == name) {
			class_node_map.erase(i++);
		} else {
			++i;
		}
	}

	for(class_definition_map::iterator i = class_definitions.begin(); i != class_definitions.end(); )
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

	for(classes_map::iterator i = classes_.begin(); i != classes_.end(); )
	{
		const std::string& class_name = i->first;
		std::string::const_iterator dot = std::find(class_name.begin(), class_name.end(), '.');
		std::string base_class(class_name.begin(), dot);
		if(base_class == name) {
			known_classes.erase(class_name);
			backup_classes_[i->first] = i->second;
			classes_.erase(i++);
		} else {
			++i;
		}
	}
}

namespace {

formula_callable_definition_ptr g_library_definition;
formula_callable_ptr g_library_obj;

}

formula_class_manager::formula_class_manager()
{
}

formula_class_manager::~formula_class_manager()
{
}

formula_callable_definition_ptr get_library_definition()
{
	if(!g_library_definition) {
		std::vector<std::string> files;

		const std::string path = "data/classes/";
		module::get_files_in_dir("data/classes/", &files, NULL);

		std::vector<std::string> classes;

		foreach(const std::string& fname, files) {
			if(fname.size() > 4 && std::equal(fname.end() - 4, fname.end(), ".cfg")) {
				const std::string class_name(fname.begin(), fname.end()-4);
				if(std::count(classes.begin(), classes.end(), class_name) == 0) {
					classes.push_back(class_name);
				}
			}
		}

		std::vector<variant_type_ptr> types;
		foreach(const std::string& class_name, classes) {
			types.push_back(variant_type::get_class(class_name));
		}

		if(!types.empty()) {
			g_library_definition = game_logic::create_formula_callable_definition(&classes[0], &classes[0] + classes.size(), NULL);
			game_logic::register_formula_callable_definition("library", g_library_definition);

			for(int n = 0; n != g_library_definition->num_slots(); ++n) {
				g_library_definition->get_entry(n)->set_variant_type(types[n]);
			}
		} else {
			g_library_definition = game_logic::create_formula_callable_definition(NULL, NULL, NULL, NULL);
		}
	}

	return g_library_definition;
}

namespace {
class library_callable : public game_logic::formula_callable
{
public:
	library_callable() {
		items_.resize(get_library_definition()->num_slots());
	}
private:
	variant get_value(const std::string& key) const {
		formula_callable_definition_ptr def = get_library_definition();
		const int slot = def->get_slot(key);
		ASSERT_LOG(slot >= 0, "Unknown library: " << key << "\n" << get_full_call_stack());
		return query_value_by_slot(slot);
	}

	variant get_value_by_slot(int slot) const {
		ASSERT_LOG(slot >= 0 && slot < items_.size(), "ILLEGAL LOOK UP IN LIBRARY: " << slot << "/" << items_.size());
		if(items_[slot].is_null()) {
			formula_callable_definition_ptr def = get_library_definition();
			const formula_callable_definition::entry* entry = def->get_entry(slot);
			ASSERT_LOG(entry != NULL, "INVALID SLOT: " << slot);
			std::string class_name;
			if(entry->variant_type->is_class(&class_name) == false) {
				ASSERT_LOG(false, "ERROR IN LIBRARY");
			}

			items_[slot] = variant(formula_object::create(class_name).get());
		}

		return items_[slot];
	}

	mutable std::vector<variant> items_;
};
}

formula_callable_ptr get_library_object()
{
	if(g_library_obj.get() == NULL) {
		g_library_obj.reset(new library_callable);
	}

	return g_library_obj;
}

}

