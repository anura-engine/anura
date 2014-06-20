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

#include "../asserts.hpp"
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
		}
	}

	void BlendMode::Set(const variant& node) 
	{
		if(node.is_string()) {
			const std::string& blend = node.as_string();
			if(blend == "add") {
				Set(BlendModeConstants::BM_ONE, BlendModeConstants::BM_ONE);
			} else if(blend == "alpha_blend") {
				Set(BlendModeConstants::BM_SRC_ALPHA, BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA);
			} else if(blend == "colour_blend") {
				Set(BlendModeConstants::BM_SRC_COLOR, BlendModeConstants::BM_ONE_MINUS_SRC_COLOR);
			} else if(blend == "modulate") {
				Set(BlendModeConstants::BM_DST_COLOR, BlendModeConstants::BM_ZERO);
			} else if(blend == "src_colour one") {
				Set(BlendModeConstants::BM_SRC_COLOR, BlendModeConstants::BM_ONE);
			} else if(blend == "src_colour zero") {
				Set(BlendModeConstants::BM_SRC_COLOR, BlendModeConstants::BM_ZERO);
			} else if(blend == "src_colour dest_colour") {
				Set(BlendModeConstants::BM_SRC_COLOR, BlendModeConstants::BM_DST_COLOR);
			} else if(blend == "dest_colour one") {
				Set(BlendModeConstants::BM_DST_COLOR, BlendModeConstants::BM_ONE);
			} else if(blend == "dest_colour src_colour") {
				Set(BlendModeConstants::BM_DST_COLOR, BlendModeConstants::BM_SRC_COLOR);
			} else {
				ASSERT_LOG(false, "BlendMode: Unrecognised scene_blend mode " << blend);
			}
		} else if(node.is_list() && node.num_elements() >= 2) {
			ASSERT_LOG(node[0].is_string() && node[1].is_string(), 
				"BlendMode: Blend mode must be specified by a list of two strings.");
			Set(parse_blend_string(node[0].as_string()), parse_blend_string(node[1].as_string()));
		} else {
			ASSERT_LOG(false, "BlendMode: Setting blend requires either a string or a list of greater than two elements." << node.to_debug_string());
		}
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

	BlendEquation::Manager::Manager(const BlendEquation& eqn)
		: impl_(DisplayDevice::GetCurrent()->getBlendEquationImpl()),
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
