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
#ifndef FORMULA_VARIABLE_STORAGE_HPP_INCLUDED
#define FORMULA_VARIABLE_STORAGE_HPP_INCLUDED

#include <boost/intrusive_ptr.hpp>

#include "formula_callable.hpp"
#include "variant.hpp"

namespace game_logic
{

class formula_variable_storage : public formula_callable
{
public:
	formula_variable_storage();
	explicit formula_variable_storage(const std::map<std::string, variant>& m);

	void set_object_name(const std::string& name);

	bool equal_to(const std::map<std::string, variant>& m) const;

	void read(variant node);
	variant write() const;
	void add(const std::string& key, const variant& value);
	void add(const formula_variable_storage& value);

	std::vector<variant>& values() { return values_; }
	const std::vector<variant>& values() const { return values_; }

	std::vector<std::string> keys() const;

	void disallow_new_keys(bool value=true) { disallow_new_keys_ = value; }

private:
	variant get_value(const std::string& key) const;
	variant get_value_by_slot(int slot) const;
	void set_value(const std::string& key, const variant& value);
	void set_value_by_slot(int slot, const variant& value);

	void get_inputs(std::vector<formula_input>* inputs) const;

	std::string debug_object_name_;
	
	std::vector<variant> values_;
	std::map<std::string, int> strings_to_values_;

	bool disallow_new_keys_;
};

typedef boost::intrusive_ptr<formula_variable_storage> formula_variable_storage_ptr;
typedef boost::intrusive_ptr<const formula_variable_storage> const_formula_variable_storage_ptr;

}

#endif
