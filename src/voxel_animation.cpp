/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

/* XXX -- needs a re-write

#include <boost/shared_array.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include "button.hpp"
#include "dialog.hpp"
#include "grid_widget.hpp"
#include "input.hpp"
#include "label.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "unit_test.hpp"
#include "voxel_model.hpp"

#define EXT_CALL(call) call
#define EXT_MACRO(macro) macro

namespace {
using namespace voxel;
using namespace gui;

class animation_renderer : public gui::widget
{
public:
	animation_renderer(const rect& area, const std::string& fname);
	void init();

	void set_animation(const std::string& anim);

	const voxel_model& model() const { return *vox_model_; }
private:
	void handleDraw() const override;
	void handleProcess() override;
	bool handleEvent(const SDL_Event& event, bool claimed) override;

	void calculate_camera();

	void render_fbo();

	boost::shared_array<GLuint> fbo_texture_ids_;
	glm::mat4 fbo_proj_;
	std::shared_ptr<GLuint> framebuffer_id_;
	std::shared_ptr<GLuint> depth_id_;

	graphics::lighting_ptr lighting_;

	gles2::program_ptr shader_;

	GLint video_framebuffer_id_;

	ffl::IntrusivePtr<camera_callable> camera_;
	GLfloat camera_hangle_, camera_vangle_, camera_distance_;

	bool focused_, dragging_view_;

	int tex_width_, tex_height_;

	ffl::IntrusivePtr<voxel_model> vox_model_;

};

animation_renderer::animation_renderer(const rect& area, const std::string& fname)
  : video_framebuffer_id_(0),
    camera_(new camera_callable),
    camera_hangle_(0.12), camera_vangle_(1.25), camera_distance_(20.0),
	focused_(false), dragging_view_(false),
    tex_width_(0), tex_height_(0)
{
	std::map<variant,variant> items;
	items[variant("model")] = variant(fname);
	vox_model_.reset(new voxel_model(variant(&items)));
	vox_model_->set_animation("stand");

	items[variant("model")] = variant("modules/ftactics/sword.cfg");

	ffl::IntrusivePtr<voxel_model> weapon(new voxel_model(variant(&items)));
	weapon->set_animation("stand");

	vox_model_->attach_child(weapon, "handle", "melee_weapon");

	shader_ = gles2::shader_program::get_global("lighted_color_shader")->shader();
	//lighting_.reset(new graphics::lighting(shader_));
	//lighting_->set_light_position(0, glm::vec3(0.0f, 0.0f, 50.0f));
	//lighting_->set_light_power(0, 2000.0f);

	setLoc(area.x(), area.y());
	setDim(area.w(), area.h());

	init();
	calculate_camera();
}

void animation_renderer::init()
{
	fbo_proj_ = glm::ortho(0.0f, float(preferences::actual_screen_width()), float(preferences::actual_screen_height()), 0.0f);

	tex_width_ = graphics::texture::allows_npot() ? width() : graphics::texture::next_power_of_2(width());
	tex_height_ = graphics::texture::allows_npot() ? height() : graphics::texture::next_power_of_2(height());

	glGetIntegerv(EXT_MACRO(GL_FRAMEBUFFER_BINDING), &video_framebuffer_id_);
	fprintf(stderr, "Init: %dx%d %d\n", tex_width_, tex_height_, (int)video_framebuffer_id_);

	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);

	fbo_texture_ids_ = boost::shared_array<GLuint>(new GLuint[1], [](GLuint* id){glDeleteTextures(1,id); delete[] id;});
	glGenTextures(1, &fbo_texture_ids_[0]);
	glBindTexture(GL_TEXTURE_2D, fbo_texture_ids_[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width_, tex_height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	framebuffer_id_ = std::shared_ptr<GLuint>(new GLuint, [](GLuint* id){glDeleteFramebuffers(1, id); delete id;});
	EXT_CALL(glGenFramebuffers)(1, framebuffer_id_.get());
	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), *framebuffer_id_);

	// attach the texture to FBO color attachment point
	EXT_CALL(glFramebufferTexture2D)(EXT_MACRO(GL_FRAMEBUFFER), EXT_MACRO(GL_COLOR_ATTACHMENT0),
                          GL_TEXTURE_2D, fbo_texture_ids_[0], 0);
	depth_id_ = std::shared_ptr<GLuint>(new GLuint, [](GLuint* id){glBindRenderbuffer(GL_RENDERBUFFER, 0); glDeleteRenderbuffers(1, id); delete id;});
	glGenRenderbuffers(1, depth_id_.get());
	glBindRenderbuffer(GL_RENDERBUFFER, *depth_id_);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, tex_width_, tex_height_);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, *depth_id_);
	ASSERT_EQ(glGetError(), 0);

	// check FBO status
	GLenum status = EXT_CALL(glCheckFramebufferStatus)(EXT_MACRO(GL_FRAMEBUFFER));
	ASSERT_EQ(glGetError(), 0);
	ASSERT_NE(status, EXT_MACRO(GL_FRAMEBUFFER_UNSUPPORTED));
	ASSERT_NE(status, EXT_MACRO(GL_FRAMEBUFFER_UNDEFINED));
	ASSERT_NE(status, EXT_MACRO(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT));
	ASSERT_NE(status, EXT_MACRO(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT));
	ASSERT_NE(status, EXT_MACRO(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER));
	ASSERT_NE(status, EXT_MACRO(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER));
	ASSERT_EQ(status, EXT_MACRO(GL_FRAMEBUFFER_COMPLETE));
}

void animation_renderer::set_animation(const std::string& anim)
{
	vox_model_->set_animation(anim);
}

void animation_renderer::render_fbo()
{
	//std::cerr << "render fbo\n";
	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), *framebuffer_id_);

	//set up the raster projection.
	glViewport(0, 0, width(), height());

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);

	//start drawing here.
	gles2::shader_program_ptr shader_program(gles2::shader_program::get_global("lighted_color_shader"));
	gles2::program_ptr shader = shader_program->shader();
	gles2::actives_map_iterator mvp_uniform_itor = shader->get_uniform_reference("mvp_matrix");

	gles2::manager gles2_manager(shader_program);

	glm::mat4 model_matrix(1.0f);

	glm::mat4 mvp = camera_->projection_mat() * camera_->view_mat() * model_matrix;

	shader->set_uniform(mvp_uniform_itor, 1, glm::value_ptr(mvp));

	if(lighting_) {
		lighting_->set_modelview_matrix(model_matrix, camera_->view_mat());
	}

	std::vector<GLfloat> varray, carray, narray;

	const GLfloat axes_vertex[] = {
		0.0, 0.0, 0.0,
		0.0, 0.0, 10.0,
		0.0, 0.0, 0.0,
		0.0, 10.0, 0.0,
		0.0, 0.0, 0.0,
		10.0, 0.0, 0.0,
	};

	for(int n = 0; n != sizeof(axes_vertex)/sizeof(*axes_vertex); ++n) {
		varray.push_back(axes_vertex[n]);
		if(n%3 == 0) {
			carray.push_back(1.0);
			carray.push_back(1.0);
			carray.push_back(1.0);
			carray.push_back(1.0);
		}
	}

	gles2::active_shader()->shader()->vertex_array(3, GL_FLOAT, 0, 0, &varray[0]);
	gles2::active_shader()->shader()->color_array(4, GL_FLOAT, 0, 0, &carray[0]);
	glDrawArrays(GL_LINES, 0, varray.size()/3);

	vox_model_->process_animation();
	vox_model_->draw(lighting_, camera_, model_matrix);

	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), video_framebuffer_id_);

	glViewport(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height());

	glDisable(GL_DEPTH_TEST);
}

void animation_renderer::handleDraw() const
{
	//std::cerr << "draw anim...\n";
	gles2::manager gles2_manager(gles2::shader_program::get_global("texture2d"));

	GLint cur_id = graphics::texture::get_currentTexture();
	glBindTexture(GL_TEXTURE_2D, fbo_texture_ids_[0]);

	const int w_odd = width() % 2;
	const int h_odd = height() % 2;
	const int w = width() / 2;
	const int h = height() / 2;

	glm::mat4 mvp = fbo_proj_ * glm::translate(glm::mat4(1.0f), glm::vec3(x()+w, y()+h, 0.0f));
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

	glPushMatrix();
	glTranslatef(x(), y(), 0.0f);
	glPopMatrix();
}

void animation_renderer::handleProcess()
{
	int num_keys = 0;
	const Uint8* keystate = SDL_GetKeyboardState(&num_keys);
	if(SDL_SCANCODE_Z < num_keys && keystate[SDL_SCANCODE_Z]) {
		camera_distance_ -= 0.2;
		if(camera_distance_ < 5.0) {
			camera_distance_ = 5.0;
		}

		calculate_camera();
	}

	if(SDL_SCANCODE_X < num_keys && keystate[SDL_SCANCODE_X]) {
		camera_distance_ += 0.2;
		if(camera_distance_ > 100.0) {
			camera_distance_ = 100.0;
		}

		calculate_camera();
	}

	render_fbo();
}

bool animation_renderer::handleEvent(const SDL_Event& event, bool claimed)
{
	switch(event.type) {
	case SDL_MOUSEBUTTONDOWN: {
		dragging_view_ = focused_;
		break;
	}

	case SDL_MOUSEMOTION: {
		const SDL_MouseMotionEvent& motion = event.motion;

		Uint8 button_state = input::sdl_get_mouse_state(nullptr, nullptr);
		if(dragging_view_ && button_state&SDL_BUTTON(SDL_BUTTON_LEFT)) {
			if(motion.xrel) {
				camera_hangle_ += motion.xrel*0.02;
			}

			if(motion.yrel) {
				camera_vangle_ += motion.yrel*0.02;
			}
			
			calculate_camera();
		}

		if(motion.x >= x() && motion.y >= y() &&
		   motion.x <= x() + width() && motion.y <= y() + height()) {
			focused_ = true;
		} else {
			focused_ = false;
		}

		break;
	}

	}

	return widget::handleEvent(event, claimed);
}

void animation_renderer::calculate_camera()
{
	const GLfloat hdist = sin(camera_vangle_)*camera_distance_;
	const GLfloat ydist = cos(camera_vangle_)*camera_distance_;

	const GLfloat xdist = sin(camera_hangle_)*hdist;
	const GLfloat zdist = cos(camera_hangle_)*hdist;

	camera_->look_at(glm::vec3(xdist, ydist, zdist), glm::vec3(0,0,0), glm::vec3(0.0, 1.0, 0.0));
}

class voxel_animation_editor : public gui::dialog
{
public:
	voxel_animation_editor(const rect& r, const std::string& fname);
	void init();
private:
	bool handleEvent(const SDL_Event& event, bool claimed) override;

	ffl::IntrusivePtr<animation_renderer> renderer_;
	rect area_;
	std::string fname_;

};

voxel_animation_editor::voxel_animation_editor(const rect& r, const std::string& fname)
  : dialog(r.x(), r.y(), r.w(), r.h()), area_(r), fname_(fname)
{
	init();
}

void voxel_animation_editor::init()
{
	clear();

	if(!renderer_) {
		renderer_.reset(new animation_renderer(rect(area_.x() + 10, area_.y() + 10, area_.w() - 200, area_.h() - 20), fname_));
	}

	addWidget(renderer_, renderer_->x(), renderer_->y());

	grid_ptr anim_grid(new grid(1));

	for(auto p : renderer_->model().animations()) {
		anim_grid->addCol(new button(new label(p.first, graphics::color("antique_white").as_sdl_color(), 14, "Montaga-Regular"), std::bind(&animation_renderer::set_animation, renderer_.get(), p.first)));
	}

	addWidget(anim_grid, renderer_->x() + renderer_->width() + 10, renderer_->y());
}

bool voxel_animation_editor::handleEvent(const SDL_Event& event, bool claimed)
{
	return dialog::handleEvent(event, claimed);
}

}

UTILITY(voxel_animator)
{
	std::deque<std::string> arguments(args.begin(), args.end());

	ASSERT_LOG(arguments.size() <= 1, "Unexpected arguments");

	std::string fname;
	if(arguments.empty() == false) {
		fname = module::map_file(arguments.front());
	}

	ffl::IntrusivePtr<voxel_animation_editor> editor(new voxel_animation_editor(rect(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height()), fname));
	editor->showModal();
}
*/
