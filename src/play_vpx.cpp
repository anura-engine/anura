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

#if defined(USE_LIBVPX)

#include "graphics.hpp"
#include "module.hpp"
#include "play_vpx.hpp"
#include "preferences.hpp"
#include "texture.hpp"

#define IVF_FILE_HDR_SZ  (32)
#define IVF_FRAME_HDR_SZ (12)

namespace movie
{
	namespace 
	{
		size_t mem_get_le32(const std::vector<uint8_t>& mem) {
			return (size_t(mem[3]) << 24)|(size_t(mem[2]) << 16)|(size_t(mem[1]) << 8)|size_t(mem[0]);
		}

		struct manager
		{
			manager(const gles2::program_ptr& shader) 
			{
				glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
				glUseProgram(shader->get());
			}
			~manager()
			{
				glUseProgram(old_program);
			}
			GLint old_program;
		};
	}

	vpx::vpx(const std::string& file, int x, int y, int width, int height, bool loop, bool cancel_on_keypress)
		: loop_(loop), cancel_on_keypress_(cancel_on_keypress), playing_(false), flags_(0), img_(NULL)
	{
		file_name_ = module::map_file(file);
		set_loc(x, y);
		set_dim(width, height);
		init();
		for(auto& ut : u_tex_) {
			ut = -1;
		}
	}

	vpx::vpx(const variant& v, game_logic::formula_callable* e)
		: widget(v, e), loop_(false), cancel_on_keypress_(false), playing_(false), flags_(0), img_(NULL)
	{
		for(auto& ut : u_tex_) {
			ut = -1;
		}
		ASSERT_LOG(v.has_key("filename") && v["filename"].is_string(), "Must have at least a 'filename' key or type string");
		file_name_ = module::map_file(v["filename"].as_string());
		if(v.has_key("loop")) {
			loop_ = v["loop"].as_bool();
		}
		if(v.has_key("cancel_on_keypress")) {
			cancel_on_keypress_ = v["cancel_on_keypress"].as_bool();
		}
		init();
	}

	void vpx::init()
	{
		shader_ = gles2::shader_program::get_global("yuv12");
		u_tex_[0] = shader_->shader()->get_fixed_uniform("tex0");
		u_tex_[1] = shader_->shader()->get_fixed_uniform("tex1");
		u_tex_[2] = shader_->shader()->get_fixed_uniform("tex2");

		u_color_ = shader_->shader()->get_fixed_uniform("color");
		a_vertex_ = shader_->shader()->get_fixed_attribute("vertex");
		a_texcoord_ = shader_->shader()->get_fixed_attribute("texcoord");

		file_.open(file_name_, std::ios::in | std::ios::binary);
		ASSERT_LOG(file_.is_open(), "Unable to open file: " << file_name_);
		file_hdr_.resize(IVF_FILE_HDR_SZ);
		file_.read(&file_hdr_[0], IVF_FILE_HDR_SZ);
		ASSERT_LOG(file_hdr_[0] == 'D' && file_hdr_[1] == 'K' && file_hdr_[2] == 'I' && file_hdr_[3] == 'F', 
			"Unknown file header found: " << std::string(&file_hdr_[0], &file_hdr_[4]));
		frame_hdr_.resize(IVF_FRAME_HDR_SZ);

		auto res = vpx_codec_dec_init(&codec_, vpx_codec_vp8_dx(), NULL, flags_);
		ASSERT_LOG(res == 0, "Codec error: " << vpx_codec_error(&codec_));

		frame_.resize(256 * 1024);
		playing_ = true;
		iter_ = NULL;
	}

	vpx::~vpx() 
	{
	}

