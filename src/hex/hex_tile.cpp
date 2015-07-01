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

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <functional>

#include "asserts.hpp"
#include "hex_object.hpp"
#include "hex_tile.hpp"
#include "variant_utils.hpp"
#include "Texture.hpp"

namespace hex 
{
	namespace
	{
		std::map<std::string, TileTypePtr>& get_tile_type_map()
		{
			static std::map<std::string, TileTypePtr> tile_map;
			return tile_map;
		}

		std::vector<TileTypePtr>& get_hex_editor_tiles()
		{
			static std::vector<TileTypePtr> tiles;
			return tiles;
		}

		std::map<std::string, TileTypePtr>& get_editor_hex_tile_map()
		{
			static std::map<std::string, TileTypePtr> tile_map;
			return tile_map;
		}

		void load_editor_tiles()
		{
			std::map<std::string, TileTypePtr>::const_iterator it = get_tile_type_map().begin();
			while(it != get_tile_type_map().end()) {
				if(it->second->getEditorInfo().name.empty() == false 
					&& it->second->getEditorInfo().type.empty() == false) {
					get_hex_editor_tiles().push_back(it->second);
				}
				++it;
			}
		}

		void load_hex_editor_tiles()
		{
			std::map<std::string, TileTypePtr>::const_iterator it = get_tile_type_map().begin();
			while(it != get_tile_type_map().end()) {
				if(it->second->getEditorInfo().type.empty() == false) {
					get_editor_hex_tile_map()[it->second->getEditorInfo().type] = it->second;
				}
				++it;
			}
		}

	}

	void loader(const variant& n)
	{
		logical::loader(n);

		if(!get_tile_type_map().empty()) {
			get_tile_type_map().clear();
		}
		for(auto p : n["tiles"].as_map()) {
			std::string key_str = p.first.as_string();
			get_tile_type_map()[key_str] = TileTypePtr(new TileType(key_str, p.second));
		}

		// get list of all tiles have non-empty "editor_info" blocks.
		if(!get_hex_editor_tiles().empty()) {
			get_hex_editor_tiles().clear();
		}
		load_editor_tiles();

		if(!get_editor_hex_tile_map().empty()) {
			get_editor_hex_tile_map().clear();
		}
		load_hex_editor_tiles();
	}

	void TileType::EditorInfo::draw(int x, int y) const
	{
		point p(HexMap::getPixelPosFromTilePos(x,y));
		// XXX todo.
	}

	TileSheet::TileSheet(const variant& value)
		: texture_(KRE::Texture::createTexture(value["image"])),
		  area_(rect(2, 2, 72, 72)), 
		  ncols_(36), 
		  pad_(4)
	{
	}

	rect TileSheet::getArea(int index) const
	{
		const int row = index/ncols_;
		const int col = index%ncols_;

		const int x = area_.x() + (area_.w()+pad_)*col;
		const int y = area_.y() + (area_.h()+pad_)*row;
		rect result(x, y, area_.w(), area_.h());

		return result;
	}

