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
#ifndef WML_FORMULA_CALLABLE_HPP_INCLUDED
#define WML_FORMULA_CALLABLE_HPP_INCLUDED

#include <map>
#include <string>

#include <stdint.h>

#include <functional>

#include <boost/intrusive_ptr.hpp>

#include "formula_callable.hpp"
#include "variant.hpp"

namespace game_logic
{

// To make a formula_callable that is serializable, follow these steps:
// - Derive from wml_serializable_formula_callable instead of formula_callable
// - Implement a constructor which takes a variant as its only argument and
//   deserializes the object. In this constructor, put
//   READ_SERIALIZABLE_CALLABLE(node) where node is the passed in variant.
// - Implement the serialize_to_wml() method which should return a variant
//   which is a map. Choose a unique string key beginning with @ that is
//   used for instances of this class and return it as part of the map.
//   For instance if your class is a foo_callable you might want to make the
//   string "@foo".
// - In your cpp file add REGISTER_SERIALIZABLE_CALLABLE(foo_callable, "@foo").
//   This will do the magic to ensure that the FSON post-processor will
//   deserialize your object when an instance is found.

class wml_serializable_formula_callable : public formula_callable
{
public:
	static int register_serializable_type(const char* name, std::function<variant(variant)> ctor);
	static const std::map<std::string, std::function<variant(variant)> >& registered_types();
	static bool deserialize_obj(const variant& var, variant* target);

	explicit wml_serializable_formula_callable(bool has_self=true) : formula_callable(has_self) {}

	virtual ~wml_serializable_formula_callable() {}

	variant write_to_wml() const;

	const std::string& addr() const { return addr_; }
protected:
	void set_addr(const std::string& addr) { addr_ = addr; }
private:
	virtual variant serialize_to_wml() const = 0;
	std::string addr_;
};

#define REGISTER_SERIALIZABLE_CALLABLE(classname, idname) \
	static const int classname##_registration_var_unique__ = game_logic::wml_serializable_formula_callable::register_serializable_type(idname, [](variant v) ->variant { return variant(new classname(v)); });

#define READ_SERIALIZABLE_CALLABLE(node) if(node.has_key("_addr")) { set_addr(node["_addr"].as_string()); }

typedef boost::intrusive_ptr<wml_serializable_formula_callable> wml_serializable_formula_callable_ptr;
typedef boost::intrusive_ptr<const wml_serializable_formula_callable> const_wml_serializable_formula_callable_ptr;

class wml_formula_callable_serialization_scope
{
public:
	static void register_serialized_object(const_wml_serializable_formula_callable_ptr ptr);
	static bool is_active();

	wml_formula_callable_serialization_scope();
	~wml_formula_callable_serialization_scope();

	variant write_objects(variant obj, int* num_objects=0) const;

private:
};

class wml_formula_callable_read_scope
{
public:
	static void register_serialized_object(intptr_t addr, wml_serializable_formula_callable_ptr ptr);
	static wml_serializable_formula_callable_ptr get_serialized_object(intptr_t addr);
	wml_formula_callable_read_scope();
	~wml_formula_callable_read_scope();

	static bool try_load_object(intptr_t id, variant& v);
private:
};

std::string serialize_doc_with_objects(variant v);
variant deserialize_doc_with_objects(const std::string& msg);
variant deserialize_file_with_objects(const std::string& fname);

}

#endif
