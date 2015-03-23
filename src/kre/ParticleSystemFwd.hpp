/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include <cmath>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/perpendicular.hpp>

#include "variant.hpp"

namespace KRE
{
	namespace Particles
	{
		class EmitObject;
		typedef std::shared_ptr<EmitObject> EmitObjectPtr;
		class ParticleSystemContainer;
		typedef std::shared_ptr<ParticleSystemContainer> ParticleSystemContainerPtr;
		class ParticleSystem;
		typedef std::shared_ptr<ParticleSystem> ParticleSystemPtr;
		class Technique;
		typedef std::shared_ptr<Technique> TechniquePtr;
		class Parameter;
		typedef std::shared_ptr<Parameter> ParameterPtr;
		class Emitter;
		typedef std::shared_ptr<Emitter> EmitterPtr;
		class Affector;
		typedef std::shared_ptr<Affector> AffectorPtr;

		float get_random_float(float min = 0.0f, float max = 1.0f);
	}
}
