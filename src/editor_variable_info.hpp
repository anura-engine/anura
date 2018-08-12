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

#pragma once

#ifndef NO_EDITOR

#include <string>
#include <vector>

#include "formula_fwd.hpp"
#include "variant.hpp"

enum class VARIABLE_TYPE { INTEGER, XPOSITION, YPOSITION, LEVEL, LABEL, TEXT, BOOLEAN, ENUM, POINTS };

class EditorVariableInfo 
{
public:
	explicit EditorVariableInfo(const variant& node);

	variant write() const;

	void setName(const std::string& name);
	void setIsProperty() { is_property_ = true; }
	bool isProperty() const { return is_property_; }

	const std::string& getVariableName() const { return name_; }
	VARIABLE_TYPE getType() const { return type_; }
	const std::vector<std::string>& getEnumValues() const { return enum_values_; }
	const std::string& getInfo() const { return info_; }
	const std::string& getHelp() const { return help_; }

	const game_logic::ConstFormulaPtr& getFormula() const { return formula_; }

	bool numericDecimal() const { return numeric_decimal_; }
	decimal numericMin() const { return numeric_min_; }
	decimal numericMax() const { return numeric_max_; }

	bool realEnum() const { return is_real_enum_; }

private:
	std::string name_;
	bool is_property_;
	VARIABLE_TYPE type_;
	bool is_real_enum_;
	std::vector<std::string> enum_values_;
	std::string info_;
	std::string help_;
	game_logic::ConstFormulaPtr formula_;

	bool numeric_decimal_;
	decimal numeric_min_, numeric_max_;
};

class EditorEntityInfo 
{
public:
	explicit EditorEntityInfo(const variant& node);

	variant write() const;

	const std::string& getCategory() const { return category_; }
	const std::string& getClassification() const { return classification_; }
	const std::vector<EditorVariableInfo>& getVars() const { return vars_; }
	const std::vector<EditorVariableInfo>& getProperties() const { return properties_; }
	const std::vector<EditorVariableInfo>& getVarsAndProperties() const { return vars_and_properties_; }
	const EditorVariableInfo* getVarInfo(const std::string& var_name) const;
	const EditorVariableInfo* getPropertyInfo(const std::string& var_name) const;
	const EditorVariableInfo* getVarOrPropertyInfo(const std::string& var_name) const;
	void addProperty(const EditorVariableInfo& prop);
	const std::string& getHelp() const { return help_; }
	const std::vector<std::string>& getEditableEvents() const { return editable_events_; }
private:
	std::string category_, classification_;
	std::vector<EditorVariableInfo> vars_, properties_, vars_and_properties_;
	std::vector<std::string> editable_events_;
	std::string help_;
};

typedef std::shared_ptr<EditorEntityInfo> EditorEntityInfoPtr;
typedef std::shared_ptr<const EditorEntityInfo> ConstEditorEntityInfoPtr;

#endif // !NO_EDITOR
