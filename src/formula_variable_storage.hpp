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

#include "intrusive_ptr.hpp"

#include "formula_callable.hpp"
#include "variant.hpp"

namespace game_logic
{
	class FormulaVariableStorage : public FormulaCallable
	{
	public:
		FormulaVariableStorage();
		explicit FormulaVariableStorage(const std::map<std::string, variant>& m);

		void setObjectName(const std::string& name);

		bool isEqualTo(const std::map<std::string, variant>& m) const;

		void read(variant node);
		variant write() const;
		void add(const std::string& key, const variant& value);
		void add(const FormulaVariableStorage& value);

		std::vector<variant>& values() { return values_; }
		const std::vector<variant>& values() const { return values_; }

		std::vector<std::string> keys() const;

		void disallowNewKeys(bool value=true) { disallow_new_keys_ = value; }

		void surrenderReferences(GarbageCollector* collector) override;

	private:
		variant getValue(const std::string& key) const override;
		variant getValueBySlot(int slot) const override;
		void setValue(const std::string& key, const variant& value) override;
		void setValueBySlot(int slot, const variant& value) override;

		void getInputs(std::vector<FormulaInput>* inputs) const override;

		std::string debug_object_name_;
	
		std::vector<variant> values_;
		std::map<std::string, int> strings_to_values_;

		bool disallow_new_keys_;
	};

	typedef ffl::IntrusivePtr<FormulaVariableStorage> FormulaVariableStoragePtr;
	typedef ffl::IntrusivePtr<const FormulaVariableStorage> ConstFormulaVariableStoragePtr;

}
