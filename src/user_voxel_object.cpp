#include <assert.h>

#include "object_events.hpp"
#include "user_voxel_object.hpp"

namespace voxel
{

user_voxel_object::user_voxel_object(const variant& node)
  : voxel_object(node), type_(voxel_object_type::get(node["type"].as_string()))
{
	data_.resize(type_->num_storage_slots());
}

user_voxel_object::user_voxel_object(const std::string& type, float x, float y, float z)
  : voxel_object(type, x, y, z), type_(voxel_object_type::get(type))
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

variant user_voxel_object::get_value_by_slot(int slot) const
{
	static const int NumBaseSlots = type_->num_base_slots();
	if(slot < NumBaseSlots) {
		return voxel_object::get_value_by_slot(slot);
	}

	slot -= NumBaseSlots;
	assert(slot < data_.size());
	return data_[slot];
}

void user_voxel_object::set_value_by_slot(int slot, const variant& value)
{
	static const int NumBaseSlots = type_->num_base_slots();
	if(slot < NumBaseSlots) {
		voxel_object::set_value_by_slot(slot, value);
		return;
	}

	slot -= NumBaseSlots;
	assert(slot < data_.size());
	data_[slot] = value;
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
