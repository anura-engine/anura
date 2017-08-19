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

#include <boost/regex.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <set>

#include "asserts.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_function.hpp"
#include "json_parser.hpp"
#include "level_object.hpp"
#include "level_solid_map.hpp"
#include "multi_tile_pattern.hpp"
#include "point_map.hpp"
#include "profile_timer.hpp"
#include "preferences.hpp"
#include "random.hpp"
#include "string_utils.hpp"
#include "thread.hpp"
#include "tile_map.hpp"
#include "variant_utils.hpp"

namespace 
{
	PREF_INT(tile_pattern_search_border, 1, "How many extra tiles to search for patterns");

	const std::map<std::string, int>& str_to_zorder() {
		static std::map<std::string, int>* instance = nullptr;
		if(!instance) {
			instance = new std::map<std::string, int>;
			variant node = json::parse_from_file("data/zorder.cfg");

			for(variant::map_pair p : node.as_map()) {
				(*instance)[p.first.as_string()] = p.second.as_int();
			}
		}

		return *instance;
	}

	const std::map<int, variant>& zorder_to_str() 
	{
		static std::map<int, variant>* instance = nullptr;
		if(!instance) {
			instance = new std::map<int, variant>;
			variant node = json::parse_from_file("data/zorder.cfg");

			for(variant::map_pair p : node.as_map()) {
				ASSERT_LOG(instance->count(p.second.as_int()) == 0, "Multiple zorders map to same value: " << p.second.as_int());

				(*instance)[p.second.as_int()] = p.first;
			}
		}

		return *instance;
	}
}

int parse_zorder(const variant& v, variant default_val)
{
	if(v.is_null() && !default_val.is_null()) {
		return parse_zorder(default_val);
	}

	if(v.is_int()) {
		return v.as_int();
	}

	const std::string& str = v.as_string();
	ASSERT_LOG(str_to_zorder().count(str), "Invalid zorder id: " << v.as_string() << ": " << v.debug_location());
	return str_to_zorder().find(str)->second;
}

variant write_zorder(int zorder)
{
	if(zorder_to_str().count(zorder)) {
		return zorder_to_str().find(zorder)->second;
	}

	return variant(zorder);
}

int get_named_zorder(const std::string& key, int default_value)
{
	std::map<std::string, int>::const_iterator itor = str_to_zorder().find(key);
	if(itor == str_to_zorder().end()) {
		return default_value;
	}

	return itor->second;
}

namespace 
{
	typedef std::map<const boost::regex*, bool> regex_match_map;
	std::map<boost::array<char, 4>, regex_match_map> re_matches;

	bool match_regex(boost::array<char, 4> str, const boost::regex* re) 
	{
		if(reinterpret_cast<intptr_t>(re)&1) {
			//the low bit in the re pointer is set, meaning this is an inverted
			//match.
			return !match_regex(str, reinterpret_cast<const boost::regex*>(reinterpret_cast<intptr_t>(re)-1));
		}

		std::map<const boost::regex*, bool>& m = re_matches[str];
		std::map<const boost::regex*, bool>::const_iterator i = m.find(re);
		if(i != m.end()) {
			return i->second;
		}

		const bool match = boost::regex_match(str.data(), str.data() + strlen(str.data()), *re);
		m[re] = match;
		return match;
	}

	struct is_whitespace 
	{
		bool operator()(char c) const { return util::c_isspace(c); }
	};
}

struct TilePattern 
{
	explicit TilePattern(variant node, const std::string& id)
	  : tile_id(id),
	    tile(new LevelObject(node, id.c_str())), 
		reverse(node["reverse"].as_bool(true)),
	    empty(node["empty"].as_bool(false)),
		filter_formula(game_logic::Formula::createOptionalFormula(node["filter"]))
	{
		variations.push_back(tile);

		pattern_str = node["pattern"].as_string();
		std::string pattern_str = node["pattern"].as_string();
		pattern_str.erase(std::remove_if(pattern_str.begin(), pattern_str.end(), is_whitespace()), pattern_str.end());

		static std::vector<std::string> patterns;
		patterns.clear();
		util::split(pattern_str, patterns, ',', 0);

		assert(!patterns.empty());

		//the main pattern is always the very middle one.
		auto main_tile = patterns.size()/2;

		int width = node["pattern_width"].as_int(static_cast<int>(sqrt(static_cast<float>(patterns.size()))));
		assert(width != 0);
		int height = static_cast<int>(patterns.size() / width);

		int top = -height/2;
		int left = -width/2;

		//pattern with size 12 is a special case
		if(patterns.size() == 12 && !node.has_key("pattern_width")) {
			width = 3;
			height = 4;
			top = -1;
			left = -1;
			main_tile = 4;
		}

		current_tile_pattern = &get_regex_from_pool(patterns[main_tile].empty() ? "^$" : patterns[main_tile]);

		for(std::vector<std::string>::size_type n = 0; n != patterns.size(); ++n) {
			if(n == main_tile) {
				continue;
			}

			const int x = static_cast<int>(left + n % width);
			const int y = static_cast<int>(top + n / width);
			surrounding_tiles.push_back(SurroundingTile(x, y, patterns[n]));
		}

		for(variant var : node["variation"].as_list()) {
			variations.push_back(LevelObjectPtr(new LevelObject(var, tile_id.c_str())));
		}

		for(variant var : node["tile"].as_list()) {
			added_tile t;
			LevelObjectPtr new_object(new LevelObject(var, tile_id.c_str()));

			t.object = new_object.get();
			t.zorder = parse_zorder(var["zorder"]);
			added_tiles.push_back(t);
			
		}
	}

