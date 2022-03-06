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

#include <map>
#include <string>
#include <vector>

#include "asserts.hpp"
#include "object_events.hpp"

namespace
{
	std::vector<std::string> create_object_event_names()
	{
		static const std::vector<std::string> res {
			"any",
			"start_level",
			"player_death",
			"load",
			"load_checkpoint",
			"construct",
			"create",
			"done_create",
			"become_active",
			"surface_damage",
			"enter_anim",
			"end_anim",
			"collide_level",
			"collide_head",
			"collide_feet",
			"collide_damage",
			"collide_side",
			"stuck",
			"jumped_on",
			"get_hit",
			"process",
			"timer",
			"enter_water",
			"exit_water",
			"change_solid_dimensions_fail",
			"add_object_fail",
			"change_animation_failure",
			"die",
			"interact",
			"child_spawned",
			"spawned",
			"draw",
			"begin_dialog",
			"cosmic_shift",
			"schedule_finished",
			"outside_level",
			"being_added",
			"being_removed",
			"window_resize",
			"settings_menu",
			"quit_game",
			"begin_transition_level",
			"mouse_down",
			"mouse_up",
			"mouse_move",
			"mouse_down*",
			"mouse_up*",
			"mouse_move*",
			"mouse_enter",
			"mouse_leave",
			"click",
			"drag",
			"drag_start",
			"drag_end",
			"mouse_wheel",
			"text_input",
			"type_updated",
			"message_received",
		};

		ASSERT_EQ(res.size(), NUM_OBJECT_BUILTIN_EVENT_IDS);
		return res;
	}

	std::vector<std::string>& object_event_names() {
		static std::vector<std::string> event_names = create_object_event_names();
		return event_names;
	}

	std::map<std::string, int> create_object_event_ids()
	{
		std::map<std::string, int> result;
		for(int n = 0; n != object_event_names().size(); ++n) {
			result[object_event_names()[n]] = n;
		}

		return result;
	}

	std::map<std::string, int>& object_event_ids() {
		static std::map<std::string, int> event_ids = create_object_event_ids();
		return event_ids;
	}
}

const std::vector<std::string>& builtin_object_event_names()
{
	static const std::vector<std::string> event_names = create_object_event_names();
	return event_names;
}

const std::string& get_object_event_str(int id)
{
	return object_event_names()[id];
}

int get_object_event_id(const std::string& str)
{
	std::map<std::string, int>::iterator itor = object_event_ids().find(str);
	if(itor != object_event_ids().end()) {
		return itor->second;
	}

	//we have to add a new entry for this new string
	object_event_ids()[str] = static_cast<int>(object_event_names().size());
	object_event_names().push_back(str);

	return static_cast<int>(object_event_names().size() - 1);
}

int get_object_event_id_maybe_proto(const std::string& str)
{
	const char* proto_str = strstr(str.c_str(), "_PROTO_");
	if(proto_str != nullptr) {
		proto_str += 7;
		return get_object_event_id(std::string(proto_str));
	}

	return get_object_event_id(str);
}

variant_type_ptr get_object_event_arg_type(int id)
{
#define EVENT_ARG(event_id, arg_string) \
	case OBJECT_EVENT_##event_id: { \
		static const variant_type_ptr p = parse_variant_type(variant(arg_string)); \
		return p; \
	}
	switch(id) {
		EVENT_ARG(BEGIN_TRANSITION_LEVEL, "{transition: string}")
		EVENT_ARG(WINDOW_RESIZE, "{width: int, height: int}")
		EVENT_ARG(MOUSE_DOWN, "{mouse_x: int, mouse_y: int, mouse_button: int, world_point: [decimal, decimal, decimal]}")
		EVENT_ARG(MOUSE_UP, "{mouse_x: int, mouse_y: int, mouse_button: int, world_point: [decimal, decimal, decimal]}")
		EVENT_ARG(MOUSE_MOVE, "{mouse_x: int, mouse_y: int, mouse_button: int, world_point: [decimal, decimal, decimal]}")
		EVENT_ARG(MOUSE_DOWN_STAR, "{mouse_x: int, mouse_y: int, mouse_button: int, world_point: [decimal, decimal, decimal], handled: bool, objects_under_mouse: [custom_obj]}")
		EVENT_ARG(MOUSE_UP_STAR, "{mouse_x: int, mouse_y: int, mouse_button: int, world_point: [decimal, decimal, decimal], handled: bool, objects_under_mouse: [custom_obj]}")
		EVENT_ARG(MOUSE_MOVE_STAR, "{mouse_x: int, mouse_y: int, mouse_button: int, world_point: [decimal, decimal, decimal], handled: bool, objects_under_mouse: [custom_obj]}")
		EVENT_ARG(MOUSE_ENTER, "{mouse_x: int, mouse_y: int, mouse_button: int, world_point: [decimal, decimal, decimal]}")
		EVENT_ARG(MOUSE_LEAVE, "{mouse_x: int, mouse_y: int, mouse_button: int, world_point: [decimal, decimal, decimal]}")
		EVENT_ARG(MOUSE_CLICK, "{mouse_x: int, mouse_y: int, mouse_button: int, world_point: [decimal, decimal, decimal]}")
		EVENT_ARG(MOUSE_DRAG, "{mouse_x: int, mouse_y: int, mouse_button: int, world_point: [decimal, decimal, decimal]}")
		EVENT_ARG(MOUSE_DRAG_START, "{mouse_x: int, mouse_y: int, mouse_button: int, world_point: [decimal, decimal, decimal]}")
		EVENT_ARG(MOUSE_DRAG_END, "{mouse_x: int, mouse_y: int, mouse_button: int, world_point: [decimal, decimal, decimal]}")
		EVENT_ARG(MOUSE_WHEEL, "{yscroll: int}")
		EVENT_ARG(SPAWNED, "{spawner: custom_obj, child: custom_obj}")
		EVENT_ARG(CHILD_SPAWNED, "{spawner: custom_obj, child: custom_obj}")
		EVENT_ARG(ADD_OBJECT_FAIL, "{collide_with: custom_obj|null, object: custom_obj|null}")
		EVENT_ARG(COLLIDE_HEAD, "{area: string|null, collide_with: custom_obj|null, collide_with_area: string|null}")
		EVENT_ARG(COLLIDE_FEET, "{area: string|null, collide_with: custom_obj|null, collide_with_area: string|null}")
		EVENT_ARG(COLLIDE_SIDE, "{area: string|null, collide_with: custom_obj|null, collide_with_area: string|null}")
		EVENT_ARG(CHANGE_ANIMATION_FAILURE, "{previous_animation: string}")
		EVENT_ARG(COSMIC_SHIFT, "{xshift: int, yshift: int}")
		EVENT_ARG(TEXT_INPUT, "{text: string}")
		default: {
			const std::string& str = get_object_event_str(id);
			if(strstr(str.c_str(), "collide_object")) {
				return parse_variant_type(variant("builtin user_collision_callable"));
			}

			return variant_type_ptr();
		}
	}
#undef EVENT_ARG
}
