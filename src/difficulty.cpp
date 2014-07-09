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

#include <boost/bimap.hpp>

#include "asserts.hpp"
#include "difficulty.hpp"
#include "json_parser.hpp"
#include "module.hpp"

namespace difficulty 
{
	namespace 
	{
		typedef boost::bimap<std::string,int> diffculty_map_type;
		typedef diffculty_map_type::value_type position;

		diffculty_map_type& get_difficulty_map()
		{
			static diffculty_map_type res;
			return res;
		}

		void create_difficulty_map()
		{
			variant diff = json::parse_from_file("data/difficulty.cfg");
			// Any option is always defined.
			get_difficulty_map().insert(position("any", -1));
			for(size_t i = 0; i < diff["difficulties"].num_elements(); i++) {
				get_difficulty_map().insert(position(diff["difficulties"][i]["text"].as_string(), diff["difficulties"][i]["value"].as_int()));
			}
		}
	}

	manager::manager()
	{
		create_difficulty_map();
	}

	manager::~manager()
	{
	}

	std::string to_string(int diff)
	{
		diffculty_map_type::right_const_iterator it = get_difficulty_map().right.find(diff);
		if(it == get_difficulty_map().right.end()) {
			LOG_WARN("Unrecognised difficulty value: \"" << diff << "\", please see the file data/difficulties.cfg for a list");
			return "";
		}
		return it->second;
	}

	int from_string(const std::string& s)
	{
		diffculty_map_type::left_const_iterator it = get_difficulty_map().left.find(s);
		if(it == get_difficulty_map().left.end()) {
			ASSERT_LOG(false, "Unrecognised difficulty value: \"" << s << "\", please see the file data/difficulties.cfg for a list");
			return -1;
		}
		return it->second;
	}

	int from_variant(variant node)
	{
		if(node.is_string()) {
			return from_string(node.as_string());
		}
		return node.as_int(-1);
	}
}
