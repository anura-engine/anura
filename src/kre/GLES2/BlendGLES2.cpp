/*
	Copyright (C) 2013-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "SDL_opengles2.h"

#include <stack>

#include "asserts.hpp"
#include "BlendModeScope.hpp"
#include "BlendGLES2.hpp"

namespace KRE
{
	namespace
	{
		GLenum convert_eqn(BlendEquationConstants eqn)
		{
			switch (eqn)
			{
				case BlendEquationConstants::BE_ADD:				return GL_FUNC_ADD;
				case BlendEquationConstants::BE_SUBTRACT:			return GL_FUNC_SUBTRACT;
				case BlendEquationConstants::BE_REVERSE_SUBTRACT:	return GL_FUNC_REVERSE_SUBTRACT;
				case BlendEquationConstants::BE_MIN:
					if(GL_EXT_blend_minmax) {
						return GL_MIN_EXT;
					}
					break;
				case BlendEquationConstants::BE_MAX:
					if(GL_EXT_blend_minmax) {
						return GL_MAX_EXT;
					}
					break;
				default: break;
			}
			ASSERT_LOG(false, "Unrecognised blend equation");
			return GL_FUNC_ADD;
		}

		GLenum convert_blend_mode(BlendModeConstants bm)
		{
			switch(bm) {
				case BlendModeConstants::BM_ZERO:						return GL_ZERO;
				case BlendModeConstants::BM_ONE:						return GL_ONE;
				case BlendModeConstants::BM_SRC_COLOR:					return GL_SRC_COLOR;
				case BlendModeConstants::BM_ONE_MINUS_SRC_COLOR:		return GL_ONE_MINUS_SRC_COLOR;
				case BlendModeConstants::BM_DST_COLOR:					return GL_DST_COLOR;
				case BlendModeConstants::BM_ONE_MINUS_DST_COLOR:		return GL_ONE_MINUS_DST_COLOR;
				case BlendModeConstants::BM_SRC_ALPHA:					return GL_SRC_ALPHA;
				case BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA:		return GL_ONE_MINUS_SRC_ALPHA;
				case BlendModeConstants::BM_DST_ALPHA:					return GL_DST_ALPHA;
				case BlendModeConstants::BM_ONE_MINUS_DST_ALPHA:		return GL_ONE_MINUS_DST_ALPHA;
				case BlendModeConstants::BM_CONSTANT_COLOR:				return GL_CONSTANT_COLOR;
				case BlendModeConstants::BM_ONE_MINUS_CONSTANT_COLOR:	return GL_ONE_MINUS_CONSTANT_COLOR;
				case BlendModeConstants::BM_CONSTANT_ALPHA:				return GL_CONSTANT_ALPHA;
				case BlendModeConstants::BM_ONE_MINUS_CONSTANT_ALPHA:	return GL_ONE_MINUS_CONSTANT_ALPHA;
			}
			ASSERT_LOG(false, "Unrecognised blend mode");
			return GL_ZERO;
		}

		std::stack<BlendEquation>& get_equation_stack()
		{
			static std::stack<BlendEquation> res;
			return res;
		}

		std::stack<BlendMode>& get_blend_mode_stack()
		{
			static std::stack<BlendMode> res;
			return res;
		}

		std::stack<bool>& get_blend_state_stack()
		{
			static std::stack<bool> res;
			return res;
		}
	}

	BlendEquationImplGLESv2::BlendEquationImplGLESv2()
	{
	}

	BlendEquationImplGLESv2::~BlendEquationImplGLESv2()
	{
	}

	void BlendEquationImplGLESv2::apply(const BlendEquation& eqn) const
	{
		if(eqn != BlendEquation()) {
			if(get_equation_stack().empty()) {
				get_equation_stack().emplace(BlendEquationConstants::BE_ADD, BlendEquationConstants::BE_ADD);
			}
			get_equation_stack().emplace(eqn);
			glBlendEquationSeparate(convert_eqn(eqn.getRgbEquation()), convert_eqn(eqn.getAlphaEquation()));
		}
	}

	void BlendEquationImplGLESv2::clear(const BlendEquation& eqn) const
	{
		if(eqn != BlendEquation()) {
			ASSERT_LOG(!get_equation_stack().empty(), "Something went badly wrong blend mode stack was empty.");
			get_equation_stack().pop();
			BlendEquation& eqn = get_equation_stack().top();
			glBlendEquationSeparate(convert_eqn(eqn.getRgbEquation()), convert_eqn(eqn.getAlphaEquation()));
		}
	}

	BlendEquationScopeGLESv2::BlendEquationScopeGLESv2(const ScopeableValue& sv)
		: stored_(false)
	{
		const BlendEquation& eqn = sv.getBlendEquation();
		if(sv.isBlendEquationSet() && eqn != BlendEquation()) {
			get_equation_stack().emplace(eqn);
			glBlendEquationSeparate(convert_eqn(eqn.getRgbEquation()), convert_eqn(eqn.getAlphaEquation()));
			stored_ = true;
		}
	}

	BlendEquationScopeGLESv2::~BlendEquationScopeGLESv2()
	{
		if(stored_) {
			ASSERT_LOG(!get_equation_stack().empty(), "Something went badly wrong blend equation stack was empty.");
			get_equation_stack().pop();
			if(!get_equation_stack().empty()) {
				BlendEquation& eqn = get_equation_stack().top();
				glBlendEquationSeparate(convert_eqn(eqn.getRgbEquation()), convert_eqn(eqn.getAlphaEquation()));
			} else {
				glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
			}
		}
	}

	BlendModeScopeGLESv2::BlendModeScopeGLESv2(const ScopeableValue& sv)
		: stored_(false),
		  state_stored_(false)
	{
		auto& bm = sv.getBlendMode();
		if(sv.isBlendStateSet()) {
			if(sv.isBlendEnabled()) {
				glEnable(GL_BLEND);
			} else {
				glDisable(GL_BLEND);
			}
			get_blend_state_stack().emplace(sv.isBlendEnabled());
			state_stored_ = true;
		}

		if(sv.isBlendModeSet() && bm != BlendMode()) {
			get_blend_mode_stack().emplace(bm);
			stored_ = true;
			glBlendFunc(convert_blend_mode(bm.src()), convert_blend_mode(bm.dst()));
		} else if(BlendModeScope::getCurrentMode() != BlendMode()) {
			auto& bm = BlendModeScope::getCurrentMode();
			get_blend_mode_stack().emplace(bm);
			stored_ = true;
			glBlendFunc(convert_blend_mode(bm.src()), convert_blend_mode(bm.dst()));
		}
	}

	BlendModeScopeGLESv2::~BlendModeScopeGLESv2()
	{
		if(stored_) {
			ASSERT_LOG(!get_blend_mode_stack().empty(), "Something went badly wrong blend mode stack was empty.");
			get_blend_mode_stack().pop();
			if(!get_blend_mode_stack().empty()) {
				BlendMode& bm = get_blend_mode_stack().top();
				glBlendFunc(convert_blend_mode(bm.src()), convert_blend_mode(bm.dst()));
			} else {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
		}

		if(state_stored_) {
			ASSERT_LOG(!get_blend_state_stack().empty(), "Something went badly wrong blend state stack was empty.");
			bool state = get_blend_state_stack().top();
			get_blend_state_stack().pop();
			if(!get_blend_state_stack().empty()) {
				if(get_blend_state_stack().top()) {
					glEnable(GL_BLEND);
				} else {
					glDisable(GL_BLEND);
				}
			} else {
				// We check this so we don't have extraneous blend enable calls.
				if(!state) {
					glEnable(GL_BLEND);
				}
			}
		}
	}
}
