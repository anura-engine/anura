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
#include <iostream>

#include "game_registry.hpp"
#include "json_parser.hpp"
#include "variant_utils.hpp"

game_registry& game_registry::instance()
{
	static game_registry* obj = new game_registry;
	static game_logic::formula_callable_ptr holder(obj);
	return *obj;
}

game_registry::game_registry()
{
	std::map<variant,variant> m;
	values_ = variant(&m);
}

void game_registry::set_contents(variant node)
{
	values_ = node;
	if(values_.is_null()) {
		*this = game_registry();
	}
}

variant game_registry::write_contents()
{
	return values_;
}

variant game_registry::get_value(const std::string& key) const
{
	return values_[variant(key)];
}

void game_registry::set_value(const std::string& key, const variant& value)
{
	values_ = values_.add_attr(variant(key), value);
}

