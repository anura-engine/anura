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
#include "AttributeSet.hpp"
#include "CameraObject.hpp"
#include "Color.hpp"
#include "ClipScopeOGL.hpp"
#include "DisplayDeviceOGL.hpp"
#include "ModelMatrixScope.hpp"
#include "ShadersOGL.hpp"
#include "StencilScopeOGL.hpp"

namespace KRE
{
	ClipScopeOGL::ClipScopeOGL(const rect& r)
		: ClipScope(r),
		  stencil_scope_(nullptr)
	{
	}

	ClipScopeOGL::~ClipScopeOGL()
	{
		stencil_scope_.reset();
	}

	void ClipScopeOGL::apply(const CameraPtr& cam) const 
	{
		stencil_scope_.reset(new StencilScopeOGL(get_stencil_mask_settings()));

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
		
		static OpenGL::ShaderProgramPtr shader = OpenGL::ShaderProgram::factory("simple");
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

	void ClipScopeOGL::clear() const 
	{
		stencil_scope_.reset();
	}


	// shape scope
	ClipShapeScopeOGL::ClipShapeScopeOGL(const RenderablePtr& r)
		: ClipShapeScope(r),
		  stencil_scope_(nullptr)
	{
	}

	ClipShapeScopeOGL::~ClipShapeScopeOGL()
	{
		stencil_scope_.reset();
	}

	void ClipShapeScopeOGL::apply(const CameraPtr& cam) const 
	{
		stencil_scope_.reset(new StencilScopeOGL(get_stencil_mask_settings()));

		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);
		glClear(GL_STENCIL_BUFFER_BIT);
	
		CameraPtr clip_cam = cam;
		if(cam == nullptr) {
			clip_cam = DisplayDevice::getCurrent()->getDefaultCamera();
		}

		//glm::mat4 mvp = clip_cam->getProjectionMat() * clip_cam->getViewMat() * get_global_model_matrix();
		auto& clip_shape = getRenderable();
		clip_shape->setCamera(clip_cam);
		DisplayDeviceOpenGL::getCurrent()->render(clip_shape.get());
		clip_shape->setCamera(nullptr);

		stencil_scope_->applyNewSettings(get_stencil_keep_settings());

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
	}

	void ClipShapeScopeOGL::clear() const 
	{
		stencil_scope_.reset();
	}

}