	std::string tile_id;
	const boost::regex* current_tile_pattern;

	struct SurroundingTile {
		SurroundingTile(int x, int y, const std::string& s)
		  : xoffset(x), yoffset(y), pattern(&get_regex_from_pool(s))
		{}
		int xoffset;
		int yoffset;
		const boost::regex* pattern;
	};

	std::vector<SurroundingTile> surrounding_tiles;

	std::string pattern_str;
	LevelObjectPtr tile;
	std::vector<LevelObjectPtr> variations;
	bool reverse;
	bool empty;

	struct added_tile {
		LevelObjectPtr object;
		int zorder;
	};

	std::vector<added_tile> added_tiles;

	game_logic::ConstFormulaPtr filter_formula;
};

namespace 
{
	std::vector<TilePattern> patterns;
	int current_patterns_version = 0;

	using namespace game_logic;

	class FilterCallable : public FormulaCallable 
	{
		DECLARE_CALLABLE(FilterCallable);
		const TileMap& m_;
		int x_, y_;
	public:
		FilterCallable(const TileMap& m, int x, int y) 
			: m_(m), x_(x), y_(y)
		{}
	};

	BEGIN_DEFINE_CALLABLE_NOBASE(FilterCallable)
//		DEFINE_FIELD(tiles, "builtin tile_map")
//			return variant(&obj.m_);
		DEFINE_FIELD(x, "int")
			return variant(obj.x_);
		DEFINE_FIELD(y, "int")
			return variant(obj.y_);
	END_DEFINE_CALLABLE(FilterCallable)

	class TileAtFunction : public FunctionExpression 
	{
	public:
		explicit TileAtFunction(const args_list& args)
		  : FunctionExpression("tile_at", args, 2)
		{}

	private:
		variant execute(const FormulaCallable& variables) const override {
			variant v = args()[0]->evaluate(variables);
			TileMap* m = v.convert_to<TileMap>();
			return variant(m->getTile(args()[1]->evaluate(variables).as_int(),
									   args()[2]->evaluate(variables).as_int()));
		}
	};

	class TileMapFunctionSymbolTable : public FunctionSymbolTable
	{
	public:
		ExpressionPtr createFunction(const std::string& fn, 
			const std::vector<ExpressionPtr>& args, 
			ConstFormulaCallableDefinitionPtr callable_def) const  override
		{
			if(fn == "tile_at") {
				return ExpressionPtr(new TileAtFunction(args));
			} else {
				return FunctionSymbolTable::createFunction(fn, args, callable_def);
			}
		}
	};

	std::map<std::string, std::vector<std::string> > files_index;
	std::set<std::string> files_loaded;
}

void TileMap::loadAll()
{
	for(auto& i : files_index) {
		for(const std::string& s : i.second) {
			load(s, i.first);
		}
	}
}

void TileMap::load(const std::string& fname, const std::string& tile_id)
{
	if(files_loaded.count(fname)) {
		return;
	}

	files_loaded.insert(fname);

	variant node;
	
	try {
		node = json::parse_from_file("data/tiles/" + fname);
	} catch(json::ParseError& e) {
		ASSERT_LOG(false, "Error parsing data/tiles/" << fname << ": " << e.errorMessage());
	}

	palette_scope palette_setter(parse_variant_list_or_csv_string(node["palettes"]));

	for(variant pattern : node["tile_pattern"].as_list()) {
		patterns.push_back(TilePattern(pattern, tile_id));
	}

	MultiTilePattern::load(node, tile_id);

	++current_patterns_version;
}

const std::vector<std::string>& TileMap::getFiles(const std::string& tile_id)
{
	if(files_index.count(tile_id)) {
		return files_index.find(tile_id)->second;
	} else {
		static const std::vector<std::string> empty;
		return empty;
	}
}

void TileMap::init(variant node)
{
	files_index.clear();

	for(const variant_pair& value : node.as_map()) {
		files_index[value.first.as_string()] = util::split(value.second.as_string());
	}

	patterns.clear();
	files_loaded.clear();

	MultiTilePattern::init(node);

	++current_patterns_version;
}

