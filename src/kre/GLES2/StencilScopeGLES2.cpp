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

#include <GL/glew.h>

#include <stack>
#include "StencilScopeGLES2.hpp"

namespace KRE
{
	namespace
	{
		typedef std::stack<StencilSettings> StencilSettingsStack;
		StencilSettingsStack& get_stencil_stack()
		{
			static StencilSettingsStack res;
			return res;
		}

		GLenum convert_face(StencilFace face)
		{
			switch(face) {
				case StencilFace::FRONT:			return GL_FRONT;
				case StencilFace::BACK:				return GL_BACK;
				case StencilFace::FRONT_AND_BACK:	return GL_FRONT_AND_BACK;
				default: break;
			}
			return GL_FRONT;
		}

		GLenum convert_stencil_op(StencilOperation op)
		{
			switch(op) {
				case StencilOperation::KEEP:			return GL_KEEP;
				case StencilOperation::ZERO:			return GL_ZERO;
				case StencilOperation::REPLACE:			return GL_REPLACE;
				case StencilOperation::INCREMENT:		return GL_INCR;
				case StencilOperation::INCREMENT_WRAP:	return GL_INCR_WRAP;
				case StencilOperation::DECREMENT:		return GL_DECR;
				case StencilOperation::DECREMENT_WRAP:	return GL_DECR_WRAP;
				case StencilOperation::INVERT:			return GL_INVERT;
				default: break;
			}
			return GL_KEEP;
		}

		GLenum convert_func(StencilFunc func)
		{
			switch(func) {
				case StencilFunc::NEVER:					return GL_NEVER;
				case StencilFunc::LESS:						return GL_LESS;
				case StencilFunc::LESS_THAN_OR_EQUAL:		return GL_LEQUAL;
				case StencilFunc::GREATER:					return GL_GREATER;
				case StencilFunc::GREATER_THAN_OR_EQUAL:	return GL_GEQUAL;
				case StencilFunc::EQUAL:					return GL_EQUAL;
				case StencilFunc::NOT_EQUAL:				return GL_NOTEQUAL;
				case StencilFunc::ALWAYS:					return GL_ALWAYS;
				default: break;
			}
			return GL_ALWAYS;
		}
	}

	StencilScopeGLESv2::StencilScopeGLESv2(const StencilSettings& settings)
		: StencilScope(settings)
	{
		get_stencil_stack().emplace(settings);
		applySettings(settings);
	}

	StencilScopeGLESv2::~StencilScopeGLESv2()
	{
		get_stencil_stack().pop();
		if(get_stencil_stack().empty()) {
			glDisable(GL_STENCIL_TEST);
			glStencilMask(0);
		} else {
			applySettings(get_stencil_stack().top());
		}
	}

	void StencilScopeGLESv2::applySettings(const StencilSettings& settings)
	{
		if(settings.enabled()) {
			glEnable(GL_STENCIL_TEST);
			if(settings.face() == StencilFace::FRONT_AND_BACK) {
				glStencilOp(convert_stencil_op(settings.sfail()), convert_stencil_op(settings.dpfail()), convert_stencil_op(settings.dppass()));
				glStencilFunc(convert_func(settings.func()), settings.ref(), settings.ref_mask());
				glStencilMask(settings.mask());
			} else {
				glStencilOpSeparate(convert_face(settings.face()), 
					convert_stencil_op(settings.sfail()),
					convert_stencil_op(settings.dpfail()),
					convert_stencil_op(settings.dppass()));
				glStencilFuncSeparate(convert_face(settings.face()), 
					convert_func(settings.func()),
					settings.ref(),
					settings.ref_mask());
					glStencilMask(settings.mask());
				glStencilMaskSeparate(convert_face(settings.face()), settings.mask());
			}
		} else {
			glDisable(GL_STENCIL_TEST);
			glStencilMask(0);
		}
	}

	void StencilScopeGLESv2::handleUpdatedMask()
	{
		if(getSettings().enabled()) {
			if(getSettings().face() == StencilFace::FRONT_AND_BACK) {
				glStencilMask(getSettings().mask());
			} else {
				glStencilMaskSeparate(convert_face(getSettings().face()), getSettings().mask());
			}
		}
	}

	void StencilScopeGLESv2::handleUpdatedSettings()
	{
		get_stencil_stack().top() = getSettings();
		applySettings(getSettings());
	}
}
