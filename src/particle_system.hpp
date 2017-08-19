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

#include "geometry.hpp"
#include "SceneObject.hpp"
#include "SceneUtil.hpp"
#include "Color.hpp"

#include "entity_fwd.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "variant.hpp"

class ParticleSystem;
typedef ffl::IntrusivePtr<ParticleSystem> ParticleSystemPtr;
typedef ffl::IntrusivePtr<const ParticleSystem> ConstParticleSystemPtr;

class ParticleSystemFactory;
typedef std::shared_ptr<const ParticleSystemFactory> ConstParticleSystemFactoryPtr;

class ParticleSystemFactory
{
public:
	static ConstParticleSystemFactoryPtr create_factory(variant node);
	
	virtual ~ParticleSystemFactory();
	virtual ParticleSystemPtr create(const Entity& e) const = 0;
};

class ParticleSystem : public game_logic::FormulaCallable, public KRE::SceneObject
{
public:
	virtual ~ParticleSystem();
	virtual bool isDestroyed() const { return false; }
	virtual bool shouldSave() const { return true; }
	virtual void process(const Entity& e) = 0;
	virtual void draw(const KRE::WindowPtr& wm, const rect& area, const Entity& e) const = 0;

	void setType(const std::string& type) { type_ = type; }
	const std::string& type() const { return type_; }
protected:
	ParticleSystem() : SceneObject("ParticleSystem") {}
private:
	DECLARE_CALLABLE(ParticleSystem);
	std::string type_;
};
