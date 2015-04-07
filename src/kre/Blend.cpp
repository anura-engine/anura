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
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include "asserts.hpp"
#include "Blend.hpp"
#include "DisplayDevice.hpp"

namespace KRE
{
	namespace
	{
		BlendModeConstants parse_blend_string(const std::string& s)
		{
			if(s == "zero") {
				return BlendModeConstants::BM_ZERO;
			} else if(s == "one") {
				return BlendModeConstants::BM_ONE;
			} else if(s == "src_color") {
				return BlendModeConstants::BM_SRC_COLOR;
			} else if(s == "one_minus_src_color") {
				return BlendModeConstants::BM_ONE_MINUS_SRC_COLOR;
			} else if(s == "dst_color") {
				return BlendModeConstants::BM_DST_COLOR;
			} else if(s == "one_minus_dst_color") {
				return BlendModeConstants::BM_ONE_MINUS_DST_COLOR;
			} else if(s == "src_alpha") {
				return BlendModeConstants::BM_SRC_ALPHA;
			} else if(s == "one_minus_src_alpha") {
				return BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA;
			} else if(s == "dst_alpha") {
				return BlendModeConstants::BM_DST_ALPHA;
			} else if(s == "one_minus_dst_alpha") {
				return BlendModeConstants::BM_ONE_MINUS_DST_ALPHA;
			} else if(s == "const_color") {
				return BlendModeConstants::BM_CONSTANT_COLOR;
			} else if(s == "one_minus_const_color") {
				return BlendModeConstants::BM_ONE_MINUS_CONSTANT_COLOR;
			} else if(s == "const_alpha") {
				return BlendModeConstants::BM_CONSTANT_ALPHA;
			} else if(s == "one_minus_const_alpha") {
				return BlendModeConstants::BM_ONE_MINUS_CONSTANT_ALPHA;
			} else {
				ASSERT_LOG(false, "parse_blend_string: Unrecognised value: " << s);
			}
			return BlendModeConstants::BM_ZERO;
		}

		std::string blend_mode_to_string(BlendModeConstants bmc)
		{
			switch (bmc)
			{
				case BlendModeConstants::BM_ZERO:						return "zero";
				case BlendModeConstants::BM_ONE:						return "one";
				case BlendModeConstants::BM_SRC_COLOR:					return "src_color";
				case BlendModeConstants::BM_ONE_MINUS_SRC_COLOR:		return "one_minus_src_color";
				case BlendModeConstants::BM_DST_COLOR:					return "dst_color";
				case BlendModeConstants::BM_ONE_MINUS_DST_COLOR:		return "one_minus_dst_color";
				case BlendModeConstants::BM_SRC_ALPHA:					return "src_alpha";
				case BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA:		return "one_minus_src_alpha";
				case BlendModeConstants::BM_DST_ALPHA:					return "dst_alpha";
				case BlendModeConstants::BM_ONE_MINUS_DST_ALPHA:		return "one_minus_dst_alpha";
				case BlendModeConstants::BM_CONSTANT_COLOR:				return "const_color";
				case BlendModeConstants::BM_ONE_MINUS_CONSTANT_COLOR:	return "one_minus_const_color";
				case BlendModeConstants::BM_CONSTANT_ALPHA:				return "const_alpha";
				case BlendModeConstants::BM_ONE_MINUS_CONSTANT_ALPHA:	return "one_minus_const_alpha";
				default: break;
			}
			ASSERT_LOG(false, "Unrecognised BlendModeConstants: " << static_cast<int>(bmc));
			return "";
		}

		std::string blend_equation_to_string(BlendEquationConstants bec)
		{
			switch (bec)
			{
				case BlendEquationConstants::BE_ADD:				return "add";
				case BlendEquationConstants::BE_REVERSE_SUBTRACT:	return "reverse_subtract";
				case BlendEquationConstants::BE_SUBTRACT:			return "subtract";
				default: break;
			}
			ASSERT_LOG(false, "Unrecognised BlendEquationConstants: " << static_cast<int>(bec));
			return "";
		}
	}

