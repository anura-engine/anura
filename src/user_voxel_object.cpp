#include <assert.h>

#include "object_events.hpp"
#include "user_voxel_object.hpp"
#include "voxel_object_functions.hpp"

namespace voxel
{

user_voxel_object::user_voxel_object(const variant& node)
  : voxel_object(node), type_(voxel_object_type::get(node["type"].as_string())), data_target_(-1), created_(false)
{
	data_.resize(type_->num_storage_slots());
	std::vector<int> require_init;
	for(const voxel_object_type::property_entry& entry : type_->slot_properties()) {
		if(entry.storage_slot != -1) {
			if(entry.init) {
				data_[entry.storage_slot] = entry.init->execute(*this);
			} else {
				data_[entry.storage_slot] = entry.default_value;
			}
		}

		if(entry.requires_initialization) {
			require_init.push_back(entry.slot);
		}
	}

	for(const variant::map_pair& p : node.as_map()) {
		auto itor = type_->properties().find(p.first.as_string());
		if(itor != type_->properties().end()) {
			set_value_by_slot(type_->num_base_slots() + itor->second.slot, p.second);
			require_init.erase(std::remove(require_init.begin(), require_init.end(), itor->second.slot), require_init.end());
		}
	}

	ASSERT_LOG(require_init.empty(), "Object " << type_->id() << " did not have field " << type_->slot_properties()[require_init.front()].id << " initialized");
}

void user_voxel_object::process(level& lvl)
{
	voxel_object::process(lvl);

	if(!created_) {
		created_ = true;
		handle_event(OBJECT_EVENT_CREATE);
	}

	handle_event(OBJECT_EVENT_PROCESS);

}

bool user_voxel_object::execute_command(const variant& b)
{
	const voxel_object_command_callable* cmd = b.try_convert<voxel_object_command_callable>();
	if(cmd) {
		cmd->run_command(*level::current().iso_world().get(), *this);
		return true;
	}

	return formula_callable::execute_command(b);
}

void user_voxel_object::handle_event(int nevent, const formula_callable* context)
{
	set_event_arg(variant(context));
	const game_logic::formula* handler = type_->event_handler(nevent);
	if(handler) {
		variant result = handler->execute(*this);
		execute_command(result);
	}
}

void user_voxel_object::handle_event(const std::string& event, const formula_callable* context)
{
	handle_event(get_object_event_id(event), context);
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
	if(slot == voxel_object_type::ENTRY_ME) {
		return variant(this);
	}

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
		data_[entry.storage_slot] = value;
		return;
	} else {
		ASSERT_LOG(false, "Illegal property set " << type_->id() << "." << entry.id);
	}
}

variant user_voxel_object::get_value(const std::string& key) const
{
	for(int slot = 0; slot != type_->slot_properties().size(); ++slot) {
		if(type_->slot_properties()[slot].id == key) {
			return get_value_by_slot(type_->num_base_slots() + slot);
		}
	}
 
	ASSERT_LOG(false, "Unknown property " << type_->id() << "." << key);
	return variant();
}
 
void user_voxel_object::set_value(const std::string& key, const variant& value)
{
	for(int slot = 0; slot != type_->slot_properties().size(); ++slot) {
		if(type_->slot_properties()[slot].id == key) {
			set_value_by_slot(type_->num_base_slots() + slot, value);
			return;
		}
	}
 
	voxel_object::set_value(key, value);
}

}
