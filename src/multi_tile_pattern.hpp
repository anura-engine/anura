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

#include <boost/regex.hpp>

#include <deque>
#include <string>
#include <vector>

#include "geometry.hpp"

#include "level_object_fwd.hpp"
#include "variant.hpp"

const boost::regex& get_regex_from_pool(const std::string& key);

class MultiTilePattern
{
public:
	//all multi tile patterns loaded. This is a deque meaning callers can
	//save pointers to members, knowing they will never be destroyed.
	static const std::deque<MultiTilePattern>& getAll();
	static void init(variant node);
	static void load(variant node, const std::string& tile_id);
	MultiTilePattern(variant node, const std::string& tile_id);

	struct TileEntry {
		LevelObjectPtr tile;
		int zorder;
	};

	struct TileInfo {
		const boost::regex* re;
		std::vector<TileEntry> tiles;
	};

	const std::string& id() const { return id_; }

	const TileInfo& getTileAt(int x, int y) const;

	int width() const;
	int height() const;

	int chance() const { return chance_; }

	const MultiTilePattern& chooseRandomAlternative(int seed) const;

	struct MatchCell {
		point loc;
		int run_length;
	};

	//the order to try matches in, optimized to eliminate things as soon
	//as we possibly can.
	const std::vector<MatchCell>& tryOrder() const { return try_order_; }
private:
	std::string default_tile_id_;
	std::string id_;
	std::vector<TileInfo> tiles_;
	std::vector<std::shared_ptr<MultiTilePattern>> alternatives_;
	std::vector<MatchCell> try_order_;
	int width_, height_;
	int chance_;
};
