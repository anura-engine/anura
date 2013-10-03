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

#pragma once

#include <boost/shared_array.hpp>

#include "asserts.hpp"
#include "geometry.hpp"
#include "graphics.hpp"

namespace graphics
{
	class fbo;
	typedef std::shared_ptr<fbo> fbo_ptr;

	class fbo
	{
	public:
		explicit fbo(const rect& area);
		explicit fbo(const rect& area, const gles2::shader_program_ptr& shader);
		virtual ~fbo();

		size_t width() const { return area_.w(); }
		size_t height() const { return area_.h(); }

		size_t x() const { return area_.x(); }
		size_t y() const { return area_.y(); }

		void enable_depth_test(bool dt = true) { depth_test_enable_ = dt; }
		bool is_depth_test_enabled() const { return depth_test_enable_; }

		void set_final_shader(const gles2::shader_program_ptr& shader) { final_shader_ = shader; }

		void init();
		void render_to_screen();
		void render_to_screen(const gles2::shader_program_ptr& shader);

		void draw_begin();
		void draw_end();

		struct render_manager {
			render_manager(fbo_ptr f) : fbo_(f) {
				ASSERT_LOG(fbo_ != NULL, "fbo::render_manager() called on null fbo pointer.");
				fbo_->draw_begin();
			}
			virtual ~render_manager() {
				fbo_->draw_end();
			}
			fbo_ptr fbo_;
		private:
			render_manager();
			render_manager(const render_manager&);
		};
	private:
		glm::mat4 proj_;
		size_t tex_width_;
		size_t tex_height_;
		rect area_;

		GLint video_framebuffer_id_;

		bool depth_test_enable_;

		gles2::shader_program_ptr final_shader_;

		boost::shared_array<GLuint> framebuffer_id_;
		boost::shared_array<GLuint> render_buffer_id_;
		boost::shared_array<GLuint> final_texture_id_;

		fbo(const fbo&);
	};
}