#ifndef NO_EDITOR
namespace 
{
	std::set<TileMap*>& all_tile_maps() 
	{
		static std::set<TileMap*>* all = new std::set<TileMap*>;
		return *all;
	}

	threading::mutex& all_tile_maps_mutex() 
	{
		static threading::mutex* m = new threading::mutex;
		return *m;
	}

	void create_tile_map(TileMap* t) 
	{
		threading::lock l(all_tile_maps_mutex());
		all_tile_maps().insert(t);
	}

	void destroy_tile_map(TileMap* t) 
	{
		threading::lock l(all_tile_maps_mutex());
		all_tile_maps().erase(t);
	}

	std::set<TileMap*> copy_tile_maps() 
	{
		threading::lock l(all_tile_maps_mutex());
		std::set<TileMap*> result = all_tile_maps();
		return result;
	}
}

void TileMap::prepareRebuildAll()
{
	const std::set<TileMap*> maps = copy_tile_maps();
	for(TileMap* m : maps) {
		m->node_ = m->write();
	}
}

void TileMap::rebuildAll()
{
	const std::set<TileMap*> maps = copy_tile_maps();
	for(TileMap* m : maps) {
		*m = TileMap(m->node_);
	}
}
#endif

TileMap::TileMap() : xpos_(0), ypos_(0), x_speed_(100), y_speed_(100), zorder_(0), patterns_version_(-1)
{
#ifndef NO_EDITOR
	create_tile_map(this);
#endif

	//turn off reference counting
//	add_ref();

	//make an entry for the empty string.
	pattern_index_.push_back(PatternIndexEntry());
	pattern_index_.back().matching_patterns.push_back(&get_regex_from_pool(""));
}

TileMap::TileMap(variant node)
  : xpos_(node["x"].as_int()), 
	ypos_(node["y"].as_int()),
	x_speed_(node["x_speed"].as_int(100)), 
	y_speed_(node["y_speed"].as_int(100)),
    zorder_(parse_zorder(node["zorder"]))
#ifndef NO_EDITOR
	, node_(node)
#endif
{
#ifndef NO_EDITOR
	create_tile_map(this);
#endif

	//turn off reference counting
//	add_ref();

	const std::string& unique_tiles = node["unique_tiles"].as_string_default();
	for(const std::string& tile : util::split(unique_tiles)) {
		const std::vector<std::string>& files = files_index[tile];
		for(const std::string& file : files) {
			load(file, tile);
		}
	}

	//make an entry for the empty string.
	pattern_index_.push_back(PatternIndexEntry());
	pattern_index_.back().matching_patterns.push_back(&get_regex_from_pool(""));

	{
	const std::string& tiles_str = node["tiles"].as_string();
	std::vector<std::string> lines;
	lines.reserve(std::count(tiles_str.begin(), tiles_str.end(), '\n')+1);

	util::split(tiles_str, lines, '\n', 0);
	for(const std::string& line : lines) {
		map_.resize(map_.size()+1);

		std::vector<tile_string> items;
		const char* ptr = line.c_str();
		bool found_end = line.empty();
		while(!found_end) {
			const char* end = strchr(ptr, ',');
			if(end == nullptr) {
				end = line.c_str() + line.size();
				found_end = true;
			}

			//We want to copy [ptr,end) to tile_string. First strip the spaces.
			while(ptr != end && util::c_isspace(*ptr)) {
				++ptr;
			}

			while(end != ptr && util::c_isspace(*(end-1))) {
				--end;
			}

			ASSERT_LOG(end - ptr <= 4, "TILE PATTERN IS TOO LONG: " << std::string(ptr, end));

			tile_string tile;
			std::fill(tile.begin(), tile.end(), '\0');
			std::copy(ptr, end, tile.begin());
			items.push_back(tile);

			if(!found_end) {
				ptr = end + 1;
			}
		}

		for(const tile_string& str : items) {
			int index_entry = 0;
			for(const PatternIndexEntry& e : pattern_index_) {
				if(strcmp(e.str.data(), str.data()) == 0) {
					break;
				}

				++index_entry;
			}

			if(index_entry == pattern_index_.size()) {
				pattern_index_.push_back(PatternIndexEntry());
				pattern_index_.back().str = str;
			}

			map_.back().push_back(index_entry);
		}
	}

	buildPatterns();
	}
}

TileMap::TileMap(const TileMap& o)
{
#ifndef NO_EDITOR
	create_tile_map(this);
#endif

//	add_ref();
	*this = o;
}

TileMap::~TileMap()
{
#ifndef NO_EDITOR
	destroy_tile_map(this);
#endif
}

