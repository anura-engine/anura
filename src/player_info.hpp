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

#include <map>
#include <string>
#include <vector>

#include "variant.hpp"

//class which contains information about the player.
class PlayerInfo
{
public:
	explicit PlayerInfo(Entity& e) : entity_(&e), slot_(0)
	{}
	PlayerInfo(Entity& e, variant node);
	
	void objectDestroyed(const std::string& level_id, int item);
	const std::vector<int>& getObjectsDestroyed(const std::string& level_id) const;

	variant write() const;

	void swapPlayerState(PlayerInfo& player) {
		items_destroyed_.swap(player.items_destroyed_);
		objects_destroyed_.swap(player.objects_destroyed_);
	}

	const Entity& getEntity() const { return *entity_; }
	Entity& getEntity() { return *entity_; }

	void setEntity(Entity& e) { entity_ = &e; }

	const std::string& currentLevel() const { return current_level_; }
	void setCurrentLevel(const std::string& lvl) { current_level_ = lvl; }

	void setPlayerSlot(int slot) { slot_ = slot; }
	int getPlayerSlot() const { return slot_; }

	void readControls(int cycle);

	
	bool hasReverseGlobalVerticalZordering() const { return entity_->hasReverseGlobalVerticalZordering(); }

private:
	Entity* entity_;

	mutable std::map<std::string, std::vector<int> > items_destroyed_;
	mutable std::map<std::string, std::vector<int> > objects_destroyed_;
	
	//the number of the player.
	int slot_;

	std::string current_level_;
};
