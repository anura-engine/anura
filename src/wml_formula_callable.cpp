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
#include <set>
#include <stack>
#include <string>

#include <boost/bind.hpp>

#include <stdio.h>

#include "asserts.hpp"
#include "foreach.hpp"
#include "formula_object.hpp"
#include "json_parser.hpp"
#include "variant_utils.hpp"
#include "wml_formula_callable.hpp"

#ifdef _MSC_VER
#define strtoll _strtoui64
#endif

namespace game_logic
{

namespace {
struct scope_info {
	std::set<const_wml_serializable_formula_callable_ptr> objects_to_write, objects_written;
};

std::stack<scope_info, std::vector<scope_info> > scopes;

std::map<std::string, std::function<variant(variant)> >& type_registry() {
	static std::map<std::string, std::function<variant(variant)> > instance;
	return instance;
}

}

int wml_serializable_formula_callable::register_serializable_type(const char* name, std::function<variant(variant)> ctor)
{
	
	std::string key(name);
	type_registry()[key] = ctor;
	return type_registry().size();
}

bool wml_serializable_formula_callable::deserialize_obj(const variant& var, variant* target)
{
	for(const std::pair<std::string, std::function<variant(variant)> >& p : type_registry()) {
		if(var.has_key(p.first)) {
			*target = p.second(var);
			return true;
		}
	}

	return false;
}

const std::map<std::string, std::function<variant(variant)> >& wml_serializable_formula_callable::registered_types()
{
	return type_registry();
}

variant wml_serializable_formula_callable::write_to_wml() const
{
	variant result = serialize_to_wml();
	char addr_buf[256];
	sprintf(addr_buf, "%p", this);
	result.add_attr(variant("_addr"), variant(addr_buf));
	return result;
}

void wml_formula_callable_serialization_scope::register_serialized_object(const_wml_serializable_formula_callable_ptr ptr)
{
	ASSERT_LOG(scopes.empty() == false, "register_serialized_object() called when there is no wml_formula_callable_serialization_scope");
	scopes.top().objects_written.insert(ptr);
}

bool wml_formula_callable_serialization_scope::is_active()
{
	return scopes.empty() == false;
}

wml_formula_callable_serialization_scope::wml_formula_callable_serialization_scope()
{
	scopes.push(scope_info());
}

wml_formula_callable_serialization_scope::~wml_formula_callable_serialization_scope()
{
	scopes.pop();
}

namespace {
void add_object_to_set(variant v, std::set<wml_serializable_formula_callable*>* set, std::set<std::string>* already_recorded) {
	if(v.is_map()) {
		variant addr = v["_addr"];
		if(addr.is_string()) {
			already_recorded->insert(addr.as_string());
		}

		return;
	}

	if(!v.is_callable()) {
		return;
	}

	wml_serializable_formula_callable* ptr = v.try_convert<wml_serializable_formula_callable>();
	if(ptr) {
		set->insert(ptr);
		wml_formula_callable_serialization_scope::register_serialized_object(ptr);
	}
}
}

variant wml_formula_callable_serialization_scope::write_objects(variant obj, int* num_objects) const
{
	std::map<variant, variant> res;
	std::set<wml_serializable_formula_callable*> objects;
	std::set<std::string> already_known;
	game_logic::formula_object::visit_variants(obj, boost::bind(add_object_to_set, _1, &objects, &already_known));

	std::vector<variant> results_list;
	foreach(wml_serializable_formula_callable* item, objects) {
		char addr_buf[256];
		sprintf(addr_buf, "%p", item);
		std::string key(addr_buf);
		if(already_known.count(key)) {
			continue;
		}

		results_list.push_back(item->write_to_wml());
	}

	if(num_objects) {
		*num_objects = objects.size();
	}

	res[variant("character")] = variant(&results_list);

	return variant(&res);
}

namespace {
std::map<intptr_t, wml_serializable_formula_callable_ptr> registered_objects;
}

void wml_formula_callable_read_scope::register_serialized_object(intptr_t addr, wml_serializable_formula_callable_ptr ptr)
{
	//fprintf(stderr, "REGISTER SERIALIZED: 0x%x\n", (int)addr);
	if(ptr.get() != NULL) {
		registered_objects[addr] = ptr;
	}
}

wml_serializable_formula_callable_ptr wml_formula_callable_read_scope::get_serialized_object(intptr_t addr)
{
	auto itor = registered_objects.find(addr);
	if(itor != registered_objects.end()) {
		return itor->second;
	} else {
		return wml_serializable_formula_callable_ptr();
	}
}

namespace {
int g_nformula_callable_read_scope = 0;
}

wml_formula_callable_read_scope::wml_formula_callable_read_scope()
{
	++g_nformula_callable_read_scope;
}

wml_formula_callable_read_scope::~wml_formula_callable_read_scope()
{
	std::set<variant*> v;
	std::set<variant*> unfound_variants;
	swap_variants_loading(v);
	for(std::set<variant*>::iterator i = v.begin(); i != v.end(); ++i) {
		variant& var = **i;
		//fprintf(stderr, "LOAD SERIALIZED: 0x%x\n", (int)var.as_callable_loading());
		auto itor = registered_objects.find(var.as_callable_loading());
		if(itor == registered_objects.end()) {
			unfound_variants.insert(*i);
		} else {
			var = variant(itor->second.get());
		}
	}

	if(unfound_variants.empty()) {
		variant::resolve_delayed();
	} else {
		swap_variants_loading(unfound_variants);
	}

	if(--g_nformula_callable_read_scope == 0) {
		registered_objects.clear();
	}
}

bool wml_formula_callable_read_scope::try_load_object(intptr_t id, variant& v)
{
	std::map<intptr_t, wml_serializable_formula_callable_ptr>::const_iterator itor = registered_objects.find(id);
	if(itor != registered_objects.end()) {
		v = variant(itor->second.get());
		return true;
	} else {
		return false;
	}
}

std::string serialize_doc_with_objects(variant v)
{
	variant orig = v;
	if(!v.is_map()) {
		std::map<variant,variant> m;
		m[variant("__serialized_doc")] = v;
		v = variant(&m);
	}
	game_logic::wml_formula_callable_serialization_scope serialization_scope;
	int num_objects = 0;
	variant serialized_objects = serialization_scope.write_objects(v, &num_objects);
	if(num_objects == 0) {
		return orig.write_json();
	}
	v.add_attr(variant("serialized_objects"), serialized_objects);
	return v.write_json();
}

namespace {

variant deserialize_doc_with_objects_internal(const std::string& msg, bool fname)
{
	variant v;
	{
		const game_logic::wml_formula_callable_read_scope read_scope;

		if(fname) {
			v = json::parse_from_file(msg);
		} else {
			try {
				v = json::parse(msg);
			} catch(json::parse_error& e) {
				ASSERT_LOG(false, "ERROR PROCESSING FSON: --BEGIN--" << msg << "--END-- ERROR: " << e.error_message());
			}
		}
	
		if(v.is_map() && v.has_key(variant("serialized_objects"))) {
			foreach(variant obj_node, v["serialized_objects"]["character"].as_list()) {
				game_logic::wml_serializable_formula_callable_ptr obj = obj_node.try_convert<game_logic::wml_serializable_formula_callable>();
				ASSERT_LOG(obj.get() != NULL, "ILLEGAL OBJECT FOUND IN SERIALIZATION");
				std::string addr_str = obj->addr();
				const intptr_t addr_id = strtoll(addr_str.c_str(), NULL, 16);

				game_logic::wml_formula_callable_read_scope::register_serialized_object(addr_id, obj);
			}

			v.remove_attr_mutation(variant("serialized_objects"));
		}
	}

	if(v.is_map() && v.has_key(variant("__serialized_doc"))) {
		return v["__serialized_doc"];
	}

	return v;
}

}

variant deserialize_doc_with_objects(const std::string& msg)
{
	return deserialize_doc_with_objects_internal(msg, false);
}

variant deserialize_file_with_objects(const std::string& fname)
{
	return deserialize_doc_with_objects_internal(fname, true);
}

}
