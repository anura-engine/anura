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

#include "variant_type.hpp"

enum OBJECT_EVENT_ID {
	OBJECT_EVENT_ANY,
	OBJECT_EVENT_START_LEVEL,
	OBJECT_EVENT_PLAYER_DEATH,
	OBJECT_EVENT_LOAD,
	OBJECT_EVENT_LOAD_CHECKPOINT,
	OBJECT_EVENT_CONSTRUCT,
	OBJECT_EVENT_CREATE,
	OBJECT_EVENT_DONE_CREATE,
	OBJECT_EVENT_BECOME_ACTIVE,
	OBJECT_EVENT_SURFACE_DAMAGE,
	OBJECT_EVENT_ENTER_ANIM,
	OBJECT_EVENT_END_ANIM,
	OBJECT_EVENT_COLLIDE_LEVEL,
	OBJECT_EVENT_COLLIDE_HEAD,
	OBJECT_EVENT_COLLIDE_FEET,
	OBJECT_EVENT_COLLIDE_DAMAGE,
	OBJECT_EVENT_COLLIDE_SIDE,
	OBJECT_EVENT_STUCK,
	OBJECT_EVENT_JUMPED_ON,
	OBJECT_EVENT_GET_HIT,
	OBJECT_EVENT_PROCESS,
	OBJECT_EVENT_TIMER,
	OBJECT_EVENT_ENTER_WATER,
	OBJECT_EVENT_EXIT_WATER,
	OBJECT_EVENT_CHANGE_SOLID_DIMENSIONS_FAIL,
	OBJECT_EVENT_ADD_OBJECT_FAIL,
	OBJECT_EVENT_CHANGE_ANIMATION_FAILURE,
	OBJECT_EVENT_DIE,
	OBJECT_EVENT_INTERACT,
	OBJECT_EVENT_CHILD_SPAWNED,
	OBJECT_EVENT_SPAWNED,
	OBJECT_EVENT_DRAW,
	OBJECT_EVENT_BEGIN_DIALOG,
	OBJECT_EVENT_COSMIC_SHIFT,
	OBJECT_EVENT_SCHEDULE_FINISHED,
	OBJECT_EVENT_OUTSIDE_LEVEL,
	OBJECT_EVENT_BEING_ADDED,
	OBJECT_EVENT_BEING_REMOVED,
	OBJECT_EVENT_WINDOW_RESIZE,
	OBJECT_EVENT_SETTINGS_MENU,
	OBJECT_EVENT_QUIT_GAME,
	OBJECT_EVENT_BEGIN_TRANSITION_LEVEL,
	OBJECT_EVENT_MOUSE_DOWN,
	OBJECT_EVENT_MOUSE_UP,
	OBJECT_EVENT_MOUSE_MOVE,
	OBJECT_EVENT_MOUSE_DOWN_STAR,
	OBJECT_EVENT_MOUSE_UP_STAR,
	OBJECT_EVENT_MOUSE_MOVE_STAR,
	OBJECT_EVENT_MOUSE_ENTER,
	OBJECT_EVENT_MOUSE_LEAVE,
	OBJECT_EVENT_MOUSE_CLICK,
	OBJECT_EVENT_MOUSE_DRAG,
	OBJECT_EVENT_MOUSE_DRAG_START,
	OBJECT_EVENT_MOUSE_DRAG_END,
	OBJECT_EVENT_MOUSE_WHEEL,
	OBJECT_EVENT_TEXT_INPUT,
	OBJECT_EVENT_TYPE_UPDATED,
	OBJECT_EVENT_MESSAGE_RECEIVED,
	NUM_OBJECT_BUILTIN_EVENT_IDS,
};

const std::vector<std::string>& builtin_object_event_names();

const std::string& get_object_event_str(int id);
int get_object_event_id(const std::string& str);

//like get_object_event_id but will collapse event ID's for
//prototypes into their base events.
int get_object_event_id_maybe_proto(const std::string& str);

variant_type_ptr get_object_event_arg_type(int id);
