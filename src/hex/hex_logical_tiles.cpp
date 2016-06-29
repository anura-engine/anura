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

#include <tuple>

#include "asserts.hpp"
#include "hex_logical_tiles.hpp"
#include "string_utils.hpp"
#include "variant_utils.hpp"

namespace hex 
{
	namespace logical
	{
		namespace
		{
			typedef std::map<std::string, TilePtr> tile_mapping_t;
			tile_mapping_t& get_loaded_tiles()
			{
				static tile_mapping_t res;
				return res;
			}

			typedef std::set<std::string> overlay_mapping_t;
			overlay_mapping_t& get_loaded_overlays()
			{
				static overlay_mapping_t res;
				return res;
			}

			int max_tile_id = 0;
		}

		void loader(const variant& n)
		{
			get_loaded_tiles().clear();

			int tile_id = 0;

			auto tiles = n["tiles"];
			for(auto p : tiles.as_map()) {
				std::string id = p.first.as_string();
				float cost = p.second["cost"].as_float(1.0f);
				int height = p.second["height"].as_int32(1000);
				std::string name = p.second["name"].as_string();
				get_loaded_tiles()[id] = TilePtr(new Tile(id, name, cost, height, tile_id++));
			}

			if(n.has_key("overlay")) {
				auto overlay = n["overlay"];
				for(auto p : overlay.as_map()) {
					std::string key = p.first.as_string();
					get_loaded_overlays().emplace(key);
				}
			}

			max_tile_id = tile_id;
		}

		Tile::Tile(const std::string& id, const std::string& name, float cost, int height, int tile_id) 
			: name_(name),
			  id_(id), 
			  cost_(cost), 
			  height_(height),
			  tile_id_(tile_id)
		{
		}

		const std::map<std::string, TilePtr>& Tile::getLoadedTiles()
		{
			return get_loaded_tiles();
		}

		int Tile::getMaxTileId()
		{
			return max_tile_id;
		}

		void Tile::setTags(std::vector<std::string>::const_iterator beg, std::vector<std::string>::const_iterator ed)
		{
			tags_.resize(std::distance(beg, ed));
			std::copy(beg, ed, tags_.begin());
		}

		TilePtr Tile::factory(const std::string& name)
		{
			std::string tile_name = name;
			// look for a pipe character which is used as a seperator for overlayed things.
			std::vector<std::string> tags;
			auto it = name.find("|");
			if(it != std::string::npos) {
				tags = util::split(name, "|");
				ASSERT_LOG(tags.size() >= 2, "Something went wrong splitting the string " << name << " less than two elements.");
				// Assume first element is tile name
				tile_name = tags[0];
			}

			auto tile = get_loaded_tiles().find(tile_name);
			ASSERT_LOG(tile != get_loaded_tiles().end(), "Unable to find a tile with name: " << tile_name);
			if(it != std::string::npos) {
				TilePtr new_tile = TilePtr(new Tile(*tile->second));
				new_tile->setTags(tags.begin() + 1, tags.end());
				return new_tile;
			}
			return tile->second;
		}

		BEGIN_DEFINE_CALLABLE_NOBASE(Tile)
			DEFINE_FIELD(cost, "decimal")
				return variant(obj.getCost());
			DEFINE_FIELD(height, "int")
				return variant(obj.getHeight());
			DEFINE_FIELD(name, "string")
				return variant(obj.name());
			DEFINE_FIELD(id, "string")
				return variant(obj.id());
			DEFINE_FIELD(tags, "[string]")
				std::vector<variant> tags;
				for(auto& s : obj.tags_) {
					tags.emplace_back(s);
				}
				return variant(&tags);
		END_DEFINE_CALLABLE(Tile)

		LogicalMapPtr LogicalMap::factory(const variant& n)
		{
			return new LogicalMap(n);
		}

		LogicalMap::LogicalMap(const variant& n)
			: changed_(true),
			  x_(n["x"].as_int32(0)),
		      y_(n["y"].as_int32(0)),
			  width_(n["width"].as_int32()), 
			  height_(0)
		{
			tiles_.reserve(width_ * width_);	// approximation
			for (auto& tile_str : n["tiles"].as_list_string()) {
				tiles_.emplace_back(Tile::factory(tile_str));
			}
			height_ = tiles_.size() / width_;
		}

		LogicalMap::LogicalMap(const LogicalMap& m)
			: changed_(true),
			  x_(m.x_),
			  y_(m.y_),
			  width_(m.width_),
			  height_(m.height_),
			  tiles_(m.tiles_)
		{
			// XX if we ever have a case where we need to modify tiles differently between the
			// internal server and here then we need to clone all the elements in m.tiles_.
		}

