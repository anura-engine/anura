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

#ifndef NO_EDITOR
#include <map>

#include "custom_object.hpp"
#include "debug_console.hpp"
#include "editor.hpp"
#include "editor_formula_functions.hpp"
#include "filesystem.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "level_solid_map.hpp"
#include "variant_utils.hpp"

namespace editor_script 
{
	using namespace game_logic;

	namespace 
	{
		class EditorCommand : public FormulaCallable 
		{
		public:
			virtual ~EditorCommand() {}
			virtual void execute(editor& e) = 0;
		private:
			variant getValue(const std::string& key) const override {
				return variant();
			}
		};

		class AddObjectCommand : public EditorCommand 
		{
			std::string id_;
			int x_, y_;
			bool facing_;
		public:
			AddObjectCommand(const std::string& id, int x, int y, bool facing)
			  : id_(id), x_(x), y_(y), facing_(facing)
			{}
		private:
			void execute(editor& e) override {
				CustomObject* obj = new CustomObject(id_, x_, y_, facing_);
				obj->setLevel(e.get_level());
				e.get_level().add_character(obj);
			}
		};

		class AddObjectFunction : public FunctionExpression 
		{
		public:
			explicit AddObjectFunction(const args_list& args)
			  : FunctionExpression("add_object", args, 4, 4)
			{}
		private:
			variant execute(const FormulaCallable& variables) const override {
				return variant(new AddObjectCommand(
						args()[0]->evaluate(variables).as_string(),
						args()[1]->evaluate(variables).as_int(),
						args()[2]->evaluate(variables).as_int(),
						args()[3]->evaluate(variables).as_bool()));
			}
		};

		class RemoveTileRectCommand : public EditorCommand 
		{
			std::string tile_id_;
			int x1_, y1_, x2_, y2_;
		public:
			RemoveTileRectCommand(const std::string& tile_id, int x1, int y1, int x2, int y2)
			  : tile_id_(tile_id), x1_(x1), y1_(y1), x2_(x2), y2_(y2)
			{}

			void execute(editor& e) override {
				e.add_tile_rect(e.get_tile_zorder(tile_id_), "", x1_, y1_, x2_, y2_);
			}
		};

		class RemoveTilesFunction : public FunctionExpression 
		{
		public:
			explicit RemoveTilesFunction(const args_list& args)
			  : FunctionExpression("remove_tiles", args, 3, 5)
			{}
		private:
			variant execute(const FormulaCallable& variables) const override {
				const std::string tile_id = args()[0]->evaluate(variables).as_string();
				const int x1 = args()[1]->evaluate(variables).as_int();
				const int y1 = args()[2]->evaluate(variables).as_int();
				const int x2 = args().size() > 3 ? args()[3]->evaluate(variables).as_int() : x1;
				const int y2 = args().size() > 4 ? args()[4]->evaluate(variables).as_int() : y1;
				return variant(new RemoveTileRectCommand(tile_id, x1*TileSize, y1*TileSize, x2*TileSize, y2*TileSize));
			}
		};

		class AddTileRectCommand : public EditorCommand 
		{
			std::string tile_id_;
			int x1_, y1_, x2_, y2_;
		public:
			AddTileRectCommand(const std::string& tile_id, int x1, int y1, int x2, int y2)
			  : tile_id_(tile_id), x1_(x1), y1_(y1), x2_(x2), y2_(y2)
			{}

			void execute(editor& e) override {
				e.add_tile_rect(e.get_tile_zorder(tile_id_), tile_id_, x1_, y1_, x2_, y2_);
			}
		};

		class AddTilesFunction : public FunctionExpression {
		public:
			explicit AddTilesFunction(const args_list& args)
			  : FunctionExpression("add_tiles", args, 3, 5)
			{}
		private:
			variant execute(const FormulaCallable& variables) const override {
				const std::string tile_id = args()[0]->evaluate(variables).as_string();
				const int x1 = args()[1]->evaluate(variables).as_int();
				const int y1 = args()[2]->evaluate(variables).as_int();
				const int x2 = args().size() > 3 ? args()[3]->evaluate(variables).as_int() : x1;
				const int y2 = args().size() > 4 ? args()[4]->evaluate(variables).as_int() : y1;
				return variant(new AddTileRectCommand(tile_id, x1*TileSize, y1*TileSize, x2*TileSize, y2*TileSize));
			}
		};

		class DebugCommand : public EditorCommand
		{
		public:
			explicit DebugCommand(const std::string& str) : str_(str)
			{}
			virtual void execute(editor& e) override {
				debug_console::addMessage(str_);
			}
		private:
			std::string str_;
		};

		class DebugFunction : public FunctionExpression 
		{
		public:
			explicit DebugFunction(const args_list& args)
			  : FunctionExpression("debug", args, 1, -1) {
			}
		private:
			variant execute(const FormulaCallable& variables) const override {
				std::string str;
				for(int n = 0; n != args().size(); ++n) {
					if(n) str += " ";
					str += args()[n]->evaluate(variables).to_debug_string();
				}

				LOG_INFO("DEBUG FUNCTION: " << str);
				return variant(new DebugCommand(str));
			}
		};

