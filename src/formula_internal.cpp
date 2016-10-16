#include "formula.hpp"
#include "formula_internal.hpp"

namespace game_logic
{
	map_callable::map_callable(const FormulaCallable& backup)
	: backup_(&backup), index_(0)
	{}

	map_callable::map_callable(const FormulaCallable& backup, const variant& list)
	: backup_(&backup), index_(0)
	{
		value_ = list[0];
	}

	void map_callable::setValue_name(const std::string& name) { value_name_ = name; }

	bool map_callable::next(const variant& list)
	{
		++index_;
		if(index_ >= list.num_elements()) {
			return false;
		} else {
			value_ = list[index_];
			return true;
		}
	}

	void map_callable::set(const variant& v, int i)
	{
		value_ = v;
		index_ = i;
	}

	void map_callable::set(const variant& k, const variant& v, int i)
	{
		key_ = k;
		value_ = v;
		index_ = i;
	}

	variant map_callable::getValue(const std::string& key) const {
		if((value_name_.empty() && key == "value") ||
		   (!value_name_.empty() && key == value_name_)) {
			return value_;
		} else if(key == "index") {
			return variant(index_);
		} else if(key == "context") {
			return variant(backup_.get());
		} else if(key == "key") {
			return key_;
		} else {
			return backup_->queryValue(key);
		}
	}

	variant map_callable::getValueBySlot(int slot) const {
		ASSERT_LOG(slot >= 0, "BAD SLOT VALUE: " << slot);
		if(slot < NUM_MAP_CALLABLE_SLOTS) {
			switch(slot) {
				case MAP_CALLABLE_VALUE: return value_;
				case MAP_CALLABLE_INDEX: return variant(index_);
				case MAP_CALLABLE_CONTEXT: return variant(backup_.get());
				case MAP_CALLABLE_KEY: return key_;
				default: ASSERT_LOG(false, "BAD GET VALUE BY SLOT");
			}
		} else if(backup_) {
			return backup_->queryValueBySlot(slot - NUM_MAP_CALLABLE_SLOTS);
		} else {
			ASSERT_LOG(false, "COULD NOT FIND VALUE FOR SLOT: " << slot);
		}
	}

	void map_callable::setValue(const std::string& key, const variant& value) {
		const_cast<FormulaCallable*>(backup_.get())->mutateValue(key, value);
	}

	void map_callable::setValueBySlot(int slot, const variant& value) {
		ASSERT_LOG(slot >= NUM_MAP_CALLABLE_SLOTS, "Illegal variable mutation");
		const_cast<FormulaCallable*>(backup_.get())->mutateValueBySlot(slot-NUM_MAP_CALLABLE_SLOTS, value);
	}

	void map_callable::surrenderReferences(GarbageCollector* collector) {
		collector->surrenderPtr(&backup_);
		collector->surrenderVariant(&key_);
		collector->surrenderVariant(&value_);
	}
}