	void vpx::gen_textures()
	{
		ASSERT_LOG(img_ != NULL, "img_ is null");

		texture_id_ = boost::shared_array<GLuint>(new GLuint[3], [](GLuint* ids){glDeleteTextures(3,ids); delete[] ids;});
		glGenTextures(3, &texture_id_[0]);

		if(graphics::texture::allows_npot()) {
			texture_width_ = img_->d_w;
			texture_height_ = img_->d_h;
		} else {
			texture_width_ = graphics::texture::next_power_of_2(img_->d_w);
			texture_height_ = graphics::texture::next_power_of_2(img_->d_h);
		}

		for(size_t i = 0; i != 3; ++i) {
			glBindTexture(GL_TEXTURE_2D, texture_id_[i]);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, img_->stride[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			unsigned width = i==0?texture_width_:texture_width_/2;
			unsigned height = i==0?texture_height_:texture_height_/2;
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
		}
		glBindTexture(GL_TEXTURE_2D, graphics::texture::get_current_texture());
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}

	void vpx::stop()
	{
		playing_ = false;
	}

	void vpx::decode_frame()
	{
		if(file_.eof()) {
			// when file_ has been read call vpx_codec_decode with data as NULL and sz as 0
			vpx_codec_decode(&codec_, NULL, 0, NULL, 0);
			playing_ = false;
			/// test loop_ here.
		} else {
			file_.read(reinterpret_cast<char*>(&frame_hdr_[0]), IVF_FRAME_HDR_SZ);
			frame_size_ = mem_get_le32(frame_hdr_);
			++frame_cnt_;

			frame_.resize(frame_size_);
			file_.read(reinterpret_cast<char*>(&frame_[0]), frame_size_);

			auto res = vpx_codec_decode(&codec_, &frame_[0], frame_size_, NULL, 0);
			ASSERT_LOG(res == 0, "Codec error: " << vpx_codec_error(&codec_) << " : " << vpx_codec_error_detail(&codec_));
		}
	}

	void vpx::handle_process()
	{
		if(!playing_) {
			return;
		}

		bool done = false;
		while(playing_ && !done) {
			if(img_ == NULL) {
				decode_frame();
				iter_ = NULL;
			}

			img_ = vpx_codec_get_frame(&codec_, &iter_);
			if(img_ != NULL) {
				done = true;
			}
		}

		if(!texture_id_) {
			gen_textures();
		}
	}

	bool vpx::handle_event(const SDL_Event& evt, bool claimed)
	{
		if(claimed) {
			return true;
		}
		if(!cancel_on_keypress_) {
			return claimed;
		}

		switch(evt.type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				stop();
				claimed = true;
				break;
			
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if(in_widget(evt.button.x, evt.button.y)) {
					stop();
					claimed = true;
				}
				break;
		}
		return claimed;
	}

	void vpx::handle_draw() const
	{
		if(img_ == NULL) {
			return;
		}

		manager m(shader_->shader());

		glUniform4f(u_color_, 1.0f, 1.0f, 1.0f, 1.0f);

		glDisable(GL_BLEND);

		for(int i = 2; i >= 0; --i) {
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, texture_id_[i]);
			void* pixels = img_->planes[i];
			glPixelStorei(GL_UNPACK_ROW_LENGTH, img_->stride[i]);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, i>0?img_->d_w/2:img_->d_w, i>0?img_->d_h/2:img_->d_h, GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);
			glUniform1i(u_tex_[i], i);
		}

		const int w_odd = width() % 2;
		const int h_odd = height() % 2;
		const int w = width() / 2;
		const int h = height() / 2;

		glPushMatrix();
		glTranslatef((x()+w)&preferences::xypos_draw_mask, (y()+h)&preferences::xypos_draw_mask, 0.0f);
		glUniformMatrix4fv(shader_->shader()->mvp_matrix_uniform(), 1, GL_FALSE, glm::value_ptr(gles2::get_mvp_matrix()));

		const GLfloat varray[8] = {
			(GLfloat)-w, (GLfloat)-h,
			(GLfloat)-w, (GLfloat)h+h_odd,
			(GLfloat)w+w_odd, (GLfloat)-h,
			(GLfloat)w+w_odd, (GLfloat)h+h_odd
		};

		const GLfloat tcx = 0.0f;
		const GLfloat tcy = 0.0f;
		const GLfloat tcx2 = GLfloat(img_->d_w)/texture_width_;
		const GLfloat tcy2 = GLfloat(img_->d_h)/texture_height_;

		const GLfloat tcarray[8] = {
			tcx, tcy,
			tcx, tcy2,
			tcx2, tcy,
			tcx2, tcy2,
		};
		glEnableVertexAttribArray(a_vertex_);
		glEnableVertexAttribArray(a_texcoord_);
		glVertexAttribPointer(a_vertex_, 2, GL_FLOAT, 0, 0, varray);
		glVertexAttribPointer(a_texcoord_, 2, GL_FLOAT, 0, 0, tcarray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDisableVertexAttribArray(a_vertex_);
		glDisableVertexAttribArray(a_texcoord_);

		glPopMatrix();

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, graphics::texture::get_current_texture());
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		glEnable(GL_BLEND);
	}
}

#endif
