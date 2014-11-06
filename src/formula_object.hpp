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
#ifndef FORMULA_OBJECT_HPP_INCLUDED
#define FORMULA_OBJECT_HPP_INCLUDED

#include <set>
#include <string>
#include <vector>

#include <boost/function.hpp>
#include <boost/uuid/uuid.hpp>

#include "formula.hpp"
#include "formula_callable_definition.hpp"
#include "variant.hpp"
#include "wml_formula_callable.hpp"

namespace game_logic
{

class formula_class;

formula_callable_definition_ptr get_class_definition(const std::string& name);

bool is_class_derived_from(const std::string& derived, const std::string& base);

class formula_object : public game_logic::wml_serializable_formula_callable
{
public:
	static void visit_variants(variant v, boost::function<void (variant)> fn, std::vector<formula_object*>* seen=NULL);
	static void map_object_into_different_tree(variant& v, const std::map<formula_object*, formula_object*>& mapping, std::vector<formula_object*>* seen=NULL);

	void update(formula_object& updated);

	static variant deep_clone(variant v);
	static variant deep_clone(variant v, std::map<formula_object*,formula_object*>& mapping);

	static void reload_classes();
	static void load_all_classes();
	static void try_load_class(const std::string& name);

	static boost::intrusive_ptr<formula_object> create(const std::string& type, variant args=variant());

	bool is_a(const std::string& class_name) const;
	const std::string& get_class_name() const;

	//construct with data representing private/internal represenation.
	explicit formula_object(variant data);
	virtual ~formula_object();

	boost::intrusive_ptr<formula_object> clone() const;

	void validate() const;
private:
	//construct with type and constructor parameters.
	//Don't call directly, use create() instead.
	explicit formula_object(const std::string& type, variant args=variant());
	void call_constructors(variant args);

	variant serialize_to_wml() const;

	variant get_value(const std::string& key) const;
	variant get_value_by_slot(int slot) const;
	void set_value(const std::string& key, const variant& value);
	void set_value_by_slot(int slot, const variant& value);

	void get_inputs(std::vector<formula_input>* inputs) const;

	boost::uuids::uuid id_;

	bool new_in_update_;
	bool orphaned_;

	boost::intrusive_ptr<formula_object> previous_;

	//overrides of the class's read-only properties.
	std::vector<formula_ptr> property_overrides_;

	std::vector<variant> variables_;

	boost::intrusive_ptr<const formula_class> class_;

	variant tmp_value_;

	//if this is non-zero, then private_data_ will be exposed via get_value.
	mutable int private_data_;
};

bool formula_class_valid(const std::string& type);

struct formula_class_manager {
	formula_class_manager();
	~formula_class_manager();
};


formula_callable_definition_ptr get_library_definition();
formula_callable_ptr get_library_object();

}

#endif
