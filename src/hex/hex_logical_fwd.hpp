/*
	Copyright (C) 2014-2015 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <memory>
#include <vector>

#include "geometry.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

namespace hex
{
	enum direction { NORTH, NORTH_EAST, SOUTH_EAST, SOUTH, SOUTH_WEST, NORTH_WEST };

	namespace logical
	{
		class Tile;
		typedef boost::intrusive_ptr<Tile> TilePtr;
		typedef boost::intrusive_ptr<const Tile> ConstTilePtr;
		class LogicalMap;
		typedef boost::intrusive_ptr<LogicalMap> LogicalMapPtr;
	}

	struct MoveCost
	{
		MoveCost(const point& p, float c) : loc(p), path_cost(c) {}
		point loc;
		float path_cost;
	};
	// XXX result_list might be better served as a std::set
	typedef std::vector<MoveCost> result_list;

	struct graph_t;
	typedef std::shared_ptr<graph_t> HexGraphPtr;

}