void TileMap::buildPatterns()
{
	std::vector<const boost::regex*> all_regexes;

	patterns_version_ = current_patterns_version;
	const unsigned begin_time = profile::get_tick_time();
	patterns_.clear();
	for(const TilePattern& p : patterns) {
		std::vector<const boost::regex*> re;
		std::vector<const boost::regex*> accepted_re;
		if(!p.current_tile_pattern->empty()) {
			re.push_back(p.current_tile_pattern);
		}
		
		for(const TilePattern::SurroundingTile& t : p.surrounding_tiles) {
			re.push_back(t.pattern);
		}

		int matches = 0;
		for(PatternIndexEntry& e : pattern_index_) {
			for(const boost::regex*& regex : re) {
				if(regex && match_regex(e.str, regex)) {
					accepted_re.push_back(regex);
					regex = nullptr;
					++matches;
					if(matches == re.size()) {
						break;
					}
				}
			}

			if(matches == re.size()) {
				break;
			}
		}

		if(matches == re.size()) {
			all_regexes.insert(all_regexes.end(), accepted_re.begin(), accepted_re.end());
			patterns_.push_back(&p);
		}
	}

	for(const MultiTilePattern& p : MultiTilePattern::getAll()) {
		std::vector<const boost::regex*> re;
		std::vector<const boost::regex*> accepted_re;

		re.reserve(p.width()*p.height());
		for(int x = 0; x < p.width(); ++x) {
			for(int y = 0; y < p.height(); ++y) {
				re.push_back(p.getTileAt(x, y).re);
			}
		}

		int matches = 0;
		for(PatternIndexEntry& e : pattern_index_) {
			for(const boost::regex*& regex : re) {
				if(regex && match_regex(e.str, regex)) {
					accepted_re.push_back(regex);
					regex = nullptr;
					++matches;
					if(matches == re.size()) {
						break;
					}
				}
			}

			if(matches == re.size()) {
				break;
			}
		}

		if(matches == re.size()) {
			all_regexes.insert(all_regexes.end(), accepted_re.begin(), accepted_re.end());
			multi_patterns_.push_back(&p);
		}
	}

	std::sort(all_regexes.begin(), all_regexes.end());
	all_regexes.erase(std::unique(all_regexes.begin(), all_regexes.end()), all_regexes.end());

	for(PatternIndexEntry& e : pattern_index_) {
		e.matching_patterns.clear();

		for(const boost::regex* re : all_regexes) {
			if(match_regex(e.str, re)) {
				e.matching_patterns.push_back(re);
			}
		}
	}

	const unsigned end_time = profile::get_tick_time();
	static unsigned total_time = 0;
	total_time += (end_time - begin_time);
}

const std::vector<const TilePattern*>& TileMap::getPatterns() const
{
	if(patterns_version_ != current_patterns_version) {
		const_cast<TileMap*>(this)->buildPatterns();
	}

	return patterns_;
}

variant TileMap::write() const
{
	variant_builder res;
	res.add("x", xpos_);
	res.add("y", ypos_);
	res.add("x_speed", x_speed_);
	res.add("y_speed", y_speed_);
	res.add("zorder", write_zorder(zorder_));

	std::vector<boost::array<char, 4> > unique_tiles;
	std::ostringstream tiles;
	bool first = true;
	for(const std::vector<int>& row : map_) {
		
		//cut off any empty cells at the end.
		auto size = row.size();
		while(size > 2 && *pattern_index_[row[size-1]].str.data() == 0) {
			--size;
		}


		if(!first) {
			tiles << "\n";
		}

		first = false;
		for(std::vector<int>::size_type i = 0; i != size; ++i) {
			if(i) {
				tiles << ",";
			}
			tiles << pattern_index_[row[i]].str.data();
			unique_tiles.push_back(pattern_index_[row[i]].str);
		}
		
		if(row.empty()) {
			tiles << ",";
		}
	}

	std::sort(unique_tiles.begin(), unique_tiles.end());
	unique_tiles.erase(std::unique(unique_tiles.begin(), unique_tiles.end()), unique_tiles.end());
	std::ostringstream unique_str;
	for(int n = 0; n != unique_tiles.size(); ++n) {
		if(n != 0) {
			unique_str << ",";
		}

		unique_str << unique_tiles[n].data();
	}

	res.add("unique_tiles", unique_str.str());

	std::ostringstream variations;
	for(const std::vector<int>& row : variations_) {
		variations << "\n";
		for(int i = 0; i != row.size(); ++i) {
			if(i) {
				variations << ",";
			}
			variations << row[i];
		}

		if(row.empty()) {
			variations << ",";
		}
	}

	res.add("tiles", tiles.str());
	res.add("variations", variations.str());
	return res.build();
}

const char* TileMap::getTileFromPixelPos(int xpos, int ypos) const
{
	const int x = (xpos - xpos_)/TileSize;
	const int y = (ypos - ypos_)/TileSize;
	return getTile(y, x);
}

const char* TileMap::getTile(int y, int x) const
{
	if(x < 0 || y < 0 
		|| static_cast<std::vector<std::vector<int>>::size_type>(y) >= map_.size() 
		|| static_cast<std::vector<std::vector<int>>::size_type>(x) >= map_[y].size()) {
		return "";
	}

	return pattern_index_[map_[y][x]].str.data();
}

