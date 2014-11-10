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

	const CustomObjectCallable* instance_ptr = NULL;
}

const CustomObjectCallable& CustomObjectCallable::instance()
{
	if(instance_ptr) {
		return *instance_ptr;
	}

	static const boost::intrusive_ptr<const CustomObjectCallable> obj(new CustomObjectCallable(true));
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
		{ "value", "any" },
		{ "_data", "any" },
		{ "arg", "object" },
		{ "consts", "any" },
		{ "type", "string" },
		{ "active", "any" },
		{ "lib", "library" },

		{ "time_in_animation", "int" },
		{ "time_in_animation_delta", "int" },
		{ "frame_in_animation", "int" },
		{ "level", "any" },

		{ "animation", "string/string|map" },
		{ "available_animations", "[string]" },

		{ "hitpoints", "int" },
		{ "max_hitpoints", "int" },
		{ "mass", "int" },
		{ "label", "string" },
		{ "x", "int/int|decimal" },
		{ "y", "int/int|decimal" },
		{ "xy", "[int]" },
		{ "z", "int" },

		{ "relative_x", "int/int|decimal" },
		{ "relative_y", "int/int|decimal" },
		{ "spawned_by", "null|custom_obj" },
		{ "spawned_children", "[custom_obj]" },

		{ "parent", "null|custom_obj" },
		{ "pivot", "string" },
		{ "zorder", "int" },
		{ "zsub_order", "int" },

		{ "previous_y", "int" },
		{ "x1", "int/int|decimal" },
		{ "x2", "int/int|decimal" },
		{ "y1", "int/int|decimal" },
		{ "y2", "int/int|decimal" },
		{ "w", "int" },
		{ "h", "int" },
		{ "mid_x", "int/int|decimal" },
		{ "mid_y", "int/int|decimal" },
		{ "mid_xy", "[int]" },
		{ "midpoint_x", "int/int|decimal" },
		{ "midpoint_y", "int/int|decimal" },
		{ "midpoint_xy", "[int]" },

		{ "solid_rect", "rect_obj" },
		{ "solid_mid_x", "int" },
		{ "solid_mid_y", "int" },
		{ "solid_mid_xy", "[int]" }, 

		{ "img_mid_x", "int" },
		{ "img_mid_y", "int" },
		{ "img_mid_xy", "int" },
		{ "img_w", "int" },
		{ "img_h", "int" },
		{ "img_wh", "int" },
		{ "front", "int" },
		{ "back", "int" },
		{ "cycle", "int" },
		{ "facing", "int" },
	
		{ "upside_down", "int" },
		{ "up", "int" },
		{ "down", "int" },
		{ "velocity_x", "int/int|decimal" },
		{ "velocity_y", "int/int|decimal" },
		{ "velocity_xy", "[int]" }, 

		{ "velocity_magnitude", "decimal" },
		{ "velocity_angle", "decimal" },

		{ "accel_x", "int/int|decimal" },
		{ "accel_y", "int/int|decimal" },
		{ "accel_xy", "[int]" },
		{ "gravity_shift", "int" },
		{ "platform_motion_x", "int" },

		{ "registry", "object" },
		{ "globals", "object" },
		{ "vars", "object" },
		{ "tmp", "object" },
		{ "group", "int" },

		{ "rotate", "decimal" },

		{ "me", "any" },
		{ "self", "any" },

		{ "red", "int" },
		{ "green", "int" },
		{ "blue", "int" },
		{ "alpha", "int" },
		{ "text_alpha", "int" },
		{ "damage", "int" },
		{ "hit_by", "null|custom_obj" },

		{ "is_standing", "bool" },
		{ "standing_info", "null|object" },
	
		{ "near_cliff_edge", "bool" },
		{ "distance_to_cliff", "int" },
	
		{ "slope_standing_on", "int" },
		{ "underwater", "bool" },
	
		{ "previous_water_bounds", "[int]" },
		{ "water_bounds", "null|[int]" },
		{ "water_object", "null|custom_obj" },
	
		{ "driver", "null|custom_obj" },
		{ "is_human", "bool" },
		{ "invincible", "int" },
	
		{ "sound_volume", "int" },
		{ "destroyed", "bool" },
		{ "is_standing_on_platform", "null|bool|custom_obj" },
		{ "standing_on", "null|custom_obj" },
	
		{ "shader", "null|shader_program" },
	{ "effects", "[shader_program]" },
		{ "variations", "[string]" },
	
		{ "attached_objects", "[custom_obj]" },
		{ "call_stack", "[string]" },
		{ "lights", "[object]" },
	
		{ "solid_dimensions_in", "[string]" },
		{ "solid_dimensions_not_in", "[string]" },
	
		{ "collide_dimensions_in", "[string]" },
		{ "collide_dimensions_not_in", "[string]" },
	
		{ "brightness", "int" },
		{ "current_generator", "object" },
		{ "tags", "object" },
		{ "draw_area", "any" },
		{ "scale", "decimal" },
	
		{ "activation_area", "null|[int|decimal]" },
		{ "clip_area", "null|[int]" },

		{ "always_active", "bool" },
		{ "activation_border", "int/int|decimal" },
		{ "fall_through_platforms", "any" },
		{ "has_feet", "bool" },
	
		{ "x_schedule", "any" },
		{ "y_schedule", "any" },
		{ "rotation_schedule", "any" },
		{ "schedule_speed", "any" },
	
		{ "schedule_expires", "any" },
	
		{ "platform_area", "null|[int]" },
		{ "platform_offsets", "[int]" },
		{ "custom_draw", "list" },
	
		{ "uv_array", "[decimal]" },
		{ "xy_array", "[decimal]" },
		{ "uv_segments", "[int]" },
	
		{ "draw_primitives", "[object]/[object|map]|map" },
		{ "event_handlers", "object" },
	
		{ "use_absolute_screen_coordinates", "bool" },
	
		{ "widgets", "object/[object|map]|object|map" },
		{ "widget_list", "[widget]" },
		{ "body", "any" },
		{ "paused", "bool" },
		{ "mouseover_delay", "int" },
		{ "mouseover_area", "[int]" },
		{ "particle_systems", "{string -> object}" },

		{ "truez", "bool" },
		{ "tx", "decimal" },
		{ "ty", "decimal" },
		{ "tz", "decimal" },

	{ "animated_movements", "[string]" },

		{ "ctrl_user_output", "any" },
	
		{ "ctrl_up", "bool" },
		{ "ctrl_down", "bool" },
		{ "ctrl_left", "bool" },
		{ "ctrl_right", "bool" },
	
		{ "ctrl_attack", "bool" },
		{ "ctrl_jump", "bool" },
		{ "ctrl_tongue", "bool" },
		{ "ctrl_user", "any" },

		//player-specific
		{ "difficulty", "int" },
		{ "can_interact", "bool" },
		{ "underwater_controls", "bool" },
		{ "ctrl_mod_key", "int" },
		{ "ctrl_keys", "[string]" },
		{ "ctrl_mice", "[[int|string]]" },
		{ "ctrl_tilt", "int" },
		{ "ctrl_x", "int" },
		{ "ctrl_y", "int" },
		{ "ctrl_reverse_ab", "bool" },
		{ "control_scheme", "string" },
		{ "vertical_look", "int" },
		{ "control_lock", "null|[string]" },
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
		return NULL;
	}

	return &entries_[slot];
}

const game_logic::FormulaCallableDefinition::Entry* CustomObjectCallable::getEntry(int slot) const
{
	if(slot < 0 || static_cast<unsigned>(slot) >= entries_.size()) {
		return NULL;
	}

	return &entries_[slot];
}

void CustomObjectCallable::addProperty(const std::string& id, variant_type_ptr type, variant_type_ptr write_type, bool requires_initialization, bool is_private)
{
	if(properties_.count(id) == 0) {
		properties_[id] = entries_.size();
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
