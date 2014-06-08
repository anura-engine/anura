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

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "CanvasOGL.hpp"
#include "ShadersOpenGL.hpp"
#include "TextureOpenGL.hpp"

namespace KRE
{
	namespace
	{
		CanvasPtr& get_instance()
		{
			static CanvasPtr res = CanvasPtr(new CanvasOGL());
			return res;
		}
	}

	CanvasOGL::CanvasOGL()
	{
		handleDimensionsChanged();
	}

	CanvasOGL::~CanvasOGL()
	{
	}

	void CanvasOGL::handleDimensionsChanged()
	{
		mvp_ = glm::ortho(0.0f, float(width()), float(height()), 0.0f);
	}

	void CanvasOGL::blitTexture(const TexturePtr& tex, const rect& src, float rotation, const rect& dst, const ColorPtr& color) const
	{
		auto texture = std::dynamic_pointer_cast<OpenGLTexture>(tex);
		ASSERT_LOG(texture != NULL, "Texture passed in was not of expected type.");

		const float tx1 = float(src.x()) / texture->Width();
		const float ty1 = float(src.y()) / texture->Height();
		const float tx2 = src.w() == 0 ? 1.0f : float(src.x2()) / texture->Width();
		const float ty2 = src.h() == 0 ? 1.0f : float(src.y2()) / texture->Height();
		const float uv_coords[] = {
			tx1, ty1,
			tx2, ty1,
			tx1, ty2,
			tx2, ty2,
		};

		const float vx1 = float(dst.x());
		const float vy1 = float(dst.y());
		const float vx2 = float(dst.x2());
		const float vy2 = float(dst.y2());
		const float vtx_coords[] = {
			vx1, vy1,
			vx2, vy1,
			vx1, vy2,
			vx2, vy2,
		};

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3((vx1+vx2)/2.0f,(vy1+vy2)/2.0f,0.0f)) * glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f,0.0f,1.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-(vx1+vx2)/2.0f,-(vy1+vy2)/2.0f,0.0f));
		glm::mat4 mvp = mvp_ * model;
		auto shader = Shader::ShaderProgram::DefaultSystemShader();
		shader->MakeActive();
		texture->Bind();
		shader->SetUniformValue(shader->GetMvpUniform(), glm::value_ptr(mvp));
		if(color) {
			shader->SetUniformValue(shader->GetColorUniform(), color->asFloatVector());
		}
		shader->SetUniformValue(shader->GetTexMapUniform(), 0);
		// XXX the following line are only temporary, obviously.
		//shader->SetUniformValue(shader->GetUniformIterator("discard"), 0);
		glEnableVertexAttribArray(shader->GetVertexAttribute()->second.location);
		glVertexAttribPointer(shader->GetVertexAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
		glEnableVertexAttribArray(shader->GetTexcoordAttribute()->second.location);
		glVertexAttribPointer(shader->GetTexcoordAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, uv_coords);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glDisableVertexAttribArray(shader->GetTexcoordAttribute()->second.location);
		glDisableVertexAttribArray(shader->GetVertexAttribute()->second.location);
	}

	void CanvasOGL::blitTexture(const MaterialPtr& mat, float rotation, const rect& dst, const ColorPtr& color) const
	{
		ASSERT_LOG(mat != NULL, "Material was null");
		const float vx1 = float(dst.x());
		const float vy1 = float(dst.y());
		const float vx2 = float(dst.x2());
		const float vy2 = float(dst.y2());
		const float vtx_coords[] = {
			vx1, vy1,
			vx2, vy1,
			vx1, vy2,
			vx2, vy2,
		};

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3((vx1+vx2)/2.0f,(vy1+vy2)/2.0f,0.0f)) * glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f,0.0f,1.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-(vx1+vy1)/2.0f,-(vy1+vy1)/2.0f,0.0f));
		glm::mat4 mvp = mvp_ * model;
		auto shader = Shader::ShaderProgram::DefaultSystemShader();
		shader->MakeActive();
		shader->SetUniformValue(shader->GetMvpUniform(), glm::value_ptr(mvp));
		if(color) {
			shader->SetUniformValue(shader->GetColorUniform(), color->asFloatVector());
		}
		shader->SetUniformValue(shader->GetTexMapUniform(), 0);

		mat->Apply();

		for(auto it = mat->GetTexture().begin(); it != mat->GetTexture().end(); ++it) {
			auto texture = std::dynamic_pointer_cast<OpenGLTexture>(*it);
			ASSERT_LOG(texture != NULL, "Texture passed in was not of expected type.");

			auto uv_coords = mat->GetNormalisedTextureCoords(it);

			texture->Bind();
			// XXX the following line are only temporary, obviously.
			//shader->SetUniformValue(shader->GetUniformIterator("discard"), 0);
			glEnableVertexAttribArray(shader->GetVertexAttribute()->second.location);
			glVertexAttribPointer(shader->GetVertexAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
			glEnableVertexAttribArray(shader->GetTexcoordAttribute()->second.location);
			glVertexAttribPointer(shader->GetTexcoordAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, &uv_coords);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			glDisableVertexAttribArray(shader->GetTexcoordAttribute()->second.location);
			glDisableVertexAttribArray(shader->GetVertexAttribute()->second.location);
		}
		mat->Unapply();
	}

	void CanvasOGL::blitTexture(const MaterialPtr& mat, const rect& src, float rotation, const rect& dst, const ColorPtr& color) const
	{
		ASSERT_LOG(mat != NULL, "Material was null");
		const float vx1 = float(dst.x());
		const float vy1 = float(dst.y());
		const float vx2 = float(dst.x2());
		const float vy2 = float(dst.y2());
		const float vtx_coords[] = {
			vx1, vy1,
			vx2, vy1,
			vx1, vy2,
			vx2, vy2,
		};

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3((vx1+vx2)/2.0f,(vy1+vy2)/2.0f,0.0f)) * glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f,0.0f,1.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-(vx1+vy1)/2.0f,-(vy1+vy1)/2.0f,0.0f));
		glm::mat4 mvp = mvp_ * model;
		auto shader = Shader::ShaderProgram::DefaultSystemShader();
		shader->MakeActive();
		shader->SetUniformValue(shader->GetMvpUniform(), glm::value_ptr(mvp));
		if(color) {
			shader->SetUniformValue(shader->GetColorUniform(), color->asFloatVector());
		}
		shader->SetUniformValue(shader->GetTexMapUniform(), 0);

		mat->Apply();

		for(auto it = mat->GetTexture().begin(); it != mat->GetTexture().end(); ++it) {
			auto texture = std::dynamic_pointer_cast<OpenGLTexture>(*it);
			ASSERT_LOG(texture != NULL, "Texture passed in was not of expected type.");

			const float tx1 = float(src.x()) / texture->Width();
			const float ty1 = float(src.y()) / texture->Height();
			const float tx2 = src.w() == 0 ? 1.0f : float(src.x2()) / texture->Width();
			const float ty2 = src.h() == 0 ? 1.0f : float(src.y2()) / texture->Height();
			const float uv_coords[] = {
				tx1, ty1,
				tx2, ty1,
				tx1, ty2,
				tx2, ty2,
			};

			texture->Bind();
			// XXX the following line are only temporary, obviously.
			//shader->SetUniformValue(shader->GetUniformIterator("discard"), 0);
			glEnableVertexAttribArray(shader->GetVertexAttribute()->second.location);
			glVertexAttribPointer(shader->GetVertexAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
			glEnableVertexAttribArray(shader->GetTexcoordAttribute()->second.location);
			glVertexAttribPointer(shader->GetTexcoordAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, &uv_coords);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			glDisableVertexAttribArray(shader->GetTexcoordAttribute()->second.location);
			glDisableVertexAttribArray(shader->GetVertexAttribute()->second.location);
		}

		mat->Unapply();
	}

	void CanvasOGL::drawRect(const rect& r, const ColorPtr& fill_color, const ColorPtr& stroke_color, float rotation) const
	{
		const float vx1 = float(r.x());
		const float vy1 = float(r.y());
		const float vx2 = float(r.x2());
		const float vy2 = float(r.y2());
		const float vtx_coords[] = {
			vx1, vy1,
			vx2, vy1,
			vx1, vy2,
			vx2, vy2,
		};

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3((vx1+vx2)/2.0f,(vy1+vy2)/2.0f,0.0f)) * glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f,0.0f,1.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-(vx1+vy1)/2.0f,-(vy1+vy1)/2.0f,0.0f));
		glm::mat4 mvp = mvp_ * model;
		static Shader::ShaderProgramPtr shader = Shader::ShaderProgram::Factory("simple");
		shader->MakeActive();
		shader->SetUniformValue(shader->GetMvpUniform(), glm::value_ptr(mvp));

		// Draw a filled rect if fill_color is specified
		if(fill_color) {
			shader->SetUniformValue(shader->GetColorUniform(), fill_color->asFloatVector());
			glEnableVertexAttribArray(shader->GetVertexAttribute()->second.location);
			glVertexAttribPointer(shader->GetVertexAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
		// Draw stroke if stroke_color is specified.
		if(stroke_color) {
			// XXX I think there is an easier way of doing this, with modern GL
			const float vtx_coords[] = {
				vx1, vy1,
				vx2, vy1,
				vx2, vy2,
				vx1, vy2,
				vx1, vy1,
			};
			shader->SetUniformValue(shader->GetColorUniform(), stroke_color->asFloatVector());
			glEnableVertexAttribArray(shader->GetVertexAttribute()->second.location);
			glVertexAttribPointer(shader->GetVertexAttribute()->second.location, 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
			// XXX this may not be right.
			glDrawArrays(GL_LINE_STRIP, 0, 5);
		}

	}

	void CanvasOGL::drawLine(const point& p1, const point& p2, const Color& color) const
	{
		// XXX
	}

	void CanvasOGL::drawCircle(const point& centre, double radius, const Color& color) const
	{
		// XXX
	}

	CanvasPtr CanvasOGL::getInstance()
	{
		return get_instance();
	}
}
