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
#include "TextureObject.hpp"

#include "random.hpp"
#include "variant_utils.hpp"

namespace hex 
{
	namespace
	{
		std::map<std::string, TileTypePtr>& get_tile_type_map()
		{
			static std::map<std::string, TileTypePtr> tile_map;
			return tile_map;
		}

		typedef std::map<std::string, HexEditorInfoPtr> editor_info_map;
		editor_info_map& get_editor_tiles()
		{
			static editor_info_map res;
			return res;
		}

		std::map<std::string, OverlayPtr>& get_overlay_map()
		{
			static std::map<std::string, OverlayPtr> res;
			return res;
		}

		void parse_editor_info(const variant& value, const std::string& id, const KRE::TexturePtr& tex, const std::string& image_file, const rect& default_rect)
		{
			ASSERT_LOG(value.is_map(), "Must have editor info map, none found in: " << id);
			std::string name  = value["name"].as_string();
			std::string group = value["group"].as_string();
			rect image_rect = default_rect;
			if(value.has_key("sheet_pos")) {
				std::string sheet_pos = value["sheet_pos"].as_string();
				if(!sheet_pos.empty()) {
					const int index = strtol(sheet_pos.c_str(), nullptr, 36);
					const int row = index / 36;
					const int col = index % 36;
					image_rect = rect(col * 72, row * 72, 72, 72);
				}
			} else if(value.has_key("rect")) {
				image_rect = rect(value["rect"]);
			}
			auto& editor_info = get_editor_tiles();
			editor_info[id] = HexEditorInfoPtr(new HexEditorInfo(name, id, group, tex, image_file, image_rect));
		}
	}

	class BasicTileType : public TileType
	{
	public:
		BasicTileType(const std::string& tile, int num_id, const variant& n);
		void calculateAdjacencyPattern(int x, int y, const HexMap* hmap, std::vector<AdjacencyPattern>* patterns) override;
		void render(int x, int y, std::vector<KRE::vertex_texcoord>* coords) const override;
		void renderAdjacent(int x, int y, std::vector<KRE::vertex_texcoord>* coords, const std::vector<AdjacencyPattern>& patterns) const override;
	private:
		variant handleWrite() const override { /* XXX todo */ return variant(); }
		std::vector<rect> sheet_area_;
	};

	class WallTileType : public TileType
	{
	public:
		WallTileType(const std::string& tile, int num_id, const variant& n);
		void calculateAdjacencyPattern(int x, int y, const HexMap* hmap, std::vector<AdjacencyPattern>* patterns) override;
		void render(int x, int y, std::vector<KRE::vertex_texcoord>* coords) const override;
		void renderAdjacent(int x, int y, std::vector<KRE::vertex_texcoord>* coords, const std::vector<AdjacencyPattern>& patterns) const override;
	private:
		variant handleWrite() const override { /* XXX todo */ return variant(); }

		struct WallInfo {
			WallInfo(const rect& a) : area(a), borders() {}
			WallInfo(const rect& a, const std::vector<int>& b) : area(a), borders() {
				ASSERT_LOG(b.size() == 4, "Must have 4 elements in list.");
				for(int n = 0; n != 4; ++n) {
					borders[n] = b[n];
				}
			}
			rect area;
			std::array<int, 4> borders;
		};

		std::vector<WallInfo> concave_convex_;

		// "bl", "br", "l", "r", "tl", "tr"
		enum WallDirections {
			CONCAVE_BL,
			CONVEX_BL,
			CONCAVE_BR,
			CONVEX_BR,
			CONCAVE_L,
			CONVEX_L,
			CONCAVE_R,
			CONVEX_R,
			CONCAVE_TL,
			CONVEX_TL,
			CONCAVE_TR,
			CONVEX_TR,
		};
	};