const TileMap::PatternIndexEntry& TileMap::getTileEntry(int y, int x) const
{
	if(x < 0 || y < 0 
		|| static_cast<std::vector<std::vector<int>>::size_type>(y) >= map_.size() 
		|| static_cast<std::vector<std::vector<int>>::size_type>(x) >= map_[y].size()) {
		return pattern_index_.front();
	}

	return pattern_index_[map_[y][x]];
}

namespace 
{
	struct cstr_less 
	{
		bool operator()(const char* a, const char* b) const {
			return strcmp(a, b) < 0;
		}
	};

	typedef std::map<const char*, std::vector<const TilePattern*>, cstr_less> TilePatternCacheMap;
	struct TilePatternCache 
	{
		TilePatternCacheMap cache;
	};
}

int TileMap::getVariations(int x, int y) const
{
	x -= xpos_/TileSize;
	y -= ypos_/TileSize;
	TilePatternCache cache;
	bool face_right = false;
	const TilePattern* p = getMatchingPattern(x, y, cache, &face_right);
	if(p == nullptr) {
		return 0;
	}

	return static_cast<int>(p->variations.size());
}

int TileMap::variation(int x, int y) const
{
	if(x < 0 || y < 0 
		|| static_cast<VariationsType::size_type>(y) >= variations_.size() 
		|| static_cast<VariationsType::size_type>(x) >= variations_[y].size()) {
		return 0;
	}

	return variations_[y][x];
}

void TileMap::flipVariation(int x, int y, int delta)
{
	const int variations = getVariations(x, y);
	if(variations <= 1) {
		return;
	}

	x -= xpos_/TileSize;
	y -= ypos_/TileSize;

	if(y < 0 || static_cast<VariationsType::size_type>(y) >= variations_.size()) {
		variations_.resize(y + 1);
	}

	std::vector<int>& row = variations_[y];
	if(x < 0 || static_cast<std::vector<int>::size_type>(x) >= row.size()) {
		row.resize(x + 1);
	}

	row[x] += delta;
	while(row[x] < 0) {
		row[x] += variations;
	}

	while(row[x] >= variations) {
		row[x] = row[x]%variations;
	}
}

void TileMap::prepareForCopyToWorkerThread()
{
#ifndef NO_EDITOR
	node_ = variant();
#endif
}

