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

#include "asserts.hpp"
#include "spline.hpp"
#include "psystem2_parameters.hpp"

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

namespace graphics
{
	namespace particles
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

		class random_parameter : public parameter
		{
		public:
			random_parameter(const variant& node);
			virtual ~random_parameter();
			virtual float get_value(float t);
		private:
			float min_value_;
			float max_value_;
			random_parameter(const random_parameter&);
		};

		class oscillate_parameter : public parameter
		{
		public:
			oscillate_parameter(const variant& node);
			virtual ~oscillate_parameter();
			virtual float get_value(float t);
		private:
			enum WaveType {
				TYPE_SINE,
				TYPE_SQUARE,
			};
			WaveType osc_type_;
			float frequency_;
			float phase_;
			float base_;
			float amplitude_;
			oscillate_parameter(const oscillate_parameter&);
		};

		class curved_parameter : public parameter
		{
		public:
			enum InterpolationType {
				INTERPOLATE_LINEAR,
				INTERPOLATE_SPLINE,
			};
			curved_parameter(InterpolationType type, const variant& node);
			virtual ~curved_parameter();
			virtual float get_value(float t);
		private:
			InterpolationType curve_type_;
			geometry::control_point_vector control_points_;

			geometry::control_point_vector::iterator find_closest_point(float t);
			curved_parameter(const curved_parameter&);
		};

		parameter_ptr parameter::factory(const variant& node)
		{
			if(node.is_decimal() || node.is_int()) {
				// single fixed attribute
				return parameter_ptr(new fixed_parameter(float(node.as_decimal().as_float())));
			}
			ASSERT_LOG(node.has_key("type"), "FATAL: PSYSTEM2: parameter must have 'type' attribute");
			const std::string& ntype = node["type"].as_string();
			if(ntype == "fixed") {
				return parameter_ptr(new fixed_parameter(node));
			} else if(ntype == "dyn_random") {
				return parameter_ptr(new random_parameter(node));
			} else if(ntype == "dyn_curved_linear") {
				return parameter_ptr(new curved_parameter(curved_parameter::INTERPOLATE_LINEAR, node));
			} else if(ntype == "dyn_curved_spline") {
				return parameter_ptr(new curved_parameter(curved_parameter::INTERPOLATE_SPLINE, node));
			} else if(ntype == "dyn_oscillate") {
				return parameter_ptr(new oscillate_parameter(node));
			} else {
				ASSERT_LOG(false, "FATAL: PSYSTEM2: Unrecognised affector type: " << ntype);
			}
			return parameter_ptr();
		}

		parameter::parameter()
		{
		}

		parameter::~parameter()
		{
		}

		fixed_parameter::fixed_parameter(float value)
			: parameter(PARAMETER_FIXED), value_(value)
		{
		}

		fixed_parameter::fixed_parameter(const variant& node)
		{
			value_ = node["value"].as_decimal().as_float();
		}

		fixed_parameter::~fixed_parameter()
		{
		}

		random_parameter::random_parameter(const variant& node)
			: parameter(PARAMETER_RANDOM), 
			min_value_(node["min"].as_decimal(decimal(0.1)).as_float()),
			max_value_(node["max"].as_decimal(decimal(1.0)).as_float())
		{
		}

		random_parameter::~random_parameter()
		{
		}

		float random_parameter::get_value(float t)
		{
			return get_random_float(min_value_, max_value_);
		}

		oscillate_parameter::oscillate_parameter(const variant& node)
			: parameter(PARAMETER_OSCILLATE), 
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
					ASSERT_LOG(false, "FATAL: PSYSTEM2: unrecognised oscillate type: " << type);
				}
			}             
		}

		oscillate_parameter::~oscillate_parameter()
		{
		}

		float oscillate_parameter::get_value(float t)
		{
			if(osc_type_ == TYPE_SINE) {
				return base_ + amplitude_ * sin(2*M_PI*frequency_*t + phase_);
			} else if(osc_type_ == TYPE_SQUARE) {
				return base_ + amplitude_ * sign(sin(2*M_PI*frequency_*t + phase_));
			}
			return 0;
		}

		curved_parameter::curved_parameter(InterpolationType type, const variant& node)
			: parameter(PARAMETER_OSCILLATE), curve_type_(type)
		{
			ASSERT_LOG(node.has_key("control_point") 
				&& node["control_point"].is_list()
				&& node["control_point"].num_elements() >= 2, 
				"FATAL: PSYSTEM2: curved parameters must have at least 2 control points.");
			for(size_t n = 0; n != node["control_point"].num_elements(); ++n) {
				ASSERT_LOG(node["control_point"][n].is_list() 
					&& node["control_point"][n].num_elements() == 2,
					"FATAL: PSYSTEM2: Control points should be list of two elements.");
				auto p = std::make_pair(node["control_point"][n][0].as_decimal().as_float(), 
					node["control_point"][n][1].as_decimal().as_float());
				control_points_.push_back(p);
			}
		}

		curved_parameter::~curved_parameter()
		{
		}

		geometry::control_point_vector::iterator curved_parameter::find_closest_point(float t)
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

		float curved_parameter::get_value(float t)
		{
			if(curve_type_ == INTERPOLATE_LINEAR) {
				auto it = find_closest_point(t);
				auto it2 = it + 1;
				if(it2 == control_points_.end()) {
					return it2->second;
				} else {
					// linear interpolate, see http://en.wikipedia.org/wiki/Linear_interpolation
					return it->second + (it2->second - it->second) * (t - it->first) / (it2->first - it->first);
				}
			} else if(curve_type_ == INTERPOLATE_SPLINE) {
				// http://en.wikipedia.org/wiki/Spline_interpolation
				geometry::spline spl(control_points_);
				return spl.interpolate(t);
			}
			return 0;
		}
	}
}
