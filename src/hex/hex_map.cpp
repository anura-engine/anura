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

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <sstream>

#include "asserts.hpp"
#include "hex_map.hpp"
#include "hex_object.hpp"
#include "hex_renderable.hpp"
#include "hex_tile.hpp"
#include "profile_timer.hpp"
#include "variant.hpp"
#include "variant_utils.hpp"

namespace hex 
{
	static const int HexTileSize = 72;

	HexMap::HexMap(const variant& value)
		: map_(),
		  zorder_(value["zorder"].as_int32(-1000)),
		  border_(value["border"].as_int32(0)),
		  tiles_(),
		  changed_(false),
		  renderable_(nullptr)
	{
	}

	HexMapPtr HexMap::factory(const variant& n)
	{
		HexMapPtr p = HexMapPtr(new HexMap(n));
		
		// create the logical version of the map.
		p->map_ = hex::logical::LogicalMap::factory(n);
		p->build();
		for(auto& obj : p->tiles_) {
			obj.setNeighborsChanged();
		}
		return p;
	}

	void HexMap::build()
	{
		profile::manager pman("HexMap::build");
		int index = 0;
		const auto& tiles_changed = map_->getTilesChanged();
		if(tiles_changed.empty() || tiles_changed.size() == tiles_.size()) {
			tiles_.clear();
			tiles_.reserve(map_->size());
			for(auto& t : *map_) {
				const int x = index % map_->width();
				const int y = index / map_->width();
				tiles_.emplace_back(t, x, y, this);
				++index;
			}
		} else {
			for(auto& t : tiles_changed) {
				const int index = t.y * map_->width() + t.x;
				tiles_[index] = HexObject(map_->getTileAt(t.x, t.y), t.x, t.y, this);
			}
		}		

		for(auto& t : tiles_) {
			t.initNeighbors();
		}
		map_->clearChangeFlag();
	}

	variant HexMap::write() const
	{
		auto v = map_->write();
		v.add_attr(variant("zorder"), variant(zorder_));
		if(border_ != 0) {
			v.add_attr(variant("border"), variant(border_));
		}
		return v;
	}

	void HexMap::process()
	{
		if(map_->isChanged()) {
			changed_ = true;
			build();
		}

		if(changed_) {
			changed_ = false;
			for(auto& obj : tiles_) {
				obj.setNeighborsChanged();
			}

			if(renderable_) {
				profile::manager pman("MapNode::update");
				renderable_->update(width(), height(), tiles_);
			}
		}
	}

	std::vector<const HexObject*> HexMap::getSurroundingTiles(int x, int y) const
	{
		std::vector<const HexObject*> res;
		for(auto dir : { NORTH, NORTH_EAST, SOUTH_EAST, SOUTH, SOUTH_WEST, NORTH_WEST }) {
			auto hp = getHexTile(dir, x, y);
			if(hp != nullptr) {
				res.emplace_back(hp);
			}
		}
		return res;
	}

	const HexObject* HexMap::getHexTile(direction d, int x, int y) const
	{
		int ox = x;
		int oy = y;
		assert(map_->x() == 0 && map_->y() == 0);
		x -= map_->x();
		y -= map_->y();
		if(d == NORTH) {
			y -= 1;
		} else if(d == SOUTH) {
			y += 1;
		} else if(d == NORTH_WEST) {
			y -= (abs(ox)%2==0) ? 1 : 0;
			x -= 1;
		} else if(d == NORTH_EAST) {
			y -= (abs(ox)%2==0) ? 1 : 0;
			x += 1;
		} else if(d == SOUTH_WEST) {
			y += (abs(ox)%2) ? 1 : 0;
			x -= 1;
		} else if(d == SOUTH_EAST) {
			y += (abs(ox)%2) ? 1 : 0;
			x += 1;
		} else {
			ASSERT_LOG(false, "Unrecognised direction: " << d);
		}
		if (x < 0 || y < 0 || y >= map_->height() || x >= map_->width()) {
			return nullptr;
		}

		const int index = y * map_->width() + x;
		assert(index >= 0 && index < static_cast<int>(tiles_.size()));
		return &tiles_[index];
	}

	point HexMap::getTilePosFromPixelPos(int mx, int my)
	{
		const int tesselation_x_size = (3 * HexTileSize) / 2;
		const int tesselation_y_size = HexTileSize;
		const int x_base = (mx>=0) ? mx / tesselation_x_size * 2 : mx / tesselation_x_size * 2 - 2;
		const int x_mod  = (mx>=0) ? mx % tesselation_x_size : tesselation_x_size + (mx % tesselation_x_size);
		const int y_base = (my>=0) ? my / tesselation_y_size : my / tesselation_y_size - 1;
		const int y_mod  = (my>=0) ? my % tesselation_y_size : tesselation_y_size + (my % tesselation_y_size);
		const int m = 2;

		int x_modifier = 0;
		int y_modifier = 0;

		if(y_mod < tesselation_y_size / 2) {
			if((x_mod * m + y_mod) < (HexTileSize / 2)) {
				x_modifier = -1;
				y_modifier = -1;
			} else if ((x_mod * m - y_mod) < (HexTileSize * 3 / 2)) {
				x_modifier = 0;
				y_modifier = 0;
			} else {
				x_modifier = 1;
				y_modifier = -1;
			}

		} else {
			if((x_mod * m - (y_mod - HexTileSize / 2)) < 0) {
				x_modifier = -1;
				y_modifier = 0;
			} else if((x_mod * m + (y_mod - HexTileSize / 2)) < HexTileSize * 2) {
				x_modifier = 0;
				y_modifier = 0;
			} else {
				x_modifier = 1;
				y_modifier = 0;
			}
		}
		return point(x_base + x_modifier, y_base + y_modifier);
	}

