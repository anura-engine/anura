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

#include "asserts.hpp"
#include "formatter.hpp"
#include "hex_tile.hpp"
#include "hex_map.hpp"
#include "hex_object.hpp"
#include "variant_utils.hpp"


namespace hex 
{
	HexObject::HexObject(const std::string& type, int x, int y, const HexMap* owner) 
		: owner_map_(owner), 
		  x_(x), 
		  y_(y), 
		  type_(type)
	{
		tile_ = TileType::factory(type_);
		ASSERT_LOG(tile_, "Could not find tile: " << type_);
	}

	const HexObject* HexObject::getTileInDir(enum direction d) const
	{
		ASSERT_LOG(owner_map_ != nullptr, "owner_map_ was null");
		return owner_map_->getHexTile(d, x_, y_);
	}

	const HexObject* HexObject::getTileInDir(const std::string& s) const
	{
		if(s == "north" || s == "n") {
			return getTileInDir(NORTH);
		} else if(s == "south" || s == "s") {
			return getTileInDir(SOUTH);
		} else if(s == "north_west" || s == "nw" || s == "northwest") {
			return getTileInDir(NORTH_WEST);
		} else if(s == "north_east" || s == "ne" || s == "northeast") {
			return getTileInDir(NORTH_EAST);
		} else if(s == "south_west" || s == "sw" || s == "southwest") {
			return getTileInDir(SOUTH_WEST);
		} else if(s == "south_east" || s == "se" || s == "southeast") {
			return getTileInDir(SOUTH_EAST);
		}
		return nullptr;
	}

	void HexObject::render(std::vector<KRE::vertex_texcoord>* coords) const
	{
		if(tile_ == nullptr) {
			return;
		}
		tile_->render(x_, y_, coords);
	}

	void HexObject::renderAdjacent(std::vector<MapRenderParams>* coords) const
	{
		for(const NeighborType& neighbor : neighbors_) {
			neighbor.type->renderAdjacent(x_, y_, &(*coords)[neighbor.type->tile_id()].coords, neighbor.dirmap);
		}
	}

	void HexObject::setNeighborsChanged()
	{
		for (auto& neighbor : neighbors_) {
			neighbor.type->calculateAdjacencyPattern(neighbor.dirmap);
		}
	}

	void HexObject::initNeighbors()
	{
		for(int n = 0; n < 6; ++n) {
			const HexObject* obj = getTileInDir(static_cast<direction>(n));
			if(obj && obj->tile() && obj->tile()->getHeight() > tile()->getHeight()) {
				NeighborType* neighbor = nullptr;
				for(NeighborType& candidate : neighbors_) {
					neighbor = &candidate;
				}

				if(neighbor == nullptr) {
					neighbors_.push_back(NeighborType());
					neighbor = &neighbors_.back();
					neighbor->type = obj->tile();
				}

				neighbor->dirmap |= (1 << n);
			}
		}
	}
}
