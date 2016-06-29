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

#include <string>
#include <vector>

#include "geometry.hpp"
#include "hex_logical_fwd.hpp"
#include "variant.hpp"

namespace hex 
{
	namespace logical
	{
		std::tuple<int,int,int> oddq_to_cube_coords(const point& p);
		int distance(int x1, int y1, int z1, int x2, int y2, int z2);
		int distance(const point& p1, const point& p2);
		std::vector<point> line(const point& p1, const point& p2);
		float rotation_between(const point& p1, const point& p2);

		class Tile : public game_logic::FormulaCallable
		{
		public:
			explicit Tile(const std::string& id, const std::string& name, float cost, int height, int tile_id);
			const std::string& name() const { return name_; }
			const std::string& id() const { return id_; }
			int tile_id() const { return tile_id_; }
			float getCost() const { return cost_; }
			int getHeight() const { return height_; }
			const std::vector<std::string>& getTags() const { return tags_; }
			void setTags(std::vector<std::string>::const_iterator beg, std::vector<std::string>::const_iterator ed);
			static TilePtr factory(const std::string& name);
			static const std::map<std::string, TilePtr>& getLoadedTiles();
			static int getMaxTileId();
		private:
			DECLARE_CALLABLE(Tile);

			std::string name_;
			std::string id_;
			int height_;
			float cost_;
			int tile_id_;
			std::vector<std::string> tags_;
		};
	
		class LogicalMap : public game_logic::FormulaCallable
		{
		public:
			typedef std::vector<TilePtr>::iterator iterator;
			typedef std::vector<TilePtr>::const_iterator const_iterator;

			explicit LogicalMap(const variant& n);
			LogicalMapPtr clone();

			int x() const { return x_; }
			int y() const { return y_; }
			int width() const { return width_; }
			int height() const { return height_; }

			// Range based for loop support.
			iterator begin() { return tiles_.begin(); }
			iterator end() { return tiles_.end(); }
			const_iterator begin() const { return tiles_.begin(); }
			const_iterator end() const { return tiles_.end(); }
			std::size_t size() { return tiles_.size(); }

			ConstTilePtr getHexTile(direction d, int x, int y) const;
			std::vector<ConstTilePtr> getSurroundingTiles(int x, int y) const;
			// Get the positions of the valid tiles surrounding the tile at (x,y)
			std::vector<point> getSurroundingPositions(int x, int y) const;
			std::vector<point> getSurroundingPositions(const point& p) const;
			void getTileRing(int x, int y, int radius, std::vector<point>* res) const;
			void getTilesInRadius(int x, int y, int radius, std::vector<point>* res) const;
			ConstTilePtr getTileAt(int xx, int yy) const;
			TilePtr getTileAt(int xx, int yy);
			ConstTilePtr getTileAt(const point& p) const;
			point getCoordinatesInDir(direction d, int x, int y) const;

			bool isChanged() const { return changed_; }
			void clearChangeFlag() { changed_ = false; tiles_changed_.clear();}
			void setChanged() { changed_ = true; }

			const std::vector<point>& getTilesChanged() const { return tiles_changed_; }

			void surrenderReferences(GarbageCollector* collector) override;

			static LogicalMapPtr factory(const variant& v);

			variant write() const;
		private:
			DECLARE_CALLABLE(LogicalMap);

			bool changed_;
			int x_;
			int y_;
			int width_;
			int height_;
			std::vector<TilePtr> tiles_;
			std::vector<point> tiles_changed_;

			LogicalMap(const LogicalMap&);
			LogicalMap() = delete;
			void operator=(const LogicalMap&) = delete;
		};

		void loader(const variant& v);
	}
}
