/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "asserts.hpp"
#include "custom_object_callable.hpp"
#include "foreach.hpp"
#include "formula_object.hpp"
#include "level.hpp"

namespace {
std::vector<custom_object_callable::entry>& global_entries() {
	static std::vector<custom_object_callable::entry> instance;
	return instance;
}

std::map<std::string, int>& keys_to_slots() {
	static std::map<std::string, int> instance;
	return instance;
}

const custom_object_callable* instance_ptr = NULL;

}

const custom_object_callable& custom_object_callable::instance()
{
	if(instance_ptr) {
		return *instance_ptr;
	}

	static const boost::intrusive_ptr<const custom_object_callable> obj(new custom_object_callable(true));
	return *obj;
}

namespace {
struct Property {
	std::string id, type;
};
}

custom_object_callable::custom_object_callable(bool is_singleton)
{
	if(is_singleton) {
		instance_ptr = this;
		set_type_name("custom_obj");
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

	{ "animation", "string|map" },
	{ "available_animations", "[string]" },

	{ "hitpoints", "int" },
	{ "max_hitpoints", "int" },
	{ "mass", "int" },
	{ "label", "string" },
	{ "x", "int" },
	{ "y", "int" },
	{ "xy", "[int]" },
	{ "z", "int" },

	{ "relative_x", "int" },
	{ "relative_y", "int" },
	{ "spawned_by", "null|custom_obj" },
	{ "spawned_children", "[custom_obj]" },

	{ "parent", "null|custom_obj" },
	{ "pivot", "string" },
	{ "zorder", "int" },
	{ "zsub_order", "int" },

	{ "previous_y", "int" },
	{ "x1", "int" },
	{ "x2", "int" },
	{ "y1", "int" },
	{ "y2", "int" },
	{ "w", "int" },
	{ "h", "int" },
	{ "mid_x", "int" },
	{ "mid_y", "int" },
	{ "mid_xy", "[int]" },
	{ "midpoint_x", "int" },
	{ "midpoint_y", "int" },
	{ "midpoint_xy", "int" }, 

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

	{ "accel_x", "int" },
	{ "accel_y", "int" },
	{ "accel_xy", "int" },
	{ "gravity_shift", "int" },
	{ "platform_motion_x", "int" },

	{ "registry", "object" },
	{ "globals", "object" },
	{ "vars", "object" },
	{ "tmp", "object" },
	{ "group", "int" },

	{ "rotate", "decimal" },
	{ "rotate_x", "decimal" },
	{ "rotate_y", "decimal" },
	{ "rotate_z", "decimal" },

	{ "me", "any" },
	{ "self", "any" },

	{ "red", "int" },
	{ "green", "int" },
	{ "blue", "int" },
	{ "alpha", "int" },
	{ "text_alpha", "int" },
	{ "damage", "int" },
	{ "hit_by", "null|custom_obj" },

	{ "distortion", "null|object" },
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
	{ "invincible", "bool" },
	
	{ "sound_volume", "int" },
	{ "destroyed", "bool" },
	{ "is_standing_on_platform", "null|bool|custom_obj" },
	{ "standing_on", "null|custom_obj" },
	
	{ "shader", "null|shader_program" },
	{ "effects", "[object]" },
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
	
	{ "activation_area", "null|[int]" },
	{ "clip_area", "null|[int]" },

	{ "always_active", "bool" },
	{ "activation_border", "int" },
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
	{ "textv", "any" },
	{ "body", "any" },
	{ "paused", "bool" },
	{ "mouseover_delay", "int" },
	{ "mouseover_area", "[int]" },
	{ "particle_systems", "{string -> object}" },

	{ "truez", "bool" },
	{ "tx", "decimal" },
	{ "ty", "decimal" },
	{ "tz", "decimal" },

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
			global_entries().push_back(entry(CustomObjectProperties[n].id));

			const std::string& type = CustomObjectProperties[n].type;
			std::string::const_iterator itor = std::find(type.begin(), type.end(), '/');
			std::string read_type(type.begin(), itor);
			global_entries().back().set_variant_type(parse_variant_type(variant(read_type)));

			if(itor != type.end()) {
				global_entries().back().write_type = parse_variant_type(variant(std::string(itor+1, type.end())));
			}
		}

		for(int n = 0; n != global_entries().size(); ++n) {
			keys_to_slots()[global_entries()[n].id] = n;
		}

		global_entries()[CUSTOM_OBJECT_ME].set_variant_type(variant_type::get_custom_object());
		global_entries()[CUSTOM_OBJECT_SELF].set_variant_type(variant_type::get_custom_object());

		const variant_type_ptr builtin = variant_type::get_builtin("level");
		global_entries()[CUSTOM_OBJECT_LEVEL].set_variant_type(builtin);
	}

	global_entries()[CUSTOM_OBJECT_PARENT].type_definition = is_singleton ? this : &instance();
	global_entries()[CUSTOM_OBJECT_LIB].type_definition = game_logic::get_library_definition().get();

	entries_ = global_entries();
}

