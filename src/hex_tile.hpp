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

#include <boost/intrusive_ptr.hpp>
#include <vector>
#include <map>

#include "kre/Material.hpp"
#include "kre/SceneObject.hpp"

#include "decimal.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "hex_object_fwd.hpp"
#include "SceneObjectCallable.hpp"
#include "variant.hpp"

namespace hex 
{
	class TileSheet
	{
	public:
		explicit TileSheet(variant node);
		const KRE::TexturePtr& getTexture() const { return texture_; }
		rect getArea(int index) const;
	private:
		KRE::TexturePtr texture_;
		rect area_;
		int nrows_, ncols_, pad_;
	};

	class TileType : public graphics::SceneObjectCallable
	{
	public:
		TileType(const std::string& id, variant node);
		
		struct EditorInfo {
			std::string name;
			std::string type;
			mutable KRE::TexturePtr texture;
			std::string group;
			rect image_rect;
			void draw(int tx, int ty) const;
		};

		const std::string& id() const { return id_; }

		const EditorInfo& getgetEditorInfo() const { return editor_info_; } 

		const std::vector<int>& sheetIndexes() const { return sheet_indexes_; }

		void draw(int x, int y) const;

		//The lowest bit of adjmap indicates if this tile type occurs to the north
		//of the target tile, the next lowest for the north-east and so forth.
		void drawAdjacent(int x, int y, unsigned char adjmap) const;

		decimal height() const { return height_; }

		void preRender(const KRE::WindowManagerPtr& wnd);
		variant write() const;
		void calculateAdjacencyPattern(unsigned char adjmap);
	private:
		DECLARE_CALLABLE(TileType);
		std::string id_;
		TileSheetPtr sheet_;
		decimal height_;

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
}
