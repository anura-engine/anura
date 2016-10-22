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

#include "formula_callable_definition.hpp"
#include "formula_function_registry.hpp"
#include "unit_test.hpp"

namespace
{
	typedef std::map<std::string, std::map<std::string, FunctionCreator*> > function_creators_table;
	typedef std::map<std::string, std::map<std::string, ffl::IntrusivePtr<game_logic::FunctionExpression> > > singleton_function_table;
	typedef std::map<std::string, std::vector<std::string> > helpstrings_table;

	// Takes ownership of the pointers in the function creator table,
	// deleting them at program termination to suppress valgrind false positives.
	struct function_creator_manager {
		function_creators_table table_;

		~function_creator_manager() {
			for(function_creators_table::value_type & v : table_) {
				for(auto u : v.second) {
					delete(u.second);
				}
			}
		}
	};

	std::map<std::string, std::map<std::string, FunctionCreator*> >& function_creators()
	{
		static function_creator_manager instance;
		return instance.table_;
	}

	singleton_function_table& singleton_functions()
	{
		static singleton_function_table instance;
		return instance;
	}

	std::vector<game_logic::FunctionExpression*> g_all_singleton_functions;
	std::map<std::pair<std::string,std::string>, int> g_all_singleton_function_indexes;

	std::map<std::string, std::vector<std::string> >& helpstrings()
	{
		static helpstrings_table instance;
		return instance;
	}
}

int get_builtin_ffl_function_index(const std::string& module, const std::string& id)
{
	std::pair<std::string,std::string> key(module, id);
	auto itor = g_all_singleton_function_indexes.find(key);
	if(itor != g_all_singleton_function_indexes.end()) {
		return itor->second;
	}

	int result = static_cast<int>(g_all_singleton_functions.size());
	g_all_singleton_functions.push_back(singleton_functions()[module][id].get());
	g_all_singleton_function_indexes[key] = result;
	return result;
}

game_logic::FunctionExpression* get_builtin_ffl_function_from_index(int index)
{
	return g_all_singleton_functions[index];
}

const std::map<std::string, FunctionCreator*>& get_function_creators(const std::string& module)
{
	return function_creators()[module];
}

int register_function_creator(const std::string& module, const std::string& id, FunctionCreator* creator)
{
	function_creators()[module][id] = creator;
	game_logic::FunctionExpression::args_list args;
	auto fn = creator->create(args);
	fn->clearUnusedArguments();
	singleton_functions()[module][id] = fn;
	return static_cast<int>(function_creators()[module].size());
}

const std::vector<std::string>& function_helpstrings(const std::string& module)
{
	return helpstrings()[module];
}

int register_function_helpstring(const std::string& module, const std::string& str)
{
	helpstrings()[module].push_back(str);
	return static_cast<int>(helpstrings()[module].size());
}

COMMAND_LINE_UTILITY(document_ffl_functions)
{
	for(std::map<std::string, std::vector<std::string> >::const_iterator i = helpstrings().begin(); i != helpstrings().end(); ++i) {
		if(i->second.empty()) {
			continue;
		}

		std::cout << "-- MODULE: " << i->first << " --\n";
		std::vector<std::string> helpstrings = function_helpstrings(i->first);
		std::sort(helpstrings.begin(), helpstrings.end());
		for(std::string s : helpstrings) {
			std::string::iterator i = std::find(s.begin(), s.end(), ':');
			if(i != s.end()) {
				s = "{{{ " + std::string(s.begin(), i) + " }}}" + std::string(i, s.end());
			}
			s = "  * " + s;
			std::cout << s << "\n";
		}

		std::cout << "\n";
	}
}
