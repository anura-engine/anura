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

#include <GL/glew.h>

#include <stack>

#include "../asserts.hpp"
#include "BlendOGL.hpp"

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
				default: break;
			}
			ASSERT_LOG(false, "Unrecognised blend equation");
			return GL_FUNC_ADD;
		}

		GLenum convert_blend_mode(BlendModeConstants bm)
		{
			switch(bm) {
				case BlendModeConstants::BM_ZERO:					return GL_ZERO;
				case BlendModeConstants::BM_ONE:						return GL_ONE;
				case BlendModeConstants::BM_SRC_COLOR:				return GL_SRC_COLOR;
				case BlendModeConstants::BM_ONE_MINUS_SRC_COLOR:		return GL_ONE_MINUS_SRC_COLOR;
				case BlendModeConstants::BM_DST_COLOR:				return GL_DST_COLOR;
				case BlendModeConstants::BM_ONE_MINUS_DST_COLOR:		return GL_ONE_MINUS_DST_COLOR;
				case BlendModeConstants::BM_SRC_ALPHA:				return GL_SRC_ALPHA;
				case BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA:		return GL_ONE_MINUS_SRC_ALPHA;
				case BlendModeConstants::BM_DST_ALPHA:				return GL_DST_ALPHA;
				case BlendModeConstants::BM_ONE_MINUS_DST_ALPHA:		return GL_ONE_MINUS_DST_ALPHA;
				case BlendModeConstants::BM_CONSTANT_COLOR:			return GL_CONSTANT_COLOR;
				case BlendModeConstants::BM_ONE_MINUS_CONSTANT_COLOR:return GL_ONE_MINUS_CONSTANT_COLOR;
				case BlendModeConstants::BM_CONSTANT_ALPHA:			return GL_CONSTANT_ALPHA;
				case BlendModeConstants::BM_ONE_MINUS_CONSTANT_ALPHA:return GL_ONE_MINUS_CONSTANT_ALPHA;
			}
			ASSERT_LOG(false, "Unrecognised blend mode");
			return GL_ZERO;
		}

		std::stack<BlendMode>& get_mode_stack()
		{
			static std::stack<BlendMode> res;
			return res;
		}

		std::stack<BlendEquation>& get_equation_stack()
		{
			static std::stack<BlendEquation> res;
			return res;
		}
	}

	BlendEquationImplOGL::BlendEquationImplOGL()
	{
	}

	BlendEquationImplOGL::~BlendEquationImplOGL()
	{
	}

	void BlendEquationImplOGL::apply(const BlendEquation& eqn) const
	{
		if(eqn.getRgbEquation() != BlendEquationConstants::BE_ADD || eqn.getAlphaEquation() != BlendEquationConstants::BE_ADD) {
			if(get_equation_stack().empty()) {
				get_equation_stack().emplace(BlendEquationConstants::BE_ADD, BlendEquationConstants::BE_ADD);
			}
			get_equation_stack().emplace(eqn);
			glBlendEquationSeparate(convert_eqn(eqn.getRgbEquation()), convert_eqn(eqn.getAlphaEquation()));
		}
	}

	void BlendEquationImplOGL::clear(const BlendEquation& eqn) const
	{
		if(eqn.getRgbEquation() != BlendEquationConstants::BE_ADD || eqn.getAlphaEquation() != BlendEquationConstants::BE_ADD) {
			ASSERT_LOG(!get_equation_stack().empty(), "Something went badly wrong blend mode stack was empty.");
			get_equation_stack().pop();
			BlendEquation& eqn = get_equation_stack().top();
			glBlendEquationSeparate(convert_eqn(eqn.getRgbEquation()), convert_eqn(eqn.getAlphaEquation()));
		}
	}

	BlendModeManagerOGL::BlendModeManagerOGL(const BlendMode& bm)
		: blend_mode_(bm)
	{
		if(blend_mode_.src() != BlendModeConstants::BM_SRC_ALPHA 
			|| blend_mode_.dst() != BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA) {
			if(get_mode_stack().empty()) {
				get_mode_stack().emplace(BlendModeConstants::BM_SRC_ALPHA, BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA);
			}
			get_mode_stack().emplace(bm);
			glBlendFunc(convert_blend_mode(blend_mode_.src()), convert_blend_mode(blend_mode_.dst()));
		}
	}

	BlendModeManagerOGL::~BlendModeManagerOGL()
	{
		if(blend_mode_.src() != BlendModeConstants::BM_SRC_ALPHA 
			|| blend_mode_.dst() != BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA) {
			ASSERT_LOG(!get_mode_stack().empty(), "Something went badly wrong blend mode stack was empty.");
			get_mode_stack().pop();
			BlendMode& bm = get_mode_stack().top();
			glBlendFunc(convert_blend_mode(bm.src()), convert_blend_mode(bm.dst()));
		}
	}
}
