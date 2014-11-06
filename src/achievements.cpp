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
#include <algorithm>
#include <map>

#include "achievements.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "formula_callable.hpp"
#include "i18n.hpp"
#include "json_parser.hpp"
#include "preferences.hpp"
#include "string_utils.hpp"
#include "variant.hpp"

namespace {
std::map<std::string, achievement_ptr> cache;
}

achievement_ptr achievement::get(const std::string& id)
{
	if(cache.empty()) {
		variant node;
		try {
			node = (json::parse_from_file("data/achievements.cfg"));
		} catch(json::parse_error&) {
			cache[""]; //mark as loaded
			return achievement_ptr();
		}

		
		foreach(variant achievement_node, node["achievement"].as_list()) {
			achievement_ptr a(new achievement(achievement_node));
			cache[a->id()] = a;
		}
	}

	return cache[id];
}

achievement::achievement(variant node)
  : id_(node["id"].as_string()), name_(i18n::tr(node["name"].as_string())),
    description_(i18n::tr(node["description"].as_string())),
	points_(node["points"].as_int())
{
}

namespace {
std::vector<std::string>* achievements = NULL;
}

bool attain_achievement(const std::string& id)
{
	if(achievements == NULL) {
		achievements = new std::vector<std::string>;
		variant val = preferences::registry()->query_value("achievements");
		if(val.is_string()) {
			*achievements = util::split(val.as_string());
			std::sort(achievements->begin(), achievements->end());
		}
	}

	if(std::binary_search(achievements->begin(), achievements->end(), id)) {
		return false;
	}
	
	achievements->push_back(id);
	std::sort(achievements->begin(), achievements->end());

	preferences::registry()->mutate_value("achievements", variant(util::join(*achievements)));

	return true;
}
