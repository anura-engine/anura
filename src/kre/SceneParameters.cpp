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

#include <chrono>
#include <random>

#include "asserts.hpp"
#include "spline.hpp"
#include "SceneParameters.hpp"

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

namespace KRE
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
		
		std::default_random_engine& get_rng_engine() 
		{
			static std::unique_ptr<std::default_random_engine> res;
			if(res == nullptr) {
				auto seed = std::chrono::system_clock::now().time_since_epoch().count();
				res.reset(new std::default_random_engine((std::default_random_engine::result_type)seed));
			}
			return *res;
		}

		float get_random_float(float min = 0.0f, float max = 1.0f)
		{
			std::uniform_real_distribution<float> gen(min, max);
			return gen(get_rng_engine());
		}
	}

	/*ParameterPtr Parameter::factory(const variant& node)
	{
		if(node.is_decimal() || node.is_int()) {
			// single fixed attribute
			return ParameterPtr(new fixed_parameter(float(node.as_decimal().as_float())));
		}
		ASSERT_LOG(node.has_key("type"), "parameter must have 'type' attribute");
		const std::string& ntype = node["type"].as_string();
		if(ntype == "fixed") {
			return ParameterPtr(new fixed_parameter(node));
		} else if(ntype == "dyn_random") {
			return ParameterPtr(new random_parameter(node));
		} else if(ntype == "dyn_curved_linear") {
			return ParameterPtr(new curved_parameter(curved_parameter::INTERPOLATE_LINEAR, node));
		} else if(ntype == "dyn_curved_spline") {
			return ParameterPtr(new curved_parameter(curved_parameter::INTERPOLATE_SPLINE, node));
		} else if(ntype == "dyn_oscillate") {
			return ParameterPtr(new oscillate_parameter(node));
		} else {
			ASSERT_LOG(false, "Unrecognised parameter type: " << ntype);
		}
		return ParameterPtr();
	}*/

	ParameterPtr Parameter::factory(float v)
	{
		return ParameterPtr(new FixedParameter(v));
	}

	ParameterPtr Parameter::factory(float mn, float mx)
	{
		return ParameterPtr(new RandomParameter(mn,mx));
	}

	ParameterPtr Parameter::factory(const std::string& s, float freq, float phase, float base, float amplitude)
	{
		return ParameterPtr(new OscillateParameter(s,freq,phase,base,amplitude));
	}

	ParameterPtr Parameter::factory(const std::string& s, const geometry::control_point_vector& control_points)
	{
		return ParameterPtr(new CurvedParameter(s,control_points));
	}

	Parameter::Parameter()
	{
	}

	Parameter::~Parameter()
	{
	}

	FixedParameter::FixedParameter(float value)
		: Parameter(PARAMETER_FIXED), value_(value)
	{
	}

	/*FixedParameter::FixedParameter(const variant& node)
	{
		value_ = node["value"].as_decimal().as_float();
	}*/

	FixedParameter::~FixedParameter()
	{
	}

	/*RandomParameter::RandomParameter(const variant& node)
		: Parameter(PARAMETER_RANDOM), 
		min_value_(node["min"].as_decimal(decimal(0.1)).as_float()),
		max_value_(node["max"].as_decimal(decimal(1.0)).as_float())
	{
	}*/

	RandomParameter::RandomParameter(float mnv, float mxv)
		: Parameter(PARAMETER_RANDOM), 
		min_value_(mnv),
		max_value_(mxv)
	{
	}

	RandomParameter::~RandomParameter()
	{
	}

	float RandomParameter::get_value(float t)
	{
		return get_random_float(min_value_, max_value_);
	}

	/*OscillateParameter::OscillateParameter(const variant& node)
		: Parameter(PARAMETER_OSCILLATE), 
		frequency_(1.0f), phase_(0.0f), base_(0.0f), amplitude_(1.0f),
		osc_type_(TYPE_SINE)
	{
		if(node.has_key("oscillate_frequency")) {
			frequency_ = node["oscillate_frequency"].as_decimal().as_float();
		} 
		if(node.has_key("oscillate_phase")) {
			phase_ = node["oscillate_phase"].as_decimal().as_float();
		} 
		if(node.has_key("oscillate_base")) {
			base_ = node["oscillate_base"].as_decimal().as_float();
		} 
		if(node.has_key("oscillate_amplitude")) {
			amplitude_ = node["oscillate_amplitude"].as_decimal().as_float();
		} 
		if(node.has_key("oscillate_type")) {
			const std::string& type = node["oscillate_type"].as_string();
			if(type == "sine") {
				osc_type_ = TYPE_SINE;
			} else if(type == "square") {
				osc_type_ = TYPE_SQUARE;
			} else {
				ASSERT_LOG(false, "unrecognised oscillate type: " << type);
			}
		}             
	}*/

	OscillateParameter::OscillateParameter(const std::string& s, float freq, float phase, float base, float amplitude)
		: Parameter(PARAMETER_OSCILLATE),
		frequency_(freq),
		phase_(phase),
		base_(base),
		amplitude_(amplitude)
	{
		if(s == "sine") {
			osc_type_ = TYPE_SINE;
		} else if(s == "square") {
			osc_type_ = TYPE_SQUARE;
		} else {
			ASSERT_LOG(false, "Unrecognised oscillate type: " << s);
		}
	}

	OscillateParameter::~OscillateParameter()
	{
	}

	float OscillateParameter::get_value(float t)
	{
		if(osc_type_ == TYPE_SINE) {
			return float(base_ + amplitude_ * sin(2*M_PI*frequency_*t + phase_));
		} else if(osc_type_ == TYPE_SQUARE) {
			return float(base_ + amplitude_ * sign(sin(2*M_PI*frequency_*t + phase_)));
		}
		return 0;
	}

	/*CurvedParameter::CurvedParameter(InterpolationType type, const variant& node)
		: Parameter(PARAMETER_OSCILLATE), curve_type_(type)
	{
		ASSERT_LOG(node.has_key("control_point") 
			&& node["control_point"].is_list()
			&& node["control_point"].num_elements() >= 2, 
			"curved parameters must have at least 2 control points.");
		for(size_t n = 0; n != node["control_point"].num_elements(); ++n) {
			ASSERT_LOG(node["control_point"][n].is_list() 
				&& node["control_point"][n].num_elements() == 2,
				"Control points should be list of two elements.");
			auto p = std::make_pair(node["control_point"][n][0].as_decimal().as_float(), 
				node["control_point"][n][1].as_decimal().as_float());
			control_points_.push_back(p);
		}
	}*/

	CurvedParameter::CurvedParameter(const std::string& s, const geometry::control_point_vector& control_points)
		: Parameter(PARAMETER_CURVED),
		curve_type_(INTERPOLATE_LINEAR),
		control_points_(control_points)
	{
		if(s == "linear") {
			curve_type_ = INTERPOLATE_LINEAR;
		} else if(s == "spline") {
			curve_type_ = INTERPOLATE_SPLINE;
		} else {
			ASSERT_LOG(false, "Unrecognised parameter type: " << s);
		}
	}

	CurvedParameter::~CurvedParameter()
	{
	}

	geometry::control_point_vector::iterator CurvedParameter::find_closest_point(float t)
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

	float CurvedParameter::get_value(float t)
	{
		if(curve_type_ == INTERPOLATE_LINEAR) {
			auto it = find_closest_point(t);
			auto it2 = it + 1;
			if(it2 == control_points_.end()) {
				return float(it2->second);
			} else {
				// linear interpolate, see http://en.wikipedia.org/wiki/Linear_interpolation
				return float(it->second + (it2->second - it->second) * (t - it->first) / (it2->first - it->first));
			}
		} else if(curve_type_ == INTERPOLATE_SPLINE) {
			// http://en.wikipedia.org/wiki/Spline_interpolation
			geometry::spline spl(control_points_);
			return float(spl.interpolate(t));
		}
		return 0.0f;
	}
}