	BlendMode::BlendMode(const variant& node)
		: src_(BlendModeConstants::BM_SRC_ALPHA), 
		  dst_(BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA)
	{
		set(node);
	}	

	void BlendMode::set(const variant& node) 
	{
		if(node.is_string()) {
			const std::string& blend = node.as_string();
			if(blend == "add") {
				set(BlendModeConstants::BM_ONE, BlendModeConstants::BM_ONE);
			} else if(blend == "alpha_blend") {
				set(BlendModeConstants::BM_SRC_ALPHA, BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA);
			} else if(blend == "colour_blend" || blend == "color_blend") {
				set(BlendModeConstants::BM_SRC_COLOR, BlendModeConstants::BM_ONE_MINUS_SRC_COLOR);
			} else if(blend == "modulate") {
				set(BlendModeConstants::BM_DST_COLOR, BlendModeConstants::BM_ZERO);
			} else if(blend == "src_colour one" || blend == "src_color one") {
				set(BlendModeConstants::BM_SRC_COLOR, BlendModeConstants::BM_ONE);
			} else if(blend == "src_colour zero" || blend == "src_color zero") {
				set(BlendModeConstants::BM_SRC_COLOR, BlendModeConstants::BM_ZERO);
			} else if(blend == "src_colour dest_colour" || blend == "src_color dest_color") {
				set(BlendModeConstants::BM_SRC_COLOR, BlendModeConstants::BM_DST_COLOR);
			} else if(blend == "dest_colour one" || blend == "dest_color one") {
				set(BlendModeConstants::BM_DST_COLOR, BlendModeConstants::BM_ONE);
			} else if(blend == "dest_colour src_colour" || blend == "dest_color src_color") {
				set(BlendModeConstants::BM_DST_COLOR, BlendModeConstants::BM_SRC_COLOR);
			} else {
				ASSERT_LOG(false, "BlendMode: Unrecognised scene_blend mode " << blend);
			}
		} else if(node.is_list() && node.num_elements() >= 2) {
			ASSERT_LOG(node[0].is_string() && node[1].is_string(), 
				"BlendMode: Blend mode must be specified by a list of two strings.");
			set(parse_blend_string(node[0].as_string()), parse_blend_string(node[1].as_string()));
		} else {
			ASSERT_LOG(false, "BlendMode: Setting blend requires either a string or a list of greater than two elements." << node.to_debug_string());
		}
	}
	
	variant BlendMode::write() const
	{
		if(src_ == BlendModeConstants::BM_ONE && dst_ == BlendModeConstants::BM_ONE) {
			return variant("add");
		} else if(src_ == BlendModeConstants::BM_SRC_ALPHA && dst_ == BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA) {
			return variant("alpha_blend");
		} else if(src_ == BlendModeConstants::BM_SRC_COLOR && dst_ == BlendModeConstants::BM_ONE_MINUS_SRC_COLOR) {
			return variant("color_blend");
		} else if(src_ == BlendModeConstants::BM_DST_COLOR && dst_ == BlendModeConstants::BM_ZERO) {
			return variant("modulate");
		}
		std::vector<variant> v;
		v.emplace_back(blend_mode_to_string(src_));
		v.emplace_back(blend_mode_to_string(dst_));
		return variant(&v);
	}

	BlendEquation::BlendEquation()
		: rgb_(BlendEquationConstants::BE_ADD),
		  alpha_(BlendEquationConstants::BE_ADD)
	{
	}

	BlendEquation::BlendEquation(BlendEquationConstants rgba_eq)
		: rgb_(rgba_eq),
		  alpha_(rgba_eq)
	{
	}

