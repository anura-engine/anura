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

#pragma once

#include "kre/Geometry.hpp"

#include "entity_fwd.hpp"
#include "formula_callable.hpp"
#include "variant.hpp"

class particle_system;
typedef boost::intrusive_ptr<particle_system> particle_system_ptr;
typedef boost::intrusive_ptr<const particle_system> const_particle_system_ptr;

class particle_system_factory;
typedef std::shared_ptr<const particle_system_factory> const_particle_system_factory_ptr;

class particle_system_factory
{
public:
	static const_particle_system_factory_ptr create_factory(variant node);
	
	virtual ~particle_system_factory();
	virtual particle_system_ptr create(const Entity& e) const = 0;
};

class particle_system : public game_logic::FormulaCallable
{
public:
	virtual ~particle_system();
	virtual bool is_destroyed() const { return false; }
	virtual bool should_save() const { return true; }
	virtual void process(const Entity& e) = 0;
	virtual void draw(const rect& area, const Entity& e) const = 0;

	void set_type(const std::string& type) { type_ = type; }
	const std::string& type() const { return type_; }
private:
	std::string type_;
};
