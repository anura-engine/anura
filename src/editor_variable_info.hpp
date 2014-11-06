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
#ifndef EDITOR_VARIABLE_INFO_HPP_INCLUDED
#define EDITOR_VARIABLE_INFO_HPP_INCLUDED
#ifndef NO_EDITOR

#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

#include "formula_fwd.hpp"
#include "variant.hpp"

class editor_variable_info {
public:
	enum VARIABLE_TYPE { TYPE_INTEGER, XPOSITION, YPOSITION, TYPE_LEVEL, TYPE_LABEL, TYPE_TEXT, TYPE_BOOLEAN, TYPE_ENUM, TYPE_POINTS };

	explicit editor_variable_info(variant node);

	variant write() const;

	void set_name(const std::string& name);
	void set_is_property() { is_property_ = true; }
	bool is_property() const { return is_property_; }

	const std::string& variable_name() const { return name_; }
	VARIABLE_TYPE type() const { return type_; }
	const std::vector<std::string>& enum_values() const { return enum_values_; }
	const std::string& info() const { return info_; }
	const std::string& help() const { return help_; }

	const game_logic::const_formula_ptr& formula() const { return formula_; }

	bool numeric_decimal() const { return numeric_decimal_; }
	decimal numeric_min() const { return numeric_min_; }
	decimal numeric_max() const { return numeric_max_; }

private:
	std::string name_;
	bool is_property_;
	VARIABLE_TYPE type_;
	std::vector<std::string> enum_values_;
	std::string info_;
	std::string help_;
	game_logic::const_formula_ptr formula_;

	bool numeric_decimal_;
	decimal numeric_min_, numeric_max_;
};

class editor_entity_info {
public:
	explicit editor_entity_info(variant node);

	variant write() const;

	const std::string& category() const { return category_; }
	const std::string& classification() const { return classification_; }
	const std::vector<editor_variable_info>& vars() const { return vars_; }
	const std::vector<editor_variable_info>& properties() const { return properties_; }
	const std::vector<editor_variable_info>& vars_and_properties() const { return vars_and_properties_; }
	const editor_variable_info* get_var_info(const std::string& var_name) const;
	const editor_variable_info* get_property_info(const std::string& var_name) const;
	const editor_variable_info* get_var_or_property_info(const std::string& var_name) const;
	void add_property(const editor_variable_info& prop);
	const std::string& help() const { return help_; }
	const std::vector<std::string>& editable_events() const { return editable_events_; }
private:
	std::string category_, classification_;
	std::vector<editor_variable_info> vars_, properties_, vars_and_properties_;
	std::vector<std::string> editable_events_;
	std::string help_;
};

typedef boost::shared_ptr<editor_entity_info> editor_entity_info_ptr;
typedef boost::shared_ptr<const editor_entity_info> const_editor_entity_info_ptr;

#endif
#endif // !NO_EDITOR

