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
#include "formula_callable_definition.hpp"
#include "formula_function_registry.hpp"
#include "tbs_functions.hpp"

using namespace game_logic;

namespace {
const std::string FunctionModule = "tbs";

class tbs_function_symbol_table : public function_symbol_table
{
public:
	expression_ptr create_function(
	                           const std::string& fn,
	                           const std::vector<expression_ptr>& args,
							   const_formula_callable_definition_ptr callable_def) const;
};

expression_ptr tbs_function_symbol_table::create_function(
                           const std::string& fn,
                           const std::vector<expression_ptr>& args,
						   const_formula_callable_definition_ptr callable_def) const
{
	const std::map<std::string, function_creator*>& creators = get_function_creators(FunctionModule);
	std::map<std::string, function_creator*>::const_iterator i = creators.find(fn);
	if(i != creators.end()) {
		return expression_ptr(i->second->create(args));
	}

	return function_symbol_table::create_function(fn, args, callable_def);
}

}

game_logic::function_symbol_table& get_tbs_functions_symbol_table()
{
	static tbs_function_symbol_table table;
	return table;
}


