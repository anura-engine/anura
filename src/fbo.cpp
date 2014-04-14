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
#include "texture_frame_buffer.hpp"

namespace graphics
{
	fbo::fbo(int x, int y, int width, int height, int screen_width, int screen_height)
		: x_(x), y_(y), width_(width), height_(height), depth_test_enable_(false),
		awidth_(screen_width), aheight_(screen_height), letterbox_width_(0), letterbox_height_(0)
	{
		init();
	}

	fbo::fbo(int x, int y, int width, int height, int screen_width, int screen_height, const gles2::shader_program_ptr& shader)
		: x_(x), y_(y), width_(width), height_(height), 
		depth_test_enable_(false), final_shader_(shader),
		awidth_(screen_width), aheight_(screen_height),
		letterbox_width_(0), letterbox_height_(0)
	{
		init();
	}

	fbo::~fbo()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, video_framebuffer_id_);
	}

	void fbo::init()
	{
		calculate_letterbox();
		proj_ = glm::ortho(GLfloat(x()), 
			GLfloat(width()+letterbox_width()), 
			GLfloat(height()+letterbox_height()), 
			GLfloat(y()));

		// Removed the power-of-2 test here as this is initialized before the texture manager
		// is. So we resort to the safest choice.
		tex_width_ = texture::next_power_of_2(awidth());
		tex_height_ = texture::next_power_of_2(aheight());
		std::cerr << "INFO: fbo texture size " << tex_width_ << "," << tex_height_ << std::endl;

		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &video_framebuffer_id_);

		if(depth_test_enable_) {
			glDepthFunc(GL_LEQUAL);
			glDepthMask(GL_TRUE);
		}

		if(get_main_window()->get_configured_msaa() != 0) {
			render_buffer_id_ = boost::shared_array<GLuint>(new GLuint[2], [](GLuint* id){glBindRenderbuffer(GL_RENDERBUFFER, 0); glDeleteRenderbuffers(2, id); delete[] id;});
			glGenRenderbuffers(2, &render_buffer_id_[0]);
			glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id_[0]);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, get_main_window()->get_configured_msaa(), GL_RGBA, tex_width_, tex_height_);

			glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id_[1]);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, get_main_window()->get_configured_msaa(), GL_DEPTH_COMPONENT, tex_width_, tex_height_);
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
			const int render_buffer_count = 1;
			glGetError();
			render_buffer_id_ = boost::shared_array<GLuint>(new GLuint[render_buffer_count], [render_buffer_count](GLuint* id){glBindRenderbuffer(GL_RENDERBUFFER, 0); glDeleteRenderbuffers(render_buffer_count, id); delete[] id;});
			glGenRenderbuffers(render_buffer_count, &render_buffer_id_[0]);
			glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id_[0]);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, tex_width_, tex_height_);
			GLenum status = glGetError();
			ASSERT_EQ(status, GL_NO_ERROR);

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
			std::cerr << "XXX: Framebuffer ID: " << framebuffer_id_[0] << std::endl;
			// attach the texture to FBO color attachment point
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, final_texture_id_[0], 0);
			glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id_[0]);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, render_buffer_id_[0]);
			status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			ASSERT_NE(status, GL_FRAMEBUFFER_UNSUPPORTED);
			ASSERT_EQ(status, GL_FRAMEBUFFER_COMPLETE);
		}

		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		draw_begin();
	}

	void fbo::draw_begin()
	{
		if(get_main_window()->get_configured_msaa() != 0) {
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_[1]);
			texture_frame_buffer::set_framebuffer_id(framebuffer_id_[1]);
		} else {
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_[0]);
			texture_frame_buffer::set_framebuffer_id(framebuffer_id_[0]);
		}

		//set up the raster projection.
		glViewport(0, 0, awidth(), aheight());

		//glClearColor(0.0, 0.0, 0.0, 0.0);
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if(depth_test_enable_) {
			glEnable(GL_DEPTH_TEST);
		}

		//start drawing here.
	}

	void fbo::draw_end()
	{
		// end drawing
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(x(), y(), width(), height());

		if(depth_test_enable_) {
			glDisable(GL_DEPTH_TEST);
		}

		if(get_main_window()->get_configured_msaa() != 0) {
			// blit from multisample FBO to final FBO
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

	void fbo::calculate_letterbox()
	{
		double aspect_actual = double(awidth())/aheight();
		double aspect_screen = double(width())/height();
		std::cerr << "INFO: aspect_actual: " << aspect_actual << ", aspect_screen: " << aspect_screen << std::endl;

		if(std::abs(aspect_actual - aspect_screen) < 1e-6) {
			letterbox_width_ = letterbox_height_ = 0;
		} else if(aspect_screen > aspect_actual) {
			// place vertical borders
			letterbox_width_ = int(double(width()) - double(awidth())*height()/aheight()) & ~1;
			ASSERT_LOG(letterbox_width_ >= 0, "FATAL: Letterbox width < 0: " << letterbox_width_);
		} else {
			// place horizontal borders
			letterbox_height_ = int(double(height()) - double(aheight())*width()/awidth()) & ~1;
			ASSERT_LOG(letterbox_height_ >= 0, "FATAL: Letterbox height < 0: " << letterbox_height_);
		}

		std::cerr << "INFO: letterbox width=" << letterbox_width_ << ", letterbox height=" << letterbox_height_ << std::endl;
	}

	void fbo::render_to_screen(const gles2::shader_program_ptr& shader)
	{
		shader_save_context ssc;
		glUseProgram(shader == NULL ? final_shader_->shader()->get() : shader->shader()->get());
		
		glClearColor(0.0f, 0.0f, 0.0f, 255.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_BLEND);

		GLint cur_id = graphics::texture::get_current_texture();
		glBindTexture(GL_TEXTURE_2D, final_texture_id_[0]);

		// Size of the display.
		const GLfloat w = GLfloat(width());
		const GLfloat h = GLfloat(height());

		glm::mat4 mvp = proj_ * glm::translate(glm::mat4(1.0f), glm::vec3(x()+letterbox_width()/2, y()+letterbox_height()/2, 0.0f));
		glUniformMatrix4fv(gles2::active_shader()->shader()->mvp_matrix_uniform(), 1, GL_FALSE, glm::value_ptr(mvp));

		GLfloat varray[] = {
			0, 0,
			0, h,
			w, 0,
			w, h,
		};
		const GLfloat tcarray[] = {
			0.0f, GLfloat(aheight())/tex_height_,
			0.0f, 0.0f,
			GLfloat(awidth())/tex_width_, GLfloat(aheight())/tex_height_,
			GLfloat(awidth())/tex_width_, 0.0f,
		};
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
		gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0, tcarray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glBindTexture(GL_TEXTURE_2D, cur_id);

		glEnable(GL_BLEND);
	}
}
