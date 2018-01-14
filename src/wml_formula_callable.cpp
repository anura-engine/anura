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

#include <set>
#include <stack>
#include <string>

#include <stdio.h>

#include "asserts.hpp"
#include "formula_object.hpp"
#include "json_parser.hpp"
#include "variant_utils.hpp"
#include "wml_formula_callable.hpp"

#ifdef _MSC_VER
#define strtoll _strtoui64
#endif

namespace game_logic
{
	namespace 
	{
		struct scope_info 
		{
			std::set<ConstWmlSerializableFormulaCallablePtr> objects_to_write, objects_written;
		};

		std::stack<scope_info, std::vector<scope_info>> scopes;

		std::map<std::string, std::function<variant(variant)>>& type_registry() 
		{
			static std::map<std::string, std::function<variant(variant)>> instance;
			return instance;
		}
	}

	WmlSerializableFormulaCallable::WmlSerializableFormulaCallable(bool has_self) : FormulaCallable(has_self), uuid_(generate_uuid()) {}
	WmlSerializableFormulaCallable::WmlSerializableFormulaCallable(const boost::uuids::uuid& uuid, bool has_self) : FormulaCallable(has_self), uuid_(uuid) {}

	std::string WmlSerializableFormulaCallable::addr() const {
		return write_uuid(uuid_);
	}


	int WmlSerializableFormulaCallable::registerSerializableType(const char* name, std::function<variant(variant)> ctor)
	{
		std::string key(name);
		type_registry()[key] = ctor;
		return static_cast<int>(type_registry().size());
	}

	bool WmlSerializableFormulaCallable::deserializeObj(const variant& var, variant* target)
	{
		for(const std::pair<std::string, std::function<variant(variant)> >& p : type_registry()) {
			if(var.has_key(p.first)) {
				*target = p.second(var);
				return true;
			}
		}

		return false;
	}

	const std::map<std::string, std::function<variant(variant)> >& WmlSerializableFormulaCallable::registeredTypes()
	{
		return type_registry();
	}

	variant WmlSerializableFormulaCallable::writeToWml() const
	{
		variant result = serializeToWml();
		result.add_attr(variant("_uuid"), variant(write_uuid(uuid_)));
		return result;
	}

	void wmlFormulaCallableSerializationScope::registerSerializedObject(ConstWmlSerializableFormulaCallablePtr ptr)
	{
		ASSERT_LOG(scopes.empty() == false, "registerSerializedObject() called when there is no wmlFormulaCallableSerializationScope");
		scopes.top().objects_written.insert(ptr);
	}

	bool wmlFormulaCallableSerializationScope::isActive()
	{
		return scopes.empty() == false;
	}

	wmlFormulaCallableSerializationScope::wmlFormulaCallableSerializationScope()
	{
		scopes.push(scope_info());
	}

	wmlFormulaCallableSerializationScope::~wmlFormulaCallableSerializationScope()
	{
		scopes.pop();
	}

	namespace 
	{
		void add_object_to_set(variant v, std::set<WmlSerializableFormulaCallable*>* set, std::set<boost::uuids::uuid>* already_recorded) 
		{
			if(v.is_map()) {
				variant addr = v["_uuid"];
				if(addr.is_string()) {
					already_recorded->insert(read_uuid(addr.as_string()));
				}

				return;
			}

			if(!v.is_callable()) {
				return;
			}

			WmlSerializableFormulaCallable* ptr = v.try_convert<WmlSerializableFormulaCallable>();
			if(ptr) {
				set->insert(ptr);
				wmlFormulaCallableSerializationScope::registerSerializedObject(ptr);
			}
		}
	}

	variant wmlFormulaCallableSerializationScope::writeObjects(variant obj, int* num_objects) const
	{
		std::map<variant, variant> res;
		std::set<WmlSerializableFormulaCallable*> objects;
		std::set<boost::uuids::uuid> already_known;
		game_logic::FormulaObject::visitVariants(obj, std::bind(add_object_to_set, std::placeholders::_1, &objects, &already_known));

		std::vector<variant> results_list;
		for(WmlSerializableFormulaCallable* item : objects) {
			if(already_known.count(item->uuid())) {
				continue;
			}

			results_list.push_back(item->writeToWml());
		}

		if(num_objects) {
			*num_objects = static_cast<int>(objects.size());
		}

		res[variant("character")] = variant(&results_list);

		return variant(&res);
	}

