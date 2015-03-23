// XXX needs fixing
#if 0 
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
		for(const voxel_object_type::PropertyEntry& entry : type_->getSlotProperties()) {
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
				setValueBySlot(type_->num_base_slots() + itor->second.slot, p.second);
				require_init.erase(std::remove(require_init.begin(), require_init.end(), itor->second.slot), require_init.end());
			}
		}

		ASSERT_LOG(require_init.empty(), "Object " << type_->id() << " did not have field " << type_->getSlotProperties()[require_init.front()].id << " initialized");
	}

	void user_voxel_object::process(level& lvl)
	{
		voxel_object::process(lvl);

		if(!created_) {
			created_ = true;
			handleEvent(OBJECT_EVENT_CREATE);
		}

		handleEvent(OBJECT_EVENT_PROCESS);

	}

	bool user_voxel_object::executeCommand(const variant& b)
	{
		const voxel_object_command_callable* cmd = b.try_convert<voxel_object_command_callable>();
		if(cmd) {
			cmd->runCommand(*Level::current().iso_world().get(), *this);
			return true;
		}

		return FormulaCallable::executeCommand(b);
	}

	void user_voxel_object::handleEvent(int nevent, const FormulaCallable* context)
	{
		set_event_arg(variant(context));
		const game_logic::Formula* handler = type_->event_handler(nevent);
		if(handler) {
			variant result = handler->execute(*this);
			executeCommand(result);
		}
	}

	void user_voxel_object::handleEvent(const std::string& event, const FormulaCallable* context)
	{
		handleEvent(get_object_event_id(event), context);
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

	variant user_voxel_object::getValueBySlot(int slot) const
	{
		static const int NumBaseSlots = type_->num_base_slots();
		if(slot < NumBaseSlots) {
			return voxel_object::getValueBySlot(slot);
		}

		slot -= NumBaseSlots;
		if(slot == voxel_object_type::ENTRY_ME) {
			return variant(this);
		}

		assert(slot < type_->getSlotProperties().size());
		const voxel_object_type::PropertyEntry& entry = type_->getSlotProperties()[slot];
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

	void user_voxel_object::setValueBySlot(int slot, const variant& value)
	{
		static const int NumBaseSlots = type_->num_base_slots();
		if(slot < NumBaseSlots) {
			voxel_object::setValueBySlot(slot, value);
			return;
		}

		slot -= NumBaseSlots;
		if(slot == voxel_object_type::ENTRY_DATA) {
			ASSERT_LOG(data_target_ >= 0 && data_target_ < data_.size(), "Illegal set of data when there is no storage slot: " << data_target_ << "/" << data_.size());
			data_[data_target_] = value;
			return;
		}

		assert(slot < type_->getSlotProperties().size());
		const voxel_object_type::PropertyEntry& entry = type_->getSlotProperties()[slot];
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

				executeCommand(cmd);
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

	variant user_voxel_object::getValue(const std::string& key) const
	{
		for(int slot = 0; slot != type_->getSlotProperties().size(); ++slot) {
			if(type_->getSlotProperties()[slot].id == key) {
				return getValueBySlot(type_->num_base_slots() + slot);
			}
		}
 
		ASSERT_LOG(false, "Unknown property " << type_->id() << "." << key);
		return variant();
	}
 
	void user_voxel_object::setValue(const std::string& key, const variant& value)
	{
		for(int slot = 0; slot != type_->getSlotProperties().size(); ++slot) {
			if(type_->getSlotProperties()[slot].id == key) {
				setValueBySlot(type_->num_base_slots() + slot, value);
				return;
			}
		}
 
		voxel_object::setValue(key, value);
	}
}
#endif

