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
#include <boost/bind.hpp>

#include "asserts.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "hex_tile.hpp"
#include "HexMap.hpp"
#include "HexObject.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "variant_utils.hpp"


namespace hex {

namespace {

std::map<std::string, TileTypePtr>& get_TileType_map()
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
	std::map<std::string, TileTypePtr>::const_iterator it = get_TileType_map().begin();
	while(it != get_TileType_map().end()) {
		if(it->second->getEditorInfo().name.empty() == false 
			&& it->second->getEditorInfo().type.empty() == false) {
			get_hex_editor_tiles().push_back(it->second);
		}
		++it;
	}
}

void load_hex_editor_tiles()
{
	std::map<std::string, TileTypePtr>::const_iterator it = get_TileType_map().begin();
	while(it != get_TileType_map().end()) {
		if(it->second->getEditorInfo().type.empty() == false) {
			get_editor_hex_tile_map()[it->second->getEditorInfo().type] = it->second;
		}
		++it;
	}
}

void load_hex_tiles(variant node)
{
	if(!get_TileType_map().empty()) {
		get_TileType_map().clear();
	}
	for(auto p : node.as_map()) {
		std::string key_str = p.first.as_string();
		get_TileType_map()[key_str] = TileTypePtr(new TileType(key_str, p.second));
	}

	// get list of all tiles have non-empty "EditorInfo" blocks.
	if(!get_hex_editor_tiles().empty()) {
		get_hex_editor_tiles().clear();
	}
	load_editor_tiles();

	if(!get_editor_hex_tile_map().empty()) {
		get_editor_hex_tile_map().clear();
	}
	load_hex_editor_tiles();
}

class get_tile_function : public game_logic::function_expression {
public:
	explicit get_tile_function(const args_list& args)
	 : function_expression("get_tile", args, 1, 1)
	{}
private:
	variant execute(const game_logic::FormulaCallable& variables) const {
		const std::string key = args()[0]->evaluate(variables).as_string();
		return variant(HexObject::get_hex_tile(key).get());
	}
};

class hex_function_symbol_table : public game_logic::function_symbol_table
{
public:
	hex_function_symbol_table()
	{}

	game_logic::expression_ptr create_function(
		const std::string& fn,
		const std::vector<game_logic::expression_ptr>& args,
		game_logic::const_FormulaCallable_definition_ptr callable_def) const
	{
		if(fn == "get_tile") {
			return game_logic::expression_ptr(new get_tile_function(args));
		}
		return function_symbol_table::create_function(fn, args, callable_def);
	}
};

game_logic::function_symbol_table& get_hex_function_symbol_table()
{
	static hex_function_symbol_table table;
	return table;
}

struct hex_engine 
{
	hex_engine() 
	{}

	explicit hex_engine(const variant& value)
	{
		rules = value["rules"].as_list_string();

		variant tiles_var = value["tiles"];
		ASSERT_LOG(tiles_var.is_map(), "\"tiles\" must be a map type.");
		load_hex_tiles(tiles_var);

		functions_var = value["functions"];
		if(functions_var.is_null() == false) {
			ASSERT_LOG(functions_var.is_string() == true || functions_var.is_list() == true, "\"functions must\" be specified as a string or list.");
			functions.reset(new game_logic::function_symbol_table);
			functions->set_backup(&get_hex_function_symbol_table());
			if(functions_var.is_string()) {
				game_logic::formula f(functions_var, functions.get());
			} else if(functions_var.is_list()) {
				for(int n = 0; n != functions_var.num_elements(); ++n) {
					game_logic::formula f(functions_var[n], functions.get());
				}
			}
		}

		variant handlers_var = value["handlers"];
		if(handlers_var.is_null() == false) {
			ASSERT_LOG(handlers_var.is_map() == true, "\"handlers\" must be specified by a map.");
			handlers.clear();
			foreach(const variant_pair& p, handlers_var.as_map()) {
				handlers[p.first.as_string()] = game_logic::formula::create_optional_formula(p.second, functions.get());
			}
		}
	}

	variant write() const
	{
		variant_builder res;
		res.add("functions", functions_var);
		std::map<std::string, game_logic::const_formula_ptr>::const_iterator it = handlers.begin();
		while(it != handlers.end()) {
			variant_builder node;
			node.add(it->first, it->second->str());
			res.add("handlers", node.build());
			++it;
		}
		foreach(const std::string& s, rules) {
			res.add("rules", s);
		}
		std::map<std::string, TileTypePtr>::const_iterator tile_it = get_TileType_map().begin();
		while(tile_it != get_TileType_map().end()) {
			variant_builder node;
			node.add(tile_it->first, tile_it->second->write());
			res.add("tiles", node.build());
			++tile_it;
		}
		return res.build();
	}

	variant functions_var;
	boost::shared_ptr<game_logic::function_symbol_table> functions;
	std::map<std::string, game_logic::const_formula_ptr> handlers;
	std::vector<std::string> rules;
};

hex_engine& generate_hex_engine()
{
	static hex_engine hexes (json::parse_from_file("data/hex_tiles.cfg"));
	return hexes;
}

}

HexObject::HexObject(const std::string& type, int x, int y, const HexMap* owner) 
	: neighbors_init_(false), owner_map_(owner), x_(x), y_(y), type_(type)
{
	generate_hex_engine(); // make sure hex engine is initialized.
	tile_ = get_TileType_map()[type_];
	ASSERT_LOG(tile_, "Could not find tile: " << type_);
}