namespace 
{
	//This function is a random hash. It takes an (x,y) position as well as a
	//zorder, and then a 'n' integer which is used to identify the number of the
	//pattern. The idea is that for the same input it always gives the same output,
	//but the output appears random, and there is a complete avalanche effect --
	//i.e. any change in the input results in a completely different output.
	//
	//The function returns a number in the range [0,99].
	int random_hash(int x, int y, int z, int n)
	{
		//The implementation is simply four arrays of hard coded random numbers.
		//We index our input into each of the arrays, summing the results and
		//returning. This does have the implication that
		//random_hash(x,y,z,n) == random_hash(x+k,y,z,n) where k is the size
		//of the array. However, this should be acceptable.
		static const unsigned int x_rng[] = {31,29,62,59,14,2,64,50,17,74,72,47,69,92,89,79,5,21,36,83,81,35,58,44,88,5,51,4,23,54,87,39,44,52,86,6,95,23,72,77,48,97,38,20,45,58,86,8,80,7,65,0,17,85,84,11,68,19,63,30,32,57,62,70,50,47,41,0,39,24,14,6,18,45,56,54,77,61,2,68,92,20,93,68,66,24,5,29,61,48,5,64,39,91,20,69,39,59,96,33,81,63,49,98,48,28,80,96,34,20,65,84,19,87,43,4,54,21,35,54,66,28,42,22,62,13,59,42,17,66,67,67,55,65,20,68,75,62,58,69,95,50,34,46,56,57,71,79,80,47,56,31,35,55,95,60,12,76,53,52,94,90,72,37,8,58,9,70,5,89,61,27,28,51,38,58,60,46,25,86,46,0,73,7,66,91,13,92,78,58,28,2,56,3,70,81,19,98,50,50,4,0,57,49,36,4,51,78,10,7,26,44,28,43,53,56,53,13,6,71,95,36,87,49,62,63,30,45,75,41,59,51,77,0,72,28,24,25,35,4,4,56,87,23,25,21,4,58,57,19,4,97,78,31,38,80,};
		static const unsigned int y_rng[] = {91,80,42,50,40,7,82,67,81,3,54,31,74,49,30,98,49,93,7,62,10,4,67,93,28,53,74,20,36,62,54,64,60,33,85,31,31,6,22,2,29,16,63,46,83,78,2,11,18,39,62,56,36,56,0,39,26,45,72,46,11,4,49,13,24,40,47,51,17,99,80,64,27,21,20,4,1,37,33,25,9,87,87,36,44,4,77,72,23,73,76,47,28,41,94,69,48,81,82,0,41,7,90,75,4,37,8,86,64,14,1,89,91,0,29,44,35,36,78,89,40,86,19,5,39,52,24,42,44,74,71,96,78,29,54,72,35,96,86,11,49,96,90,79,79,70,50,36,15,50,34,31,86,99,77,97,19,15,32,54,58,87,79,85,49,71,91,78,98,64,18,82,55,66,39,35,86,63,87,41,25,73,79,99,43,2,29,16,53,42,43,26,45,45,95,70,35,75,55,73,58,62,45,86,46,90,12,10,72,88,29,77,10,8,92,72,22,3,1,49,5,51,41,86,65,66,95,23,60,87,64,86,55,30,48,76,21,76,43,52,52,23,40,64,69,43,69,97,34,39,18,87,46,8,96,50,};
		static const unsigned int z_rng[] = {91,80,42,50,40,7,82,67,81,3,54,31,74,49,30,98,49,93,7,62,10,4,67,93,28,53,74,20,36,62,54,64,60,33,85,31,31,6,22,2,29,16,63,46,83,78,2,11,18,39,62,56,36,56,0,39,26,45,72,46,11,4,49,13,24,40,47,51,17,99,80,64,27,21,20,4,1,37,33,25,9,87,87,36,44,4,77,72,23,73,76,47,28,41,94,69,48,81,82,0,41,7,90,75,4,37,8,86,64,14,1,89,91,0,29,44,35,36,78,89,40,86,19,5,39,52,24,42,44,74,71,96,78,29,54,72,35,96,86,11,49,96,90,79,79,70,50,36,15,50,34,31,86,99,77,97,19,15,32,54,58,87,79,85,49,71,91,78,98,64,18,82,55,66,39,35,86,63,87,41,25,73,79,99,43,2,29,16,53,42,43,26,45,45,95,70,35,75,55,73,58,62,45,86,46,90,12,10,72,88,29,77,10,8,92,72,22,3,1,49,5,51,41,86,65,66,95,23,60,87,64,86,55,30,48,76,21,76,43,52,52,23,40,64,69,43,69,97,34,39,18,87,46,8,96,50,};
		static const unsigned int n_rng[] = {28,61,82,84,31,6,65,20,50,87,22,52,92,28,39,81,54,48,21,10,5,45,32,62,51,46,60,65,11,67,59,50,48,73,42,40,30,88,33,59,88,33,32,7,15,74,38,6,0,76,66,29,32,40,22,62,62,39,17,24,64,75,35,75,99,57,43,98,6,16,63,72,62,39,10,48,48,82,88,94,26,79,49,98,4,40,8,54,67,85,81,66,69,46,27,76,45,68,76,49,94,59,21,74,26,36,97,34,22,98,84,33,7,17,43,56,75,51,32,74,23,67,29,43,32,89,28,50,11,37,30,2,81,6,4,83,99,7,76,46,32,12,3,33,83,19,0,47,19,32,59,97,92,71,45,93,5,55,53,99,77,96,49,90,16,98,99,6,22,14,5,47,10,49,42,61,7,33,21,84,68,19,22,47,28,8,87,66,65,74,21,21,50,70,64,97,29,54,96,94,42,18,88,79,72,66,93,92,3,93,22,62,73,63,69,27,35,45,27,1,88,23,78,10,61,26,70,67,11,43,16,43,99,42,39,43,89,3,84,90,65,49,67,71,60,45,38,95,32,27,7,30,77,75,24,46,};

		return (
		  x_rng[x%(sizeof(x_rng)/sizeof(*x_rng))] +
		  y_rng[y%(sizeof(y_rng)/sizeof(*y_rng))] +
		  z_rng[z%(sizeof(z_rng)/sizeof(*z_rng))] +
		  n_rng[n%(sizeof(n_rng)/sizeof(*n_rng))]);
	}
}