		variant LogicalMap::write() const
		{
			variant_builder res;
			res.add("x", x_);
			res.add("y", y_);
			res.add("width", width_);
			for(const auto& t : tiles_) {
				std::stringstream ss;
				ss << t->id();
				for(const auto& tag : t->getTags()) {
					ss << "|" << tag;
				}
				res.add("tiles", ss.str());
			}

			return res.build();
		}

		ConstTilePtr LogicalMap::getHexTile(direction d, int xx, int yy) const
		{
			int ox = xx;
			int oy = yy;
			ASSERT_LOG(x() == 0 && y() == 0, "x/y values not zero (" << x() << "," << y() << ")");
			xx -= x();
			yy -= y();
			if(d == NORTH) {
				yy -= 1;
			} else if(d == SOUTH) {
				yy += 1;
			} else if(d == NORTH_WEST) {
				yy -= (abs(ox)%2==0) ? 1 : 0;
				xx -= 1;
			} else if(d == NORTH_EAST) {
				yy -= (abs(ox)%2==0) ? 1 : 0;
				xx += 1;
			} else if(d == SOUTH_WEST) {
				yy += (abs(ox)%2) ? 1 : 0;
				xx -= 1;
			} else if(d == SOUTH_EAST) {
				yy += (abs(ox)%2) ? 1 : 0;
				xx += 1;
			} else {
				ASSERT_LOG(false, "Unrecognised direction: " << d);
			}
			if (xx < 0 || yy < 0 || yy >= height() || xx >= width()) {
				return nullptr;
			}

			const int index = yy * width() + xx;
			ASSERT_LOG(index >= 0 && index < static_cast<int>(tiles_.size()), "Index out of bounds." << index << " >= " << tiles_.size());
			return tiles_[index];
		}

		point LogicalMap::getCoordinatesInDir(direction d, int xx, int yy) const
		{
			int ox = xx;
			int oy = yy;
			xx -= x();
			yy -= y();
			switch (d) {
				case NORTH:			yy -= 1; break;
				case NORTH_EAST:
					yy -= (abs(ox)%2==0) ? 1 : 0;
					xx += 1;
					break;
				case SOUTH_EAST:
					yy += (abs(ox)%2) ? 1 : 0;
					xx += 1;
					break;
				case SOUTH:			yy += 1; break;
				case SOUTH_WEST:
					yy += (abs(ox)%2) ? 1 : 0;
					xx -= 1;
					break;
				case NORTH_WEST:
					yy -= (abs(ox)%2==0) ? 1 : 0;
					xx -= 1;
					break;
				default:
					ASSERT_LOG(false, "Unrecognised direction: " << d);
					break;
			}
			return point(xx, yy) + point(x(), y());
		}

		std::vector<ConstTilePtr> LogicalMap::getSurroundingTiles(int x, int y) const
		{
			std::vector<ConstTilePtr> res;
			for(auto dir : { NORTH, NORTH_EAST, SOUTH_EAST, SOUTH, SOUTH_WEST, NORTH_WEST }) {
				auto hp = getHexTile(dir, x, y);
				if(hp != nullptr) {
					res.emplace_back(hp);
				}
			}
			return res;
		}

		std::vector<point> LogicalMap::getSurroundingPositions(int xx, int yy) const
		{
			std::vector<point> res;
			for(auto dir : { NORTH, NORTH_EAST, SOUTH_EAST, SOUTH, SOUTH_WEST, NORTH_WEST }) {
				auto p = getCoordinatesInDir(dir, xx, yy);
				if(p.x >= 0 && p.x >= 0 && p.x < width() && p.y < height()) {
					res.emplace_back(p);
				}
			}
			return res;
		}

		std::vector<point> LogicalMap::getSurroundingPositions(const point& p) const
		{
			return getSurroundingPositions(p.x, p.y);
		}

		void LogicalMap::getTileRing(int x, int y, int radius, std::vector<point>* res) const
		{
			if(radius <= 0) {
				res->push_back(point(x, y));
				return;
			}

			y -= radius;
			point p = point(x,y);
			for(auto dir : { SOUTH_EAST, SOUTH, SOUTH_WEST, NORTH_WEST, NORTH, NORTH_EAST }) {
				for(int i = 0; i != radius; ++i) {
					if(getTileAt(p.x,p.y).get() != nullptr) {
						res->push_back(p);
					}
					p = getCoordinatesInDir(dir, p.x, p.y);
				}
			}
		}

