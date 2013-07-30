/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/
#if defined(USE_ISOMAP)

#include <vector>

#include "asserts.hpp"
#include "surface.hpp"
#include "surface_cache.hpp"
#include "skybox.hpp"

namespace graphics
{
	namespace
	{
		enum
		{
			RIGHT_FACE,
			LEFT_FACE,
			TOP_FACE,
			BOTTOM_FACE,
			FRONT_FACE,
			BACK_FACE,
			MAX_FACES,
		};
		const char* const directions[] = {
			"right",
			"left",
			"top",
			"bottom",
			"front",
			"back",
		};
		const GLenum gl_faces[] = {
			GL_TEXTURE_CUBE_MAP_POSITIVE_X,
			GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
			GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
			GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
			GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
			GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
		};
	}

	skybox::skybox(const variant& node)
	{
		tex_id_ = boost::shared_ptr<GLuint>(new GLuint, [](GLuint* id){glDeleteTextures(1,id); delete id;});
		glGenTextures(1, tex_id_.get());
		glBindTexture(GL_TEXTURE_CUBE_MAP, *tex_id_);

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		for(int n = RIGHT_FACE; n != MAX_FACES; ++n) {
			ASSERT_LOG(node.has_key(directions[n]), "skybox must have " << directions[n] << " attribute");
			surface s = surface_cache::get(node[directions[n]].as_string());
			ASSERT_LOG(s->w == s->h, " skybox images must be square: " << directions[n] << " : " << s->w << "," << s->h);			
			glTexImage2D(gl_faces[n], 0, GL_RGB, s->w, s->h, 0, s->format->BytesPerPixel == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
		}
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

		if(node.has_key("color")) {
			color_ = color(node["color"]);
		} else {
			color_ = color(255, 255, 255, 255);
		}
		ASSERT_LOG(node.has_key("shader"), "skybox must have 'shader' attribute");
		shader_ = gles2::shader_program::get_global(node["shader"].as_string())->shader();

		u_texture_id_ = shader_->get_fixed_uniform("texture_map");
		ASSERT_LOG(u_texture_id_ != -1, "skybox: texture_map == -1");
		u_color_ = shader_->get_fixed_uniform("color");
		ASSERT_LOG(u_color_ != -1, "skybox: color == -1");
		u_mv_inverse_matrix_ = shader_->get_fixed_uniform("mv_inverse_matrix");
		ASSERT_LOG(u_mv_inverse_matrix_ != -1, "skybox: mv_inverse_matrix == -1");
		u_p_inverse_matrix_ = shader_->get_fixed_uniform("p_inverse_matrix");
		ASSERT_LOG(u_p_inverse_matrix_ != -1, "skybox: p_inverse_matrix == -1");

		a_position_ = shader_->get_fixed_attribute("vertex");
		ASSERT_LOG(a_position_ != -1, "skybox: vertex == -1");
	}

	skybox::~skybox()
	{
	}

	void skybox::draw(const lighting_ptr lighting, const camera_callable_ptr& camera) const
	{
		// Lighting isn't used.
		shader_save_context ctx;
		glUseProgram(shader_->get());

		glBindTexture(GL_TEXTURE_CUBE_MAP, *tex_id_);

		glm::mat4 mv_inv = camera->view_mat();
		mv_inv[3][0] = 0; mv_inv[3][1] = 0; mv_inv[3][2] = 0; 
		mv_inv = glm::inverse(mv_inv);
		glUniformMatrix4fv(u_mv_inverse_matrix_, 1, GL_FALSE, glm::value_ptr(mv_inv));
		glm::mat4 p_inv = glm::inverse(camera->projection_mat());
		glUniformMatrix4fv(u_p_inverse_matrix_, 1, GL_FALSE, glm::value_ptr(p_inv));

		glUniform1i(u_texture_id_, 0);
		glUniform4f(u_color_, color_.r()/255.0f, color_.g()/255.0f, color_.b()/255.0f, color_.a()/255.0f);

		const GLfloat cube_verts[] = {
			 1.0f, -1.0f, 0.0f, 
			-1.0f, -1.0f, 0.0f, 
			 1.0f,  1.0f, 0.0f, 

			 1.0f,  1.0f, 0.0f, 
			-1.0f, -1.0f, 0.0f, 
			-1.0f,  1.0f, 0.0f, 
		};

		glEnableVertexAttribArray(a_position_);
		glVertexAttribPointer(a_position_, 3, GL_FLOAT, GL_FALSE, 0, cube_verts);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisableVertexAttribArray(a_position_);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(skybox)
		DEFINE_FIELD(color, "[int,int,int,int]|string")
			return obj.color_.write();
		DEFINE_SET_FIELD_TYPE("[int,int,int,int]")
			obj.color_ = graphics::color(value);
	END_DEFINE_CALLABLE(skybox)

}

#endif