	BlendEquation::BlendEquation(BlendEquationConstants rgb_eq, BlendEquationConstants alpha_eq)
		: rgb_(rgb_eq),
		  alpha_(alpha_eq)
	{
	}

	namespace 
	{
		BlendEquationConstants convert_string_to_equation(const std::string& s) 
		{
			if(s == "add" || s == "ADD") {
				return BlendEquationConstants::BE_ADD;
			} else if(s == "subtract" || s == "SUBTRACT") {
				return BlendEquationConstants::BE_SUBTRACT;
			} else if(s == "reverse_subtract" || s == "REVERSE_SUBTRACT" 
				|| s == "rsubtract" || s == " RSUBSTRACT" || s == "reverseSubtract") {
				return BlendEquationConstants::BE_REVERSE_SUBTRACT;
			} else {
				ASSERT_LOG(false, "Unrecognised value for blend equation: " << s);
			}
			return BlendEquationConstants::BE_ADD;
		}
	}

	BlendEquation::BlendEquation(const variant& node)
		: rgb_(BlendEquationConstants::BE_ADD),
		  alpha_(BlendEquationConstants::BE_ADD)
	{
		if(node.is_map()) {
			if(node.has_key("rgba")) {
				rgb_ = alpha_ = convert_string_to_equation(node["rgba"].as_string());
			} 
			if(node.has_key("rgb")) {
				rgb_ = convert_string_to_equation(node["rgb"].as_string());
			}
			if(node.has_key("alpha")) {
				alpha_ = convert_string_to_equation(node["alpha"].as_string());
			}
			if(node.has_key("a")) {
				alpha_ = convert_string_to_equation(node["a"].as_string());
			}
		} else if(node.is_list()) {
			ASSERT_LOG(node.num_elements() > 0, "When using a list for blend equation must give at least one element");
			if(node.num_elements() == 1) {
				rgb_ = alpha_ = convert_string_to_equation(node[0].as_string());
			} else {
				rgb_   = convert_string_to_equation(node[0].as_string());
				alpha_ = convert_string_to_equation(node[1].as_string());
			}
		} else if(node.is_string()) {
			// simply setting the rgb/alpha values that same, from string
			rgb_ = alpha_ = convert_string_to_equation(node.as_string());
		} else {
			ASSERT_LOG(false, "Unrecognised type for blend equation: " << node.to_debug_string());
		}
	}

	void BlendEquation::setRgbEquation(BlendEquationConstants rgb_eq)
	{
		rgb_ = rgb_eq;
	}

	void BlendEquation::setAlphaEquation(BlendEquationConstants alpha_eq)
	{
		alpha_ = alpha_eq;
	}

	void BlendEquation::setEquation(BlendEquationConstants rgba_eq)
	{
		rgb_ = rgba_eq;
		alpha_ = rgba_eq;
	}

	BlendEquationConstants BlendEquation::getRgbEquation() const 
	{
		return rgb_;
	}

	BlendEquationConstants BlendEquation::getAlphaEquation() const 
	{
		return alpha_;
	}

	variant BlendEquation::write() const
	{
		if(rgb_ == alpha_) {
			//if(rgb_ == BlendEquationConstants::BE_ADD) {
			//	return variant();
			//}
			return variant(blend_equation_to_string(rgb_));
		}
		std::vector<variant> v;
		v.emplace_back(blend_equation_to_string(rgb_));
		v.emplace_back(blend_equation_to_string(alpha_));
		return variant(&v);
	}

	BlendEquation::Manager::Manager(const BlendEquation& eqn)
		: impl_(DisplayDevice::getCurrent()->getBlendEquationImpl()),
		  eqn_(eqn)
	{
		impl_->apply(eqn_);
	}

	BlendEquation::Manager::~Manager()
	{
		impl_->clear(eqn_);
	}

	BlendEquationImplBase::BlendEquationImplBase()
	{
	}

	BlendEquationImplBase::~BlendEquationImplBase()
	{
	}
}