		void LogicalMap::getTilesInRadius(int x, int y, int radius, std::vector<point>* res) const
		{
			for(int i = 0; i <= radius; ++i) {
				getTileRing(x, y, i, res);
			}
		}

		TilePtr LogicalMap::getTileAt(int xx, int yy)
		{
			xx -= x();
			yy -= y();
			if (xx < 0 || yy < 0 || yy >= height() || xx >= width()) {
				return nullptr;
			}

			const int index = yy * width() + xx;
			ASSERT_LOG(index >= 0 && index < static_cast<int>(tiles_.size()), "");
			return tiles_[index];
		}

		ConstTilePtr LogicalMap::getTileAt(int xx, int yy) const
		{
			xx -= x();
			yy -= y();
			if (xx < 0 || yy < 0 || yy >= height() || xx >= width()) {
				return nullptr;
			}

			const int index = yy * width() + xx;
			ASSERT_LOG(index >= 0 && index < static_cast<int>(tiles_.size()), "");
			return tiles_[index];
		}

		ConstTilePtr LogicalMap::getTileAt(const point& p) const
		{
			return getTileAt(p.x, p.y);
		}

		LogicalMapPtr LogicalMap::clone()
		{
			return LogicalMapPtr(new LogicalMap(*this));
		}

		std::tuple<int,int,int> oddq_to_cube_coords(const point& p)
		{
			int x1 = p.x;
			int z1 = p.y - (p.x - (p.x & 1)) / 2;
			int y1 = -(x1 + z1);
			return std::make_tuple(x1,y1,z1);
		}

		int distance(int x1, int y1, int z1, int x2, int y2, int z2)
		{
			return (abs(x1 - x2) + abs(y1 - y2) + abs(z1 - z2)) / 2;
		}

		int distance(const point& p1, const point& p2)
		{
			int x1, y1, z1;
			std::tie(x1,y1,z1) = oddq_to_cube_coords(p1);
			int x2, y2, z2;
			std::tie(x2,y2,z2) = oddq_to_cube_coords(p2);
			return distance(x1, y1, z1, x2, y2, z2);
		}

		std::tuple<int,int,int> hex_round(float x, float y, float z) 
		{
			int rx = static_cast<int>(std::round(x));
			int ry = static_cast<int>(std::round(y));
			int rz = static_cast<int>(std::round(z));

			float x_diff = std::abs(rx - x);
			float y_diff = std::abs(ry - y);
			float z_diff = std::abs(rz - z);

			if(x_diff > y_diff && x_diff > z_diff) {
				rx = -(ry+rz);
			} else if(y_diff > z_diff) {
				ry = -(rx+rz);
			} else {
				rz = -(rx+ry);
			}
			return std::make_tuple(rx, ry, rz);
		}

		point cube_to_oddq_coords(const std::tuple<int,int,int>& xyz)
		{
			return point(std::get<0>(xyz), std::get<2>(xyz) + (std::get<0>(xyz) - (std::get<0>(xyz) & 1)) / 2);
		}

		std::vector<point> line(const point& p1, const point& p2)
		{
			std::vector<point> res;
			int n = distance(p1, p2);
			int x1, y1, z1;
			std::tie(x1,y1,z1) = oddq_to_cube_coords(p1);
			int x2, y2, z2;
			std::tie(x2,y2,z2) = oddq_to_cube_coords(p2);
			for(int i = 0; i <= n; ++i) {
				const float i_over_n  = static_cast<float>(i)/n;
				const float xt = x1 * (1.0f - i_over_n) + x2 * i_over_n + 1e-6f;
				const float yt = y1 * (1.0f - i_over_n) + y2 * i_over_n + 1e-6f;
				const float zt = z1 * (1.0f - i_over_n) + z2 * i_over_n - 2e-6f;
				res.emplace_back(cube_to_oddq_coords(hex_round(xt, yt, zt)));
			}

			return res;
		}

		float rotation_between(const point& p1, const point& p2)
		{
			// hack it somewhat to just work for p1 and p2 being adjacent.
			const int dx = p2.x - p1.x;
			const int dy = p2.y - p1.y;
			ASSERT_LOG(dx <= 1 && dx >= -1 && dy >= -1 && dy <= 1, "hex::logical::rotation_between only works for adjacent tiles.");
			if(dx == 0 && dy == 0) {
				return 0.0f;
			}
			if(dy == 0) {
				return dx > 0 ? 60.0f : 300.0f;
			} else if(dy > 0) {
				return dx == 0 ? 180.0f : dx > 0 ? 120.0f : dx == 240.0f;
			} else {
				// dy < 0
				return 0.0f;
			}
		}

