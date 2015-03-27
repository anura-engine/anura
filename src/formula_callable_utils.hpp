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

#include <boost/intrusive_ptr.hpp>

#include <vector>

#include "asserts.hpp"
#include "formula_callable.hpp"
#include "variant.hpp"

namespace game_logic
{
	class SlotFormulaCallable : public FormulaCallable
	{
	public:
		SlotFormulaCallable() : value_names_(nullptr), base_slot_(0)
		{}

		void setNames(const std::vector<std::string>* names) {
			value_names_ = names;
		}
		void setFallback(const ConstFormulaCallablePtr& fallback) { fallback_ = fallback; }
		void add(const variant& val) { values_.push_back(val); }
		variant& backDirectAccess() { return values_.back(); }
		void reserve(size_t n) { values_.reserve(n); }

		variant getValue(const std::string& key) const {
			if(value_names_) {
				for(int n = 0; n != value_names_->size(); ++n) {
					if((*value_names_)[n] == key) {
						return values_[n];
					}
				}
			}

			if(fallback_) {
				return fallback_->queryValue(key);
			}
			ASSERT_LOG(false, "GET VALUE " << key << " FROM SLOT CALLABLE");
			return variant();
		}

		variant getValueBySlot(int slot) const {
			if(slot < base_slot_) {
				return fallback_->queryValueBySlot(slot);
			}

			slot -= base_slot_;
			ASSERT_INDEX_INTO_VECTOR(slot, values_);
			return values_[slot];
		}

		void clear() {
			value_names_ = 0;
			values_.clear();
			fallback_ = ConstFormulaCallablePtr();
		}

		void setBaseSlot(int base) { base_slot_ = base; }

		int getNumArgs() const { return static_cast<int>(values_.size()); }

	private:
		const std::vector<std::string>* value_names_;
		std::vector<variant> values_;
		ConstFormulaCallablePtr fallback_;

		int base_slot_;
	};
}
