/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if defined(USE_ISOMAP)

#include "asserts.hpp"
#include "level.hpp"
#include "preferences.hpp"
#include "view3d_widget.hpp"
#include "widget_factory.hpp"

namespace gui
{
	view3d_widget::view3d_widget(int x, int y, int width, int height)
		: tex_width_(0), tex_height_(0), camera_distance_(-150.0f)
	{
		setLoc(x, y);
		setDim(width, height);

		init();
	}

	view3d_widget::view3d_widget(const variant& v, game_logic::FormulaCallable* e)
		: widget(v,e), tex_width_(0), tex_height_(0), camera_distance_(float(v["camera_distance"].as_decimal(decimal(-150.0)).as_float()))
	{
		
		init();

		if(v.has_key("camera")) {
			camera_.reset(new camera_callable(v["camera"]));
		}

		if(v.has_key("children")) {
			reset_contents(v["children"]);
		}
	}

	void view3d_widget::reset_contents(const variant& v)
	{
		children_.clear();
		if(v.is_null()) {
			return;
		}
		if(v.is_list()) {
			for(int n = 0; n != v.num_elements(); ++n) {
				children_.push_back(widget_factory::create(v[n],getEnvironment()));
			}
		} else {
			children_.push_back(widget_factory::create(v,getEnvironment()));
		}
	}

	view3d_widget::~view3d_widget()
	{
	}

	void view3d_widget::init()
	{
		camera_.reset(new camera_callable);
		camera_->set_fov(60.0);
		camera_->set_aspect(float(width())/float(height()));

		proj_2d_ = glm::ortho(0.0f, float(preferences::actual_screen_width()), float(preferences::actual_screen_height()), 0.0f);

		tex_width_ = graphics::texture::allows_npot() ? width() : graphics::texture::next_power_of_2(width());
		tex_height_ = graphics::texture::allows_npot() ? height() : graphics::texture::next_power_of_2(height());

		texture_ = boost::shared_ptr<GLuint>(new GLuint, [](GLuint* id){glDeleteTextures(1,id); delete id;});
		glGenTextures(1, texture_.get());
		glBindTexture(GL_TEXTURE_2D, *texture_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width_, tex_height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		fbo_ = boost::shared_ptr<GLuint>(new GLuint, [](GLuint* id){glDeleteFramebuffers(1, id); delete id;});
		glGenFramebuffers(1, fbo_.get());
		glBindFramebuffer(GL_FRAMEBUFFER, *fbo_);

		// attach the texture to FBO color attachment point
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *texture_, 0);

		depth_id_ = boost::shared_ptr<GLuint>(new GLuint, [](GLuint* id){glBindRenderbuffer(GL_RENDERBUFFER, 0); glDeleteRenderbuffers(1, id); delete id;});
		glGenRenderbuffers(1, depth_id_.get());
		glBindRenderbuffer(GL_RENDERBUFFER, *depth_id_);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, tex_width_, tex_height_);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, *depth_id_);

		// check FBO status
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		ASSERT_NE(status, GL_FRAMEBUFFER_UNSUPPORTED);
		ASSERT_EQ(status, GL_FRAMEBUFFER_COMPLETE);
	}

	void view3d_widget::render_fbo() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, *fbo_);

		//set up the raster projection.
		glViewport(0, 0, width(), height());
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//start drawing here.
		if(level::current().iso_world()) {
			level::current().iso_world()->draw(camera_);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height());
	}

	void view3d_widget::render_texture() const
	{
		gles2::manager gles2_manager(gles2::shader_program::get_global("texture2d"));

		GLint cur_id = graphics::texture::get_currentTexture();
		glBindTexture(GL_TEXTURE_2D, *texture_);

		const int w_odd = width() % 2;
		const int h_odd = height() % 2;
		const int w = width() / 2;
		const int h = height() / 2;

		glm::mat4 mvp = proj_2d_ * glm::translate(glm::mat4(1.0f), glm::vec3(x()+w, y()+h, 0.0f));
		glUniformMatrix4fv(gles2::active_shader()->shader()->mvp_matrix_uniform(), 1, GL_FALSE, glm::value_ptr(mvp));

		GLfloat varray[] = {
			GLfloat(-w), GLfloat(-h),
			GLfloat(-w), GLfloat(h+h_odd),
			GLfloat(w+w_odd), GLfloat(-h),
			GLfloat(w+w_odd), GLfloat(h+h_odd)
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

	void view3d_widget::handleDraw() const
	{
		// render to fbo
		render_fbo();

		// render texture
		render_texture();

		for(auto child : children_) {
			child->draw();
		}
	}

	bool view3d_widget::handleEvent(const SDL_Event& event, bool claimed)
	{
	    for(auto child : children_) {
			claimed = child->processEvent(event, claimed);
			if(claimed) {
				return true;
			}
		}
		return false;
	}

	void view3d_widget::handleProcess()
	{
		glm::vec3 cam_pos = level::current().camera()->position() + level::current().camera()->direction() * camera_distance_;
		camera_->look_at(cam_pos, level::current().camera()->position(), glm::vec3(0,1,0));

	    for(auto child : children_) {
			child->process();
		}
	}

	BEGIN_DEFINE_CALLABLE(view3d_widget, widget)
	DEFINE_FIELD(children, "[widget]")
		std::vector<variant> v;
	    for(auto w : obj.children_) {
			v.push_back(variant(w.get()));
		}
		return variant(&v);
	DEFINE_SET_FIELD_TYPE("list|map")
		obj.reset_contents(value);
	DEFINE_FIELD(camera, "builtin camera_callable")
		return variant(obj.camera_.get());
	DEFINE_FIELD(camera_distance, "decimal")
		return variant(obj.camera_distance_);
	DEFINE_SET_FIELD
		obj.camera_distance_ = float(value.as_decimal().as_float());
	END_DEFINE_CALLABLE(view3d_widget)
}

#endif
