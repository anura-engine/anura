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

#include "asserts.hpp"
#include "formula_variable_storage.hpp"
#include "variant_utils.hpp"

namespace game_logic
{
	FormulaVariableStorage::FormulaVariableStorage() 
		: disallow_new_keys_(false)
	{}

	FormulaVariableStorage::FormulaVariableStorage(const std::map<std::string, variant>& m) 
		: disallow_new_keys_(false)
	{
		for(std::map<std::string, variant>::const_iterator i = m.begin(); i != m.end(); ++i) {
			add(i->first, i->second);
		}
	}

	void FormulaVariableStorage::setObjectName(const std::string& name)
	{
		debug_object_name_ = name;
	}

	bool FormulaVariableStorage::isEqualTo(const std::map<std::string, variant>& m) const
	{
		if(m.size() != strings_to_values_.size()) {
			return false;
		}

		std::map<std::string, int>::const_iterator i = strings_to_values_.begin();
		std::map<std::string, variant>::const_iterator j = m.begin();

		while(i != strings_to_values_.end()) {
			if(i->first != j->first || j->second != values_[i->second]) {
				return false;
			}

			++i;
			++j;
		}

		return true;
	}

	void FormulaVariableStorage::read(variant node)
	{
		if(node.is_null()) {
			return;
		}

		for(const variant_pair& val : node.as_map()) {
			add(val.first.as_string(), val.second);
		}
	}

	variant FormulaVariableStorage::write() const
	{
		variant_builder node;
		for(std::map<std::string,int>::const_iterator i = strings_to_values_.begin(); i != strings_to_values_.end(); ++i) {
			node.add(i->first, values_[i->second]);
		}

		return node.build();
	}

	void FormulaVariableStorage::add(const std::string& key, const variant& value)
	{
		std::map<std::string,int>::const_iterator i = strings_to_values_.find(key);
		if(i != strings_to_values_.end()) {
			values_[i->second] = value;
		} else {
			ASSERT_LOG(!disallow_new_keys_, "UNKNOWN KEY SET IN VAR STORAGE: " << key << " in object '" << debug_object_name_ << "'");
			strings_to_values_[key] = static_cast<int>(values_.size());
			values_.push_back(value);
		}
	}

	void FormulaVariableStorage::add(const FormulaVariableStorage& value)
	{
		for(std::map<std::string, int>::const_iterator i = value.strings_to_values_.begin(); i != value.strings_to_values_.end(); ++i) {
			add(i->first, value.values_[i->second]);
		}
	}

	variant FormulaVariableStorage::getValue(const std::string& key) const
	{
		std::map<std::string,int>::const_iterator i = strings_to_values_.find(key);
		if(i != strings_to_values_.end()) {
			return values_[i->second];
		} else {
			ASSERT_LOG(!disallow_new_keys_, "UNKNOWN KEY ACCESSED IN VAR STORAGE: " << key);
			return variant();
		}
	}

	variant FormulaVariableStorage::getValueBySlot(int slot) const
	{
		return values_[slot];
	}

	void FormulaVariableStorage::setValue(const std::string& key, const variant& value)
	{
		add(key, value);
	}

	void FormulaVariableStorage::setValueBySlot(int slot, const variant& value)
	{
		values_[slot] = value;
	}

	void FormulaVariableStorage::getInputs(std::vector<FormulaInput>* inputs) const
	{
		for(std::map<std::string,int>::const_iterator i = strings_to_values_.begin(); i != strings_to_values_.end(); ++i) {
			inputs->push_back(FormulaInput(i->first, FORMULA_ACCESS_TYPE::READ_WRITE));
		}
	}

	std::vector<std::string> FormulaVariableStorage::keys() const
	{
		std::vector<std::string> result;
		for(std::map<std::string, int>::const_iterator i = strings_to_values_.begin(); i != strings_to_values_.end(); ++i) {
			result.push_back(i->first);
		}

		return result;
	}
}
