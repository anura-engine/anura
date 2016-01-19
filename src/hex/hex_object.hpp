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

#include "hex_fwd.hpp"
#include "hex_map.hpp"
#include "hex_renderable.hpp"
#include "hex_tile.hpp"
#include "variant.hpp"

namespace hex 
{
	class HexObject 
	{
	public:
		HexObject(const logical::TilePtr& type, int x, int y, const HexMap* owner);

		void draw(const point& cam) const;
	
		void build();
		const std::string& type() const { return type_; }

		const HexObject* getTileInDir(enum direction d) const;
		const HexObject* getTileInDir(const std::string& s) const;

		int x() const { return x_; }
		int y() const { return y_; }

		TileTypePtr tile() const { return tile_; }
		const logical::TilePtr& logical_tile() const { return logical_tile_; }

		void initNeighbors();
		void setNeighborsChanged();

		void render(std::vector<KRE::vertex_texcoord>* coords) const;
		void renderAdjacent(std::vector<MapRenderParams>* coords) const;
		void renderOverlay(const Alternate& alternative, const KRE::TexturePtr& tex, std::vector<KRE::vertex_texcoord>* coords) const;
	private:
		// map coordinates.
		int x_;
		int y_;

		TileTypePtr tile_;
		logical::TilePtr logical_tile_;

		struct NeighborType {
			NeighborType() : dirmap(0) {}
			TileTypePtr type;
			unsigned char dirmap;
		};

		std::vector<NeighborType> neighbors_;

		// String representing the base type of this tile.
		std::string type_;
		// raw pointer to the map that owns this.
		const HexMap* owner_map_;
	};
}
