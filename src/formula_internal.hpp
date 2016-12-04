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

#include "formula_function.hpp"

namespace game_logic
{
	enum MAP_CALLABLE_SLOT { MAP_CALLABLE_VALUE, MAP_CALLABLE_INDEX, MAP_CALLABLE_CONTEXT, MAP_CALLABLE_KEY, NUM_MAP_CALLABLE_SLOTS };

	class map_callable : public FormulaCallable {
	public:
		map_callable(const FormulaCallable& backup, int num_slots);

		//set to the first item in the given list.
		map_callable(const FormulaCallable& backup, const variant& list, int num_slots);
		void setValue_name(const std::string& name);

		//load next item in the given list.
		bool next(const variant& list);

		void set(const variant& v, int i);
		void set(const variant& k, const variant& v, int i);
	private:
		variant getValue(const std::string& key) const override;
		variant getValueBySlot(int slot) const override;
		void setValue(const std::string& key, const variant& value) override;
		void setValueBySlot(int slot, const variant& value) override;
		void surrenderReferences(GarbageCollector* collector) override;

		const ConstFormulaCallablePtr backup_;
		variant key_;
		variant value_;
		int index_, num_slots_;

		std::string value_name_;
	};
}
