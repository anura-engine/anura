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
#include <map>

#include "hex_fwd.hpp"
#include "hex_logical_tiles.hpp"
#include "variant.hpp"

#include "Texture.hpp"

namespace hex 
{
	class TileSheet
	{
	public:
		explicit TileSheet(const variant& n);
		const KRE::TexturePtr& getTexture() const { return texture_; }
		rect getArea(int index) const;
	private:
		KRE::TexturePtr texture_;
		rect area_;
		int nrows_, ncols_, pad_;
	};

	class TileType
	{
	public:
		TileType(const std::string& id, const variant& n);
		
		struct EditorInfo {
			std::string name;
			std::string type;
			KRE::TexturePtr texture;
			std::string group;
			rect image_rect;
			void draw(int tx, int ty) const;
		};

		const std::string& id() const { return tile_->id(); }

		const EditorInfo& getEditorInfo() const { return editor_info_; } 

		const std::vector<int>& getSheetIndexes() const { return sheet_indexes_; }

		float getCost() const { return tile_->getCost(); }

		void draw(int x, int y, const point& cam) const;

		//The lowest bit of adjmap indicates if this tile type occurs to the north
		//of the target tile, the next lowest for the north-east and so forth.
		void drawAdjacent(int x, int y, const point& cam, unsigned char adjmap) const;

		float height() const { return tile_->getHeight(); }

		variant write() const;
		void calculateAdjacencyPattern(unsigned char adjmap);

		static TileTypePtr factory(const std::string& name);
	private:
		logical::TilePtr tile_;
		TileSheetPtr sheet_;

		std::vector<int> sheet_indexes_;

		struct AdjacencyPattern {
			AdjacencyPattern() : init(false), depth(0)
			{}
			bool init;
			int depth;
			std::vector<int> sheet_indexes;
		};

		AdjacencyPattern adjacency_patterns_[64];

		EditorInfo editor_info_;
	};

	void loader(const variant& n);
}
