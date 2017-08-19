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
#include "ScissorGLES2.hpp"

namespace KRE
{
	namespace
	{
		typedef std::stack<rect> scissor_stack;
		scissor_stack& get_scissor_stack()
		{
			static scissor_stack res;
			return res;
		}
	}

	ScissorGLESv2::ScissorGLESv2(const rect& area)
		: Scissor(area)
	{
	}

	ScissorGLESv2::~ScissorGLESv2()
	{
	}

	void ScissorGLESv2::apply() 
	{
		if(get_scissor_stack().empty()) {
			glEnable(GL_SCISSOR_TEST);
		}
		glScissor(getArea().x(), getArea().y(), getArea().w(), getArea().h());
		get_scissor_stack().emplace(getArea());
	}

	void ScissorGLESv2::clear() 
	{
		get_scissor_stack().pop();
		if(get_scissor_stack().empty()) {
			glDisable(GL_SCISSOR_TEST);
		} else {
			const rect& r = get_scissor_stack().top();
			glScissor(r.x(), r.y(), r.w(), r.h());
		}
	}
}
