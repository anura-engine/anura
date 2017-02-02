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

			geometry::control_point_vector::const_iterator find_closest_point(const geometry::control_point_vector& control_points, float t)
			{
				// find nearest control point to t
				auto it = control_points.cbegin();
				for(; it != control_points.cend(); ++it) {
					if(t < it->first) {
						if(it == control_points.cbegin()) {
							return it;
						} else {
							return --it;
						}
					}
				}
				return --it;
			}
		}


		ParameterPtr Parameter::factory(const variant& node)
		{
			if(node.is_float() || node.is_int()) {
				// single fixed attribute
				return std::make_shared<Parameter>(node.as_float());
			}
			ASSERT_LOG(node.has_key("type"), "parameter must have 'type' attribute");
			const std::string& ntype = node["type"].as_string();

			geometry::control_point_vector cps;
			if(ntype == "curved_linear" || ntype == "curved_spline") {
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
					cps.emplace_back(p);
				}
			}

			if(ntype == "fixed") {
				return std::make_shared<Parameter>(node["value"].as_float());
			} else if(ntype == "random") {
				return std::make_shared<Parameter>(node["min"].as_float(), node["max"].as_float());
			} else if(ntype == "curved_linear") {
				return std::make_shared<Parameter>(InterpolationType::LINEAR, cps);
			} else if(ntype == "curved_spline") {
				return std::make_shared<Parameter>(InterpolationType::SPLINE, cps);
			} else if(ntype == "oscillate") {
				WaveType osc_type = WaveType::SINE;
				float freq = 1.0f;
				float base = 0.0f;
				float phase = 0.0f;
				float ampl = 0.0f;
				if(node.has_key("oscillate_frequency")) {
					freq = node["oscillate_frequency"].as_float();
				} 
				if(node.has_key("oscillate_phase")) {
					phase = node["oscillate_phase"].as_float();
				} 
				if(node.has_key("oscillate_base")) {
					base = node["oscillate_base"].as_float();
				} 
				if(node.has_key("oscillate_amplitude")) {
					ampl = node["oscillate_amplitude"].as_float();
				} 
				if(node.has_key("oscillate_type")) {
					const std::string& type = node["oscillate_type"].as_string();
					if(type == "sine" || type == "sin") {
						osc_type = WaveType::SINE;
					} else if(type == "square" || type == "sq") {
						osc_type = WaveType::SQUARE;
					} else {
						ASSERT_LOG(false, "unrecognised oscillate type: " << type);
					}
				}
				return std::make_shared<Parameter>(osc_type, freq, phase, base, ampl);
			} else {
				ASSERT_LOG(false, "Unrecognised parameter type: " << ntype);
			}
			return ParameterPtr();
		}

		Parameter::~Parameter()
		{
		}

		variant Parameter::write() const
		{
			variant_builder res;
			switch(type_) {
				case KRE::Particles::ParameterType::FIXED: {
					// Fixed parameters can be just returned as a single value.
					return variant(fixed_.value);
				}
				case KRE::Particles::ParameterType::RANDOM: {
					res.add("type", "random");
					res.add("min", random_.min_value);
					res.add("max", random_.max_value);
					break;
				}
				case KRE::Particles::ParameterType::CURVED_LINEAR: {
					res.add("type", "curved_linear");
					std::vector<variant> v;
					for(const auto& cp : curved_.control_points) {
						v.emplace_back(variant(cp.first));
						v.emplace_back(variant(cp.second));
						res.add("control_point", &v);
					}
					break;
				}
				case KRE::Particles::ParameterType::CURVED_SPLINE: {
					res.add("type", "curved_spline");
					std::vector<variant> v;
					for(const auto& cp : curved_.control_points) {
						v.emplace_back(variant(cp.first));
						v.emplace_back(variant(cp.second));
						res.add("control_point", &v);
					}
					break;
				}
				case KRE::Particles::ParameterType::OSCILLATE: {
					res.add("type", "oscillate");
					res.add("oscillate_type", oscillate_.osc_type == WaveType::SINE ? "sine" : "square");
					res.add("oscillate_frequency", oscillate_.frequency);
					res.add("oscillate_phase", oscillate_.phase);
					res.add("oscillate_base", oscillate_.base);
					res.add("oscillate_amplitude", oscillate_.amplitude);
					break;
				}
				default: 
					ASSERT_LOG(false, "Something went wrong with the parameter type: " << static_cast<int>(type_));
					break;
			}
			return res.build();
		}

		float Parameter::getValue(float t)
		{
			switch(type_) {
				case ParameterType::FIXED:
					return fixed_.value;

				case ParameterType::RANDOM:
					return get_random_float(random_.min_value, random_.max_value);

				case ParameterType::CURVED_LINEAR: {
					if(curved_.control_points.size() < 2) {
						return 0.0f;
					}
					auto it = find_closest_point(curved_.control_points, t);
					auto it2 = it + 1;
					if(it2 == curved_.control_points.end()) {
						return static_cast<float>(it2->second);
					} else {
						// linear interpolate, see http://en.wikipedia.org/wiki/Linear_interpolation
						return static_cast<float>(it->second + (it2->second - it->second) * (t - it->first) / (it2->first - it->first));
					}
				}
				case ParameterType::CURVED_SPLINE: {
					if(curved_.control_points.size() < 2) {
						return 0.0f;
					}
					// http://en.wikipedia.org/wiki/Spline_interpolation
					geometry::spline spl(curved_.control_points);
					return static_cast<float>(spl.interpolate(t));
				}
				case ParameterType::OSCILLATE:
					if(oscillate_.osc_type == WaveType::SINE) {
						return static_cast<float>(oscillate_.base + oscillate_.amplitude * sin(2*M_PI*oscillate_.frequency*t + oscillate_.phase));
					} else if(oscillate_.osc_type == WaveType::SQUARE) {
						return static_cast<float>(oscillate_.base + oscillate_.amplitude * sign(sin(2*M_PI*oscillate_.frequency*t + oscillate_.phase)));
					}
					return 0;
				default:
					ASSERT_LOG(false, "Unknown type for parameter: " << static_cast<int>(type_));
			}
			return 0.0f;
		}
	}
}
