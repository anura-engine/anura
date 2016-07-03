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

#include "SceneUtil.hpp"
#include "Texture.hpp"

namespace hex 
{
	class HexEditorInfo;
	typedef boost::intrusive_ptr<HexEditorInfo> HexEditorInfoPtr;

	class HexEditorInfo : public game_logic::FormulaCallable
	{
		public:
			HexEditorInfo();
			explicit HexEditorInfo(const std::string& name, const std::string& type, const std::string& group, const KRE::TexturePtr& image, const std::string& image_file, const rect& r);
			static std::vector<variant> getHexEditorInfo();
		private:
			DECLARE_CALLABLE(HexEditorInfo);
			std::string name_;
			std::string type_;
			std::string image_file_;
			KRE::TexturePtr image_;
			std::string group_;
			rect image_rect_;
	};

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
		TileType(const std::string& tile, int num_id, const variant& n);
		
		const std::string& id() const { return tile_id_; }

		int numeric_id() const { return num_id_; }

		variant write() const;
		void calculateAdjacencyPattern(unsigned short adjmap);

		static TileTypePtr factory(const std::string& tile);

		KRE::TexturePtr getTexture() const { return sheet_ != nullptr ? sheet_->getTexture() : nullptr; }

		void render(int x, int y, std::vector<KRE::vertex_texcoord>* coords) const;
		void renderAdjacent(int x, int y, std::vector<KRE::vertex_texcoord>* coords, unsigned short adjmap) const;
	private:
		int num_id_;
		std::string tile_id_;
		TileSheetPtr sheet_;

		void renderInternal(int x, int y, const rect& area, std::vector<KRE::vertex_texcoord>* coords) const;

		std::vector<rect> sheet_area_;

		struct AdjacencyPattern {
			AdjacencyPattern() : init(false), depth(0)
			{}
			bool init;
			int depth;
			std::vector<rect> sheet_areas;
		};

		AdjacencyPattern adjacency_patterns_[1 << 12];
	};

	struct Alternate 
	{
		rect r;
		std::array<int, 4> border;
	};

	class Overlay : public game_logic::FormulaCallable
	{
	public:
		explicit Overlay(const std::string& name, const std::string& image, const std::map<std::string, std::vector<variant>>& alts);
		static OverlayPtr create(const std::string& name, const std::string& image, std::map<std::string, std::vector<variant>>& alts);
		static OverlayPtr getOverlay(const std::string& name);
		const Alternate& getAlternative(const std::string& type = std::string()) const;
		KRE::TexturePtr getTexture() const { return texture_; }
		static std::vector<variant> getOverlayInfo();
	private:
		DECLARE_CALLABLE(Overlay);

		std::string name_;
		std::string image_name_;
		KRE::TexturePtr texture_;

		std::map<std::string, std::vector<Alternate>> alternates_;

		Overlay(const Overlay&) = delete;
		Overlay() = delete;
		Overlay& operator=(const Overlay&) = delete;
	};

	void loader(const variant& n);
}
