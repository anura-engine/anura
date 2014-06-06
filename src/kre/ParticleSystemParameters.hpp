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
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include "ParticleSystemFwd.hpp"

namespace KRE
{
	namespace Particles
	{
		// Multi-valued parameter.
		class parameter
		{
		public:
			enum ParameterType {
				PARAMETER_FIXED,
				PARAMETER_RANDOM,
				PARAMETER_CURVED,
				PARAMETER_OSCILLATE,
			};

			explicit parameter(ParameterType t) : type_(t) {}

			virtual float get_value(float t) = 0;
			static parameter_ptr factory(const variant& node);

			ParameterType type() { return type_; }
		protected:
			parameter();
			virtual ~parameter();
		private:
			ParameterType type_;
			parameter(const parameter&);
		};

		class fixed_parameter : public parameter
		{
		public:
			fixed_parameter(float value);
			fixed_parameter(const variant& node);
			virtual ~fixed_parameter();

			virtual float get_value(float t) {
				return value_;
			}
		private:
			float value_;
			fixed_parameter(const fixed_parameter&);
		};
	}
}
