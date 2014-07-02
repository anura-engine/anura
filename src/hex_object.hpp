/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <boost/intrusive_ptr.hpp>
#include <vector>

#include "nocopy.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "variant.hpp"
#include "hex_object_fwd.hpp"
#include "hex_map.hpp"
#include "hex_tile.hpp"

namespace hex 
{
	class HexObject : public game_logic::FormulaCallable
	{
	public:
		HexObject(const std::string& type, int x, int y, const HexMap* owner);
		virtual ~HexObject() {}

		virtual variant getValue(const std::string&) const;
		virtual void setValue(const std::string& key, const variant& value);

		virtual void draw() const;
	
		void build();
		void applyRules(const std::string& rule);

		void neighborsChanged();

		const std::string& type() const { return type_; }
		virtual bool executeCommand(const variant& var);

		HexObjectPtr getTileInDir(enum direction d) const;
		HexObjectPtr getTileInDir(const std::string& s) const;

		int x() const { return x_; }
		int y() const { return y_; }

		TileTypePtr tile() const { return tile_; }

		static std::vector<std::string> getRules();
		static std::vector<TileTypePtr> getHexTiles();
		static std::vector<TileTypePtr>& getEditorTiles();
		static TileTypePtr getHexTile(const std::string& type);
	private:
		DISALLOW_COPY_ASSIGN_AND_DEFAULT(HexObject);

		// map coordinates.
		int x_;
		int y_;

		TileTypePtr tile_;

		struct NeighborType 
		{
			NeighborType() : dirmap(0) {}
			TileTypePtr type;
			unsigned char dirmap;
		};

		mutable std::vector<NeighborType> neighbors_;
		mutable bool neighbors_init_;
		void initNeighbors() const;

		// String representing the base type of this tile.
		std::string type_;
		// raw pointer to the map that owns this.
		const HexMap* owner_map_;
	};
}