void TileMap::applyMatchingMultiPattern(int& x, int y,
	const MultiTilePattern& pattern,
	point_map<LevelObject*>& mapping,
	std::map<point_zorder, LevelObject*>& different_zorder_mapping) const
{

	if(pattern.chance() < 100 && random_hash(x, y, zorder_, 0)%100 > pattern.chance()) {
		return;
	}

	bool match = true;
	for(int n = 0; n != pattern.tryOrder().size() && match; ++n) {
		const int xpos = pattern.tryOrder()[n].loc.x;
		const int ypos = pattern.tryOrder()[n].loc.y;

		const PatternIndexEntry& entry = getTileEntry(y + ypos, x + xpos);
		if(std::find(entry.matching_patterns.begin(), entry.matching_patterns.end(), pattern.getTileAt(xpos, ypos).re) == entry.matching_patterns.end()) {
			//the regex doesn't match
			match = false;

			//because this didn't match, we can skip over all patterns that
			//have the same pattern repeating.
			x += pattern.tryOrder()[n].run_length;
			break;
		}

		if(pattern.getTileAt(xpos, ypos).tiles.empty() == false && mapping.get(point(x + xpos, y + ypos))) {
			//there is already another pattern filling this tile.
			match = false;
			break;
		}
	}

	if(match) {
		const int hash = random_hash(x, y, zorder_, 0);
		const MultiTilePattern& chosen_pattern = pattern.chooseRandomAlternative(hash);
		for(int xpos = 0; xpos < chosen_pattern.width() && match; ++xpos) {
			for(int ypos = 0; ypos < chosen_pattern.height() && match; ++ypos) {
				const MultiTilePattern::TileInfo& info = chosen_pattern.getTileAt(xpos, ypos);
				for(const MultiTilePattern::TileEntry& entry : info.tiles) {
					LevelObject* ob = entry.tile.get();
					if(ob) {
						if(entry.zorder == std::numeric_limits<int>::min() || entry.zorder == zorder_) {
							mapping.insert(point(x + xpos, y + ypos), ob);
						} else {
							different_zorder_mapping[point_zorder(point(x + xpos, y + ypos), entry.zorder)] = ob;
						}
					}
				}
			}
		}
	}
}

void TileMap::buildTiles(std::vector<LevelTile>* tiles, const rect* r) const
{
	const int begin_time = profile::get_tick_time();
	int width = 0;
	for(const auto& row : map_) {
		int rs = static_cast<int>(row.size());
		if(rs > width) {
			width = rs;
		}
	}

	point_map<LevelObject*> multi_pattern_matches;
	std::map<point_zorder, LevelObject*> different_zorder_multi_pattern_matches;

	for(const MultiTilePattern* p : multi_patterns_) {
		for(int y = -p->height(); y < static_cast<int>(map_.size()) + p->height(); ++y) {
			const int ypos = ypos_ + y*TileSize;
	
			if((r && ypos < r->y()) || (r && ypos > r->y2())) {
				continue;
			}

			for(int x = -p->width(); x < width + p->width(); ++x) {
				applyMatchingMultiPattern(x, y, *p, multi_pattern_matches, different_zorder_multi_pattern_matches);
			}
		}
	}

	//add all tiles in different zorders to our own.
	for(auto& i : different_zorder_multi_pattern_matches) {
		const LevelObject* obj = i.second;
		const int x = i.first.first.x;
		const int y = i.first.first.y;

		const int xpos = xpos_ + x*TileSize;
		const int ypos = ypos_ + y*TileSize;

		LevelTile t;
		t.x = xpos;
		t.y = ypos;
		t.layer_from = zorder_;
		t.zorder = i.first.second;
		t.object = i.second;
		t.face_right = false;
		tiles->emplace_back(t);
	}


	TilePatternCache cache;

	int ntiles = 0;
	for(int y = -g_tile_pattern_search_border; y < static_cast<int>(map_.size()) + g_tile_pattern_search_border; ++y) {
		const int ypos = ypos_ + y*TileSize;

		if((r && ypos < r->y()) || (r && ypos > r->y2())) {
			continue;
		}

		for(int x = -g_tile_pattern_search_border; x < width + g_tile_pattern_search_border; ++x) {
			const int xpos = xpos_ + x*TileSize;

			const LevelObject* obj = multi_pattern_matches.get(point(x, y));
			if(obj) {
				LevelTile t;
				t.x = xpos;
				t.y = ypos;
				t.layer_from = zorder_;
				t.zorder = zorder_;
				t.object = obj;
				t.face_right = false;
				tiles->emplace_back(t);
				continue;
			}

			bool face_right = true;
			const TilePattern* p = getMatchingPattern(x, y, cache, &face_right);
			if(p == nullptr) {
				continue;
			}


			if((r && xpos < r->x()) || (r && xpos > r->x2())) {
				continue;
			}

			++ntiles;

			LevelTile t;
			t.x = xpos;
			t.y = ypos;
			t.layer_from = zorder_;
			t.zorder = zorder_;

			int variation_num = variation(x, y);
			if(variation_num >= static_cast<int>(p->variations.size())) {
				variation_num = 0;
			}
			assert(p->variations[variation_num]);
			t.object = p->variations[variation_num].get();
			t.face_right = face_right;
			if(t.object->flipped()) {
				t.face_right = !t.face_right;
			}
			tiles->emplace_back(t);

			for(const TilePattern::added_tile& a : p->added_tiles) {
				LevelTile t;
				t.x = xpos;
				t.y = ypos;
				t.layer_from = zorder_;
				t.zorder = zorder_;
				if(a.zorder) {
					t.zorder = a.zorder;
				}
				t.object = a.object.get();
				t.face_right = face_right;
				if(t.object->flipped()) {
					t.face_right = !t.face_right;
				}

				tiles->emplace_back(t);
			}
		}
	}
	LOG_DEBUG("done build tiles: " << ntiles << " " << (profile::get_tick_time() - begin_time));
}

