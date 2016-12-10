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

#include "formula_garbage_collector.hpp"
#include "formula_callable.hpp"
#include "formula_function.hpp"

namespace game_logic
{
	//helper struct which contains info for a where expression.
	struct WhereVariablesInfo : public FormulaCallable 
	{
		explicit WhereVariablesInfo(int nslot) : base_slot(nslot) {}
		variant getValue(const std::string& key) const override { return variant(); }
		std::vector<std::string> names;
		std::vector<ExpressionPtr> entries;
		int base_slot;
		ConstFormulaCallableDefinitionPtr callable_where_def;
	};

	typedef ffl::IntrusivePtr<WhereVariablesInfo> WhereVariablesInfoPtr;

	class WhereVariables : public FormulaCallable {
	public:
		WhereVariables(const FormulaCallable &base, WhereVariablesInfoPtr info);

	private:
		ffl::IntrusivePtr<const FormulaCallable> base_;
		WhereVariablesInfoPtr info_;

		struct CacheEntry {
			CacheEntry() : have_result(false)
			{}

			variant result;
			bool have_result;
		};

		mutable std::vector<CacheEntry> results_cache_;

		void surrenderReferences(GarbageCollector* collector) override;
		void setValueBySlot(int slot, const variant& value) override;
		void setValue(const std::string& key, const variant& value) override;
		variant getValueBySlot(int slot) const override;
		variant getValue(const std::string& key) const override;

	};

}
