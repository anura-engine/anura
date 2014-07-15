/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "asserts.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "hex_tile.hpp"
#include "hex_map.hpp"
#include "hex_object.hpp"
#include "json_parser.hpp"
#include "module.hpp"
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
				if(it->second->getgetEditorInfo().name.empty() == false 
					&& it->second->getgetEditorInfo().type.empty() == false) {
					get_hex_editor_tiles().push_back(it->second);
				}
				++it;
			}
		}

		void load_hex_editor_tiles()
		{
			std::map<std::string, TileTypePtr>::const_iterator it = get_tile_type_map().begin();
			while(it != get_tile_type_map().end()) {
				if(it->second->getgetEditorInfo().type.empty() == false) {
					get_editor_hex_tile_map()[it->second->getgetEditorInfo().type] = it->second;
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

		class GetTileFunction : public game_logic::function_expression 
		{
		public:
			explicit GetTileFunction(const args_list& args)
			 : function_expression("get_tile", args, 1, 1)
			{}
		private:
			variant execute(const game_logic::FormulaCallable& variables) const {
				const std::string key = args()[0]->evaluate(variables).as_string();
				return variant(HexObject::getHexTile(key).get());
			}
		};

		class HexFunctionSymbolTable : public game_logic::FunctionSymbolTable
		{
		public:
			HexFunctionSymbolTable()
			{}

			game_logic::expression_ptr create_function(
				const std::string& fn,
				const std::vector<game_logic::expression_ptr>& args,
				game_logic::ConstFormulaCallableDefinitionPtr callable_def) const
			{
				if(fn == "get_tile") {
					return game_logic::expression_ptr(new GetTileFunction(args));
				}
				return FunctionSymbolTable::create_function(fn, args, callable_def);
			}
		};

		game_logic::FunctionSymbolTable& get_hex_function_symbol_table()
		{
			static HexFunctionSymbolTable table;
			return table;
		}

		struct HexEngine 
		{
			HexEngine() 
			{}

			explicit HexEngine (const variant& value)
			{
				rules = value["rules"].as_list_string();

				variant tiles_var = value["tiles"];
				ASSERT_LOG(tiles_var.is_map(), "\"tiles\" must be a map type.");
				load_hex_tiles(tiles_var);

				functions_var = value["functions"];
				if(functions_var.is_null() == false) {
					ASSERT_LOG(functions_var.is_string() == true || functions_var.is_list() == true, "\"functions must\" be specified as a string or list.");
					functions.reset(new game_logic::FunctionSymbolTable());
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
					for(const variant_pair& p : handlers_var.as_map()) {
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
				for(const std::string& s : rules) {
					res.add("rules", s);
				}
				for(auto& it : get_tile_type_map()) {
					variant_builder node;
					node.add(it.first, it.second->write());
					res.add("tiles", node.build());
				}
				return res.build();
			}

			variant functions_var;
			std::shared_ptr<game_logic::FunctionSymbolTable> functions;
			std::map<std::string, game_logic::const_formula_ptr> handlers;
			std::vector<std::string> rules;
		};

		HexEngine& generate_hex_engine()
		{
			static HexEngine hexes (json::parse_from_file("data/hex_tiles.cfg"));
			return hexes;
		}
	}

	HexObject::HexObject(const std::string& type, int x, int y, const HexMap* owner) 
		: owner_map_(owner), x_(x), y_(y), type_(type)
	{
		generate_hex_engine(); // make sure hex engine is initialized.
		tile_ = get_tile_type_map()[type_];
		ASSERT_LOG(tile_, "Could not find tile: " << type_);
	}

	std::vector<std::string> HexObject::getRules()
	{
		return generate_hex_engine().rules;
	}

	HexObjectPtr HexObject::getTileInDir(Direction d) const
	{
		return owner_map_->getHexTile(d, x_, y_);
	}

	HexObjectPtr HexObject::getTileInDir(const std::string& s) const
	{
		if(s == "north" || s == "n") {
			return getTileInDir(Direction::NORTH);
		} else if(s == "south" || s == "s") {
			return getTileInDir(Direction::SOUTH);
		} else if(s == "north_west" || s == "nw" || s == "northwest") {
			return getTileInDir(Direction::NORTH_WEST);
		} else if(s == "north_east" || s == "ne" || s == "northeast") {
			return getTileInDir(Direction::NORTH_EAST);
		} else if(s == "south_west" || s == "sw" || s == "southwest") {
			return getTileInDir(Direction::SOUTH_WEST);
		} else if(s == "south_east" || s == "se" || s == "southeast") {
			return getTileInDir(Direction::SOUTH_EAST);
		}
		return HexObjectPtr();
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(HexObject)
		DEFINE_FIELD(north, "builtin HexObject|null")
			return variant(obj.getTileInDir(Direction::NORTH).get());
		DEFINE_FIELD(n, "builtin HexObject|null")
			return variant(obj.getTileInDir(Direction::NORTH).get());
		DEFINE_FIELD(south, "builtin HexObject|null")
			return variant(obj.getTileInDir(Direction::SOUTH).get());
		DEFINE_FIELD(s, "builtin HexObject|null")
			return variant(obj.getTileInDir(Direction::SOUTH).get());
		DEFINE_FIELD(ne, "builtin HexObject|null")
			return variant(obj.getTileInDir(Direction::NORTH_EAST).get());
		DEFINE_FIELD(northeast, "builtin HexObject|null")
			return variant(obj.getTileInDir(Direction::NORTH_EAST).get());
		DEFINE_FIELD(se, "builtin HexObject|null")
			return variant(obj.getTileInDir(Direction::SOUTH_EAST).get());
		DEFINE_FIELD(southeast, "builtin HexObject|null")
			return variant(obj.getTileInDir(Direction::SOUTH_EAST).get());
		DEFINE_FIELD(nw, "builtin HexObject|null")
			return variant(obj.getTileInDir(Direction::NORTH_WEST).get());
		DEFINE_FIELD(north_west, "builtin HexObject|null")
			return variant(obj.getTileInDir(Direction::NORTH_WEST).get());
		DEFINE_FIELD(sw, "builtin HexObject|null")
			return variant(obj.getTileInDir(Direction::SOUTH_WEST).get());
		DEFINE_FIELD(southwest, "builtin HexObject|null")
			return variant(obj.getTileInDir(Direction::SOUTH_WEST).get());
		DEFINE_FIELD(base_type, "string")
			return variant(obj.type_);
		DEFINE_FIELD(type, "string|null")
			if(!obj.tile_) {
				return variant();
			}
			return variant(obj.tile_->id());
		DEFINE_FIELD(x, "int")
			return variant(obj.x_);
		DEFINE_FIELD(x, "int")
			return variant(obj.y_);
		DEFINE_FIELD(xy, "[int,int]")
			std::vector<variant> v;
			v.emplace_back(obj.x_);
			v.emplace_back(obj.y_);
			return variant(&v);
	END_DEFINE_CALLABLE(HexObject)

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
				cmd->runCommand(*this);
			}
		}
		return result;
	}

	void HexObject::applyRules(const std::string& rule)
	{
		using namespace game_logic;
		std::map<std::string, const_formula_ptr>::const_iterator it = generate_hex_engine().handlers.find(rule);
		ASSERT_LOG(it != generate_hex_engine().handlers.end(), "Unable to find rule \"" << rule << "\" in the list of handlers.");
		MapFormulaCallablePtr callable(new MapFormulaCallable(this));
		variant& a = callable->add_direct_access("hex");
		a = variant(this);
		variant value = it->second->execute(*callable.get());
		executeCommand(value);
	}

	void HexObject::draw() const
	{
		// Draw base tile.
		if(tile_ == NULL) {
			return;
		}

		tile_->draw(x_, y_);

		for(const NeighborType& neighbor : neighbors_) {
			neighbor.type->drawAdjacent(x_, y_, neighbor.dirmap);
		}
	}

	void HexObject::initNeighbors()
	{
		for(int n = 0; n < 6; ++n) {
			HexObjectPtr obj = getTileInDir(static_cast<Direction>(n));
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

		for(auto& neighbor : neighbors_) {
			neighbor.type->calculateAdjacencyPattern(neighbor.dirmap);
		}
	}

	std::vector<TileTypePtr> HexObject::getHexTiles()
	{
		using std::placeholders::_1;
		std::vector<TileTypePtr> v;
		std::transform(get_tile_type_map().begin(), get_tile_type_map().end(), 
			std::back_inserter(v), 
			std::bind(&std::map<std::string, TileTypePtr>::value_type::second,_1));
		return v;
	}

	std::vector<TileTypePtr>& HexObject::getEditorTiles()
	{
		return get_hex_editor_tiles();
	}

	TileTypePtr HexObject::getHexTile(const std::string& type)
	{
		std::map<std::string, TileTypePtr>::const_iterator it 
			= get_editor_hex_tile_map().find(type);
		if(it == get_editor_hex_tile_map().end()) {
			it = get_tile_type_map().find(type);
			if(it == get_tile_type_map().end()) {
				return TileTypePtr();
			}
		}
		return it->second;
	}
}
