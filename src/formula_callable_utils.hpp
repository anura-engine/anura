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

#include <vector>

#include "asserts.hpp"
#include "formula_callable.hpp"
#include "variant.hpp"

namespace game_logic
{
	template<size_t N>
	class NSlotFormulaCallable : public FormulaCallable
	{
	public:
		NSlotFormulaCallable(const ConstFormulaCallablePtr& fallback, int nbase_slot=0) : fallback_(fallback), base_slot_(0)
		{}

		~NSlotFormulaCallable() {
			for(int n = 0; n != N; ++n) {
				((variant*)&buf_[n*sizeof(variant)])->~variant();
			}
		}

		void set(int nslot, const variant& v) {
			new (&buf_[nslot*sizeof(variant)]) variant(v);
		}
		
		variant getValue(const std::string& key) const override {
			if(fallback_) {
				return fallback_->queryValue(key);
			}
			ASSERT_LOG(false, "GET VALUE " << key << " FROM SLOT CALLABLE");
			return variant();
		}

		variant getValueBySlot(int slot) const override {
			if(slot < base_slot_) {
				return fallback_->queryValueBySlot(slot);
			}

			slot -= base_slot_;

			return *(const variant*)buf_[slot*sizeof(variant)];
		}
	private:
		char buf_[sizeof(variant)*N];
		ConstFormulaCallablePtr fallback_;
		int base_slot_;
	};

	class SlotFormulaCallable : public FormulaCallable
	{
	public:
		SlotFormulaCallable() : value_names_(nullptr), base_slot_(0)
		{}

		void setNames(const std::vector<std::string>* names) {
			value_names_ = names;
		}
		void setFallback(const ConstFormulaCallablePtr& fallback) { fallback_ = fallback; }
		void add(const variant& val) { values_.emplace_back(val); }
		void setValues(const std::vector<variant>& values) { values_ = values; }
		void setValues(std::vector<variant>* values) { values->swap(values_); }
		variant& backDirectAccess() { return values_.back(); }
		void reserve(size_t n) { values_.reserve(n); }

		variant getValue(const std::string& key) const override {
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

		variant getValueBySlot(int slot) const override {
			if(slot < base_slot_) {
				return fallback_->queryValueBySlot(slot);
			}

			slot -= base_slot_;
			ASSERT_INDEX_INTO_VECTOR(slot, values_);
			return values_[slot];
		}

		void setValueBySlot(int slot, const variant& value) override {
			if(slot < base_slot_) {
				const_cast<FormulaCallable*>(fallback_.get())->mutateValueBySlot(slot, value);
				return;
			}

			ASSERT_LOG(false, "Trying to set value in non-mutable type");
		}

		void setValue(const std::string& key, const variant& value) override {
			const_cast<FormulaCallable*>(fallback_.get())->mutateValue(key, value);
		}

		void clear() {
			value_names_ = 0;
			values_.clear();
			fallback_ = ConstFormulaCallablePtr();
		}

		void setBaseSlot(int base) { base_slot_ = base; }

		int getNumArgs() const { return static_cast<int>(values_.size()); }

		void surrenderReferences(GarbageCollector* collector) override {
			collector->surrenderPtr(&fallback_);
			for(variant& v : values_) {
				collector->surrenderVariant(&v);
			}
		}
	
	protected:
		int baseSlot() const { return base_slot_; }
		const std::vector<variant>& getValues() const { return values_; }
		void setValueInternal(int slot, const variant& value) { values_[slot] = value; }

		const std::vector<std::string>* getValueNames() const { return value_names_; }

	private:
		const std::vector<std::string>* value_names_;
		std::vector<variant> values_;
		ConstFormulaCallablePtr fallback_;

		int base_slot_;
	};

	class MutableSlotFormulaCallable : public SlotFormulaCallable
	{
	public:

		void setValueBySlot(int slot, const variant& value) override {
			if(slot < baseSlot()) {
				SlotFormulaCallable::setValueBySlot(slot, value);
				return;
			}

			slot -= baseSlot();
			ASSERT_LOG(slot >= 0 && slot < static_cast<int>(getValues().size()), "Unknown slot: " << slot);

			setValueInternal(slot, value);
		}

		void setValue(const std::string& key, const variant& value) override {

			auto names = getValueNames();
			if(names) {
				int slot = 0;
				for(const std::string& name : *names) {
					if(name == key) {
						setValueInternal(slot, value);
						return;
					}

					++slot;
				}
			}

			SlotFormulaCallable::setValue(key, value);
		}
	};

}
