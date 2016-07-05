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

	void loader(const variant& n)
	{
		logical::loader(n);

		if(!get_tile_type_map().empty()) {
			get_tile_type_map().clear();
		}
		
		int tile_id = 0;

		for(auto p : n["tiles"].as_map()) {
			std::string key_str = p.first.as_string();
			get_tile_type_map()[key_str] = TileTypePtr(new TileType(key_str, tile_id++, p.second));
		}

		if(n.has_key("overlay")) {
			for(auto p : n["overlay"].as_map()) {
				ASSERT_LOG(p.first.is_string(), "First element of overlay must be a string key. " << p.first.debug_location());
				const std::string key = p.first.as_string();
				ASSERT_LOG(p.second.is_map(), "Second element of overlay must be a map. " << p.second.debug_location());
				std::string image;
				std::map<std::string, std::vector<variant>> normals;

				for(const auto el : p.second.as_map()) {
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

	TileSheet::TileSheet(const variant& value)
		: texture_(KRE::Texture::createTexture(value["image"])),
		  area_(rect(0, 0, 72, 72)), 
		  ncols_(36), 
		  pad_(0)
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

	TileType::TileType(const std::string& tile, int num_id, const variant& value)
	  : num_id_(num_id),
	    tile_id_(tile),
	    sheet_(new TileSheet(value)),
		sheet_area_(),
		adjacency_patterns_()
	{
		if(value.has_key("sheet_pos")) {
			for (const std::string& index_str : value["sheet_pos"].as_list_string()) {
				const int index = strtol(index_str.c_str(), nullptr, 36);				
				sheet_area_.emplace_back(sheet_->getArea(index));
			}
		} else if(value.has_key("rect")) {
			for(const auto& v : value["rect"].as_list()) {
				sheet_area_.emplace_back(rect(v));
			}
		} else {
			ASSERT_LOG(false, "Tile definition needs either 'sheet_pos' or 'rect' attribute.");
		}

		for (auto p : value["adjacent"].as_map()) {
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
					pattern.sheet_areas.emplace_back(sheet_->getArea(index));
				} else {
					const rect r{ ndx };
					pattern.sheet_areas.emplace_back(r);
				}
			}

			pattern.init = true;
			pattern.depth = 0;
		}

		ASSERT_LOG(sheet_area_.empty() == false, "No sheet areas defined in the hex tile sheet: " << tile_id_);

		if (value.has_key("editor_info")) {
			std::string image_file = KRE::Texture::findImageNames(value["image"]).front();
			parse_editor_info(value["editor_info"], tile_id_, sheet_->getTexture(), image_file, sheet_area_.front());
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

	void TileType::renderInternal(int x, int y, const rect& area, std::vector<KRE::vertex_texcoord>* coords) const
	{
		const point p(HexMap::getPixelPosFromTilePos(x, y));
		const KRE::TexturePtr& tex = sheet_->getTexture();
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

	void TileType::render(int x, int y, std::vector<KRE::vertex_texcoord>* coords) const
	{
		if(sheet_area_.empty()) {
			return;
		}

		int index = 0;

		if(sheet_area_.size() > 1) {
			index = random_hash(x, y) % sheet_area_.size();
		}
		renderInternal(x, y, sheet_area_[index], coords);
	}

	void TileType::renderAdjacent(int x, int y, std::vector<KRE::vertex_texcoord>* coords, unsigned short adjmap) const
	{
		const AdjacencyPattern& pattern = adjacency_patterns_[adjmap];
		assert(pattern.init);
		for(auto& area : pattern.sheet_areas) {
			renderInternal(x, y, area, coords);
		}
	}

	void TileType::calculateAdjacencyPattern(unsigned short adjmap)
	{
		if(adjacency_patterns_[adjmap].init) {
			return;
		}

		int best = -1;
		for(int dir = 0; dir < 6; ++dir) {
			unsigned short mask = 1 << dir;
			if(adjmap & mask) {
				unsigned short newmap = adjmap & ~mask;
				if(newmap != 0) {
					calculateAdjacencyPattern(newmap);

					if(best == -1 || adjacency_patterns_[newmap].depth < adjacency_patterns_[best].depth) {
						best = newmap;
					}
				}
			}
		}

		if(best != -1) {
			adjacency_patterns_[adjmap].sheet_areas.insert(adjacency_patterns_[adjmap].sheet_areas.end(), adjacency_patterns_[best].sheet_areas.begin(), adjacency_patterns_[best].sheet_areas.end());
			adjacency_patterns_[adjmap].depth = adjacency_patterns_[best].depth + 1;

			best = adjmap & ~best;
			calculateAdjacencyPattern(best);
			adjacency_patterns_[adjmap].sheet_areas.insert(adjacency_patterns_[adjmap].sheet_areas.end(), adjacency_patterns_[best].sheet_areas.begin(), adjacency_patterns_[best].sheet_areas.end());
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
		const auto& alt = it->second[rng::generate() % alternates_.size()];
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
