/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#include <boost/bind.hpp>

#include "asserts.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "hex_tile.hpp"
#include "hex_map.hpp"
#include "hex_object.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "variant_utils.hpp"


namespace hex {

namespace {

std::map<std::string, tile_type_ptr>& get_tile_type_map()
{
	static std::map<std::string, tile_type_ptr> tile_map;
	return tile_map;
}

std::vector<tile_type_ptr>& get_hex_editor_tiles()
{
	static std::vector<tile_type_ptr> tiles;
	return tiles;
}

std::map<std::string, tile_type_ptr>& get_editor_hex_tile_map()
{
	static std::map<std::string, tile_type_ptr> tile_map;
	return tile_map;
}

void load_editor_tiles()
{
	std::map<std::string, tile_type_ptr>::const_iterator it = get_tile_type_map().begin();
	while(it != get_tile_type_map().end()) {
		if(it->second->get_editor_info().name.empty() == false 
			&& it->second->get_editor_info().type.empty() == false) {
			get_hex_editor_tiles().push_back(it->second);
		}
		++it;
	}
}

void load_hex_editor_tiles()
{
	std::map<std::string, tile_type_ptr>::const_iterator it = get_tile_type_map().begin();
	while(it != get_tile_type_map().end()) {
		if(it->second->get_editor_info().type.empty() == false) {
			get_editor_hex_tile_map()[it->second->get_editor_info().type] = it->second;
		}
		++it;
	}
}

void load_hex_tiles(variant node)
{
	if(!get_tile_type_map().empty()) {
		get_tile_type_map().clear();
	}
	for(auto p : node.as_map()) {
		std::string key_str = p.first.as_string();
		get_tile_type_map()[key_str] = tile_type_ptr(new tile_type(key_str, p.second));
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

class get_tile_function : public game_logic::function_expression {
public:
	explicit get_tile_function(const args_list& args)
	 : function_expression("get_tile", args, 1, 1)
	{}
private:
	variant execute(const game_logic::formula_callable& variables) const {
		const std::string key = args()[0]->evaluate(variables).as_string();
		return variant(hex_object::get_hex_tile(key).get());
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
		game_logic::const_formula_callable_definition_ptr callable_def) const
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
		std::map<std::string, tile_type_ptr>::const_iterator tile_it = get_tile_type_map().begin();
		while(tile_it != get_tile_type_map().end()) {
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

hex_object::hex_object(const std::string& type, int x, int y, const hex_map* owner) 
	: neighbors_init_(false), owner_map_(owner), x_(x), y_(y), type_(type)
{
	generate_hex_engine(); // make sure hex engine is initialized.
	tile_ = get_tile_type_map()[type_];
	ASSERT_LOG(tile_, "Could not find tile: " << type_);
}

std::vector<std::string> hex_object::get_rules()
{
	return generate_hex_engine().rules;
}

hex_object_ptr hex_object::get_tile_in_dir(enum direction d) const
{
	return owner_map_->get_hex_tile(d, x_, y_);
}

hex_object_ptr hex_object::get_tile_in_dir(const std::string& s) const
{
	if(s == "north" || s == "n") {
		return get_tile_in_dir(NORTH);
	} else if(s == "south" || s == "s") {
		return get_tile_in_dir(SOUTH);
	} else if(s == "north_west" || s == "nw" || s == "northwest") {
		return get_tile_in_dir(NORTH_WEST);
	} else if(s == "north_east" || s == "ne" || s == "northeast") {
		return get_tile_in_dir(NORTH_EAST);
	} else if(s == "south_west" || s == "sw" || s == "southwest") {
		return get_tile_in_dir(SOUTH_WEST);
	} else if(s == "south_east" || s == "se" || s == "southeast") {
		return get_tile_in_dir(SOUTH_EAST);
	}
	return hex_object_ptr();
}

variant hex_object::get_value(const std::string& key) const
{
	ASSERT_LOG(owner_map_ != NULL, "Hex object not associated with a map! owner_ == NULL");
	hex_object_ptr ho = get_tile_in_dir(key);
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

void hex_object::set_value(const std::string& key, const variant& value)
{

#ifdef USE_SHADERS
	if(key == "shader") {
		ASSERT_LOG(value.is_map() && value.has_key("program"), 
			"shader must be specified by map having a \"program\" attribute");
		shader_.reset(new gles2::shader_program(value));
#endif
	}
}

void hex_object::build()
{
	// XXX
}

bool hex_object::execute_command(const variant& value)
{
	bool result = true;
	if(value.is_null()) {
		return result;
	}

	if(value.is_list()) {
		const int num_elements = value.num_elements();
		for(int n = 0; n != num_elements; ++n) {
			if(value[n].is_null() == false) {
				result = execute_command(value[n]) && result;
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

void hex_object::apply_rules(const std::string& rule)
{
	using namespace game_logic;
	std::map<std::string, const_formula_ptr>::const_iterator it = generate_hex_engine().handlers.find(rule);
	ASSERT_LOG(it != generate_hex_engine().handlers.end(), "Unable to find rule \"" << rule << "\" in the list of handlers.");
	map_formula_callable_ptr callable(new map_formula_callable(this));
	variant& a = callable->add_direct_access("hex");
	a = variant(this);
	variant value = it->second->execute(*callable.get());
	execute_command(value);
}

void hex_object::neighbors_changed()
{
	neighbors_init_ = false;
}

void hex_object::draw() const
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
		neighbor.type->draw_adjacent(x_, y_, neighbor.dirmap);
	}
}

void hex_object::init_neighbors() const
{
	if(neighbors_init_) {
		return;
	}

	neighbors_init_ = true;

	for(int n = 0; n < 6; ++n) {
		hex_object_ptr obj = get_tile_in_dir(static_cast<direction>(n));
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

std::vector<tile_type_ptr> hex_object::get_hex_tiles()
{
	std::vector<tile_type_ptr> v;
	std::transform(get_tile_type_map().begin(), get_tile_type_map().end(), 
		std::back_inserter(v), 
		boost::bind(&std::map<std::string, tile_type_ptr>::value_type::second,_1));
	return v;
}

std::vector<tile_type_ptr>& hex_object::get_editor_tiles()
{
	return get_hex_editor_tiles();
}

tile_type_ptr hex_object::get_hex_tile(const std::string& type)
{
	std::map<std::string, tile_type_ptr>::const_iterator it 
		= get_editor_hex_tile_map().find(type);
	if(it == get_editor_hex_tile_map().end()) {
		it = get_tile_type_map().find(type);
		if(it == get_tile_type_map().end()) {
			return tile_type_ptr();
		}
	}
	return it->second;
}

}