		class EditorCommandFunctionSymbolTable : public FunctionSymbolTable
		{
		public:
			static EditorCommandFunctionSymbolTable& instance() {
				static EditorCommandFunctionSymbolTable result;
				return result;
			}

			ExpressionPtr createFunction(
									   const std::string& fn,
									   const std::vector<ExpressionPtr>& args,
									   ConstFormulaCallableDefinitionPtr callable_def) const override
			{
				if(fn == "remove_tiles") {
					return ExpressionPtr(new RemoveTilesFunction(args));
				} else if(fn == "add_tiles") {
					return ExpressionPtr(new AddTilesFunction(args));
				} else if(fn == "add_object") {
					return ExpressionPtr(new AddObjectFunction(args));
				} else if(fn == "debug") {
					return ExpressionPtr(new DebugFunction(args));
				} else {
					return FunctionSymbolTable::createFunction(fn, args, callable_def);
				}
			}
		};

		void executeCommand(variant cmd, editor& e) {
			if(cmd.is_list()) {
				for(int n = 0; n != cmd.num_elements(); ++n) {
					executeCommand(cmd[n], e);
				}
			} else if(cmd.is_callable()) {
				EditorCommand* command = cmd.try_convert<EditorCommand>();
				if(command) {
					command->execute(e);
				}
			}
		}

		class TileCallable : public FormulaCallable 
		{
		public:
			TileCallable(editor& e, int x, int y)
			  : editor_(e), 
			  x_(x), 
			  y_(y)
			{}

		private:
			variant getValue(const std::string& key) const override {
				if(key == "x") {
					return variant(x_);
				} else if(key == "y") {
					return variant(y_);
				} else if(key == "tiles") {
					return get_tiles(x_, y_);
				} else if(key == "up") {
					return variant(new TileCallable(editor_, x_, y_-1));
				} else if(key == "down") {
					return variant(new TileCallable(editor_, x_, y_+1));
				} else if(key == "left") {
					return variant(new TileCallable(editor_, x_-1, y_));
				} else if(key == "right") {
					return variant(new TileCallable(editor_, x_+1, y_));
				} else {
					return variant();
				}
			}

			variant get_tiles(int x, int y) const {
				std::vector<variant> result;

				std::map<int, std::vector<std::string> > m;
				editor_.get_level().getAllTilesRect(x*TileSize, y*TileSize, x*TileSize, y*TileSize, m);
				for(auto i : m) {
					for(const std::string& s : i.second) {
						result.push_back(variant(s));
					}
				}

				return variant(&result);
			}

			editor& editor_;
			int x_, y_;
		};

		class EditorCommandCallable : public FormulaCallable 
		{
		public:
			explicit EditorCommandCallable(editor& e) : editor_(e)
			{}
		private:
			variant getValue(const std::string& key) const override {
				if(key == "cells") {
					std::vector<variant> result;

					const editor::tile_selection& selection = editor_.selection();
					if(selection.empty()) {
						const rect& dim = editor_.get_level().boundaries();
						for(int y = dim.y() - dim.y()%TileSize; y < dim.y2(); y += TileSize) {
							for(int x = dim.x() - dim.x()%TileSize; x < dim.x2(); x += TileSize) {
								result.push_back(variant(new TileCallable(editor_, x/TileSize, y/TileSize)));
							}
						}
					} else {
						for(const point& p : selection.tiles) {
							result.push_back(variant(new TileCallable(editor_, p.x, p.y)));
						}
					}

					return variant(&result);
				} else {
					return variant();
				}
			}
			editor& editor_;
		};


		std::vector<info> scripts_info;
		std::map<std::string, ConstFormulaPtr> scripts;

		void load_scripts()
		{
			if(scripts_info.empty() == false) {
				return;
			}

			if(!sys::file_exists("data/editor_scripts.cfg")) {
				return;
			}

			variant node = json::parse_from_file("data/editor_scripts.cfg");

			//load any functions defined here.
			for(variant function_node : node["function"].as_list()) {
			}

			for(variant script_node : node["script"].as_list()) {
				const std::string& id = script_node["id"].as_string();
				info script = { id };
				scripts_info.push_back(script);
				scripts[id].reset(new Formula(script_node["script"], &EditorCommandFunctionSymbolTable::instance()));
			}
		}
	}

	std::vector<info> all_scripts() 
	{
		load_scripts();
		return scripts_info;
	}

	void execute(const std::string& id, editor& e)
	{
		load_scripts();

		std::map<std::string, ConstFormulaPtr>::const_iterator itor = scripts.find(id);
		if(itor == scripts.end() || !itor->second) {
			return;
		}

		FormulaCallablePtr callable(new EditorCommandCallable(e));
		const variant cmd = itor->second->execute(*callable);

		//execute the command, making sure the editor allows the user to undo the
		//entire script in one go.
		e.begin_command_group();
		executeCommand(cmd, e);
		e.end_command_group();
	}
}

#endif // NO_EDITOR