		void LogicalMap::surrenderReferences(GarbageCollector* collector)
		{
			for(auto& t : tiles_) {
				collector->surrenderPtr(&t, "HEX::LOGICALMAP::TILE");
			}
		}

		BEGIN_DEFINE_CALLABLE_NOBASE(LogicalMap)
			DEFINE_FIELD(width, "int")
				return variant(obj.width());
			DEFINE_FIELD(height, "int")
				return variant(obj.height());

			DEFINE_FIELD(changed, "bool")
				return variant::from_bool(obj.isChanged());
			DEFINE_SET_FIELD
				if(value.as_bool() == true) {
					obj.setChanged();
				} else {
					obj.clearChangeFlag();
				}

			DEFINE_FIELD(tiles, "[[builtin tile]]")
				std::vector<variant> rows;
				std::vector<variant> cols;

				int w = 0;
				for(auto& t : obj.tiles_) {
					cols.emplace_back(t.get());
					if(++w >= obj.width()) {
						rows.emplace_back(&cols);
					}
				}				
				return variant(&rows);
			DEFINE_SET_FIELD
				std::vector<variant> rows = value.as_list();
				obj.height_ = rows.size();
				obj.tiles_.clear();
				for(auto& col : rows) {
					//obj.tiles_.emplace_back(col.as_callable());
				}
				obj.changed_ = true;

			BEGIN_DEFINE_FN(tile_at, "([int,int]) ->builtin tile")
				variant v = FN_ARG(0);
				int x = v[0].as_int();
				int y = v[1].as_int();

				ConstTilePtr tile = obj.getTileAt(x, y);
				ASSERT_LOG(tile, "Illegal tile at " << x << ", " << y);

				return variant(tile.get());				
			END_DEFINE_FN

			BEGIN_DEFINE_FN(adjacent_tiles, "([int,int]) ->[[int,int]]")
				variant v = FN_ARG(0);
				int x = v[0].as_int();
				int y = v[1].as_int();

				std::vector<point> res;
				obj.getTileRing(x, y, 1, &res);

				std::vector<variant> points;
				points.reserve(res.size());
				for(const point& p : res) {
					std::vector<variant> v;
					v.reserve(2);
					v.emplace_back(p.x);
					v.emplace_back(p.y);
					points.emplace_back(&v);
				}

				return variant(&points);
			END_DEFINE_FN

			BEGIN_DEFINE_FN(create_tile, "(string) ->builtin tile")
				auto tile = Tile::factory(FN_ARG(0).as_string());
				return variant(tile.get());
			END_DEFINE_FN
			
			BEGIN_DEFINE_FN(tiles_in_radius, "([int,int], int) ->[[int,int]]")
				variant v = FN_ARG(0);
				const int x = v[0].as_int();
				const int y = v[1].as_int();
				const int radius = FN_ARG(1).as_int();

				std::vector<point> res;
				obj.getTilesInRadius(x, y, radius, &res);

				std::vector<variant> points;
				points.reserve(res.size());
				for(const point& p : res) {
					std::vector<variant> v;
					v.reserve(2);
					v.emplace_back(p.x);
					v.emplace_back(p.y);
					points.emplace_back(&v);
				}
				return variant(&points);
			END_DEFINE_FN

			BEGIN_DEFINE_FN(set_tile_at, "([int,int], string) ->commands")
				variant v = FN_ARG(0);
				const int x = v[0].as_int();
				const int y = v[1].as_int();
				std::string name = FN_ARG(1).as_string();
				auto tile = Tile::factory(name);

				// Ugly const_cast so that we can modify the map in FnCommandCallable.
				// We also convert to an intrusive_ptr so that the lifetime is extended
				// While the lambda is still live.
				boost::intrusive_ptr<LogicalMap> map_ref = &const_cast<LogicalMap&>(obj);
				
				const int index = y * obj.width() + x;
				ASSERT_LOG(index >= 0 && index < static_cast<int>(obj.tiles_.size()), "Index out of bounds." << index << " >= " << obj.tiles_.size());
				return variant(new game_logic::FnCommandCallable([=]() {					
					map_ref->setChanged();
					map_ref->tiles_changed_.emplace_back(point(x,y));
					map_ref->tiles_[index] = tile;
				}));
			END_DEFINE_FN
		END_DEFINE_CALLABLE(LogicalMap)
	}
}