std::vector<std::string> HexObject::get_rules()
{
	return generate_hex_engine().rules;
}

HexObjectPtr HexObject::getTileInDir(enum direction d) const
{
	return owner_map_->get_hex_tile(d, x_, y_);
}

HexObjectPtr HexObject::getTileInDir(const std::string& s) const
{
	if(s == "north" || s == "n") {
		return getTileInDir(NORTH);
	} else if(s == "south" || s == "s") {
		return getTileInDir(SOUTH);
	} else if(s == "north_west" || s == "nw" || s == "northwest") {
		return getTileInDir(NORTH_WEST);
	} else if(s == "north_east" || s == "ne" || s == "northeast") {
		return getTileInDir(NORTH_EAST);
	} else if(s == "south_west" || s == "sw" || s == "southwest") {
		return getTileInDir(SOUTH_WEST);
	} else if(s == "south_east" || s == "se" || s == "southeast") {
		return getTileInDir(SOUTH_EAST);
	}
	return HexObjectPtr();
}

variant HexObject::getValue(const std::string& key) const
{
	ASSERT_LOG(owner_map_ != NULL, "Hex object not associated with a map! owner_ == NULL");
	HexObjectPtr ho = getTileInDir(key);
	if(ho != NULL) {
		return variant(ho.get());
	} else if(key == "self") {
		return variant(this);
	} else if(key == "base_type") {
		return variant(type_);
	} else if(key == "type") {
		if(tile_) {
			return variant(tile_->id());
		}
	} else if(key == "x") {
		return variant(x_);
	} else if(key == "y") {
		return variant(y_);
	} else if(key == "xy") {
		std::vector<variant> v;
		v.push_back(variant(x_));
		v.push_back(variant(y_));
		return variant(&v);
#ifdef USE_SHADERS
	} else if(key == "shader") {
		return variant(shader_.get());
#endif
	}
	 
	return variant();
}

void HexObject::setValue(const std::string& key, const variant& value)
{

#ifdef USE_SHADERS
	if(key == "shader") {
		ASSERT_LOG(value.is_map() && value.has_key("program"), 
			"shader must be specified by map having a \"program\" attribute");
		shader_.reset(new gles2::shader_program(value));
#endif
	}
}

void HexObject::build()
{
	// XXX
}

bool HexObject::executeCommand(const variant& value)
{
	bool result = true;
	if(value.is_null()) {
		return result;
	}

	if(value.is_list()) {
		const int num_elements = value.num_elements();
		for(int n = 0; n != num_elements; ++n) {
			if(value[n].is_null() == false) {
				result = executeCommand(value[n]) && result;
			}
		}
	} else {
		game_logic::command_callable* cmd = value.try_convert<game_logic::command_callable>();
		if(cmd != NULL) {
			cmd->run_command(*this);
		}
	}
	return result;
}

void HexObject::applyRules(const std::string& rule)
{
	using namespace game_logic;
	std::map<std::string, const_formula_ptr>::const_iterator it = generate_hex_engine().handlers.find(rule);
	ASSERT_LOG(it != generate_hex_engine().handlers.end(), "Unable to find rule \"" << rule << "\" in the list of handlers.");
	map_FormulaCallablePtr callable(new map_FormulaCallable(this));
	variant& a = callable->add_direct_access("hex");
	a = variant(this);
	variant value = it->second->execute(*callable.get());
	executeCommand(value);
}

void HexObject::neighborsChanged()
{
	neighbors_init_ = false;
}

void HexObject::draw() const
{
	// Draw base tile.
	if(tile_ == NULL) {
		return;
	}

	init_neighbors();

#ifdef USE_SHADERS
	gles2::manager gles2_manager(shader_);
#endif

	tile_->draw(x_, y_);

	for(const NeighborType& neighbor : neighbors_) {
		neighbor.type->drawAdjacent(x_, y_, neighbor.dirmap);
	}
}

void HexObject::init_neighbors() const
{
	if(neighbors_init_) {
		return;
	}

	neighbors_init_ = true;

	for(int n = 0; n < 6; ++n) {
		HexObjectPtr obj = getTileInDir(static_cast<direction>(n));
		if(obj && obj->tile() && obj->tile()->height() > tile()->height()) {
			NeighborType* neighbor = NULL;
			for(NeighborType& candidate : neighbors_) {
				neighbor = &candidate;
			}

			if(!neighbor) {
				neighbors_.push_back(NeighborType());
				neighbor = &neighbors_.back();
				neighbor->type = obj->tile();
			}

			neighbor->dirmap = neighbor->dirmap | (1 << n);
		}
	}
}

std::vector<TileTypePtr> HexObject::get_hex_tiles()
{
	std::vector<TileTypePtr> v;
	std::transform(get_TileType_map().begin(), get_TileType_map().end(), 
		std::back_inserter(v), 
		boost::bind(&std::map<std::string, TileTypePtr>::value_type::second,_1));
	return v;
}

std::vector<TileTypePtr>& HexObject::get_editor_tiles()
{
	return get_hex_editor_tiles();
}

TileTypePtr HexObject::get_hex_tile(const std::string& type)
{
	std::map<std::string, TileTypePtr>::const_iterator it 
		= get_editor_hex_tile_map().find(type);
	if(it == get_editor_hex_tile_map().end()) {
		it = get_TileType_map().find(type);
		if(it == get_TileType_map().end()) {
			return TileTypePtr();
		}
	}
	return it->second;
}

}
