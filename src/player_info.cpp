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

#include "controls.hpp"
#include "entity.hpp"
#include "formatter.hpp"
#include "joystick.hpp"
#include "json_parser.hpp"
#include "player_info.hpp"
#include "string_utils.hpp"
#include "variant_utils.hpp"

PlayerInfo::PlayerInfo(Entity& e, variant node)
	: entity_(&e),
	  slot_(0)
{
	for(variant objects_node : node["objects_destroyed"].as_list()) {
		std::vector<int>& v = objects_destroyed_[objects_node["level"].as_string()];
		v = objects_node["objects"].as_list_int();
	}
}

void PlayerInfo::objectDestroyed(const std::string& level_id, int object)
{
	objects_destroyed_[level_id].push_back(object);
}

const std::vector<int>& PlayerInfo::getObjectsDestroyed(const std::string& level_id) const
{
	std::vector<int>& v = objects_destroyed_[level_id];
	std::sort(v.begin(), v.end());
	v.erase(std::unique(v.begin(), v.end()), v.end());
	return v;
}

variant PlayerInfo::write() const
{
	variant_builder result;
	for(std::map<std::string, std::vector<int> >::const_iterator i = objects_destroyed_.begin(); i != objects_destroyed_.end(); ++i) {
		getObjectsDestroyed(i->first); //remove duplicates.

		variant_builder objects;
		objects.add("level", i->first);

		objects.add("objects", vector_to_variant(i->second));

		result.add("objects_destroyed", objects.build());
	}

	return result.build();
}

void PlayerInfo::readControls(int cycle)
{
	bool status[controls::NUM_CONTROLS];
	const std::string* user = nullptr;
	controls::get_controlStatus(cycle, slot_, status, &user);

	if(status[controls::CONTROL_LEFT] && status[controls::CONTROL_RIGHT]) {
		//if both left and right are held, treat it as if neither are.
		status[controls::CONTROL_LEFT] = status[controls::CONTROL_RIGHT] = false;
	}

	for(int n = 0; n != controls::NUM_CONTROLS; ++n) {
		entity_->setControlStatus(static_cast<controls::CONTROL_ITEM>(n), status[n]);
	}

	variant user_value;
	if(user && !user->empty()) {
		user_value = json::parse(*user, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
	}

	entity_->setControlStatusUser(user_value);
}
