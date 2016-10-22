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

#include "asserts.hpp"
#include "custom_object_callable.hpp"
#include "formula_object.hpp"

namespace 
{
	std::vector<CustomObjectCallable::Entry>& global_entries() 
	{
		static std::vector<CustomObjectCallable::Entry> instance;
		return instance;
	}

	std::map<std::string, int>& keys_to_slots() 
	{
		static std::map<std::string, int> instance;
		return instance;
	}

	const CustomObjectCallable* instance_ptr = nullptr;
}

const CustomObjectCallable& CustomObjectCallable::instance()
{
	if(instance_ptr) {
		return *instance_ptr;
	}

	static const ffl::IntrusivePtr<const CustomObjectCallable> obj(new CustomObjectCallable(true));
	return *obj;
}

namespace 
{
	struct Property 
	{
		std::string id, type;
	};
}

CustomObjectCallable::CustomObjectCallable(bool is_singleton)
{
	if(is_singleton) {
		instance_ptr = this;
		setTypeName("custom_obj");
	}

	//make sure 'library' is initialized as a valid type.
	game_logic::get_library_definition();

	static const Property CustomObjectProperties[] = {
#define CUSTOM_OBJECT_CALLABLE_INC(def, value, vtype)	{ value, vtype },
#include "custom_object.inc"
#undef CUSTOM_OBJECT_CALLABLE_INC
	};
	ASSERT_EQ(NUM_CUSTOM_OBJECT_PROPERTIES, sizeof(CustomObjectProperties)/sizeof(*CustomObjectProperties));

	if(global_entries().empty()) {
		for(int n = 0; n != sizeof(CustomObjectProperties)/sizeof(*CustomObjectProperties); ++n) {
			global_entries().push_back(Entry(CustomObjectProperties[n].id));

			const std::string& type = CustomObjectProperties[n].type;
			std::string::const_iterator itor = std::find(type.begin(), type.end(), '/');
			std::string read_type(type.begin(), itor);
			global_entries().back().setVariantType(parse_variant_type(variant(read_type)));

			if(itor != type.end()) {
				global_entries().back().write_type = parse_variant_type(variant(std::string(itor+1, type.end())));
			}
		}

		for(int n = 0; n != global_entries().size(); ++n) {
			keys_to_slots()[global_entries()[n].id] = n;
		}

		global_entries()[CUSTOM_OBJECT_ME].setVariantType(variant_type::get_custom_object());
		global_entries()[CUSTOM_OBJECT_SELF].setVariantType(variant_type::get_custom_object());

		const variant_type_ptr builtin = variant_type::get_builtin("level");
		global_entries()[CUSTOM_OBJECT_LEVEL].setVariantType(builtin);
	}

	global_entries()[CUSTOM_OBJECT_PARENT].type_definition = is_singleton ? this : &instance();
	global_entries()[CUSTOM_OBJECT_LIB].type_definition = game_logic::get_library_definition().get();

	entries_ = global_entries();
}

void CustomObjectCallable::setObjectType(variant_type_ptr type)
{
	entries_[CUSTOM_OBJECT_ME].setVariantType(type);
	entries_[CUSTOM_OBJECT_SELF].setVariantType(type);
}

int CustomObjectCallable::getKeySlot(const std::string& key)
{
	std::map<std::string, int>::const_iterator itor = keys_to_slots().find(key);
	if(itor == keys_to_slots().end()) {
		return -1;
	}

	return itor->second;
}

int CustomObjectCallable::getSlot(const std::string& key) const
{
	std::map<std::string, int>::const_iterator itor = properties_.find(key);
	if(itor == properties_.end()) {
		return getKeySlot(key);
	} else {
		return itor->second;
	}
}

game_logic::FormulaCallableDefinition::Entry* CustomObjectCallable::getEntry(int slot)
{
	if(slot < 0 || static_cast<unsigned>(slot) >= entries_.size()) {
		return nullptr;
	}

	return &entries_[slot];
}

const game_logic::FormulaCallableDefinition::Entry* CustomObjectCallable::getEntry(int slot) const
{
	if(slot < 0 || static_cast<unsigned>(slot) >= entries_.size()) {
		return nullptr;
	}

	return &entries_[slot];
}

void CustomObjectCallable::addProperty(const std::string& id, variant_type_ptr type, variant_type_ptr write_type, bool requires_initialization, bool is_private)
{
	if(properties_.count(id) == 0) {
		properties_[id] = static_cast<int>(entries_.size());
		entries_.push_back(Entry(id));
	}

	const int slot = properties_[id];

	if(requires_initialization && std::count(slots_requiring_initialization_.begin(), slots_requiring_initialization_.end(), slot) == 0) {
		slots_requiring_initialization_.push_back(slot);
	}

	//do NOT call setVariantType() because that will do queries of
	//objects and such and we want this operation to avoid doing that, because
	//it might be called at a sensitive time when we don't want to
	//instantiate new object definitions.
	entries_[slot].variant_type = type;
	entries_[slot].write_type = write_type;
	entries_[slot].private_counter = is_private ? 1 : 0;
}

void CustomObjectCallable::finalizeProperties()
{
	for(Entry& e : entries_) {
		e.setVariantType(e.variant_type);
	}
}

void CustomObjectCallable::pushPrivateAccess()
{
	for(Entry& e : entries_) {
		e.private_counter--;
	}
}

void CustomObjectCallable::popPrivateAccess()
{
	for(Entry& e : entries_) {
		e.private_counter++;
	}
}

CustomObjectCallableExposePrivateScope::CustomObjectCallableExposePrivateScope(CustomObjectCallable& c) : c_(c)
{
	c_.pushPrivateAccess();
}

CustomObjectCallableExposePrivateScope::~CustomObjectCallableExposePrivateScope()
{
	c_.popPrivateAccess();
}

CustomObjectCallableModifyScope::CustomObjectCallableModifyScope(const CustomObjectCallable& c, int slot, variant_type_ptr type)
	: c_(const_cast<CustomObjectCallable&>(c)), entry_(*c.getEntry(slot)),
	  slot_(slot)
{
	c_.getEntry(slot_)->setVariantType(type);
}

CustomObjectCallableModifyScope::~CustomObjectCallableModifyScope()
{
	*c_.getEntry(slot_) = entry_;
}
