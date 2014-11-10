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

#include "ParticleSystemFwd.hpp"

namespace KRE
{
	namespace Particles
	{
		enum class ParameterType {
			FIXED,
			RANDOM,
			CURVED,
			OSCILLATE,
		};

		// Multi-valued parameter.
		class Parameter
		{
		public:
			explicit Parameter(ParameterType t) : type_(t) {}

			virtual float getValue(float t=1.0f) = 0;
			static ParameterPtr factory(const variant& node);

			ParameterType type() { return type_; }
		protected:
			Parameter();
			virtual ~Parameter();
		private:
			ParameterType type_;
			Parameter(const Parameter&);
		};

		class FixedParameter : public Parameter
		{
		public:
			FixedParameter(float value);
			FixedParameter(const variant& node);
			virtual ~FixedParameter();

			virtual float getValue(float t) {
				return value_;
			}
		private:
			float value_;
			FixedParameter(const FixedParameter&);
		};
	}
}
