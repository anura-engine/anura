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
#include "controls.hpp"
#include "entity.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "joystick.hpp"
#include "json_parser.hpp"
#include "player_info.hpp"
#include "string_utils.hpp"
#include "variant_utils.hpp"

PlayerInfo::PlayerInfo(entity& e, variant node)
  : entity_(&e),
	slot_(0)
{
	foreach(variant objects_node, node["objects_destroyed"].as_list()) {
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
	const std::string* user = NULL;
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
		user_value = json::parse(*user, json::JSON_NO_PREPROCESSOR);
	}

	entity_->setControlStatusUser(user_value);
}
