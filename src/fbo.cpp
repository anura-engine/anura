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
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "fbo.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "texture.hpp"

namespace graphics
{
	fbo::fbo(const rect& area) : area_(area), depth_test_enable_(false)
	{
		init();
	}

	fbo::fbo(const rect& area, const gles2::shader_program_ptr& shader)
		: area_(area), depth_test_enable_(false), final_shader_(shader)
	{
		init();
	}

	fbo::~fbo()
	{
	}

	void fbo::init()
	{
		proj_ = glm::ortho(0.0f, float(preferences::actual_screen_width()), float(preferences::actual_screen_height()), 0.0f);

		tex_width_ = texture::allows_npot() ? width() : texture::next_power_of_2(width());
		tex_height_ = texture::allows_npot() ? height() : texture::next_power_of_2(height());

		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &video_framebuffer_id_);

		if(depth_test_enable_) {
			glDepthFunc(GL_LEQUAL);
			glDepthMask(GL_TRUE);
		}

		if(graphics::get_configured_msaa() != 0) {
			render_buffer_id_ = boost::shared_array<GLuint>(new GLuint[2], [](GLuint* id){glBindRenderbuffer(GL_RENDERBUFFER, 0); glDeleteRenderbuffers(2, id); delete[] id;});
			glGenRenderbuffers(2, &render_buffer_id_[0]);
			glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id_[0]);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, graphics::get_configured_msaa(), GL_RGBA, tex_width_, tex_height_);

			glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id_[1]);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, graphics::get_configured_msaa(), GL_DEPTH_COMPONENT, tex_width_, tex_height_);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);

			// check FBO status
			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			ASSERT_NE(status, GL_FRAMEBUFFER_UNSUPPORTED);
			ASSERT_EQ(status, GL_FRAMEBUFFER_COMPLETE);

			// Create Other FBO
			final_texture_id_ = boost::shared_array<GLuint>(new GLuint[2], [](GLuint* id){glDeleteTextures(2,id); delete[] id;});
			glGenTextures(2, &final_texture_id_[0]);
			glBindTexture(GL_TEXTURE_2D, final_texture_id_[0]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width_, tex_height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
			glBindTexture(GL_TEXTURE_2D, final_texture_id_[1] );
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
			glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, tex_width_, tex_height_, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL );
			glBindTexture(GL_TEXTURE_2D, 0);

			framebuffer_id_ = boost::shared_array<GLuint>(new GLuint[2], [](GLuint* id){glDeleteFramebuffers(2, id); delete[] id;});
			glGenFramebuffers(2, &framebuffer_id_[0]);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_[1]);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, render_buffer_id_[0]);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, render_buffer_id_[1]);
			status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			ASSERT_NE(status, GL_FRAMEBUFFER_UNSUPPORTED);
			ASSERT_EQ(status, GL_FRAMEBUFFER_COMPLETE);

			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_[0]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, final_texture_id_[0], 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, final_texture_id_[1], 0);
			status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			ASSERT_NE(status, GL_FRAMEBUFFER_UNSUPPORTED);
			ASSERT_EQ(status, GL_FRAMEBUFFER_COMPLETE);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		} else {
			render_buffer_id_ = boost::shared_array<GLuint>(new GLuint[1], [](GLuint* id){glBindRenderbuffer(GL_RENDERBUFFER, 0); glDeleteRenderbuffers(1, id); delete[] id;});
			glGenRenderbuffers(1, &render_buffer_id_[0]);
			glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id_[0]);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, tex_width_, tex_height_);

			final_texture_id_ = boost::shared_array<GLuint>(new GLuint[1], [](GLuint* id){glDeleteTextures(1,id); delete[] id;});
			glGenTextures(1, &final_texture_id_[0]);
			glBindTexture(GL_TEXTURE_2D, final_texture_id_[0]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width_, tex_height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);

			framebuffer_id_ = boost::shared_array<GLuint>(new GLuint[1], [](GLuint* id){glDeleteFramebuffers(1, id); delete[] id;});
			glGenFramebuffers(1, &framebuffer_id_[0]);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_[0]);
			// attach the texture to FBO color attachment point
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, final_texture_id_[0], 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, render_buffer_id_[0]);
		}
	}

	void fbo::draw_begin()
	{
		if(graphics::get_configured_msaa() != 0) {
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_[1]);
		} else {
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_[0]);
		}

		//set up the raster projection.
		glViewport(0, 0, width(), height());

		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if(depth_test_enable_) {
			glEnable(GL_DEPTH_TEST);
		}

		//start drawing here.
	}

	void fbo::draw_end()
	{
		// end drawing
		glBindFramebuffer(GL_FRAMEBUFFER, video_framebuffer_id_);

		glViewport(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height());

		if(depth_test_enable_) {
			glDisable(GL_DEPTH_TEST);
		}

		if(graphics::get_configured_msaa() != 0) {
			// blit from multisample FBO to final FBO
			glBindFramebuffer(GL_FRAMEBUFFER, 0 );
			glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_id_[1]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_id_[0]);
			glBlitFramebuffer(0, 0, tex_width_, tex_height_, 0, 0, tex_width_, tex_height_, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		}
	}

	void fbo::render_to_screen()
	{
		render_to_screen(final_shader_);
	}

	void fbo::render_to_screen(const gles2::shader_program_ptr& shader)
	{
		gles2::manager gles2_manager(shader == NULL ? final_shader_ : shader);

		GLint cur_id = graphics::texture::get_current_texture();
		glBindTexture(GL_TEXTURE_2D, final_texture_id_[0]);

		const int w_odd = width() % 2;
		const int h_odd = height() % 2;
		const int w = width() / 2;
		const int h = height() / 2;

		glm::mat4 mvp = proj_ * glm::translate(glm::mat4(1.0f), glm::vec3(x()+w, y()+h, 0.0f));
		glUniformMatrix4fv(gles2::active_shader()->shader()->mvp_matrix_uniform(), 1, GL_FALSE, glm::value_ptr(mvp));

		GLfloat varray[] = {
			(GLfloat)-w, (GLfloat)-h,
			(GLfloat)-w, (GLfloat)h+h_odd,
			(GLfloat)w+w_odd, (GLfloat)-h,
			(GLfloat)w+w_odd, (GLfloat)h+h_odd
		};
		const GLfloat tcarray[] = {
			0.0f, GLfloat(height())/tex_height_,
			0.0f, 0.0f,
			GLfloat(width())/tex_width_, GLfloat(height())/tex_height_,
			GLfloat(width())/tex_width_, 0.0f,
		};
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
		gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0, tcarray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glBindTexture(GL_TEXTURE_2D, cur_id);
	}
}
