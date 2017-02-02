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
#include "spline.hpp"

namespace KRE
{
	namespace Particles
	{
		enum class InterpolationType {
			LINEAR,
			SPLINE,
		};

		enum class ParameterType {
			FIXED,
			RANDOM,
			CURVED_LINEAR,
			CURVED_SPLINE,
			OSCILLATE,
		};

		enum class WaveType {
			SINE,
			SQUARE,
		};

		struct FixedParams 
		{
			FixedParams() : value(0.0f) {}
			FixedParams(float v) : value(v) {}
			float value;
		};
		struct RandomParams
		{
			RandomParams() : min_value(0.0f), max_value(0.0f) {}
			RandomParams(float mnv, float mxv) 
				: min_value(mnv), max_value(mxv) 
			{
				if(min_value > max_value) {
					max_value = min_value;
				}
				if(max_value < min_value) {
					min_value = max_value;
				}
			}
			void operator=(RandomParams const& rhs) {
				min_value = rhs.min_value;
				max_value = rhs.max_value;
				if(min_value > max_value) {
					max_value = min_value;
				}
				if(max_value < min_value) {
					min_value = max_value;
				}
			}
			float min_value;
			float max_value;
		};
		struct CurvedParams
		{
			CurvedParams() : control_points() {}
			CurvedParams(const geometry::control_point_vector& cps) : control_points(cps) {}
			geometry::control_point_vector control_points;
		};
		struct OscillationParams
		{
			OscillationParams() : osc_type(WaveType::SINE), frequency(1.0f), phase(0.0f), base(0.0f), amplitude(1.0f) {}
			OscillationParams(WaveType ot, float f, float ph, float bas, float ampl) 
				: osc_type(ot),
				  frequency(f),
				  phase(ph),
				  base(bas),
				  amplitude(ampl)
			{}
			WaveType osc_type;
			float frequency;
			float phase;
			float base;
			float amplitude;
		};

		// Multi-valued parameter.
		class Parameter
		{
		public:
			explicit Parameter(float value) : type_(ParameterType::FIXED), fixed_(value), random_(), curved_(), oscillate_() {}
			explicit Parameter(float minvalue, float maxvalue) : type_(ParameterType::RANDOM), fixed_(), random_(minvalue, maxvalue), curved_(), oscillate_() {}
			explicit Parameter(InterpolationType it, const geometry::control_point_vector& cps) : type_(it == InterpolationType::LINEAR ? ParameterType::CURVED_LINEAR : ParameterType::CURVED_SPLINE), fixed_(), random_(), curved_(cps), oscillate_() {}
			explicit Parameter(WaveType ot, float f, float ph, float bas, float ampl) : type_(ParameterType::OSCILLATE), fixed_(), random_(), curved_(), oscillate_(ot, f, ph, bas, ampl) {}
			virtual ~Parameter();

			float getValue(float t=1.0f);
			static ParameterPtr factory(const variant& node);

			void setType(ParameterType type) { type_ = type; }
			ParameterType getType() const { return type_; }

			void getFixedValue(FixedParams* value) const { *value = fixed_; }
			void getRandomRange(RandomParams* value) { *value = random_; }
			void getCurvedParams(CurvedParams* cp) const { *cp = curved_; }
			void getOscillation(OscillationParams* op) const { *op = oscillate_; }

			void setFixedValue(const FixedParams& fp) {
				type_ = ParameterType::FIXED;
				fixed_ = fp;
			}
			void setRandomRange(const RandomParams& rp) {
				type_ = ParameterType::RANDOM;
				random_ = rp;
			}
			void setControlPoints(InterpolationType it, const CurvedParams& cp) {
				type_ = it == InterpolationType::LINEAR ? ParameterType::CURVED_LINEAR : ParameterType::CURVED_SPLINE;
				curved_ = cp;
			}
			void setOscillation(const OscillationParams& op) {
				type_ = ParameterType::OSCILLATE;
				oscillate_ = op;
			}
			variant write() const;
		private:
			ParameterType type_;
			// fixed
			FixedParams fixed_;

			//random
			RandomParams random_;

			// oscillate
			OscillationParams oscillate_;

			// curved, linear & spline
			CurvedParams curved_;

			Parameter() = delete;
			Parameter(const Parameter&) = delete;
		};
	}
}