	namespace 
	{
		std::map<boost::uuids::uuid, WmlSerializableFormulaCallablePtr>& get_registered_objects()
		{
			static std::map<boost::uuids::uuid, WmlSerializableFormulaCallablePtr> res;
			return res;
		}
		
	}

	void wmlFormulaCallableReadScope::registerSerializedObject(const boost::uuids::uuid& uuid, WmlSerializableFormulaCallablePtr ptr)
	{
		if(ptr.get() != nullptr) {
			get_registered_objects()[uuid] = ptr;
		}
	}

	WmlSerializableFormulaCallablePtr wmlFormulaCallableReadScope::getSerializedObject(const boost::uuids::uuid& uuid)
	{
		auto itor = get_registered_objects().find(uuid);
		if(itor != get_registered_objects().end()) {
			return itor->second;
		} else {
			return WmlSerializableFormulaCallablePtr();
		}
	}

	namespace 
	{
		int g_nFormulaCallableReadScope = 0;
	}

	int wmlFormulaCallableReadScope::isActive()
	{
		return g_nFormulaCallableReadScope;
	}

	wmlFormulaCallableReadScope::wmlFormulaCallableReadScope()
	{
		++g_nFormulaCallableReadScope;
	}

	wmlFormulaCallableReadScope::~wmlFormulaCallableReadScope()
	{
		std::set<variant*> v;
		std::set<variant*> unfound_variants;
		swap_variants_loading(v);
		for(auto& i : v) {
			variant& var = *i;
			auto itor = get_registered_objects().find(var.as_callable_loading());
			if(itor == get_registered_objects().end()) {
				unfound_variants.insert(i);
			} else {
				var = variant(itor->second.get());
			}
		}

		if(unfound_variants.empty()) {
			variant::resolve_delayed();
		} else {
			swap_variants_loading(unfound_variants);
		}

		if(--g_nFormulaCallableReadScope == 0) {
			get_registered_objects().clear();
		}
	}

	bool wmlFormulaCallableReadScope::try_load_object(const boost::uuids::uuid& id, variant& v)
	{
		std::map<boost::uuids::uuid, WmlSerializableFormulaCallablePtr>::const_iterator itor = get_registered_objects().find(id);
		if(itor != get_registered_objects().end()) {
			v = variant(itor->second.get());
			return true;
		}

		return false;
	}

	variant serialize_doc_with_objects(variant v)
	{
		variant orig = v;
		if(!v.is_map()) {
			std::map<variant,variant> m;
			m[variant("__serialized_doc")] = v;
			v = variant(&m);
		}
		game_logic::wmlFormulaCallableSerializationScope serialization_scope;
		int num_objects = 0;
		variant serialized_objects = serialization_scope.writeObjects(v, &num_objects);
		if(num_objects == 0) {
			return orig;
		}
		v.add_attr(variant("serialized_objects"), serialized_objects);
		return v;
	}

	namespace 
	{
		variant deserialize_doc_with_objects_internal(const std::string& msg, bool fname)
		{
			variant v;
			{
				const game_logic::wmlFormulaCallableReadScope read_scope;

				if(fname) {
					v = json::parse_from_file(msg);
				} else {
					try {
						v = json::parse(msg, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);

						if(v.is_map() && v.has_key(variant("serialized_objects"))) {
							v = json::parse(msg);
						}
					} catch(json::ParseError& e) {
						ASSERT_LOG(false, "ERROR PROCESSING FSON: --BEGIN--" << msg << "--END-- ERROR: " << e.errorMessage());
					}
				}
	
				if(v.is_map() && v.has_key(variant("serialized_objects"))) {
					for(variant& obj_node : v["serialized_objects"]["character"].as_list()) {
						game_logic::WmlSerializableFormulaCallablePtr obj = obj_node.try_convert<game_logic::WmlSerializableFormulaCallable>();
						ASSERT_LOG(obj.get() != nullptr, "ILLEGAL OBJECT FOUND IN SERIALIZATION");

						game_logic::wmlFormulaCallableReadScope::registerSerializedObject(obj->uuid(), obj);
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
