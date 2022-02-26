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

#include <iostream>

#include "game_registry.hpp"
#include "json_parser.hpp"
#include "variant_utils.hpp"

GameRegistry& GameRegistry::getInstance()
{
	static GameRegistry* obj = new GameRegistry();
	static game_logic::FormulaCallablePtr holder(obj);
	return *obj;
}

GameRegistry::GameRegistry()
{
	std::map<variant,variant> m;
	values_ = variant(&m);
}

void GameRegistry::setContents(variant node)
{
	values_ = node;
	if(values_.is_null()) {
		*this = GameRegistry();
	}
}

variant GameRegistry::writeContents()
{
	return values_;
}

variant GameRegistry::getValue(const std::string& key) const
{
	return values_[variant(key)];
}

void GameRegistry::setValue(const std::string& key, const variant& value)
{
	values_ = values_.add_attr(variant(key), value);
}