const TilePattern* TileMap::getMatchingPattern(int x, int y, TilePatternCache& cache, bool* face_right) const
{

	if (!*getTile(y, x) &&
	    !*getTile(y-1, x) &&
		!*getTile(y+1, x) &&
		!*getTile(y, x-1) &&
		!*getTile(y, x+1)) {
		return nullptr;
	}

	const int xpos = xpos_ + x*TileSize;

	ffl::IntrusivePtr<FilterCallable> callable_ptr(new FilterCallable(*this, x, y));
	FilterCallable& callable = *callable_ptr;

	const char* current_tile = getTile(y,x);

	//we build a cache of all of the patterns which have some chance of
	//matching the current tile.
	TilePatternCacheMap::iterator itor = cache.cache.find(current_tile);
	if(itor == cache.cache.end()) {
		itor = cache.cache.insert(std::pair<const char*,std::vector<const TilePattern*> >(current_tile, std::vector<const TilePattern*>())).first;
		for(const TilePattern* p : getPatterns()) {
			if(!p->current_tile_pattern->empty() && !boost::regex_match(current_tile, current_tile + strlen(current_tile), *p->current_tile_pattern)) {
				continue;
			}

			itor->second.push_back(&*p);
		}
	}

	const std::vector<const TilePattern*>& matching_patterns = itor->second;

	for(const TilePattern* ptr : matching_patterns) {
		const TilePattern& p = *ptr;
		if(p.filter_formula && p.filter_formula->execute(callable).as_bool() == false) {
			continue;
		}

		bool match = true;
		for(const TilePattern::SurroundingTile& t : p.surrounding_tiles) {
			const PatternIndexEntry& entry = getTileEntry(y + t.yoffset, x + t.xoffset);
			if(std::find(entry.matching_patterns.begin(), entry.matching_patterns.end(), t.pattern) == entry.matching_patterns.end()) {
				match = false;
				break;
			}
		}

		if(match) {
			if(p.empty) {
				return nullptr;
			}

			*face_right = false;
			return &p;
		}

		if(p.reverse) {
			match = true;

			for(const TilePattern::SurroundingTile& t : p.surrounding_tiles) {
				const PatternIndexEntry& entry = getTileEntry(y + t.yoffset, x - t.xoffset);
				if(std::find(entry.matching_patterns.begin(), entry.matching_patterns.end(), t.pattern) == entry.matching_patterns.end()) {
					match = false;
					break;
				}
			}
		}

		if(match) {
			if(p.empty) {
				return nullptr;
			}

			*face_right = true;
			return &p;
		}
	}

	return nullptr;
}

bool TileMap::setTile(int xpos, int ypos, const std::string& str)
{
	if(str.empty() && (xpos < xpos_ || ypos < ypos_)) {
		return false;
	}

	tile_string empty_tile;
	std::fill(empty_tile.begin(), empty_tile.end(), '\0');
	if(xpos < xpos_) {
		const int add_tiles = abs((xpos - xpos_)/TileSize);
		std::vector<int> insert(add_tiles, getPatternIndexEntry(empty_tile));
		for(std::vector<int>& row : map_) {
			row.insert(row.begin(), insert.begin(), insert.end());
		}

		xpos_ = xpos;
	}

	while(ypos < ypos_) {
		map_.insert(map_.begin(), std::vector<int>());
		ypos_ -= TileSize;
	}

	const int x = (xpos - xpos_)/TileSize;
	const int y = (ypos - ypos_)/TileSize;
	assert(x >= 0);
	assert(y >= 0);
	if(map_.size() <= static_cast<std::vector<std::vector<int>>::size_type>(y)) {
		map_.resize(y + 1);
	}

	std::vector<int>& row = map_[y];

	tile_string tstr;
	memset(&tstr[0], 0, tstr.size());
	std::string::const_iterator end = str.end();
	if(str.size() > 3) {
		end = str.begin() + 3;
	}

	std::copy(str.begin(), end, tstr.begin());

	const int index = getPatternIndexEntry(tstr);
	if(row.size() > static_cast<unsigned>(x) && row[x] == index) {
		return false;
	}

	const int empty_index = getPatternIndexEntry(empty_tile);
	while(row.size() <= static_cast<unsigned>(x)) {
		row.push_back(empty_index);
	}

	row[x] = index;

	// clear out variations info
	if (static_cast<unsigned>(y) < variations_.size() && static_cast<unsigned>(x) < variations_[y].size()) {
		variations_[y][x] = 0;
	}
	return true;
}

int TileMap::getPatternIndexEntry(const tile_string& str) {
	int index = 0;
	for(PatternIndexEntry& e : pattern_index_) {
		if(strcmp(e.str.data(), str.data()) == 0) {
			return index;
		}
		++index;
	}

	pattern_index_.push_back(PatternIndexEntry());
	pattern_index_.back().str = str;
	buildPatterns();
	return index;
}
