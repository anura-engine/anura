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

#include <vector>

#include "geometry.hpp"
#include "hex_fwd.hpp"
#include "hex_logical_tiles.hpp"
#include "hex_object.hpp"
#include "hex_renderable_fwd.hpp"
#include "variant.hpp"

namespace hex 
{
	class HexMap : public game_logic::FormulaCallable
	{
	public:
		HexMap() : zorder_(-1000) {}
		virtual ~HexMap() {}
		explicit HexMap(const variant& n);
		int getZorder() const { return zorder_; }
		void setZorder(int zorder) { zorder_ = zorder; }

		int x() const { return map_->x(); }
		int y() const { return map_->y(); }

		size_t width() const { return map_->width(); }
		size_t height() const { return map_->height(); }
		size_t size() const { return map_->width() * map_->height(); }
		void build();
		variant write() const;

		void process();

		bool setTile(int x, int y, const std::string& tile);

		std::vector<const HexObject*> getSurroundingTiles(int x, int y) const;
		const HexObject* getHexTile(direction d, int x, int y) const;
		const HexObject* getTileAt(int x, int y) const;
		const HexObject* getTileFromPixelPos(int x, int y) const;
		static point getTilePosFromPixelPos(int x, int y);
		static point getPixelPosFromTilePos(int x, int y);
		static point getPixelPosFromTilePos(const point& p);

		static point getLocInDir(int x, int y, direction d);
		static point getLocInDir(int x, int y, const std::string& s);

		void setRenderable(MapNodePtr renderable) { renderable_ = renderable; changed_ = true; }

		// this is a convenience function.
		logical::LogicalMapPtr getLogicalMap() { return map_; }

		void surrenderReferences(GarbageCollector* collector) override;

		static HexMapPtr factory(const variant& n);
	private:
		DECLARE_CALLABLE(HexMap);

		logical::LogicalMapPtr map_;
		int zorder_;
		int border_;
		std::vector<HexObject> tiles_;
		bool changed_;

		MapNodePtr renderable_;

		HexMap(const HexMap&);
		void operator=(const HexMap&);
	};
}