	void loader(const variant& n)
	{
		logical::loader(n);

		if(!get_tile_type_map().empty()) {
			get_tile_type_map().clear();
		}
		
		int tile_id = 0;

		auto tiles_map = n["tiles"].as_map();
		for(auto& p : tiles_map) {
			std::string key_str = p.first.as_string();
			get_tile_type_map()[key_str] = std::make_shared<BasicTileType>(key_str, tile_id++, p.second);
		}

		if(n.has_key("walls")) {
			auto walls_map = n["walls"].as_map();
			for(auto& p : walls_map) {
				std::string key_str = p.first.as_string();
				get_tile_type_map()[key_str] = std::make_shared<WallTileType>(key_str, tile_id++, p.second);
			}
		}

		if(n.has_key("overlay")) {
			auto overlay_map = n["overlay"].as_map();
			for(auto& p : overlay_map) {
				ASSERT_LOG(p.first.is_string(), "First element of overlay must be a string key. " << p.first.debug_location());
				const std::string key = p.first.as_string();
				ASSERT_LOG(p.second.is_map(), "Second element of overlay must be a map. " << p.second.debug_location());
				std::string image;
				std::map<std::string, std::vector<variant>> normals;

				auto element = p.second.as_map();
				for(auto& el : element) {
					ASSERT_LOG(el.first.is_string(), "First element must be string. " << el.first.debug_location());
					const std::string ekey = el.first.as_string();
					if(ekey == "image") {
						ASSERT_LOG(el.second.is_string(), "'image' attribute should have a string value. " << el.second.debug_location());
						image = el.second.as_string();
					} else if(ekey == "editor_info") {
						// skip
					} else {
						const std::string name = el.first.as_string();
						ASSERT_LOG(el.second.is_list(), "'" << name << "' attribute should have a list value. " << el.second.debug_location());
						normals[name] = el.second.as_list();
					}
				}
				ASSERT_LOG(!image.empty(), "No 'image' tag found.");

				// Add element here for key
				get_overlay_map()[key] = Overlay::create(key, image, normals);
			}
		}
	}

