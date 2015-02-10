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

#include "SceneFwd.hpp"

namespace KRE
{
	// Multi-valued parameter.
	class Parameter
	{
	public:
		enum ParameterType {
			PARAMETER_FIXED,
			PARAMETER_RANDOM,
			PARAMETER_CURVED,
			PARAMETER_OSCILLATE,
		};

		explicit Parameter(ParameterType t) : type_(t) {}

		virtual float get_value(float t) = 0;
		static ParameterPtr factory(float v);
		static ParameterPtr factory(float mnv, float mxv);
		static ParameterPtr factory(const std::string&, float freq=1.0f, float phase=0.0f, float base=0.0f, float amplitude=1.0f);
		static ParameterPtr factory(const std::string&, const geometry::control_point_vector& control_points);
		//static ParameterPtr factory(const variant& node);

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
		explicit FixedParameter(float value);
		//FixedParameter(const variant& node);
		virtual ~FixedParameter();

		float get_value(float t) {
			return value_;
		}		
	private:
		float value_;
		FixedParameter();
		FixedParameter(const FixedParameter&);
	};

	class RandomParameter : public Parameter
	{
	public:
		explicit RandomParameter(float mnv=0.0f, float mxv=1.0f);
		//RandomParameter(const variant& node);
		virtual ~RandomParameter();
		virtual float get_value(float t);
	private:
		float min_value_;
		float max_value_;
		RandomParameter(const RandomParameter&);
	};

	class OscillateParameter : public Parameter
	{
	public:
		explicit OscillateParameter(const std::string&, float freq=1.0f, float phase=0.0f, float base=0.0f, float amplitude=1.0f);
		//OscillateParameter(const variant& node);
		virtual ~OscillateParameter();
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
		OscillateParameter();
		OscillateParameter(const OscillateParameter&);
	};

	class CurvedParameter : public Parameter
	{
	public:
		enum InterpolationType {
			INTERPOLATE_LINEAR,
			INTERPOLATE_SPLINE,
		};
		explicit CurvedParameter(const std::string&, const geometry::control_point_vector& control_points);
		//CurvedParameter(InterpolationType type, const variant& node);
		virtual ~CurvedParameter();
		virtual float get_value(float t);
	private:
		InterpolationType curve_type_;
		geometry::control_point_vector control_points_;

		geometry::control_point_vector::iterator find_closest_point(float t);
		CurvedParameter();
		CurvedParameter(const CurvedParameter&);
	};
}
