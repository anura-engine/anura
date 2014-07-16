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
	std::map<std::string, std::map<std::string, FunctionCreator*> >& function_creators()
	{
		static std::map<std::string, std::map<std::string, FunctionCreator*> > instance;
		return instance;
	}

	std::map<std::string, std::vector<std::string> >& helpstrings()
	{
		static std::map<std::string, std::vector<std::string> > instance;
		return instance;
	}
}

const std::map<std::string, FunctionCreator*>& get_function_creators(const std::string& module)
{
	return function_creators()[module];
}

int register_function_creator(const std::string& module, const std::string& id, FunctionCreator* creator)
{
	function_creators()[module][id] = creator;
	return function_creators()[module].size();
}

const std::vector<std::string>& function_helpstrings(const std::string& module)
{
	return helpstrings()[module];
}

int register_function_helpstring(const std::string& module, const std::string& str)
{
	helpstrings()[module].push_back(str);
	return helpstrings()[module].size();
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