	BasicTileType::BasicTileType(const std::string& tile, int num_id, const variant& value)
	  : TileType(TileTypeId::BASIC, tile, num_id),
		sheet_area_()
	{
		if(value.has_key("sheet_pos")) {
			for (const std::string& index_str : value["sheet_pos"].as_list_string()) {
				const int index = strtol(index_str.c_str(), nullptr, 36);
				const int x = 72 * (index % 36);
				const int y = 72 * (index / 36);
				sheet_area_.emplace_back(rect(x, y, 72, 72));
			}
		} else if(value.has_key("rect")) {
			for(const auto& v : value["rect"].as_list()) {
				sheet_area_.emplace_back(rect(v));
			}
		} else {
			ASSERT_LOG(false, "Tile definition needs either 'sheet_pos' or 'rect' attribute.");
		}

		const std::string image = value["image"].as_string();
		setTexture(image);

		/*auto adjacent = value["adjacent"].as_map();
		for (auto& p : adjacent) {
			unsigned short dirmap = 0;
			std::vector<std::string> dir;
			std::string adj = p.first.as_string();
			boost::split(dir, adj, boost::is_any_of(","));
			for(auto d : dir) {
				static const std::string Directions[] = { "n", "ne", "se", "s", "sw", "nw" };
				const std::string* dir_str = std::find(Directions, Directions+6, d);
				const int index = dir_str - Directions;
				if(index >= 6) {
					LOG_WARN("skipped direction string '" << p.first << "' " << p.first.to_debug_string());
				//ASSERT_LOG(index < 6, "Unrecognized direction string: " << p.first << " " << p.first.to_debug_string());
				} else {
					dirmap |= (1 << index);
				}
			}

			AdjacencyPattern& pattern = adjacency_patterns_[dirmap];
			const auto plist = p.second.as_list();
			ASSERT_LOG(!plist.empty(), "No adjency tiles defined.");
			for(const auto& ndx : plist) {
				if(ndx.is_string()) {
					const std::string index_str = ndx.as_string();
					const int index = strtol(index_str.c_str(), nullptr, 36);
					const int x = 72 * (index % 36);
					const int y = 72 * (index / 36);
					pattern.sheet_areas.emplace_back(rect(x, y, 72, 72));
				} else {
					const rect r{ ndx };
					pattern.sheet_areas.emplace_back(r);
				}
			}

			pattern.init = true;
			pattern.depth = 0;
		}*/

		if (value.has_key("editor_info")) {
			parse_editor_info(value["editor_info"], id(), getTexture(), image, sheet_area_.front());
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

	void TileType::renderInternal(int x, int y, int ox, int oy, const rect& area, std::vector<KRE::vertex_texcoord>* coords) const
	{
		const point p = HexMap::getPixelPosFromTilePos(x, y) + point(ox, oy);
		const KRE::TexturePtr& tex = getTexture();
		const rectf uv = tex->getTextureCoords(0, area);

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

	void BasicTileType::render(int x, int y, std::vector<KRE::vertex_texcoord>* coords) const
	{
		int index = 0;

		if(sheet_area_.size() > 1) {
			index = random_hash(x, y) % sheet_area_.size();
		}
		renderInternal(x, y, 0, 0, sheet_area_[index], coords);
	}

	void BasicTileType::renderAdjacent(int x, int y, std::vector<KRE::vertex_texcoord>* coords, const std::vector<AdjacencyPattern>& patterns) const
	{
		//const AdjacencyPattern& pattern = adjacency_patterns_[adjmap];
		//assert(pattern.init);
		//for(auto& area : pattern.sheet_areas) {
		//	renderInternal(x, y, 0, 0, area, coords);
		//}
	}

	void TileType::setTexture(const std::string & filename)
	{
		tex_ = KRE::Texture::createTexture(filename);
	}

	void BasicTileType::calculateAdjacencyPattern(int x, int y, const HexMap* hmap, std::vector<AdjacencyPattern>* patterns)
	{
	}

	TileTypePtr TileType::factory(const std::string& name)
	{
		auto it = get_tile_type_map().find(name);
		ASSERT_LOG(it != get_tile_type_map().end(), "Couldn't find tile with name: " << name);
		return it->second;
	}

	WallTileType::WallTileType(const std::string& tile, int num_id, const variant& n)
		: TileType(TileTypeId::WALL, tile, num_id),
		  concave_convex_()
	{
		const std::string image = n["image"].as_string();
		setTexture(image);

		ASSERT_LOG(n.has_key("concave") && n.has_key("convex"), "Wall entries must have 'convex' and 'concave' attributes." << n.debug_location());
		static const std::vector<std::string> tags{ "bl", "br", "l", "r", "tl", "tr" };
		for(auto& t : tags) {
			ASSERT_LOG(n["concave"].has_key(t), "No attribute '" << t << "' found in 'concave' section of '" << id() << "'" << n["concave"].debug_location());
			const auto& cave = n["concave"][t];
			if(cave.is_map() && cave.has_key("rect")) {
				std::vector<int> borders;
				borders.resize(4);
				if(cave.has_key("border")) {
					ASSERT_LOG(cave["border"].is_list() && cave["border"].num_elements() == 4, "'border' attribute must be list of 4 integers." << cave["border"].debug_location());
					borders = cave["border"].as_list_int();
				} else {
					borders[0] = borders[1] = borders[2] = borders[3] = 0;
				}
				concave_convex_.emplace_back(rect(cave["rect"]), borders);
			} else {
				concave_convex_.emplace_back(rect(cave));
			}

			ASSERT_LOG(n["convex"].has_key(t), "No attribute '" << t << "' found in 'convex' section of '" << id() << "'" << n["convex"].debug_location());
			const auto& vex = n["convex"][t];
			if(vex.is_map() && vex.has_key("rect")) {
				std::vector<int> borders;
				borders.resize(4);
				if(vex.has_key("border")) {
					ASSERT_LOG(vex["border"].is_list() && vex["border"].num_elements() == 4, "'border' attribute must be list of 4 integers." << vex["border"].debug_location());
					borders = vex["border"].as_list_int();
				} else {
					borders[0] = borders[1] = borders[2] = borders[3] = 0;
				}
				concave_convex_.emplace_back(rect(vex["rect"]), borders);
			} else {
				concave_convex_.emplace_back(rect(vex));
			}
		}
		if(n.has_key("editor_info")) {
			parse_editor_info(n["editor_info"], id(), getTexture(), image, rect());
		}
	}

	void WallTileType::calculateAdjacencyPattern(int x, int y, const HexMap* hmap, std::vector<AdjacencyPattern>* patterns)
	{
		ASSERT_LOG(hmap != nullptr, "HexMap was null.");
		ASSERT_LOG(patterns != nullptr, "adjacency pattern list was null.");
		auto n  = hmap->getHexTile(NORTH, x, y);
		auto ne = hmap->getHexTile(NORTH_EAST, x, y);
		auto se = hmap->getHexTile(SOUTH_EAST, x, y);
		auto s  = hmap->getHexTile(SOUTH, x, y);
		auto sw = hmap->getHexTile(SOUTH_WEST, x, y);
		auto nw = hmap->getHexTile(NORTH_WEST, x, y);

		static const int ox = -54;
		static const int oy = -108;

		// bl, br, l, r, tl, tr
		static const std::vector<point> oh{ point{0,72}, point{54, 108}, point{0, 72}, point{54, 36}, point{0, 0}, point{54, 36} };
		enum { BL, BR, L, R, TL, TR };

		if(n != nullptr && n->tile()->getTileTypeId() != getTileTypeId()) {
			static const point pdiff = HexMap::getPixelPosFromTilePos(x, y) - HexMap::getPixelPosFromTilePos(n->x(), n->y());
			if(ne != nullptr && ne->tile()->getTileTypeId() != getTileTypeId()) {
				auto& cc = concave_convex_[CONVEX_TR];
				patterns->emplace_back(ox + oh[TR].x + cc.borders[0], oy + oh[TR].y + cc.borders[1], cc.area);
			} else {
				auto& cc = concave_convex_[CONCAVE_BR];
				patterns->emplace_back(ox + oh[BR].x - pdiff.x + cc.borders[0], oy + oh[BR].y - pdiff.y + cc.borders[1], cc.area);
			}
			if(nw != nullptr && nw->tile()->getTileTypeId() != getTileTypeId()) {
				auto& cc = concave_convex_[CONVEX_TL];
				patterns->emplace_back(ox + oh[TL].x + cc.borders[0], oy + oh[TL].y + cc.borders[1], cc.area);
			} else {
				auto& cc = concave_convex_[CONCAVE_BL];
				patterns->emplace_back(ox + oh[BL].x - pdiff.x + cc.borders[0], oy + oh[BL].y - pdiff.y + cc.borders[1], cc.area);
			}
		}

		if(se != nullptr && se->tile()-> getTileTypeId() != getTileTypeId()) {
			if(ne != nullptr && ne->tile()-> getTileTypeId() != getTileTypeId()) {
				auto& cc = concave_convex_[CONVEX_R];
				patterns->emplace_back(ox + oh[R].x + cc.borders[0], oy + oh[R].y + cc.borders[1], cc.area);
			} else {
				static const point pdiff = HexMap::getPixelPosFromTilePos(x, y) - HexMap::getPixelPosFromTilePos(se->x(), se->y());
				auto& cc = concave_convex_[CONCAVE_TL];
				patterns->emplace_back(ox + oh[TL].x - pdiff.x + cc.borders[0], oy + oh[TL].y - pdiff.y + cc.borders[1], cc.area);
			}
		}

		if(sw != nullptr && sw->tile()-> getTileTypeId() != getTileTypeId()) {
			if(nw != nullptr && nw->tile()-> getTileTypeId() != getTileTypeId()) {
				auto& cc = concave_convex_[CONVEX_L];
				patterns->emplace_back(ox + oh[L].x + cc.borders[0], oy + oh[L].y + cc.borders[1], cc.area);
			} else {
				static const point pdiff = HexMap::getPixelPosFromTilePos(x, y) - HexMap::getPixelPosFromTilePos(sw->x(), sw->y());
				auto& cc = concave_convex_[CONCAVE_TR];
				patterns->emplace_back(ox + oh[TR].x - pdiff.x + cc.borders[0], oy + oh[TR].y - pdiff.y + cc.borders[1], cc.area);
			}
		}

		if(s != nullptr && s->tile()->getTileTypeId() == getTileTypeId()) {
			if(se != nullptr && se->tile()-> getTileTypeId() != getTileTypeId()) {
				static const point pdiff = HexMap::getPixelPosFromTilePos(x, y) - HexMap::getPixelPosFromTilePos(se->x(), se->y());
				auto& cc = concave_convex_[CONCAVE_L];
				patterns->emplace_back(ox + oh[L].x - pdiff.x + cc.borders[0], oy + oh[L].y - pdiff.y + cc.borders[1], cc.area);
			}
			if(sw != nullptr && sw->tile()-> getTileTypeId() != getTileTypeId()) {
				static const point pdiff = HexMap::getPixelPosFromTilePos(x, y) - HexMap::getPixelPosFromTilePos(sw->x(), sw->y());
				auto& cc = concave_convex_[CONCAVE_R];
				patterns->emplace_back(ox + oh[R].x - pdiff.x + cc.borders[0], oy + oh[R].y - pdiff.y + cc.borders[1], cc.area);
			}
		}

		if(s != nullptr && s->tile()->getTileTypeId() != getTileTypeId()) {
			static const point pdiff = HexMap::getPixelPosFromTilePos(x, y) - HexMap::getPixelPosFromTilePos(s->x(), s->y());
			if(se != nullptr && se->tile()-> getTileTypeId() != getTileTypeId()) {
				auto& cc = concave_convex_[CONVEX_BR];
				patterns->emplace_back(ox + oh[BR].x + cc.borders[0], oy + oh[BR].y + cc.borders[1], concave_convex_[CONVEX_BR].area);
			} else {
				auto& cc = concave_convex_[CONCAVE_TR];
				patterns->emplace_back(ox + oh[TR].x - pdiff.x + cc.borders[0], oy + oh[TR].y - pdiff.y + cc.borders[1], cc.area);
			}
			if(sw != nullptr && sw->tile()-> getTileTypeId() != getTileTypeId()) {
				auto& cc = concave_convex_[CONVEX_BL];
				patterns->emplace_back(ox + oh[BL].x + cc.borders[0], oy + oh[BL].y + cc.borders[1], cc.area);
			} else {
				auto& cc = concave_convex_[CONCAVE_TL];
				patterns->emplace_back(ox + oh[TL].x - pdiff.x + cc.borders[0], oy + oh[TL].y - pdiff.y + cc.borders[1], cc.area);
			}
		} 
	
	}


	void WallTileType::render(int x, int y, std::vector<KRE::vertex_texcoord>* coords) const
	{
	}

	void WallTileType::renderAdjacent(int x, int y, std::vector<KRE::vertex_texcoord>* coords, const std::vector<AdjacencyPattern>& patterns) const
	{
		for(auto& adj : patterns) {
			renderInternal(x, y, adj.ox, adj.oy, adj.area, coords);
		}
	}

	OverlayPtr Overlay::create(const std::string& name, const std::string& image, std::map<std::string, std::vector<variant>>& alts)
	{
		return OverlayPtr(new Overlay(name, image, alts));
	}

	Overlay::Overlay(const std::string& name, const std::string& image, const std::map<std::string, std::vector<variant>>& alts)
		: name_(name),
		  image_name_(image),
		  texture_(KRE::Texture::createTexture(image)),
		  alternates_()
	{
		for(const auto& alternate : alts) {
			std::vector<Alternate> a;
			a.resize(alternate.second.size());
			auto it = a.begin();
			for(const auto& v : alternate.second) {
				// should consist of a series of rectangles (x1 y1 x2 y2) in the 'rect' attribute and an optional 'border' attribute.
				ASSERT_LOG(v.has_key("rect"), "Unable to find key 'rect' while parsing the overlays");
				it->r = rect(v["rect"]);
				if(v.has_key("border")) {
					ASSERT_LOG(v["border"].is_list() && v["border"].num_elements() == 4, "The 'border' attribute should be a list of 4(four) elements.");
					for(int n = 0; n != 4; ++n) {
						it->border[n] = v["border"][n].as_int32();
					}
				} else {
					for(int n = 0; n != 4; ++n) {
						it->border[n] = 0;
					}
				}
				++it;
			}
			alternates_[alternate.first] = a;
		}
	}

	OverlayPtr Overlay::getOverlay(const std::string& name)
	{
		auto it = get_overlay_map().find(name);
		ASSERT_LOG(it != get_overlay_map().end(), "Couldn't find an overlay named '" << name << "'");
		return it->second;
	}

	const Alternate& Overlay::getAlternative(const std::string& type) const
	{		
		ASSERT_LOG(!alternates_.empty(), "No alternatives found, must be at least one.");
		auto it = alternates_.find(type.empty() ? "default" : type);
		ASSERT_LOG(it != alternates_.end(), "Unknown alternate '" << (type.empty() ? "default" : type) << "'");
		const auto& alt = it->second[rng::generate() % it->second.size()];
		return alt;
	}

	std::vector<variant> Overlay::getOverlayInfo()
	{
		std::vector<variant> res;
		for(const auto& o : get_overlay_map()) {
			res.emplace_back(o.second.get());
		}
		return res;
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(Overlay)
		DEFINE_FIELD(name, "string")
			return variant(obj.name_);
		DEFINE_FIELD(image_file, "string")
			return variant(obj.image_name_);
		DEFINE_FIELD(alternates, "{string -> [{string -> [int,int,int,int]}]}")
			std::map<variant, variant> alt_map;
			for(const auto& alt : obj.alternates_) {
				std::vector<variant> alt_list;
				for(const auto& altlst : alt.second) {
					variant_builder res;
					res.add("rect", altlst.r.write());
					for(const auto& border : altlst.border) {
						res.add("border", border);
					}
					alt_list.emplace_back(res.build());
				}
				alt_map[variant(alt.first)] = variant(&alt_list);
			}
			return variant(&alt_map);
	END_DEFINE_CALLABLE(Overlay)

	HexEditorInfo::HexEditorInfo()
		: name_(),
		  type_(),
		  image_(),
		  group_(),
		  image_file_(),
		  image_rect_()
	{
	}

	HexEditorInfo::HexEditorInfo(const std::string& name, const std::string& type, const std::string& group, const KRE::TexturePtr& image, const std::string& image_file, const rect& r)
		: name_(name),
		  type_(type),
		  image_(image),
		  group_(group),
		  image_file_(image_file),
		  image_rect_(r)
	{
	}

	std::vector<variant> HexEditorInfo::getHexEditorInfo()
	{
		std::vector<variant> res;
		auto& editor_info = get_editor_tiles();
		for(const auto& ei : editor_info) {
			res.emplace_back(ei.second.get());
		}
		return res;
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(HexEditorInfo)
		DEFINE_FIELD(name, "string")
			return variant(obj.name_);
		DEFINE_FIELD(type, "string")
			return variant(obj.type_);
		DEFINE_FIELD(group, "string")
			return variant(obj.group_);
		DEFINE_FIELD(image_rect, "[int,int,int,int]")
			return obj.image_rect_.write();
		DEFINE_FIELD(image_file, "string")
			return variant(obj.image_file_);
		DEFINE_FIELD(image, "builtin texture_object")
			return variant(new TextureObject(obj.image_));
	END_DEFINE_CALLABLE(HexEditorInfo)
}
