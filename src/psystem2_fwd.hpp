/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>

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

#include <cmath>
#include <memory>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/perpendicular.hpp>

#include "variant.hpp"

namespace graphics
{
	namespace particles
	{
		class particle_system_widget;
		typedef boost::intrusive_ptr<particle_system_widget> particle_system_widget_ptr;

		class emit_object;
		typedef boost::intrusive_ptr<emit_object> emit_object_ptr;
		class particle_system_container;
		class particle_system;
		typedef boost::intrusive_ptr<particle_system> particle_system_ptr;
		class technique;
		typedef boost::intrusive_ptr<technique> technique_ptr;
		class material;
		typedef std::shared_ptr<material> material_ptr;
		class parameter;
		typedef std::shared_ptr<parameter> parameter_ptr;
		class emitter;
		typedef boost::intrusive_ptr<emitter> emitter_ptr;
		class affector;
		typedef boost::intrusive_ptr<affector> affector_ptr;

		float get_random_float(float min = 0.0f, float max = 1.0f);
	}
}