void custom_object_callable::set_object_type(variant_type_ptr type)
{
	entries_[CUSTOM_OBJECT_ME].set_variant_type(type);
	entries_[CUSTOM_OBJECT_SELF].set_variant_type(type);
}

int custom_object_callable::get_key_slot(const std::string& key)
{
	std::map<std::string, int>::const_iterator itor = keys_to_slots().find(key);
	if(itor == keys_to_slots().end()) {
		return -1;
	}

	return itor->second;
}

int custom_object_callable::get_slot(const std::string& key) const
{
	std::map<std::string, int>::const_iterator itor = properties_.find(key);
	if(itor == properties_.end()) {
		return get_key_slot(key);
	} else {
		return itor->second;
	}
}

game_logic::formula_callable_definition::entry* custom_object_callable::get_entry(int slot)
{
	if(slot < 0 || slot >= entries_.size()) {
		return NULL;
	}

	return &entries_[slot];
}

const game_logic::formula_callable_definition::entry* custom_object_callable::get_entry(int slot) const
{
	if(slot < 0 || slot >= entries_.size()) {
		return NULL;
	}

	return &entries_[slot];
}

void custom_object_callable::add_property(const std::string& id, variant_type_ptr type, variant_type_ptr write_type, bool requires_initialization, bool is_private)
{
	if(properties_.count(id) == 0) {
		properties_[id] = entries_.size();
		entries_.push_back(entry(id));
	}

	const int slot = properties_[id];

	if(requires_initialization && std::count(slots_requiring_initialization_.begin(), slots_requiring_initialization_.end(), slot) == 0) {
		slots_requiring_initialization_.push_back(slot);
	}

	//do NOT call set_variant_type() because that will do queries of
	//objects and such and we want this operation to avoid doing that, because
	//it might be called at a sensitive time when we don't want to
	//instantiate new object definitions.
	entries_[slot].variant_type = type;
	entries_[slot].write_type = write_type;
	entries_[slot].private_counter = is_private ? 1 : 0;
}

void custom_object_callable::finalize_properties()
{
	foreach(entry& e, entries_) {
		e.set_variant_type(e.variant_type);
	}
}

void custom_object_callable::push_private_access()
{
	foreach(entry& e, entries_) {
		e.private_counter--;
	}
}

void custom_object_callable::pop_private_access()
{
	foreach(entry& e, entries_) {
		e.private_counter++;
	}
}

custom_object_callable_expose_private_scope::custom_object_callable_expose_private_scope(custom_object_callable& c) : c_(c)
{
	c_.push_private_access();
}

custom_object_callable_expose_private_scope::~custom_object_callable_expose_private_scope()
{
	c_.pop_private_access();
}

custom_object_callable_modify_scope::custom_object_callable_modify_scope(const custom_object_callable& c, int slot, variant_type_ptr type)
	: c_(const_cast<custom_object_callable&>(c)), entry_(*c.get_entry(slot)),
	  slot_(slot)
{
	c_.get_entry(slot_)->set_variant_type(type);
}

custom_object_callable_modify_scope::~custom_object_callable_modify_scope()
{
	*c_.get_entry(slot_) = entry_;
}
