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
#ifndef FORMULA_OBJECT_HPP_INCLUDED
#define FORMULA_OBJECT_HPP_INCLUDED

#include <set>
#include <string>
#include <vector>

#include <boost/uuid/uuid.hpp>

#include "formula.hpp"
#include "formula_callable_definition.hpp"
#include "variant.hpp"
#include "wml_formula_callable.hpp"

namespace game_logic
{

class formula_class;

FormulaCallableDefinitionPtr get_class_definition(const std::string& name);

bool is_class_derived_from(const std::string& derived, const std::string& base);

class formula_object : public game_logic::WmlSerializableFormulaCallable
{
public:
	static void visit_variants(variant v, std::function<void (variant)> fn, std::vector<formula_object*>* seen=NULL);
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

	variant serializeToWml() const;

	variant getValue(const std::string& key) const;
	variant getValue_by_slot(int slot) const;
	void setValue(const std::string& key, const variant& value);
	void setValueBySlot(int slot, const variant& value);

	void getInputs(std::vector<formula_input>* inputs) const;

	boost::uuids::uuid id_;

	bool new_in_update_;
	bool orphaned_;

	boost::intrusive_ptr<formula_object> previous_;

	//overrides of the class's read-only properties.
	std::vector<formula_ptr> property_overrides_;

	std::vector<variant> variables_;

	boost::intrusive_ptr<const formula_class> class_;

	variant tmp_value_;

	//if this is non-zero, then private_data_ will be exposed via getValue.
	mutable int private_data_;
};

bool formula_class_valid(const std::string& type);

struct formula_class_manager {
	formula_class_manager();
	~formula_class_manager();
};


FormulaCallableDefinitionPtr get_library_definition();
FormulaCallablePtr get_library_object();

}

#endif