	const HexObject* HexMap::getTileFromPixelPos(int mx, int my) const
	{
		point p = getTilePosFromPixelPos(mx, my);
		return getTileAt(p.x, p.y);
	}

	point HexMap::getPixelPosFromTilePos(const point& p)
	{
		return getPixelPosFromTilePos(p.x, p.y);
	}

	point HexMap::getPixelPosFromTilePos(int x, int y)
	{
		const int HexTileSizeHalf = HexTileSize/2;
		const int HexTileSizeThreeQuarters = (HexTileSize*3)/4;
		const int tx = x*HexTileSizeThreeQuarters;
		const int ty = HexTileSize*y + (abs(x)%2)*HexTileSizeHalf;
		return point(tx, ty);
	}

	const HexObject* HexMap::getTileAt(int x, int y) const
	{
		x -= map_->x();
		y -= map_->y();
		if (x < 0 || y < 0 || y >= map_->height() || x >= map_->width()) {
			return nullptr;
		}

		const int index = y * map_->width() + x;
		assert(index >= 0 && index < static_cast<int>(tiles_.size()));
		return &tiles_[index];
	}

	bool HexMap::setTile(int xx, int yy, const std::string& tile)
	{
		if(xx < 0 || yy < 0 || xx >= map_->width() || yy >= map_->height()) {
			return false;
		}

		const int index = yy * map_->width() + xx;
		assert(index >= 0 && index < static_cast<int>(tiles_.size()));

		auto ltp = logical::Tile::getLoadedTiles().find(tile);
		ASSERT_LOG(ltp != logical::Tile::getLoadedTiles().end(), "Couldn't find tile named " << tile);
		tiles_[index] = HexObject(ltp->second, xx, yy, this);
		for(auto t : tiles_) {
			t.setNeighborsChanged();
		}
		return true;
	}

	point HexMap::getLocInDir(int x, int y, direction d)
	{
		int ox = x;
		int oy = y;
		if(d == NORTH) {
			y -= 1;
		} else if(d == SOUTH) {
			y += 1;
		} else if(d == NORTH_WEST) {
			y -= (abs(ox)%2==0) ? 1 : 0;
			x -= 1;
		} else if(d == NORTH_EAST) {
			y -= (abs(ox)%2==0) ? 1 : 0;
			x += 1;
		} else if(d == SOUTH_WEST) {
			y += (abs(ox)%2) ? 1 : 0;
			x -= 1;
		} else if(d == SOUTH_EAST) {
			y += (abs(ox)%2) ? 1 : 0;
			x += 1;
		} else {
			ASSERT_LOG(false, "Unrecognised direction: " << d);
		}
		return point(x, y);
	}

	point HexMap::getLocInDir(int x, int y, const std::string& s)
	{
		if(s == "north" || s == "n") {
			return getLocInDir(x, y, NORTH);
		} else if(s == "south" || s == "s") {
			return getLocInDir(x, y, SOUTH);
		} else if(s == "north_west" || s == "nw" || s == "northwest") {
			return getLocInDir(x, y, NORTH_WEST);
		} else if(s == "north_east" || s == "ne" || s == "northeast") {
			return getLocInDir(x, y, NORTH_EAST);
		} else if(s == "south_west" || s == "sw" || s == "southwest") {
			return getLocInDir(x, y, SOUTH_WEST);
		} else if(s == "south_east" || s == "se" || s == "southeast") {
			return getLocInDir(x, y, SOUTH_EAST);
		}
		ASSERT_LOG(false, "Unreognised direction " << s);
		return point();
	}

	void HexMap::surrenderReferences(GarbageCollector* collector)
	{
		collector->surrenderPtr(&map_);
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(HexMap)
		DEFINE_FIELD(zorder, "int")
			return variant(obj.zorder_);
		DEFINE_FIELD(logical, "builtin logical_map")
			return variant(obj.map_.get());
		DEFINE_FIELD(changed, "bool")
			return variant::from_bool(obj.changed_);
		DEFINE_SET_FIELD
			obj.changed_ = value.as_bool();
		DEFINE_FIELD(tile_height, "int")
			return variant(HexTileSize);
		BEGIN_DEFINE_FN(write, "() -> map")
			return obj.write();
		END_DEFINE_FN
		BEGIN_DEFINE_FN(tile_loc_from_pixel_pos, "([int,int]) ->[int,int]")
			variant v = FN_ARG(0);
			int x = v[0].as_int();
			int y = v[1].as_int();

			point p = HexMap::getTilePosFromPixelPos(x, y);
			std::vector<variant> res;
			res.push_back(variant(p.x));
			res.push_back(variant(p.y));
			return variant(&res);
		END_DEFINE_FN
		BEGIN_DEFINE_FN(tile_pixel_pos_from_loc, "([int,int]) ->[int,int]")
			variant v = FN_ARG(0);
			int x = v[0].as_int();
			int y = v[1].as_int();

			point p = HexMap::getPixelPosFromTilePos(x, y);
			std::vector<variant> res;
			res.push_back(variant(p.x));
			res.push_back(variant(p.y));
			return variant(&res);
		END_DEFINE_FN
	END_DEFINE_CALLABLE(HexMap)
}