	TileType::TileType(const std::string& id, const variant& value)
	  : tile_(logical::Tile::factory(id)),
	    sheet_(new TileSheet(value))
	{
		for (const std::string& index_str : value["sheet_pos"].as_list_string()) {
			const int index = strtol(index_str.c_str(), nullptr, 36);
			sheet_indexes_.push_back(index);
		}

		for (auto p : value["adjacent"].as_map()) {
			unsigned char dirmap = 0;
			std::vector<std::string> dir;
			std::string adj = p.first.as_string();
			boost::split(dir, adj, boost::is_any_of(","));
			for(auto d : dir) {
				static const std::string Directions[] = { "n", "ne", "se", "s", "sw", "nw" };
				const std::string* dir_str = std::find(Directions, Directions+6, d);
				const int index = dir_str - Directions;
				ASSERT_LOG(index < 6, "Unrecognized direction string: " << p.first << " " << p.first.to_debug_string());

				dirmap |= (1 << index);
			}

			AdjacencyPattern& pattern = adjacency_patterns_[dirmap];
			for(const std::string& index_str : p.second.as_list_string()) {
				const int index = strtol(index_str.c_str(), nullptr, 36);
				pattern.sheet_indexes.push_back(index);
			}

			pattern.init = true;
			pattern.depth = 0;
		}

		ASSERT_LOG(sheet_indexes_.empty() == false, "No sheet indexes in hex tile sheet: " << id);

		if (value.has_key("editor_info")) {
			ASSERT_LOG(value["editor_info"].is_map(), "Must have editor info map, none found in: " << tile_->id());
			editor_info_.texture = sheet_->getTexture();
			editor_info_.name = value["editor_info"]["name"].as_string();
			editor_info_.group = value["editor_info"]["group"].as_string();
			editor_info_.type = id;
			editor_info_.image_rect = sheet_->getArea(0);
		}
	}

	variant TileType::write() const
	{
		ASSERT_LOG(false, "XXX write me tile_type::write()");
		return variant();
	}

	namespace 
	{
		int random_hash(int x, int y)
		{
			static const unsigned int x_rng[] = {31,29,62,59,14,2,64,50,17,74,72,47,69,92,89,79,5,21,36,83,81,35,58,44,88,5,51,4,23,54,87,39,44,52,86,6,95,23,72,77,48,97,38,20,45,58,86,8,80,7,65,0,17,85,84,11,68,19,63,30,32,57,62,70,50,47,41,0,39,24,14,6,18,45,56,54,77,61,2,68,92,20,93,68,66,24,5,29,61,48,5,64,39,91,20,69,39,59,96,33,81,63,49,98,48,28,80,96,34,20,65,84,19,87,43,4,54,21,35,54,66,28,42,22,62,13,59,42,17,66,67,67,55,65,20,68,75,62,58,69,95,50,34,46,56,57,71,79,80,47,56,31,35,55,95,60,12,76,53,52,94,90,72,37,8,58,9,70,5,89,61,27,28,51,38,58,60,46,25,86,46,0,73,7,66,91,13,92,78,58,28,2,56,3,70,81,19,98,50,50,4,0,57,49,36,4,51,78,10,7,26,44,28,43,53,56,53,13,6,71,95,36,87,49,62,63,30,45,75,41,59,51,77,0,72,28,24,25,35,4,4,56,87,23,25,21,4,58,57,19,4,97,78,31,38,80,};
			static const unsigned int y_rng[] = {91,80,42,50,40,7,82,67,81,3,54,31,74,49,30,98,49,93,7,62,10,4,67,93,28,53,74,20,36,62,54,64,60,33,85,31,31,6,22,2,29,16,63,46,83,78,2,11,18,39,62,56,36,56,0,39,26,45,72,46,11,4,49,13,24,40,47,51,17,99,80,64,27,21,20,4,1,37,33,25,9,87,87,36,44,4,77,72,23,73,76,47,28,41,94,69,48,81,82,0,41,7,90,75,4,37,8,86,64,14,1,89,91,0,29,44,35,36,78,89,40,86,19,5,39,52,24,42,44,74,71,96,78,29,54,72,35,96,86,11,49,96,90,79,79,70,50,36,15,50,34,31,86,99,77,97,19,15,32,54,58,87,79,85,49,71,91,78,98,64,18,82,55,66,39,35,86,63,87,41,25,73,79,99,43,2,29,16,53,42,43,26,45,45,95,70,35,75,55,73,58,62,45,86,46,90,12,10,72,88,29,77,10,8,92,72,22,3,1,49,5,51,41,86,65,66,95,23,60,87,64,86,55,30,48,76,21,76,43,52,52,23,40,64,69,43,69,97,34,39,18,87,46,8,96,50,};
			return x_rng[x%(sizeof(x_rng)/sizeof(*x_rng))] +
				   y_rng[y%(sizeof(y_rng)/sizeof(*y_rng))];
		}
	}

