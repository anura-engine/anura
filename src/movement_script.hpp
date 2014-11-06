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
#ifndef MOVEMENT_SCRIPT_HPP_INCLUDED
#define MOVEMENT_SCRIPT_HPP_INCLUDED

#include "entity.hpp"
#include "formula.hpp"
#include "variant.hpp"

#include <map>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

class active_movement_script {
public:
	~active_movement_script();

	void modify(entity_ptr entity, const std::map<std::string, game_logic::const_formula_ptr>& handlers);
private:
	struct entity_mod {
		entity_ptr entity;
		std::map<std::string, game_logic::const_formula_ptr> handlers_backup;
	};

	std::vector<entity_mod> mods_;
};

typedef boost::shared_ptr<active_movement_script> active_movement_script_ptr;
typedef boost::shared_ptr<const active_movement_script> const_active_movement_script_ptr;

class movement_script
{
public:
	movement_script() {}
	explicit movement_script(variant node);
	active_movement_script_ptr begin_execution(const game_logic::formula_callable& callable) const;
	const std::string& id() const { return id_; }

	variant write() const;
private:
	struct modification {
		game_logic::const_formula_ptr target_formula;
		std::map<std::string, game_logic::const_formula_ptr> handlers;
	};

	std::string id_;
	std::vector<modification> modifications_;
};

#endif
