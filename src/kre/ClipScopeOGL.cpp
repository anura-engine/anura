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

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Color.hpp"
#include "ClipScopeOGL.hpp"
#include "ShadersOGL.hpp"
#include "StencilScopeOGL.hpp"

namespace KRE
{
	ClipScopeOGL::ClipScopeOGL(const rect& r)
		: ClipScope(r)
	{
	}

	ClipScopeOGL::~ClipScopeOGL()
	{
	}

	void ClipScopeOGL::apply() const 
	{
		StencilScopeOGL stencil_scope(StencilSettings(true, 
			StencilFace::FRONT_AND_BACK, 
			StencilFunc::NEVER, 
			0xff,
			0x01,
			0x01,
			StencilOperation::REPLACE,
			StencilOperation::KEEP,
			StencilOperation::KEEP)); 
		
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glClear(GL_STENCIL_BUFFER_BIT);

		const float varray[] = {
			area().x(), area().y(),
			area().x2(), area().y(),
			area().x(), area().y2(),
			area().x2(), area().y2(),
		};

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3((area().x()+area().x2())/2.0f,(area().y()+area().y2())/2.0f,0.0f)) 
			* glm::translate(glm::mat4(1.0f), glm::vec3(-(area().x()+area().y())/2.0f,-(area().y()+area().y())/2.0f,0.0f));
		glm::mat4 mvp = /*mvp_ **/ model;
		static OpenGL::ShaderProgramPtr shader = OpenGL::ShaderProgram::factory("simple");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));
		shader->setUniformValue(shader->getColorUniform(), Color::colorWhite().asFloatVector());

		glEnableVertexAttribArray(shader->getVertexAttribute()->second.location);
		glVertexAttribPointer(shader->getVertexAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}

	void ClipScopeOGL::clear() const 
	{
	}
}
