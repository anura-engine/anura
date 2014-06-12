/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "asserts.hpp"
#include "foreach.hpp"
#include "HexObject.hpp"
#include "hex_tile.hpp"
#include "string_utils.hpp"
#include "variant_utils.hpp"

namespace hex {
/*
basic_hex_tile::basic_hex_tile(const variant node, hex_tile* owner)
	: cycle_(0), chance_(node["chance"].as_int(100)), owner_(owner), zorder_(node["zorder"].as_int(-500)),
	offset_x_(0), offset_y_(0)
{
	ASSERT_LOG(chance_ >= 1 && chance_ <= 100, "Chance must be between 1 and 100, inclusive.");
	image_ = node["image"].as_string_default();
	if(node.has_key("rect")) {
		ASSERT_LOG(node["rect"].num_elements() == 4 && node["rect"].is_list(), "rect must be a list of four(4) integers.");
		rect_ = rect::from_coordinates(node["rect"][0].as_int(), node["rect"][1].as_int(), node["rect"][2].as_int(), node["rect"][3].as_int());
	}
	if(node.has_key("animation")) {	
		if(node.is_list()) {
			nodes_ = node["animation"].as_list();
		} else {
			nodes_.push_back(node["animation"]);
		}
		frame_.reset(new frame(nodes_.front()));
	}
	if(node.has_key("offset")) {
		ASSERT_LOG(node["offset"].num_elements() == 2 && node["offset"].is_list(), "Offset field is specified as a list of two(2) elements");
		offset_x_ = node["offset"][0].as_int();
		offset_y_ = node["offset"][1].as_int();
	}
}

basic_hex_tile::~basic_hex_tile()
{}

void basic_hex_tile::draw(int x, int y) const
{
	point p(HexMap::get_pixel_pos_from_tile_pos(x,y));
	p.x -= offset_x_;
	p.y -= offset_y_;
	if(frame_) {
		frame_->draw(p.x, p.y, true, false, cycle_);
		if(++cycle_ >= frame_->duration()) {
			cycle_ = 0;
			// XXX: here we could do stuff like cycling through animations automatically
			// or calling event handlers to grab the next animation to play etc.
		}
	} else {
		graphics::blit_texture(texture_, p.x, p.y, rect_.w(), rect_.h(), 0.0f, 
			GLfloat(rect_.x())/GLfloat(texture_.width()),
			GLfloat(rect_.y())/GLfloat(texture_.height()),
			GLfloat(rect_.x2())/GLfloat(texture_.width()),
			GLfloat(rect_.y2())/GLfloat(texture_.height()));
	}
}

void basic_hex_tile::getTexture()
{
	if(!texture_.valid() && !image_.empty()) {
		texture_ = graphics::texture::get(image_);
	}
}

variant basic_hex_tile::getValue(const std::string& key) const
{
	if(key == "self") {
		return variant(this);
	} else if(key == "type") {
		return variant(type());
	} else if(key == "owner") {
		return variant(owner_);
	}
	return variant();
}

void basic_hex_tile::setValue(const std::string& key, const variant& value)
{
}

variant basic_hex_tile::write() const
{
	// XXX todo
	return variant();
}

std::string basic_hex_tile::type() const 
{ 
	ASSERT_LOG(owner_ != NULL, "Owner of tile was set to NULL!");
	return owner_->type(); 
}


hex_tile::hex_tile(const std::string& type, variant node)
	: type_(type), name_(node["name"].as_string())
{

	if(node.has_key("EditorInfo")) {
		ASSERT_LOG(node["EditorInfo"].is_map(), "Must have editor info map, none found in: " << type_);
		EditorInfo_.name = node["EditorInfo"]["name"].as_string();
		EditorInfo_.image = node["EditorInfo"]["image"].as_string();
		EditorInfo_.group = node["EditorInfo"]["group"].as_string();
		EditorInfo_.type = node["EditorInfo"]["type"].as_string();
		ASSERT_LOG(node["EditorInfo"]["rect"].num_elements() == 4 && node["EditorInfo"]["rect"].is_list(), "rect must be a list of four(4) integers.");
		EditorInfo_.image_rect = rect::from_coordinates(node["EditorInfo"]["rect"][0].as_int(), 
			node["EditorInfo"]["rect"][1].as_int(), 
			node["EditorInfo"]["rect"][2].as_int(), 
			node["EditorInfo"]["rect"][3].as_int());

		if(!EditorInfo_.texture.valid() && !EditorInfo_.image.empty()) {
			EditorInfo_.texture = graphics::texture::get(EditorInfo_.image);
		}
	}

	if(node.has_key("variations")) {
		ASSERT_LOG(node["variations"].is_list(), "Variations field in \"" << type_ << "\" must be a list type.");
		for(size_t i = 0; i < node["variations"].num_elements(); ++i) {
			variations_.push_back(basic_hex_tile_ptr(new basic_hex_tile(node["variations"][i], this)));
		}
	}
	if(node.has_key("transitions")) {
		ASSERT_LOG(node["transitions"].is_map(), "Transitions field in \"" << type_ << "\" must be a map type.");
		foreach(const variant_pair& p, node["transitions"].as_map()) {
			ASSERT_LOG(p.second.is_map(), "Inner of transitions of \"" << type_ << "\" must be a map type.");
			transition_map tmap;
			foreach(const variant_pair& p2, p.second.as_map()) {
				std::vector<basic_hex_tile_ptr> v;
				if(p2.second.is_list()) {
					for(size_t i = 0; i != p2.second.num_elements(); ++i) {
						v.push_back(basic_hex_tile_ptr(new basic_hex_tile(p2.second[i], this)));
					}
				} else {
					v.push_back(basic_hex_tile_ptr(new basic_hex_tile(p2.second, this)));
				}
				tmap[p2.first.as_string()] = v;
			}
			transitions_[p.first.as_string()] = tmap;
		}
	}
}

hex_tile::~hex_tile()
{}

transition_map* hex_tile::find_transition(const std::string& key)
{
	std::map<std::string, transition_map>::iterator it = transitions_.find(key);
	if(it == transitions_.end()) {
		return NULL;
	}
	return &it->second;
}

variant hex_tile::get_transitions()
{
	std::vector<variant> v;
	std::map<std::string, transition_map>::const_iterator it = transitions_.begin();
	while(it != transitions_.end()) {
		v.push_back(variant(it->first));
		++it;
	}
	return variant(&v);
}

class transition_map_callable : public game_logic::FormulaCallable 
{
	hex_tile_ptr tile_;
	transition_map* tm_;
	variant getValue(const std::string& key) const 
	{
		transition_map::const_iterator it = tm_->find(key);
		if(it == tm_->end()) {
			if(key == "values") {
				it = tm_->begin();
				std::vector<variant> v;
				while(it != tm_->end()) {
					v.push_back(variant(it->first));
					++it;
				}
				return variant(&v);
			}
			return variant();
		}

		int roll = rand() % it->second.size();
		it->second[roll]->getTexture();
		return variant(it->second[roll].get());
	}
	void setValue(const std::string& key, const variant& value) 
	{}
public:
	explicit transition_map_callable(const hex_tile& tile, transition_map* tm) 
		: tile_(const_cast<hex_tile*>(&tile)), tm_(tm)
	{}
};

class transition_callable : public game_logic::FormulaCallable 
{
	hex_tile_ptr tile_;
	variant getValue(const std::string& key) const 
	{
		transition_map* tm = tile_->find_transition(key);
		if(tm) {
			return variant(new transition_map_callable(*tile_, tm));
		}
		if(key == "values") {
			return tile_->get_transitions();
		}
		return variant();
	}
	void setValue(const std::string& key, const variant& value) 
	{}
public:
	explicit transition_callable(const hex_tile& tile) 
		: tile_(const_cast<hex_tile*>(&tile))
	{}
};

class EditorInfo_callable : public game_logic::FormulaCallable
{
	hex_tile_ptr tile_;
	variant getValue(const std::string& key) const
	{
		if(key == "type") {
			return variant(tile_->getEditorInfo().type);
		} else if(key == "name") {
			return variant(tile_->getEditorInfo().name);
		} else if(key == "image") {
			return variant(tile_->getEditorInfo().image);
		} else if(key == "rect") {
			std::vector<variant> v;
			v.push_back(variant(tile_->getEditorInfo().image_rect.x()));
			v.push_back(variant(tile_->getEditorInfo().image_rect.y()));
			v.push_back(variant(tile_->getEditorInfo().image_rect.w()));
			v.push_back(variant(tile_->getEditorInfo().image_rect.h()));
			return variant(&v);
		} else if(key == "group") {
			return variant(tile_->getEditorInfo().group);
		}
		std::map<variant, variant> m;
		m[variant("type")] = variant(tile_->getEditorInfo().type);
		m[variant("name")] = variant(tile_->getEditorInfo().name);
		m[variant("image")] = variant(tile_->getEditorInfo().image);
		m[variant("group")] = variant(tile_->getEditorInfo().group);
		std::vector<variant> v;
		v.push_back(variant(tile_->getEditorInfo().image_rect.x()));
		v.push_back(variant(tile_->getEditorInfo().image_rect.y()));
		v.push_back(variant(tile_->getEditorInfo().image_rect.w()));
		v.push_back(variant(tile_->getEditorInfo().image_rect.h()));
		m[variant("rect")] = variant(&v);
		return variant(&m);
	}
	void setValue(const std::string& key, const variant& value)
	{}
public:
	explicit EditorInfo_callable(const hex_tile& tile)
		: tile_(const_cast<hex_tile*>(&tile))
	{}
};

variant hex_tile::getValue(const std::string& key) const
{
	if(key == "variations") {
	} else if(key == "transitions") {
		return variant(new transition_callable(*this));
	} else if(key == "type") {
		return variant(type());
	} else if(key == "name") {
		return variant(name());
	} else if(key == "EditorInfo") {
		return variant(new EditorInfo_callable(*this));
	}
	return variant();
}

void hex_tile::setValue(const std::string& key, const variant& value)
{
}

variant hex_tile::write() const
{
	// XXX todo
	return variant();
}

basic_hex_tile_ptr hex_tile::get_single_tile()
{
	// Select a tile from among the variations.
	ASSERT_LOG(!variations_.empty(), "No tiles found! " << type());
	int roll = rand() % 100;
	foreach(const basic_hex_tile_ptr& htp, variations_) {
		if(roll < htp->chance()) {
			htp->getTexture();
			return htp;
		}
		roll -= htp->chance();
	}
	// Ideally this shouldn't happen, but we'll just return the front item if it does.
	variations_.front()->getTexture();
	return variations_.front();
}
*/

void TileType::EditorInfo::draw(int x, int y) const
{
	point p(HexMap::get_pixel_pos_from_tile_pos(x,y));
	graphics::blit_texture(texture, p.x, p.y, image_rect.w(), image_rect.h(), 0.0f, 
		GLfloat(image_rect.x())/GLfloat(texture.width()),
		GLfloat(image_rect.y())/GLfloat(texture.height()),
		GLfloat(image_rect.x2())/GLfloat(texture.width()),
		GLfloat(image_rect.y2())/GLfloat(texture.height()));
}

TileSheet::TileSheet(variant node)
    : texture_(graphics::texture::get(node["image"].as_string())),
	  area_(rect(2, 2, 72, 72)), ncols_(36), pad_(4)
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

TileType::TileType(const std::string& id, variant node)
  : id_(id), sheet_(new TileSheet(node)),
    height_(node["height"].as_decimal())
{
	for(const std::string& index_str : node["sheet_pos"].as_list_string()) {
		const int index = strtol(index_str.c_str(), NULL, 36);
		sheetIndexes_.push_back(index);
	}

	for(auto p : node["adjacent"].as_map()) {
		unsigned char dirmap = 0;
		std::vector<std::string> dir = util::split(p.first.as_string());
		for(auto d : dir) {
			static const std::string Directions[] = { "n", "ne", "se", "s", "sw", "nw" };
			const std::string* dir_str = std::find(Directions, Directions+6, d);
			const int index = dir_str - Directions;
			ASSERT_LOG(index < 6, "Unrecognized direction string: " << p.first << " " << p.first.debug_location());

			dirmap = dirmap | (1 << index);

		}

		AdjacencyPattern& pattern = adjacency_patterns_[dirmap];
		for(const std::string& index_str : p.second.as_list_string()) {
			const int index = strtol(index_str.c_str(), NULL, 36);
			pattern.sheetIndexes.push_back(index);
		}

		pattern.init = true;
		pattern.depth = 0;
	}

	ASSERT_LOG(sheetIndexes_.empty() == false, "No sheet indexes in hex tile sheet: " << id);

	if(node.has_key("EditorInfo")) {
		ASSERT_LOG(node["EditorInfo"].is_map(), "Must have editor info map, none found in: " << id_);
		EditorInfo_.texture = sheet_->getTexture();
		EditorInfo_.name = node["EditorInfo"]["name"].as_string();
		EditorInfo_.group = node["EditorInfo"]["group"].as_string();
		EditorInfo_.type = id;
		EditorInfo_.image_rect = sheet_->getArea(0);
	}
}

variant TileType::write() const
{
	std::map<variant,variant> m;
	m[variant("id")] = variant(id_);
	m[variant("height")] = variant(height_);
	return variant(&m);
}

namespace {
int random_hash(int x, int y)
{
	static const unsigned int x_rng[] = {31,29,62,59,14,2,64,50,17,74,72,47,69,92,89,79,5,21,36,83,81,35,58,44,88,5,51,4,23,54,87,39,44,52,86,6,95,23,72,77,48,97,38,20,45,58,86,8,80,7,65,0,17,85,84,11,68,19,63,30,32,57,62,70,50,47,41,0,39,24,14,6,18,45,56,54,77,61,2,68,92,20,93,68,66,24,5,29,61,48,5,64,39,91,20,69,39,59,96,33,81,63,49,98,48,28,80,96,34,20,65,84,19,87,43,4,54,21,35,54,66,28,42,22,62,13,59,42,17,66,67,67,55,65,20,68,75,62,58,69,95,50,34,46,56,57,71,79,80,47,56,31,35,55,95,60,12,76,53,52,94,90,72,37,8,58,9,70,5,89,61,27,28,51,38,58,60,46,25,86,46,0,73,7,66,91,13,92,78,58,28,2,56,3,70,81,19,98,50,50,4,0,57,49,36,4,51,78,10,7,26,44,28,43,53,56,53,13,6,71,95,36,87,49,62,63,30,45,75,41,59,51,77,0,72,28,24,25,35,4,4,56,87,23,25,21,4,58,57,19,4,97,78,31,38,80,};
	static const unsigned int y_rng[] = {91,80,42,50,40,7,82,67,81,3,54,31,74,49,30,98,49,93,7,62,10,4,67,93,28,53,74,20,36,62,54,64,60,33,85,31,31,6,22,2,29,16,63,46,83,78,2,11,18,39,62,56,36,56,0,39,26,45,72,46,11,4,49,13,24,40,47,51,17,99,80,64,27,21,20,4,1,37,33,25,9,87,87,36,44,4,77,72,23,73,76,47,28,41,94,69,48,81,82,0,41,7,90,75,4,37,8,86,64,14,1,89,91,0,29,44,35,36,78,89,40,86,19,5,39,52,24,42,44,74,71,96,78,29,54,72,35,96,86,11,49,96,90,79,79,70,50,36,15,50,34,31,86,99,77,97,19,15,32,54,58,87,79,85,49,71,91,78,98,64,18,82,55,66,39,35,86,63,87,41,25,73,79,99,43,2,29,16,53,42,43,26,45,45,95,70,35,75,55,73,58,62,45,86,46,90,12,10,72,88,29,77,10,8,92,72,22,3,1,49,5,51,41,86,65,66,95,23,60,87,64,86,55,30,48,76,21,76,43,52,52,23,40,64,69,43,69,97,34,39,18,87,46,8,96,50,};
	return x_rng[x%(sizeof(x_rng)/sizeof(*x_rng))] +
	       y_rng[y%(sizeof(y_rng)/sizeof(*y_rng))];
}
}

void TileType::draw(int x, int y) const
{
	if(sheetIndexes_.empty()) {
		return;
	}

	int index = 0;

	if(sheetIndexes_.size() > 1) {
		index = random_hash(x, y)%sheetIndexes_.size();
	}

	point p(HexMap::get_pixel_pos_from_tile_pos(x, y));
	rect area = sheet_->getArea(sheetIndexes_[index]);
	
	graphics::blit_texture(sheet_->getTexture(),
	    p.x, p.y, area.w(), area.h(), 0.0f, 
		GLfloat(area.x())/GLfloat(sheet_->getTexture().width()),
		GLfloat(area.y())/GLfloat(sheet_->getTexture().height()),
		GLfloat(area.x2())/GLfloat(sheet_->getTexture().width()),
		GLfloat(area.y2())/GLfloat(sheet_->getTexture().height()));
}

void TileType::drawAdjacent(int x, int y, unsigned char adjmap) const
{
	calculateAdjacencyPattern(adjmap);
	const AdjacencyPattern& pattern = adjacency_patterns_[adjmap];
	assert(pattern.init);
	for(int index : pattern.sheetIndexes) {
		point p(HexMap::get_pixel_pos_from_tile_pos(x, y));
		rect area = sheet_->getArea(index);
	
		graphics::blit_texture(sheet_->getTexture(),
		    p.x, p.y, area.w(), area.h(), 0.0f, 
			GLfloat(area.x())/GLfloat(sheet_->getTexture().width()),
			GLfloat(area.y())/GLfloat(sheet_->getTexture().height()),
			GLfloat(area.x2())/GLfloat(sheet_->getTexture().width()),
			GLfloat(area.y2())/GLfloat(sheet_->getTexture().height()));
	}
}

void TileType::calculateAdjacencyPattern(unsigned char adjmap) const
{
	if(adjacency_patterns_[adjmap].init) {
		return;
	}

	int best = -1;
	for(int dir = 0; dir < 6; ++dir) {
		unsigned char mask = 1 << dir;
		if(adjmap&mask) {
			unsigned char newmap = adjmap&(~mask);
			if(newmap != 0) {
				calculateAdjacencyPattern(newmap);

				if(best == -1 || adjacency_patterns_[newmap].depth < adjacency_patterns_[best].depth) {
					best = newmap;
				}
			}
		}
	}

	if(best != -1) {
		adjacency_patterns_[adjmap].sheetIndexes.insert(adjacency_patterns_[adjmap].sheetIndexes.end(), adjacency_patterns_[best].sheetIndexes.begin(), adjacency_patterns_[best].sheetIndexes.end());
		adjacency_patterns_[adjmap].depth = adjacency_patterns_[best].depth + 1;

		best = adjmap&(~best);
		calculateAdjacencyPattern(best);
		adjacency_patterns_[adjmap].sheetIndexes.insert(adjacency_patterns_[adjmap].sheetIndexes.end(), adjacency_patterns_[best].sheetIndexes.begin(), adjacency_patterns_[best].sheetIndexes.end());
		if(adjacency_patterns_[best].depth + 1 > adjacency_patterns_[adjmap].depth) {
			adjacency_patterns_[adjmap].depth = adjacency_patterns_[best].depth + 1;
		}
	}

	adjacency_patterns_[adjmap].init = true;
}

BEGIN_DEFINE_CALLABLE_NOBASE(TileType)
DEFINE_FIELD(type, "string")
	return variant(obj.id_);
END_DEFINE_CALLABLE(TileType)

}
