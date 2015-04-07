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

#include "asserts.hpp"
#include "spline.hpp"
#include "ParticleSystemParameters.hpp"

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

namespace KRE
{
	namespace Particles
	{
		namespace
		{
			template<class T>
			T sign(T x)
			{
				if(x < 0) {
					return T(-1);
				} else if(x > 0) {
					return T(1);
				}
				return 0;
			}
		}

		class RandomParameter : public Parameter
		{
		public:
			RandomParameter(const variant& node);
			virtual ~RandomParameter();
			virtual float getValue(float t);
		private:
			float min_value_;
			float max_value_;
			RandomParameter(const RandomParameter&);
		};

		class OscillateParameter : public Parameter
		{
		public:
			OscillateParameter(const variant& node);
			virtual ~OscillateParameter();
			virtual float getValue(float t);
		private:
			enum class WaveType {
				SINE,
				SQUARE,
			};
			WaveType osc_type_;
			float frequency_;
			float phase_;
			float base_;
			float amplitude_;
			OscillateParameter(const OscillateParameter&);
		};

		class CurvedParameter : public Parameter
		{
		public:
			enum class InterpolationType {
				LINEAR,
				SPLINE,
			};
			CurvedParameter(InterpolationType type, const variant& node);
			virtual ~CurvedParameter();
			virtual float getValue(float t);
		private:
			InterpolationType curve_type_;
			geometry::control_point_vector control_points_;

			geometry::control_point_vector::iterator findClosestPoint(float t);
			CurvedParameter(const CurvedParameter&);
		};

		ParameterPtr Parameter::factory(const variant& node)
		{
			if(node.is_float() || node.is_int()) {
				// single fixed attribute
				return ParameterPtr(new FixedParameter(float(node.as_float())));
			}
			ASSERT_LOG(node.has_key("type"), "parameter must have 'type' attribute");
			const std::string& ntype = node["type"].as_string();
			if(ntype == "fixed") {
				return ParameterPtr(new FixedParameter(node));
			} else if(ntype == "dyn_random") {
				return ParameterPtr(new RandomParameter(node));
			} else if(ntype == "dyn_curved_linear") {
				return ParameterPtr(new CurvedParameter(CurvedParameter::InterpolationType::LINEAR, node));
			} else if(ntype == "dyn_curved_spline") {
				return ParameterPtr(new CurvedParameter(CurvedParameter::InterpolationType::SPLINE, node));
			} else if(ntype == "dyn_oscillate") {
				return ParameterPtr(new OscillateParameter(node));
			} else {
				ASSERT_LOG(false, "Unrecognised affector type: " << ntype);
			}
			return ParameterPtr();
		}

		Parameter::Parameter()
		{
		}

		Parameter::~Parameter()
		{
		}

		FixedParameter::FixedParameter(float value)
			: Parameter(ParameterType::FIXED), 
			  value_(value)
		{
		}

		FixedParameter::FixedParameter(const variant& node)
		{
			value_ = node["value"].as_float();
		}

		FixedParameter::~FixedParameter()
		{
		}

		RandomParameter::RandomParameter(const variant& node)
			: Parameter(ParameterType::RANDOM), 
			min_value_(node["min"].as_float(0.1f)),
			max_value_(node["max"].as_float(1.0f))
		{
		}

		RandomParameter::~RandomParameter()
		{
		}

		float RandomParameter::getValue(float t)
		{
			return get_random_float(min_value_, max_value_);
		}

		OscillateParameter::OscillateParameter(const variant& node)
			: Parameter(ParameterType::OSCILLATE), 
			  frequency_(1.0f), 
			  phase_(0.0f), 
			  base_(0.0f), 
			  amplitude_(1.0f),
			  osc_type_(WaveType::SINE)
		{
			if(node.has_key("oscillate_frequency")) {
				frequency_ = node["oscillate_frequency"].as_float();
			} 
			if(node.has_key("oscillate_phase")) {
				phase_ = node["oscillate_phase"].as_float();
			} 
			if(node.has_key("oscillate_base")) {
				base_ = node["oscillate_base"].as_float();
			} 
			if(node.has_key("oscillate_amplitude")) {
				amplitude_ = node["oscillate_amplitude"].as_float();
			} 
			if(node.has_key("oscillate_type")) {
				const std::string& type = node["oscillate_type"].as_string();
				if(type == "sine" || type == "sin") {
					osc_type_ = WaveType::SINE;
				} else if(type == "square" || type == "sq") {
					osc_type_ = WaveType::SQUARE;
				} else {
					ASSERT_LOG(false, "unrecognised oscillate type: " << type);
				}
			}             
		}

		OscillateParameter::~OscillateParameter()
		{
		}

		float OscillateParameter::getValue(float t)
		{
			if(osc_type_ == WaveType::SINE) {
				return float(base_ + amplitude_ * sin(2*M_PI*frequency_*t + phase_));
			} else if(osc_type_ == WaveType::SQUARE) {
				return float(base_ + amplitude_ * sign(sin(2*M_PI*frequency_*t + phase_)));
			}
			return 0;
		}

		CurvedParameter::CurvedParameter(InterpolationType type, const variant& node)
			: Parameter(ParameterType::CURVED), 
			  curve_type_(type)
		{
			ASSERT_LOG(node.has_key("control_point") 
				&& node["control_point"].is_list()
				&& node["control_point"].num_elements() >= 2, 
				"curved parameters must have at least 2 control points.");
			for(size_t n = 0; n != node["control_point"].num_elements(); ++n) {
				ASSERT_LOG(node["control_point"][n].is_list() 
					&& node["control_point"][n].num_elements() == 2,
					"Control points should be list of two elements.");
				auto p = std::make_pair(node["control_point"][n][0].as_float(), 
					node["control_point"][n][1].as_float());
				control_points_.push_back(p);
			}
		}

		CurvedParameter::~CurvedParameter()
		{
		}

		geometry::control_point_vector::iterator CurvedParameter::findClosestPoint(float t)
		{
			// find nearest control point to t
			auto it = control_points_.begin();
			for(; it != control_points_.end(); ++it) {
				if(t < it->first) {
					if(it == control_points_.begin()) {
						return it;
					} else {
						return --it;
					}
				}
			}
			return --it;
		}

		float CurvedParameter::getValue(float t)
		{
			if(curve_type_ == InterpolationType::LINEAR) {
				auto it = findClosestPoint(t);
				auto it2 = it + 1;
				if(it2 == control_points_.end()) {
					return float(it2->second);
				} else {
					// linear interpolate, see http://en.wikipedia.org/wiki/Linear_interpolation
					return float(it->second + (it2->second - it->second) * (t - it->first) / (it2->first - it->first));
				}
			} else if(curve_type_ == InterpolationType::SPLINE) {
				// http://en.wikipedia.org/wiki/Spline_interpolation
				geometry::spline spl(control_points_);
				return float(spl.interpolate(t));
			}
			return 0;
		}
	}
}
