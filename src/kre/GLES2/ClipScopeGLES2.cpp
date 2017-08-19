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

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "AttributeSet.hpp"
#include "CameraObject.hpp"
#include "Color.hpp"
#include "ClipScopeGLES2.hpp"
#include "DisplayDevice.hpp"
#include "ModelMatrixScope.hpp"
#include "ShadersGLES2.hpp"
#include "StencilScopeGLES2.hpp"

namespace KRE
{
	ClipScopeGLESv2::ClipScopeGLESv2(const rect& r)
		: ClipScope(r),
		  stencil_scope_(nullptr)
	{
	}

	ClipScopeGLESv2::~ClipScopeGLESv2()
	{
		stencil_scope_.reset();
	}

	void ClipScopeGLESv2::apply(const CameraPtr& cam) const 
	{
		stencil_scope_.reset(new StencilScopeGLESv2(get_stencil_mask_settings()));

		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);
		glClear(GL_STENCIL_BUFFER_BIT);
	
		const float varray[] = {
			area().x(), area().y(),
			area().x2(), area().y(),
			area().x(), area().y2(),
			area().x2(), area().y2(),
		};
		
		CameraPtr clip_cam = cam;
		if(cam == nullptr) {
			clip_cam = DisplayDevice::getCurrent()->getDefaultCamera();
		}

		glm::mat4 mvp = clip_cam->getProjectionMat() * clip_cam->getViewMat() * get_global_model_matrix();
		
		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("simple");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));
		shader->setUniformValue(shader->getColorUniform(), Color::colorWhite().asFloatVector());

		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		stencil_scope_->applyNewSettings(get_stencil_keep_settings());

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
	}

	void ClipScopeGLESv2::clear() const 
	{
		stencil_scope_.reset();
	}
}
