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

#include "CanvasGLES2.hpp"
#include "ShadersGLES2.hpp"
#include "TextureGLES2.hpp"

namespace KRE
{
	namespace
	{
		CanvasPtr& get_instance()
		{
			static CanvasPtr res = CanvasPtr(new CanvasGLESv2());
			return res;
		}
	}

	CanvasGLESv2::CanvasGLESv2()
	{
		handleDimensionsChanged();
	}

	CanvasGLESv2::~CanvasGLESv2()
	{
	}

	void CanvasGLESv2::handleDimensionsChanged()
	{
	}

	void CanvasGLESv2::blitTexture(const TexturePtr& texture, const rect& src, float rotation, const rect& dst, const Color& color, CanvasBlitFlags flags) const
	{
		const float tx1 = texture->getTextureCoordW(0, src.x());
		const float ty1 = texture->getTextureCoordH(0, src.y());
		const float tx2 = texture->getTextureCoordW(0, src.w() == 0 ? texture->surfaceWidth() : src.x2());
		const float ty2 = texture->getTextureCoordH(0, src.h() == 0 ? texture->surfaceHeight() : src.y2());
		const float uv_coords[] = {
			tx1, ty1,
			tx2, ty1,
			tx1, ty2,
			tx2, ty2,
		};

		auto& tex_dst = texture->getSourceRect();
		float vx1 = static_cast<float>(dst.x());
		float vy1 = static_cast<float>(dst.y());
		float vx2 = static_cast<float>(dst.w() == 0 ? tex_dst.w() == 0 ? texture->surfaceWidth() : dst.x() + tex_dst.w() : dst.x2());
		float vy2 = static_cast<float>(dst.h() == 0 ? tex_dst.h() == 0 ? texture->surfaceHeight() : dst.y() + tex_dst.h() : dst.y2());

		if(flags & CanvasBlitFlags::FLIP_VERTICAL) {
			std::swap(vx1, vx2);
		}
		if(flags & CanvasBlitFlags::FLIP_HORIZONTAL) {
			std::swap(vy1, vy2);
		}
		const float vtx_coords[] = {
			vx1, vy1,
			vx2, vy1,
			vx1, vy2,
			vx2, vy2,
		};
		
		//LOG_DEBUG("blit: " << src << "," << dst);
		//LOG_DEBUG("blit: " << tx1 << "," << ty1 << "," << tx2 << "," << ty2 << " : " << vx1 << "," << vy1 << "," << vx2 << "," << vy2);

		glm::mat4 mvp;
		if(std::abs(rotation) > FLT_EPSILON) {
			glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3((vx1+vx2)/2.0f,(vy1+vy2)/2.0f,0.0f)) * glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f,0.0f,1.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-(vx1+vx2)/2.0f,-(vy1+vy2)/2.0f,0.0f));
			mvp = getPVMatrix() * model * get_global_model_matrix();
		} else {
			mvp = getPVMatrix() * get_global_model_matrix();
		}
		auto shader = getCurrentShader();
		shader->makeActive();
		shader->setUniformsForTexture(texture);
		auto uniform_draw_fn = shader->getUniformDrawFunction();
		if(uniform_draw_fn) {
			uniform_draw_fn(shader);
		}
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));
		if(color != KRE::Color::colorWhite()) {
			shader->setUniformValue(shader->getColorUniform(), (color*getColor()).asFloatVector());
		} else {
			shader->setUniformValue(shader->getColorUniform(), getColor().asFloatVector());
		}
		// XXX the following line are only temporary, obviously.
		//shader->SetUniformValue(shader->GetUniformIterator("discard"), 0);
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
		glEnableVertexAttribArray(shader->getTexcoordAttribute());
		glVertexAttribPointer(shader->getTexcoordAttribute(), 2, GL_FLOAT, GL_FALSE, 0, uv_coords);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glDisableVertexAttribArray(shader->getTexcoordAttribute());
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::blitTexture(const TexturePtr& tex, const std::vector<vertex_texcoord>& vtc, float rotation, const Color& color)
	{
		glm::mat4 model = glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0, 0, 1.0f));
		glm::mat4 mvp = getPVMatrix() * model * get_global_model_matrix();
		auto shader = getCurrentShader();
		shader->makeActive();
		shader->setUniformsForTexture(tex);
		auto uniform_draw_fn = shader->getUniformDrawFunction();
		if(uniform_draw_fn) {
			uniform_draw_fn(shader);
		}
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));
		if(color != KRE::Color::colorWhite()) {
			shader->setUniformValue(shader->getColorUniform(), (color*getColor()).asFloatVector());
		} else {
			shader->setUniformValue(shader->getColorUniform(), getColor().asFloatVector());
		}
		// XXX the following line are only temporary, obviously.
		//shader->SetUniformValue(shader->GetUniformIterator("discard"), 0);
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, sizeof(vertex_texcoord), reinterpret_cast<const unsigned char*>(&vtc[0]) + offsetof(vertex_texcoord, vtx));
		glEnableVertexAttribArray(shader->getTexcoordAttribute());
		glVertexAttribPointer(shader->getTexcoordAttribute(), 2, GL_FLOAT, GL_FALSE, sizeof(vertex_texcoord), reinterpret_cast<const unsigned char*>(&vtc[0]) + offsetof(vertex_texcoord, tc));

		glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vtc.size()));

		glDisableVertexAttribArray(shader->getTexcoordAttribute());
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawSolidRect(const rect& r, const Color& fill_color, const Color& stroke_color, float rotation) const
	{
		rectf vtx = r.as_type<float>();
		const float vtx_coords[] = {
			vtx.x1(), vtx.y1(),
			vtx.x2(), vtx.y1(),
			vtx.x1(), vtx.y2(),
			vtx.x2(), vtx.y2(),
		};

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(vtx.mid_x(),vtx.mid_y(),0.0f)) * glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f,0.0f,1.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-vtx.mid_x(),-vtx.mid_y(),0.0f));
		glm::mat4 mvp = getPVMatrix() * model * get_global_model_matrix();
		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("simple");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		// Draw a filled rect
		shader->setUniformValue(shader->getColorUniform(), fill_color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// Draw stroke if stroke_color is specified.
		// XXX I think there is an easier way of doing this, with modern GL
		const float vtx_coords_line[] = {
			vtx.x1(), vtx.y1(),
			vtx.x2(), vtx.y1(),
			vtx.x2(), vtx.y2(),
			vtx.x1(), vtx.y2(),
			vtx.x1(), vtx.y1(),
		};
		shader->setUniformValue(shader->getColorUniform(), stroke_color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, vtx_coords_line);
		// XXX this may not be right.
		glDrawArrays(GL_LINE_STRIP, 0, 5);
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawSolidRect(const rect& r, const Color& fill_color, float rotation) const
	{
		rectf vtx = r.as_type<float>();
		const float vtx_coords[] = {
			vtx.x1(), vtx.y1(),
			vtx.x2(), vtx.y1(),
			vtx.x1(), vtx.y2(),
			vtx.x2(), vtx.y2(),
		};

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(vtx.mid_x(),vtx.mid_y(),0.0f)) * glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f,0.0f,1.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-vtx.mid_x(),-vtx.mid_y(),0.0f));
		glm::mat4 mvp = getPVMatrix() * model * get_global_model_matrix();
		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("simple");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		// Draw a filled rect
		shader->setUniformValue(shader->getColorUniform(), fill_color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawHollowRect(const rect& r, const Color& stroke_color, float rotation) const
	{
		rectf vtx = r.as_type<float>();
		const float vtx_coords_line[] = {
			vtx.x1(), vtx.y1(),
			vtx.x2(), vtx.y1(),
			vtx.x2(), vtx.y2(),
			vtx.x1(), vtx.y2(),
			vtx.x1(), vtx.y1(),
		};

		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(vtx.mid_x(),vtx.mid_y(),0.0f)) * glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f,0.0f,1.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-vtx.mid_x(),-vtx.mid_y(),0.0f));
		glm::mat4 mvp = getPVMatrix() * model * get_global_model_matrix();

		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("simple");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		// Draw stroke if stroke_color is specified.
		// XXX I think there is an easier way of doing this, with modern GL
		shader->setUniformValue(shader->getColorUniform(), stroke_color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, vtx_coords_line);
		glDrawArrays(GL_LINE_STRIP, 0, 5);
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawLine(const point& p1, const point& p2, const Color& color) const
	{
		const float vtx_coords_line[] = {
			static_cast<float>(p1.x), static_cast<float>(p1.y),
			static_cast<float>(p2.x), static_cast<float>(p2.y),
		};
		// This draws an aliased line -- consider making this a nicer unaliased line.
		glm::mat4 mvp = getPVMatrix() * get_global_model_matrix();

		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("simple");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		shader->setUniformValue(shader->getColorUniform(), color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, vtx_coords_line);
		glDrawArrays(GL_LINES, 0, 2);
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawLines(const std::vector<glm::vec2>& varray, float line_width, const Color& color) const 
	{
		/*static OpenGL::ShaderProgramPtr shader = OpenGL::ShaderProgram::factory("complex");
		shader->makeActive();
		shader->setUniformValue(shader->getMvUniform(), glm::value_ptr(get_global_model_matrix()));
		shader->setUniformValue(shader->getPUniform(), glm::value_ptr(mvp_));

		if(shader->getNormalAttribute() == ShaderProgram::INVALID_ATTRIBUTE 
			|| shader->getVertexAttribute() == ShaderProgram::INVALID_ATTRIBUTE) {
			return;
		}

		std::vector<glm::vec2> vertices;
		vertices.reserve(varray.size() * 2);
		std::vector<glm::vec2> normals;
		normals.reserve(varray.size() * 2);
		
		for(int n = 0; n != varray.size(); n += 2) {
			const float dx = varray[n+1].x - varray[n+0].x;
			const float dy = varray[n+1].y - varray[n+0].y;
			const glm::vec2 d1 = glm::normalize(glm::vec2(dy, -dx));
			const glm::vec2 d2 = glm::normalize(glm::vec2(-dy, dx));

			vertices.emplace_back(varray[n+0]);
			vertices.emplace_back(varray[n+0]);
			vertices.emplace_back(varray[n+1]);
			vertices.emplace_back(varray[n+1]);
						
			normals.emplace_back(d1);
			normals.emplace_back(d2);
			normals.emplace_back(d1);
			normals.emplace_back(d2);
		}

		static auto blur_uniform = shader->getUniform("u_blur");
		shader->setUniformValue(blur_uniform, 2.0f);
		shader->setUniformValue(shader->getLineWidthUniform(), line_width);
		shader->setUniformValue(shader->getColorUniform(), color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glEnableVertexAttribArray(shader->getNormalAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, &vertices[0]);
		glVertexAttribPointer(shader->getNormalAttribute(), 2, GL_FLOAT, GL_FALSE, 0, &normals[0]);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size());
		glDisableVertexAttribArray(shader->getNormalAttribute());
		glDisableVertexAttribArray(shader->getVertexAttribute());*/
		glm::mat4 mvp = getPVMatrix() * get_global_model_matrix();

		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("simple");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		shader->setUniformValue(shader->getLineWidthUniform(), line_width);
		shader->setUniformValue(shader->getColorUniform(), color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, &varray[0]);
		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(varray.size()));
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawLines(const std::vector<glm::vec2>& varray, float line_width, const std::vector<glm::u8vec4>& carray) const 
	{
		ASSERT_LOG(varray.size() == carray.size(), "Vertex and color array sizes don't match.");
		// This draws an aliased line -- consider making this a nicer unaliased line.
		glm::mat4 mvp = getPVMatrix() * get_global_model_matrix();

		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("attr_color_shader");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		/// XXX FIXME no line_width in attr_color_shader
		//shader->setUniformValue(shader->getLineWidthUniform(), line_width);
		shader->setUniformValue(shader->getColorUniform(), glm::value_ptr(glm::vec4(1.0f)));
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glEnableVertexAttribArray(shader->getColorAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, &varray[0]);
		glVertexAttribPointer(shader->getColorAttribute(), 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, &carray[0]);
		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(varray.size()));
		glDisableVertexAttribArray(shader->getColorAttribute());
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawLineStrip(const std::vector<glm::vec2>& varray, float line_width, const Color& color) const 
	{
		// This draws an aliased line -- consider making this a nicer unaliased line.
		glm::mat4 mvp = getPVMatrix() * get_global_model_matrix();

		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("simple");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		shader->setUniformValue(shader->getLineWidthUniform(), line_width);
		shader->setUniformValue(shader->getColorUniform(), color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, &varray[0]);
		glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(varray.size()));
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawLineLoop(const std::vector<glm::vec2>& varray, float line_width, const Color& color) const 
	{
		// This draws an aliased line -- consider making this a nicer unaliased line.
		glm::mat4 mvp = getPVMatrix() * get_global_model_matrix();

		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("simple");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		shader->setUniformValue(shader->getLineWidthUniform(), line_width);
		shader->setUniformValue(shader->getColorUniform(), color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, &varray[0]);
		glDrawArrays(GL_LINE_LOOP, 0, static_cast<GLsizei>(varray.size()));
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawLine(const pointf& p1, const pointf& p2, const Color& color) const 
	{
		const float vtx_coords_line[] = {
			p1.x, p1.y,
			p2.x, p2.y,
		};
		// This draws an aliased line -- consider making this a nicer unaliased line.
		glm::mat4 mvp = getPVMatrix() * get_global_model_matrix();

		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("simple");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		shader->setUniformValue(shader->getColorUniform(), color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, vtx_coords_line);
		glDrawArrays(GL_LINES, 0, 2);
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawPolygon(const std::vector<glm::vec2>& varray, const Color& color) const 
	{
		// This draws an aliased line -- consider making this a nicer unaliased line.
		glm::mat4 mvp = getPVMatrix() * get_global_model_matrix();

		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("simple");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		shader->setUniformValue(shader->getLineWidthUniform(), 1.0f);
		shader->setUniformValue(shader->getColorUniform(), color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, &varray[0]);
		// XXX Replaced GL_POLYGON with GL_TRIANGLE_FAN, not convinced this is actually the right thing. To check.
		glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(varray.size()));
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawSolidCircle(const point& centre, float radius, const Color& color) const 
	{
		drawSolidCircle(pointf(static_cast<float>(centre.x), static_cast<float>(centre.y)), radius, color);
	}

	void CanvasGLESv2::drawSolidCircle(const point& centre, float radius, const std::vector<glm::u8vec4>& color) const 
	{
		drawSolidCircle(pointf(static_cast<float>(centre.x), static_cast<float>(centre.y)), radius, color);
	}

	void CanvasGLESv2::drawHollowCircle(const point& centre, float outer_radius, float inner_radius, const Color& color) const 
	{
		drawHollowCircle(pointf(static_cast<float>(centre.x), static_cast<float>(centre.y)), outer_radius, inner_radius, color);
	}

	void CanvasGLESv2::drawSolidCircle(const pointf& centre, float radius, const Color& color) const 
	{
		glm::mat4 mvp = getPVMatrix() * get_global_model_matrix();

		rectf vtx(centre.x - radius - 2, centre.y - radius - 2, 2 * radius + 4, 2 * radius + 4);
		const float vtx_coords[] = {
			vtx.x1(), vtx.y1(),
			vtx.x2(), vtx.y1(),
			vtx.x1(), vtx.y2(),
			vtx.x2(), vtx.y2(),
		};

		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("circle");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		try {
			static auto screen_dim = shader->getUniform("screen_dimensions");
			shader->setUniformValue(screen_dim, glm::value_ptr(glm::vec2(getWindow()->width(), getWindow()->height())));
		} catch(ShaderUniformError&) {
		}
		try {
			static auto radius_it = shader->getUniform("outer_radius");
			shader->setUniformValue(radius_it, radius);
		} catch(ShaderUniformError&) {
		}
		try {
			static auto inner_radius_it = shader->getUniform("inner_radius");
			shader->setUniformValue(inner_radius_it, 0.0f);
		} catch(ShaderUniformError&) {
		}
		try {
			static auto centre_it = shader->getUniform("centre");
			shader->setUniformValue(centre_it, glm::value_ptr(glm::vec2(centre.x, centre.y)));
		} catch(ShaderUniformError&) {
		}
		shader->setUniformValue(shader->getColorUniform(), color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawSolidCircle(const pointf& centre, float radius, const std::vector<glm::u8vec4>& color) const 
	{
		glm::mat4 mvp = getPVMatrix() * get_global_model_matrix();

		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("attr_color_shader");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));
		shader->setUniformValue(shader->getColorUniform(), getColor().asFloatVector());

		// XXX figure out a nice way to do this with shaders.
		std::vector<glm::vec2> varray;
		varray.reserve(color.size());
		varray.emplace_back(centre.x, centre.y);
		// First color co-ordinate is center of the circle
		for(int n = 0; n != color.size()-2; ++n) {
			const float angle = static_cast<float>(n) * static_cast<float>(M_PI * 2.0) / static_cast<float>(color.size() - 2);
			varray.emplace_back(centre.x + radius * std::cos(angle), centre.y + radius * std::sin(angle));
		}
		// last co-ordinate is repeated first point on circle.
		varray.emplace_back(varray[1]);

		glEnableVertexAttribArray(shader->getVertexAttribute());
		glEnableVertexAttribArray(shader->getColorAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, &varray[0]);
		glVertexAttribPointer(shader->getColorAttribute(), 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, &color[0]);
		glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(varray.size()));
		glDisableVertexAttribArray(shader->getColorAttribute());
		glDisableVertexAttribArray(shader->getVertexAttribute());

	}

	void CanvasGLESv2::drawHollowCircle(const pointf& centre, float outer_radius, float inner_radius, const Color& color) const 
	{
		glm::mat4 mvp = getPVMatrix() * get_global_model_matrix();

		rectf vtx(centre.x - outer_radius - 2, centre.y - outer_radius - 2, 2 * outer_radius + 4, 2 * outer_radius + 4);
		const float vtx_coords[] = {
			vtx.x1(), vtx.y1(),
			vtx.x2(), vtx.y1(),
			vtx.x1(), vtx.y2(),
			vtx.x2(), vtx.y2(),
		};

		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("circle");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		try {
			static auto screen_dim = shader->getUniform("screen_dimensions");
			shader->setUniformValue(screen_dim, glm::value_ptr(glm::vec2(width(), height())));
		} catch(ShaderUniformError&) {
		}
		try {
			static auto radius_it = shader->getUniform("outer_radius");
			shader->setUniformValue(radius_it, outer_radius);
		} catch(ShaderUniformError&) {
		}
		try {
			static auto inner_radius_it = shader->getUniform("inner_radius");
			shader->setUniformValue(inner_radius_it, inner_radius);
		} catch(ShaderUniformError&) {
		}
		try {
			static auto centre_it = shader->getUniform("centre");
			shader->setUniformValue(centre_it, glm::value_ptr(glm::vec2(centre.x, centre.y)));
		} catch(ShaderUniformError&) {
		}
		shader->setUniformValue(shader->getColorUniform(), color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, vtx_coords);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	void CanvasGLESv2::drawPoints(const std::vector<glm::vec2>& varray, float radius, const Color& color) const 
	{
		// This draws an aliased line -- consider making this a nicer unaliased line.
		glm::mat4 mvp = getPVMatrix() * get_global_model_matrix();

		static GLESv2::ShaderProgramPtr shader = GLESv2::ShaderProgram::factory("simple");
		shader->makeActive();
		shader->setUniformValue(shader->getMvpUniform(), glm::value_ptr(mvp));

		static auto it = shader->getUniform("point_size");
		shader->setUniformValue(it, radius);
		shader->setUniformValue(shader->getColorUniform(), color.asFloatVector());
		glEnableVertexAttribArray(shader->getVertexAttribute());
		glVertexAttribPointer(shader->getVertexAttribute(), 2, GL_FLOAT, GL_FALSE, 0, &varray[0]);
		glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(varray.size()));
		glDisableVertexAttribArray(shader->getVertexAttribute());
	}

	CanvasPtr CanvasGLESv2::getInstance()
	{
		return get_instance();
	}
}
