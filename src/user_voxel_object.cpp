#include <assert.h>

#include "object_events.hpp"
#include "user_voxel_object.hpp"

namespace voxel
{

user_voxel_object::user_voxel_object(const variant& node)
  : voxel_object(node), type_(voxel_object_type::get(node["type"].as_string())), data_target_(-1)
{
	data_.resize(type_->num_storage_slots());
}

user_voxel_object::user_voxel_object(const std::string& type, float x, float y, float z)
  : voxel_object(type, x, y, z), type_(voxel_object_type::get(type)), data_target_(-1)
{
	data_.resize(type_->num_storage_slots());
}

void user_voxel_object::process(level& lvl)
{
	voxel_object::process(lvl);

	handle_event(OBJECT_EVENT_PROCESS);
}

void user_voxel_object::handle_event(int nevent)
{
	const game_logic::formula* handler = type_->event_handler(nevent);
	if(handler) {
		variant result = handler->execute(*this);
		execute_command(result);
	}
}

void user_voxel_object::handle_event(const std::string& event)
{
	handle_event(get_object_event_id(event));
}

namespace {
template<typename T>
class ValueScopeSetter {
	T backup_;
	T* target_;
public:
	ValueScopeSetter(T* target, const T& value) : backup_(*target), target_(target)
	{
		*target_ = value;
	}

	~ValueScopeSetter()
	{
		*target_ = backup_;
	}
};
}

variant user_voxel_object::get_value_by_slot(int slot) const
{
	static const int NumBaseSlots = type_->num_base_slots();
	if(slot < NumBaseSlots) {
		return voxel_object::get_value_by_slot(slot);
	}

	slot -= NumBaseSlots;
	assert(slot < type_->slot_properties().size());
	const voxel_object_type::property_entry& entry = type_->slot_properties()[slot];
	if(entry.getter) {
		const ValueScopeSetter<variant> value_scope_setter(const_cast<variant*>(&data_[voxel_object_type::ENTRY_DATA]), entry.storage_slot == -1 ? variant() : data_[entry.storage_slot]);
		return entry.getter->execute(*this);
	} else if(entry.const_value) {
		return *entry.const_value;
	} else {
		assert(entry.storage_slot >= 0 && entry.storage_slot < data_.size());
		return data_[entry.storage_slot];
	}
}

void user_voxel_object::set_value_by_slot(int slot, const variant& value)
{
	static const int NumBaseSlots = type_->num_base_slots();
	if(slot < NumBaseSlots) {
		voxel_object::set_value_by_slot(slot, value);
		return;
	}

	slot -= NumBaseSlots;
	if(slot == voxel_object_type::ENTRY_DATA) {
		ASSERT_LOG(data_target_ >= 0 && data_target_ < data_.size(), "Illegal set of data when there is no storage slot: " << data_target_ << "/" << data_.size());
		data_[data_target_] = value;
		return;
	}

	assert(slot < type_->slot_properties().size());
	const voxel_object_type::property_entry& entry = type_->slot_properties()[slot];
	if(entry.setter) {
		variant cmd;
		{
			//make it so 'data' will evaluate to the backing variable in
			//this scope.
			const ValueScopeSetter<variant> value_scope_setter(&data_[voxel_object_type::ENTRY_VALUE], value);
			cmd = entry.setter->execute(*this);
		}

		{
			//make it so mutating 'value' will mutate the target storage
			//in this scope.
			ValueScopeSetter<int> target_scope_setter(&data_target_, entry.storage_slot);

			execute_command(cmd);
		}

		return;
	} else if(entry.storage_slot >= 0) {
		assert(entry.storage_slot < data_.size());
		data_[slot] = value;
		return;
	} else {
		ASSERT_LOG(false, "Illegal property set " << type_->id() << "." << entry.id);
	}
}

variant user_voxel_object::get_value(const std::string& key) const
{
	auto itor = type_->properties().find(key);
	if(itor != type_->properties().end()) {
		return get_value_by_slot(type_->num_base_slots() + itor->second.storage_slot);
	}

	ASSERT_LOG(false, "Unknown property " << type_->id() << "." << key);
	return variant();
}

void user_voxel_object::set_value(const std::string& key, const variant& value)
{
	auto itor = type_->properties().find(key);
	if(itor != type_->properties().end()) {
		set_value_by_slot(type_->num_base_slots() + itor->second.storage_slot, value);
		return;
	}

	ASSERT_LOG(false, "Unknown property " << type_->id() << "." << key);
}

}