	void TileType::renderInternal(int x, int y, int index, std::vector<KRE::vertex_texcoord>* coords) const
	{
		const point p(HexMap::getPixelPosFromTilePos(x, y));
		const rect area = sheet_->getArea(index);
		const KRE::TexturePtr& tex = sheet_->getTexture();
		rectf uv = tex->getTextureCoords(0, area);

		const float vx1 = static_cast<float>(p.x);
		const float vy1 = static_cast<float>(p.y);
		const float vx2 = static_cast<float>(p.x + area.w());
		const float vy2 = static_cast<float>(p.y + area.h());

		coords->emplace_back(glm::vec2(vx1, vy1), glm::vec2(uv.x1(), uv.y1()));
		coords->emplace_back(glm::vec2(vx2, vy1), glm::vec2(uv.x2(), uv.y1()));
		coords->emplace_back(glm::vec2(vx2, vy2), glm::vec2(uv.x2(), uv.y2()));

		coords->emplace_back(glm::vec2(vx2, vy2), glm::vec2(uv.x2(), uv.y2()));
		coords->emplace_back(glm::vec2(vx1, vy1), glm::vec2(uv.x1(), uv.y1()));
		coords->emplace_back(glm::vec2(vx1, vy2), glm::vec2(uv.x1(), uv.y2()));
	}

	void TileType::render(int x, int y, std::vector<KRE::vertex_texcoord>* coords) const
	{
		if(sheet_indexes_.empty()) {
			return;
		}

		int index = 0;

		if(sheet_indexes_.size() > 1) {
			index = random_hash(x, y) % sheet_indexes_.size();
		}
		renderInternal(x, y, sheet_indexes_[index], coords);
	}

	void TileType::renderAdjacent(int x, int y, std::vector<KRE::vertex_texcoord>* coords, unsigned char adjmap) const
	{
		const AdjacencyPattern& pattern = adjacency_patterns_[adjmap];
		assert(pattern.init);
		for(int index : pattern.sheet_indexes) {
			renderInternal(x, y, index, coords);
		}
	}

	void TileType::calculateAdjacencyPattern(unsigned char adjmap)
	{
		if(adjacency_patterns_[adjmap].init) {
			return;
		}

		int best = -1;
		for(int dir = 0; dir < 6; ++dir) {
			unsigned char mask = 1 << dir;
			if(adjmap & mask) {
				unsigned char newmap = adjmap & ~mask;
				if(newmap != 0) {
					calculateAdjacencyPattern(newmap);

					if(best == -1 || adjacency_patterns_[newmap].depth < adjacency_patterns_[best].depth) {
						best = newmap;
					}
				}
			}
		}

		if(best != -1) {
			adjacency_patterns_[adjmap].sheet_indexes.insert(adjacency_patterns_[adjmap].sheet_indexes.end(), adjacency_patterns_[best].sheet_indexes.begin(), adjacency_patterns_[best].sheet_indexes.end());
			adjacency_patterns_[adjmap].depth = adjacency_patterns_[best].depth + 1;

			best = adjmap & ~best;
			calculateAdjacencyPattern(best);
			adjacency_patterns_[adjmap].sheet_indexes.insert(adjacency_patterns_[adjmap].sheet_indexes.end(), adjacency_patterns_[best].sheet_indexes.begin(), adjacency_patterns_[best].sheet_indexes.end());
			if(adjacency_patterns_[best].depth + 1 > adjacency_patterns_[adjmap].depth) {
				adjacency_patterns_[adjmap].depth = adjacency_patterns_[best].depth + 1;
			}
		}

		adjacency_patterns_[adjmap].init = true;
	}

	TileTypePtr TileType::factory(const std::string& name)
	{
		auto it = get_tile_type_map().find(name);
		ASSERT_LOG(it != get_tile_type_map().end(), "Couldn't find tile with name: " << name);
		return it->second;
	}
}
