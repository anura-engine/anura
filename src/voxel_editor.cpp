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
#include <boost/array.hpp>
#include <boost/shared_array.hpp>
#include "intrusive_ptr.hpp"

#include <assert.h>

#include <algorithm>
#include <deque>
#include <numeric>
#include <map>
#include <vector>

#include "SDL.h"

#include "border_widget.hpp"
#include "button.hpp"
#include "checkbox.hpp"
#include "color_picker.hpp"
#include "dialog.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "grid_widget.hpp"
#include "gui_section.hpp"
#include "input.hpp"
#include "isochunk.hpp"
#include "json_parser.hpp"
#include "label.hpp"
#include "level_runner.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "slider.hpp"
#include "unit_test.hpp"
#include "voxel_model.hpp"

#if defined(_MSC_VER)
#include <boost/math/special_functions/round.hpp>
#define bmround	boost::math::round
#else
#define bmround	round
#endif

#define EXT_CALL(call) call
#define EXT_MACRO(macro) macro

using namespace voxel;

namespace {

struct Command {
	Command(std::function<void()> redo_fn, std::function<void()> undo_fn)
	  : redo(redo_fn), undo(undo_fn)
	{}
	std::function<void()> redo, undo;
};

const char* ToolIcons[] = {
	"editor_pencil",
	"editor_add_object",
	"editor_eyedropper",
	"editor_rect_select",
	"editor_flood",
	nullptr
};

enum VOXEL_TOOL {
	TOOL_PENCIL,
	TOOL_PENCIL_ABOVE,
	TOOL_PICKER,
	TOOL_SELECT,
	TOOL_FLOOD,
	NUM_VOXEL_TOOLS,
};

class iso_renderer;

class voxel_editor : public gui::dialog
{
public:
	voxel_editor(const rect& r, const std::string& fname);
	~voxel_editor();
	void init();

	const VoxelMap& voxels() const { return voxels_; }

	void set_voxel(const VoxelPos& pos, const Voxel& voxel);
	void delete_voxel(const VoxelPos& pos);
	bool setCursor(const VoxelPos& pos);

	const VoxelPos* get_cursor() const { return cursor_.get(); }
	const VoxelArea* get_selection() const { return selection_.get(); }

	void setSelection(const VoxelArea& area) { selection_.reset(new VoxelArea(area)); }
	void clear_selection() { selection_.reset(); }

	VoxelPos get_selected_voxel(const VoxelPos& pos, int facing, bool reverse);

	graphics::color current_color() const { return ColorPicker_->getSelectedColor(); }
	gui::ColorPicker& get_ColorPicker() { return *ColorPicker_; }
	Layer& layer() { assert(current_layer_ >= 0 && current_layer_ < layers_.size()); return layers_[current_layer_]; }

	const std::vector<VoxelPair>& get_clipboard() const { return clipboard_; }
	void set_clipboard(const std::vector<VoxelPair>& value) { clipboard_ = value; }

	int nhighlight_layer() const { return highlight_layer_; }

	VOXEL_TOOL tool() const {
		const bool ctrl = (SDL_GetModState()&KMOD_CTRL) != 0;
		const bool shift = (SDL_GetModState()&KMOD_SHIFT) != 0;
		if(shift && tool_ == TOOL_PENCIL) {
			return TOOL_PENCIL_ABOVE;
		} else if(ctrl && (tool_ == TOOL_PENCIL || tool_ == TOOL_PENCIL_ABOVE)) {
			return TOOL_PICKER;
		}

		return tool_;
	}

	void executeCommand(std::function<void()> redo, std::function<void()> undo);
	void executeCommand(const Command& cmd);

	void build_voxels();

private:
	bool handleEvent(const SDL_Event& event, bool claimed) override;

	void on_color_changed(const graphics::color& color);
	void on_change_layer_button_clicked(int nlayer);


	void select_tool(VOXEL_TOOL tool);

	void set_symmetric(bool value);

	void mouseover_layer(int nlayer);
	void select_layer(int nlayer, gui::grid* layer_grid);

	void onSave();
	void undo();
	void redo();

	void handleProcess() override;

	const Layer& layer() const { return layers_[current_layer_]; }

	rect area_;

	int current_layer_, highlight_layer_;
	std::vector<Layer> layers_;
	Model model_;
	VoxelMap voxels_;

	std::vector<VoxelPair> clipboard_;

	std::unique_ptr<VoxelPos> cursor_;

	std::unique_ptr<VoxelArea> selection_;

	gui::LabelPtr pos_label_;

	std::string fname_;

	ffl::IntrusivePtr<iso_renderer> iso_renderer_;
	ffl::IntrusivePtr<gui::ColorPicker> ColorPicker_;

	std::vector<Command> undo_, redo_;

	VOXEL_TOOL tool_;

	std::vector<gui::BorderWidget*> tool_borders_;

	bool symmetric_;
};

voxel_editor* g_voxel_editor;

voxel_editor& get_editor() {
	assert(g_voxel_editor);
	return *g_voxel_editor;
}

void expand_flood(std::set<VoxelPos, VoxelPosLess>& flood, const Voxel& vox, const VoxelMap& map)
{
	bool found = false;
	std::set<VoxelPos, VoxelPosLess> flood_copy = flood;
	for(const VoxelPos& pos : flood_copy) {
		VoxelPos adj[6];
		get_voxel_pos_adjacent(pos, adj);
		for(const VoxelPos& adj_pos : adj) {
			if(flood.count(adj_pos) == 0) {
				auto itor = map.find(adj_pos);
				if(itor != map.end() && itor->second.color == vox.color) {
					flood.insert(adj_pos);
					found = true;
				}
			}
		}
	}
	
	if(found) {
		expand_flood(flood, vox, map);
	}
}

void flood_voxel()
{
	if(get_editor().get_cursor()) {
		VoxelPos cursor = *get_editor().get_cursor();
		const Layer& layer = get_editor().layer();
		if(layer.map.count(cursor) == 0) {
			return;
		}

		Voxel vox = layer.map.find(cursor)->second;

		std::set<VoxelPos, VoxelPosLess> flood;
		flood.insert(cursor);
		expand_flood(flood, vox, layer.map);
		fprintf(stderr, "FLOOD: %d\n", (int)flood.size());

		VoxelMap old_values;
		for(const VoxelPos& pos : flood) {
			old_values.insert(*layer.map.find(pos));
		}

		vox.color = get_editor().current_color();

		get_editor().executeCommand(
			[=]() {
				for(const VoxelPos& pos : flood) {
					get_editor().set_voxel(pos, vox);
				}
			},
			[=]() {
				for(auto p : old_values) {
					get_editor().set_voxel(p.first, p.second);
				}
			}
		);
	}
}

void pencil_voxel()
{
	if(get_editor().get_cursor()) {
		VoxelPos cursor = *get_editor().get_cursor();
		Voxel voxel;
		voxel.color = get_editor().current_color();

		Voxel old_voxel;
		bool currently_has_voxel = false;

		auto current_itor = get_editor().layer().map.find(cursor);
		if(current_itor != get_editor().layer().map.end()) {
			old_voxel = current_itor->second;
			currently_has_voxel = true;
		}

		get_editor().executeCommand(
		  [cursor, voxel]() { get_editor().set_voxel(cursor, voxel); },
		  [cursor, old_voxel, currently_has_voxel]() {
			if(currently_has_voxel) {
				get_editor().set_voxel(cursor, old_voxel);
			} else {
				get_editor().delete_voxel(cursor);
			}
		});
	}
}

void delete_voxel()
{
	if(get_editor().get_cursor()) {
		VoxelPos cursor = *get_editor().get_cursor();
		auto current_itor = get_editor().layer().map.find(cursor);
		if(current_itor == get_editor().layer().map.end()) {
			return;
		}

		Voxel old_voxel = current_itor->second;

		get_editor().executeCommand(
			[cursor]() { get_editor().delete_voxel(cursor); },
			[cursor, old_voxel]() { get_editor().set_voxel(cursor, old_voxel); }
		);
	}
}

using namespace gui;

class iso_renderer : public gui::widget
{
public:
	explicit iso_renderer(const rect& area);
	~iso_renderer();
	void init();
	void handleDraw() const override;

	bool maximized() const { return maximized_; }

	const camera_callable& camera() const { return *camera_; }
private:
	void maximize();
	void handleProcess() override;
	bool handleEvent(const SDL_Event& event, bool claimed) override;

	glm::ivec3 position_to_cube(int xp, int yp, glm::ivec3* facing);

	void render_fbo();
	ffl::IntrusivePtr<camera_callable> camera_;
	GLfloat camera_hangle_, camera_vangle_, camera_distance_;

	void calculate_camera();

	glm::mat4 fbo_proj_;
	boost::shared_array<GLuint> framebuffer_id_;
	boost::shared_array<GLuint> render_buffer_id_;
	boost::shared_array<GLuint> final_texture_id_;

	boost::array<GLfloat, 3> vector_;

	GLuint u_lightposition_;
	GLuint u_lightpower_;
	GLuint u_shininess_;
	GLuint u_m_matrix_;
	GLuint u_v_matrix_;
	GLuint a_normal_;

	size_t tex_width_;
	size_t tex_height_;
	GLint video_framebuffer_id_;

	SliderPtr light_power_slider_;
	void light_power_SliderChange(double p);
	float light_power_;
	float specularity_coef_;

	bool focused_;
	bool dragging_view_;

	ButtonPtr maximize_button_;
	bool maximized_;

	iso_renderer();
	iso_renderer(const iso_renderer&);
};

iso_renderer* g_iso_renderer;
iso_renderer& get_iso_renderer() {
	assert(g_iso_renderer);
	return *g_iso_renderer;
}

iso_renderer::iso_renderer(const rect& area)
  : video_framebuffer_id_(0),
    camera_(new camera_callable),
    camera_hangle_(0.12), camera_vangle_(1.25), camera_distance_(20.0),
	tex_width_(0), tex_height_(0),
	focused_(false), dragging_view_(false), light_power_(10000.0f), specularity_coef_(5.0f), maximized_(false)
{
	camera_->set_clip_planes(0.1f, 200.0f);
	g_iso_renderer = this;
	setLoc(area.x(), area.y());
	setDim(area.w(), area.h());
	vector_[0] = 1.0;
	vector_[1] = 1.0;
	vector_[2] = 1.0;

	calculate_camera();

	light_power_slider_.reset(new Slider(150, std::bind(&iso_renderer::light_power_SliderChange, this, _1), 1));
	light_power_slider_->setPosition(light_power_/20000.0);

	init();
}

iso_renderer::~iso_renderer()
{
	if(g_iso_renderer == this) {
		g_iso_renderer = nullptr;
	}
}

void iso_renderer::light_power_SliderChange(double p)
{
	light_power_ = float(p * 20000.0);
}

void iso_renderer::calculate_camera()
{
	const GLfloat hdist = sin(camera_vangle_)*camera_distance_;
	const GLfloat ydist = cos(camera_vangle_)*camera_distance_;

	const GLfloat xdist = sin(camera_hangle_)*hdist;
	const GLfloat zdist = cos(camera_hangle_)*hdist;

	camera_->look_at(glm::vec3(xdist, ydist, zdist), glm::vec3(0,0,0), glm::vec3(0.0, 1.0, 0.0));
}

void iso_renderer::handleDraw() const
{
	gles2::manager gles2_manager(gles2::shader_program::get_global("texture2d"));

	GLint cur_id = graphics::texture::get_currentTexture();
	glBindTexture(GL_TEXTURE_2D, final_texture_id_[0]);

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
	if(light_power_slider_) {
			light_power_slider_->draw();
	}
	glPopMatrix();

	maximize_button_->draw();
}

void iso_renderer::handleProcess()
{
	maximize_button_->setLoc(x() + 5, y() + 5);
	maximize_button_->process();

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

namespace 
{
	std::string facing_name(const glm::ivec3& facing)
	{
		if(facing.x > 0) {
			return "right";
		} else if(facing.x < 0) {
			return "left";
		}
		if(facing.y > 0) {
			return "top";
		} else if(facing.y < 0) {
			return "bottom";
		}
		if(facing.z > 0) {
			return "front";
		} else if(facing.z < 0) {
			return "back";
		}
		return "unknown";
	}
}

glm::ivec3 iso_renderer::position_to_cube(int xp, int yp, glm::ivec3* facing)
{
	// Before calling screen_to_world we need to bind the fbo
	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), framebuffer_id_[0]);
	glm::vec3 world_coords = camera_->screen_to_world(xp, yp, width(), height());
	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), video_framebuffer_id_);
	glm::ivec3 voxel_coord = glm::ivec3(
		abs(world_coords[0]-bmround(world_coords[0])) < 0.05f ? int(bmround(world_coords[0])) : int(floor(world_coords[0])),
		abs(world_coords[1]-bmround(world_coords[1])) < 0.05f ? int(bmround(world_coords[1])) : int(floor(world_coords[1])),
		abs(world_coords[2]-bmround(world_coords[2])) < 0.05f ? int(bmround(world_coords[2])) : int(floor(world_coords[2])));
	*facing = camera_->get_facing(world_coords);
	if(facing->x > 0) {
		--voxel_coord.x; 
	}
	if(facing->y > 0) {
		--voxel_coord.y; 
	}
	if(facing->z > 0) {
		--voxel_coord.z; 
	}
	return voxel_coord;
}

bool iso_renderer::handleEvent(const SDL_Event& event, bool claimed)
{
	claimed = maximize_button_->processEvent(getPos(), event, claimed);
	if(claimed) {
		return widget::handleEvent(event, claimed);
	}

	if(light_power_slider_) {
		SDL_Event ev(event);
		//normalizeEvent(&ev);
		if(light_power_slider_->processEvent(getPos(), ev, claimed)) {
			return claimed;
		}
	}
	switch(event.type) {
	case SDL_MOUSEWHEEL: {
		if(!focused_) {
			break;
		}

		if(event.wheel.y > 0) {
			camera_distance_ -= 5.0;
			if(camera_distance_ < 5.0) {
				camera_distance_ = 5.0;
			}

			calculate_camera();
		} else {
			camera_distance_ += 5.0;
			if(camera_distance_ > 100.0) {
				camera_distance_ = 100.0;
			}

			calculate_camera();
		}
		
		break;
	}

	case SDL_MOUSEBUTTONDOWN: {
		const SDL_MouseButtonEvent& e = event.button;

		dragging_view_ = false;
		if(focused_) {
			glm::ivec3 facing;
			glm::ivec3 voxel_coord = position_to_cube(event.button.x-x(), event.button.y-y(), &facing);

			VoxelPos pos(voxel_coord.x, voxel_coord.y, voxel_coord.z);
			VoxelPos pencil_pos(voxel_coord.x, voxel_coord.y, voxel_coord.z);
			if(SDL_GetModState()&KMOD_SHIFT) {
				pencil_pos = voxel_coord + facing;
			}

			auto it = get_editor().voxels().find(pos);
			if(it != get_editor().voxels().end()) {
				get_editor().setCursor(pencil_pos);
				if(e.button == SDL_BUTTON_LEFT) {
					pencil_voxel();
				} else if(e.button == SDL_BUTTON_RIGHT) {
					delete_voxel();
				}
			} else {
				dragging_view_ = true;
			}
		}
		break;
	}
	case SDL_MOUSEBUTTONUP: {
		dragging_view_ = false;
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
						
			glm::ivec3 facing;
			glm::ivec3 voxel_coord = position_to_cube(motion.x-x(), motion.y-y(), &facing);

			VoxelPos pos(voxel_coord.x, voxel_coord.y, voxel_coord.z);
			auto it = get_editor().voxels().find(pos);
			if(it != get_editor().voxels().end()) {
				if(SDL_GetModState()&KMOD_SHIFT) {
					pos = voxel_coord + facing;
				}
				get_editor().setCursor(pos);
			}
		} else {
			focused_ = false;
		}
		break;
	}
	}

	return widget::handleEvent(event, claimed);
}

void iso_renderer::maximize()
{
	maximized_ = !maximized_;
	get_editor().init();
}

void iso_renderer::init()
{
	light_power_slider_->setLoc((width()-light_power_slider_->width())/2, height()-light_power_slider_->height());
	maximize_button_.reset(new button("Max.", std::bind(&iso_renderer::maximize, this)));

	fbo_proj_ = glm::ortho(0.0f, float(preferences::actual_screen_width()), float(preferences::actual_screen_height()), 0.0f);

	tex_width_ = graphics::texture::allows_npot() ? width() : graphics::texture::next_power_of_2(width());
	tex_height_ = graphics::texture::allows_npot() ? height() : graphics::texture::next_power_of_2(height());

	glGetIntegerv(EXT_MACRO(GL_FRAMEBUFFER_BINDING), &video_framebuffer_id_);

	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);

	if(get_main_window()->get_configured_msaa() != 0) {
		render_buffer_id_ = boost::shared_array<GLuint>(new GLuint[2], [](GLuint* id){glBindRenderbuffer(GL_RENDERBUFFER, 0); glDeleteRenderbuffers(2, id); delete[] id;});
		glGenRenderbuffers(2, &render_buffer_id_[0]);
		glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id_[0]);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, get_main_window()->get_configured_msaa(), GL_RGBA, tex_width_, tex_height_);

		glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id_[1]);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, get_main_window()->get_configured_msaa(), GL_DEPTH_COMPONENT, tex_width_, tex_height_);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		// check FBO status
		GLenum status = EXT_CALL(glCheckFramebufferStatus)(EXT_MACRO(GL_FRAMEBUFFER));
		ASSERT_NE(status, EXT_MACRO(GL_FRAMEBUFFER_UNSUPPORTED));
		ASSERT_EQ(status, EXT_MACRO(GL_FRAMEBUFFER_COMPLETE));

		// Create Other FBO
		final_texture_id_ = boost::shared_array<GLuint>(new GLuint[2], [](GLuint* id){glDeleteTextures(2,id); delete[] id;});
		glGenTextures(2, &final_texture_id_[0]);
		glBindTexture(GL_TEXTURE_2D, final_texture_id_[0]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width_, tex_height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
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
		glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, tex_width_, tex_height_, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr );
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
		// XXX
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

	// Grab uniforms and normal attribute
	gles2::program_ptr shader = gles2::shader_program::get_global("iso_color_line")->shader();
	u_lightposition_ = shader->get_uniform("LightPosition_worldspace");
	u_lightpower_ = shader->get_uniform("LightPower");
	u_shininess_ = shader->get_uniform("Shininess");
	u_m_matrix_ = shader->get_uniform("m_matrix");
	u_v_matrix_ = shader->get_uniform("v_matrix");
	a_normal_ = shader->get_attribute("a_normal");
}

void iso_renderer::render_fbo()
{
	if(get_main_window()->get_configured_msaa() != 0) {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_[1]);
	} else {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_[0]);
	}

	//set up the raster projection.
	glViewport(0, 0, width(), height());

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);

	//start drawing here.
	gles2::shader_program_ptr shader_program(gles2::shader_program::get_global("iso_color_line"));
	gles2::program_ptr shader = shader_program->shader();
	gles2::actives_map_iterator mvp_uniform_itor = shader->get_uniform_reference("mvp_matrix");

	gles2::manager gles2_manager(shader_program);

	glm::mat4 model_matrix(1.0f);

	glm::mat4 mvp = camera_->projection_mat() * camera_->view_mat() * model_matrix;

	shader->set_uniform(mvp_uniform_itor, 1, glm::value_ptr(mvp));

	////////////////////////////////////////////////////////////////////////////
	// Lighting stuff.
	glUniform3f(u_lightposition_, 0.0f, 20.0f, 150.0f);
	glUniform1f(u_lightpower_, light_power_);
	glUniform1f(u_shininess_, specularity_coef_);
	glUniformMatrix4fv(u_m_matrix_, 1, GL_FALSE, glm::value_ptr(model_matrix));
	glUniformMatrix4fv(u_v_matrix_, 1, GL_FALSE, camera_->view());
	////////////////////////////////////////////////////////////////////////////

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

	if(get_editor().get_cursor()) {
		const VoxelPos& cursor = *get_editor().get_cursor();
		const GLfloat cursor_vertex[] = {
			(GLfloat)cursor[0], (GLfloat)cursor[1], (GLfloat)cursor[2],
			(GLfloat)cursor[0]+1.0f, (GLfloat)cursor[1], (GLfloat)cursor[2],
			(GLfloat)cursor[0]+1.0f, (GLfloat)cursor[1], (GLfloat)cursor[2],
			(GLfloat)cursor[0]+1.0f, (GLfloat)cursor[1]+1.0f, (GLfloat)cursor[2],
			(GLfloat)cursor[0]+1.0f, (GLfloat)cursor[1]+1.0f, (GLfloat)cursor[2],
			(GLfloat)cursor[0], (GLfloat)cursor[1]+1.0f, (GLfloat)cursor[2],
			(GLfloat)cursor[0], (GLfloat)cursor[1]+1.0f, (GLfloat)cursor[2],
			(GLfloat)cursor[0], (GLfloat)cursor[1], (GLfloat)cursor[2],

			(GLfloat)cursor[0], (GLfloat)cursor[1], (GLfloat)cursor[2]+1.0f,
			(GLfloat)cursor[0]+1.0f, (GLfloat)cursor[1], (GLfloat)cursor[2]+1.0f,
			(GLfloat)cursor[0]+1.0f, (GLfloat)cursor[1], (GLfloat)cursor[2]+1.0f,
			(GLfloat)cursor[0]+1.0f, (GLfloat)cursor[1]+1.0f, (GLfloat)cursor[2]+1.0f,
			(GLfloat)cursor[0]+1.0f, (GLfloat)cursor[1]+1.0f, (GLfloat)cursor[2]+1.0f,
			(GLfloat)cursor[0], (GLfloat)cursor[1]+1.0f, (GLfloat)cursor[2]+1.0f,
			(GLfloat)cursor[0], (GLfloat)cursor[1]+1.0f, (GLfloat)cursor[2]+1.0f,
			(GLfloat)cursor[0], (GLfloat)cursor[1], (GLfloat)cursor[2]+1.0f,

			(GLfloat)cursor[0], (GLfloat)cursor[1], (GLfloat)cursor[2],
			(GLfloat)cursor[0], (GLfloat)cursor[1], (GLfloat)cursor[2]+1.0f,
			(GLfloat)cursor[0]+1.0f, (GLfloat)cursor[1], (GLfloat)cursor[2],
			(GLfloat)cursor[0]+1.0f, (GLfloat)cursor[1], (GLfloat)cursor[2]+1.0f,
			(GLfloat)cursor[0]+1.0f, (GLfloat)cursor[1]+1.0f, (GLfloat)cursor[2],
			(GLfloat)cursor[0]+1.0f, (GLfloat)cursor[1]+1.0f, (GLfloat)cursor[2]+1.0f,
			(GLfloat)cursor[0], (GLfloat)cursor[1]+1.0f, (GLfloat)cursor[2],
			(GLfloat)cursor[0], (GLfloat)cursor[1]+1.0f, (GLfloat)cursor[2]+1.0f,
		};

		for(int n = 0; n != sizeof(cursor_vertex)/sizeof(*cursor_vertex); ++n) {
			varray.push_back(cursor_vertex[n]);
			if(n%3 == 0) {
				carray.push_back(1.0);
				carray.push_back(1.0);
				carray.push_back(0.0);
				carray.push_back(1.0);
			}
		}
	}

	gles2::active_shader()->shader()->vertex_array(3, GL_FLOAT, 0, 0, &varray[0]);
	gles2::active_shader()->shader()->color_array(4, GL_FLOAT, 0, 0, &carray[0]);
	glDrawArrays(GL_LINES, 0, varray.size()/3);

	varray.clear();
	carray.clear();
	narray.clear();
	
	for(const VoxelPair& p : get_editor().voxels()) {
		const VoxelPos& pos = p.first;

		const GLfloat vertex[] = {
			0, 0, 0, // back face lower
			1, 0, 0,
			1, 1, 0,

			0, 0, 0, // back face upper
			0, 1, 0,
			1, 1, 0,

			0, 0, 1, // front face lower
			1, 0, 1,
			1, 1, 1,

			0, 0, 1, // front face upper
			0, 1, 1,
			1, 1, 1,

			0, 0, 0, // left face upper
			0, 1, 0,
			0, 1, 1,

			0, 0, 0, // left face lower
			0, 0, 1,
			0, 1, 1,

			1, 0, 0, // right face upper
			1, 1, 0,
			1, 1, 1,

			1, 0, 0, // right face lower
			1, 0, 1,
			1, 1, 1,

			0, 0, 0, // bottom face right
			1, 0, 0,
			1, 0, 1,

			0, 0, 0, // bottom face left
			0, 0, 1,
			1, 0, 1,

			0, 1, 0, // top face right
			1, 1, 0,
			1, 1, 1,

			0, 1, 0, // top face left
			0, 1, 1,
			1, 1, 1,
		};

		const GLfloat normal [] =
		{
			0, 0, -1,
			0, 0, -1,
			0, 0, -1,

			0, 0, -1,
			0, 0, -1,
			0, 0, -1,

			0, 0, 1,
			0, 0, 1,
			0, 0, 1,

			0, 0, 1,
			0, 0, 1,
			0, 0, 1,

			-1, 0, 0,
			-1, 0, 0,
			-1, 0, 0,

			-1, 0, 0,
			-1, 0, 0,
			-1, 0, 0,

			1, 0, 0,
			1, 0, 0,
			1, 0, 0,

			1, 0, 0,
			1, 0, 0,
			1, 0, 0,

			0, -1, 0,
			0, -1, 0,
			0, -1, 0,

			0, -1, 0,
			0, -1, 0,
			0, -1, 0,

			0, 1, 0,
			0, 1, 0,
			0, 1, 0,

			0, 1, 0,
			0, 1, 0,
			0, 1, 0,
		};

		assert(sizeof(normal) == sizeof(vertex));

		graphics::color color = p.second.color;
		const bool is_selected = get_editor().get_cursor() && *get_editor().get_cursor() == pos || get_editor().nhighlight_layer() >= 0 && p.second.nlayer == get_editor().nhighlight_layer();
		if(is_selected) {
			const int delta = sin(profile::get_tick_time()*0.01)*64;
			graphics::color_transform transform(delta, delta, delta, 0);
			graphics::color_transform new_color = graphics::color_transform(color) + transform;
			color = new_color.to_color();
		}

		int face = 0;

		for(int n = 0; n != sizeof(vertex)/sizeof(*vertex); ++n) {
			varray.push_back(pos[n%3]+vertex[n]);
			narray.push_back(normal[n]);
			if(n%3 == 0) {
				carray.push_back(color.r()); 
				carray.push_back(color.g()); 
				carray.push_back(color.b()); 
				carray.push_back(color.a());
			}
		}
	}

	if(!varray.empty()) {
		assert(varray.size() == narray.size());
		shader->vertex_array(3, GL_FLOAT, GL_FALSE, 0, &varray[0]);
		shader->color_array(4, GL_FLOAT, GL_FALSE, 0, &carray[0]);
		shader->vertex_attrib_array(a_normal_, 3, GL_FLOAT, GL_FALSE, 0, &narray[0]);
		glDrawArrays(GL_TRIANGLES, 0, varray.size()/3);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, video_framebuffer_id_);

	glViewport(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height());

	glDisable(GL_DEPTH_TEST);

	if(get_main_window()->get_configured_msaa() != 0) {
		// blit from multisample FBO to final FBO
		glBindFramebuffer(GL_FRAMEBUFFER, 0 );
		glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_id_[1]);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_id_[0]);
		glBlitFramebuffer(0, 0, tex_width_, tex_height_, 0, 0, tex_width_, tex_height_, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}
}

class perspective_renderer : public gui::widget
{
public:
	perspective_renderer(int xdir, int ydir, int zdir);
	void handleDraw() const override;

	void zoomIn();
	void zoomOut();

	//converts given pos to [x,y,0]
	VoxelPos normalize_pos(const VoxelPos& pos) const;

	VoxelPos denormalize_pos(const VoxelPos& pos) const;
private:
	VoxelPos get_mouse_pos(int mousex, int mousey) const;
	bool handleEvent(const SDL_Event& event, bool claimed) override;
	bool calculate_cursor(int mousex, int mousey);
	VoxelArea calculate_selection(int mousex1, int mousey1, int mousex2, int mousey2);
	int touching_selection_border(int mousex, int mousey) const;

	bool is_flipped() const { return vector_[0] + vector_[1] + vector_[2] < 0; }
	int vector_[3];
	int facing_; //0=x, 1=y, 2=z
	int voxel_width_;

	int last_select_x_, last_select_y_;

	int invert_y_;

	bool dragging_on_;
	int anchor_drag_x_, anchor_drag_y_;
	struct VoxelPosLess
	{
		bool operator()(VoxelPos const& p1, VoxelPos const& p2) const {
			return p1.x < p2.x || p1.x == p2.x && p1.y < p2.y ||
			       p1.x == p2.x && p1.y == p2.y && p1.z < p2.z;
		}
	};
	std::set<VoxelPos, VoxelPosLess> voxels_drawn_on_this_drag_;

	bool focus_;
	int selection_border_;
	std::unique_ptr<VoxelArea> starting_selection_;
};

perspective_renderer::perspective_renderer(int xdir, int ydir, int zdir)
  : voxel_width_(20), last_select_x_(std::numeric_limits<int>::min()), last_select_y_(std::numeric_limits<int>::min()),
    invert_y_(1), dragging_on_(false), anchor_drag_x_(0), anchor_drag_y_(0),
	focus_(false), selection_border_(-1)
{
	vector_[0] = xdir;
	vector_[1] = ydir;
	vector_[2] = zdir;

	for(int n = 0; n != 3; ++n) {
		if(vector_[n]) {
			facing_ = n;
			break;
		}
	}

	if(facing_ != 1) {
		invert_y_ *= -1;
	}
};

void perspective_renderer::zoomIn()
{
	if(voxel_width_ < 80) {
		voxel_width_ *= 2;
	}
}

void perspective_renderer::zoomOut()
{
	if(voxel_width_ > 5) {
		voxel_width_ /= 2;
	}
}

VoxelPos perspective_renderer::normalize_pos(const VoxelPos& pos) const
{
	VoxelPos result;
	result[2] = 0;
	int* out = &result[0];

	int dimensions[3] = {0, 2, 1};
	for(int n = 0; n != 3; ++n) {
		if(dimensions[n] != facing_) {
			*out++ = pos[dimensions[n]];
		}
	}

	return result;
}

VoxelPos perspective_renderer::denormalize_pos(const VoxelPos& pos2d) const
{
	const int* p = &pos2d[0];

	VoxelPos pos;
	int dimensions[3] = {0,2,1};
	for(int n = 0; n != 3; ++n) {
		if(dimensions[n] != facing_) {
			pos[dimensions[n]] = *p++;
		} else {
			pos[dimensions[n]] = 0;
		}
	}

	return pos;
}

VoxelPos perspective_renderer::get_mouse_pos(int mousex, int mousey) const
{
	int xpos = mousex - (x() + width()/2);
	int ypos = mousey - (y() + height()/2);

	if(xpos < 0) {
		xpos -= voxel_width_;
	}

	if(ypos > 0) {
		ypos += voxel_width_;
	}

	const int xselect = xpos/voxel_width_;
	const int yselect = ypos/voxel_width_;
	VoxelPos result;
	result[0] = xselect;
	result[1] = yselect*invert_y_;
	result[2] = 0;
	return result;
}

bool perspective_renderer::calculate_cursor(int mousex, int mousey)
{
	if(mousex == std::numeric_limits<int>::min()) {
		return false;
	}

	const VoxelPos pos2d = get_mouse_pos(mousex, mousey);
	const VoxelPos pos = denormalize_pos(pos2d);

	VoxelPos cursor = get_editor().get_selected_voxel(pos, facing_, vector_[facing_] < 0);
	if(get_editor().tool() == TOOL_PENCIL_ABOVE && get_editor().voxels().count(cursor)) {
		for(int n = 0; n != 3; ++n) {
			cursor[n] += vector_[n];
		}
	}

	return get_editor().setCursor(cursor);

}

int perspective_renderer::touching_selection_border(int mousex, int mousey) const
{
	if(get_editor().get_selection() == nullptr) {
		return -1;
	}

	const VoxelArea& area = *get_editor().get_selection();

	const VoxelPos top_left = normalize_pos(area.top_left);
	const VoxelPos bot_right = normalize_pos(area.bot_right);

	int x1 = top_left[0]*voxel_width_;
	int y1 = (top_left[1] - (invert_y_ < 0 ? 0 : 1))*voxel_width_*invert_y_;

	int x2 = bot_right[0]*voxel_width_;
	int y2 = (bot_right[1] - (invert_y_ < 0 ? 0 : 1))*voxel_width_*invert_y_;

	const int xpos = mousex - (x() + width()/2);
	const int ypos = mousey - (y() + height()/2);

	if(xpos >= std::min(x1,x2) && xpos <= std::max(x1,x2)) {
		if(ypos >= y1-3 && ypos <= y1+3) {
			return 0;
		}

		if(ypos >= y2-3 && ypos <= y2+3) {
			return 2;
		}
	}

	if(ypos >= std::min(y1,y2) && ypos <= std::max(y1,y2)) {
		if(xpos >= x1-3 && xpos <= x1+3) {
			return 3;
		}

		if(xpos >= x2-3 && xpos <= x2+3) {
			return 1;
		}
	}
	
	return -1;
}

VoxelArea perspective_renderer::calculate_selection(int mousex1, int mousey1, int mousex2, int mousey2)
{
	VoxelPos top_left = get_mouse_pos(mousex1, mousey1);
	VoxelPos bot_right = get_mouse_pos(mousex2, mousey2);
	if(top_left[0] > bot_right[0]) {
		std::swap(top_left[0], bot_right[0]);
	}

	if(top_left[1] > bot_right[1]) {
		std::swap(top_left[1], bot_right[1]);
	}

	int min_value = std::numeric_limits<int>::min(), max_value = std::numeric_limits<int>::min();

	for(const VoxelPair& vp : get_editor().layer().map) {
		const VoxelPos pos = normalize_pos(vp.first);
		if(pos[0] >= top_left[0] && pos[1] >= top_left[1] &&
		   pos[0] < bot_right[0] && pos[1] < bot_right[1]) {
			int zpos = vp.first[facing_];
			if(min_value == std::numeric_limits<int>::min() || zpos < min_value) {
				min_value = zpos;
			}

			if(max_value == std::numeric_limits<int>::min() || zpos > max_value) {
				max_value = zpos;
			}
		}
	}

	if(min_value != std::numeric_limits<int>::min()) {
		top_left = denormalize_pos(top_left);
		bot_right = denormalize_pos(bot_right);
		top_left[facing_] = min_value;
		bot_right[facing_] = max_value+1;
		VoxelArea area = { top_left, bot_right };
		return area;
	} else {
		VoxelArea result;
		result.top_left[0] = result.top_left[1] = result.top_left[2] = result.bot_right[0] = result.bot_right[1] = result.bot_right[2] = std::numeric_limits<int>::min();
		return result;
	}
}

bool perspective_renderer::handleEvent(const SDL_Event& event, bool claimed)
{
	switch(event.type) {
	case SDL_KEYUP:
	case SDL_KEYDOWN: {
		const SDL_Keymod mod = SDL_GetModState();
		const SDL_Keycode key = event.key.keysym.sym;

		calculate_cursor(last_select_x_, last_select_y_);

		if(focus_ && get_editor().tool() == TOOL_SELECT && get_editor().get_selection()) {
			if(event.type == SDL_KEYDOWN && key == SDLK_x && (mod&KMOD_CTRL)) {
				std::vector<VoxelPair> old_clipboard = get_editor().get_clipboard();
				std::vector<VoxelPair> items;
				VoxelArea selection = *get_editor().get_selection();
				for(const VoxelPair& vp : get_editor().layer().map) {
					bool in_area = true;
					for(int n = 0; in_area && n != 3; ++n) {
						in_area = vp.first[n] >= selection.top_left[n] && vp.first[n] < selection.bot_right[n];
					}

					if(in_area) {
						items.push_back(vp);
					}
				}

				get_editor().executeCommand(
					[=]() {
						for(const VoxelPair& vp : items) {
							get_editor().layer().map.erase(vp.first);
						}

						get_editor().build_voxels();
						get_editor().clear_selection();
						get_editor().set_clipboard(items);
					},
					[=]() {
						for(const VoxelPair& vp : items) {
							get_editor().layer().map[vp.first] = vp.second;
						}

						get_editor().build_voxels();
						get_editor().setSelection(selection);
						get_editor().set_clipboard(old_clipboard);
					}
				);
			}
		} else if(focus_ && event.type == SDL_KEYDOWN && key == SDLK_v && (mod&KMOD_CTRL) && get_editor().get_clipboard().empty() == false) {
			std::cerr << "PASTE!\n";
			std::vector<VoxelPair> clipboard = get_editor().get_clipboard();
			std::vector<VoxelPair> old_values;

			for(const VoxelPair& vp : clipboard) {
				auto itor = get_editor().layer().map.find(vp.first);
				if(itor != get_editor().layer().map.end()) {
					old_values.push_back(*itor);
				}
			}

			get_editor().executeCommand(
				[=]() {
					for(const VoxelPair& vp : clipboard) {
						get_editor().layer().map[vp.first] = vp.second;
					}

					get_editor().build_voxels();
				},

				[=]() {
					for(const VoxelPair& vp : clipboard) {
						get_editor().layer().map.erase(vp.first);
					}

					for(const VoxelPair& vp : old_values) {
						get_editor().layer().map[vp.first] = vp.second;
					}

					get_editor().build_voxels();
				}
			);
		}
		break;
	}

	case SDL_MOUSEWHEEL: {
		int mx, my;
		input::sdl_get_mouse_state(&mx, &my);
		if(!focus_ || get_editor().get_cursor() == nullptr) {
			break;
		}

		VoxelPos cursor = *get_editor().get_cursor();

		if(event.wheel.y > 0) {
			cursor[facing_] -= vector_[facing_];
		} else {
			cursor[facing_] += vector_[facing_];
		}
		get_editor().setCursor(cursor);

		break;
	}

	case SDL_MOUSEBUTTONUP: {
		const SDL_MouseButtonEvent& e = event.button;
		if(get_editor().tool() == TOOL_SELECT && dragging_on_ &&
		   e.x >= x() && e.y >= y() &&
		   e.x <= x() + width() && e.y <= y() + height()) {
			if(selection_border_ == -1) {
				VoxelArea selection = calculate_selection(anchor_drag_x_, anchor_drag_y_, e.x, e.y);
				if(selection.top_left[0] == std::numeric_limits<int>::min()) {
					get_editor().clear_selection();
				} else {
					get_editor().setSelection(selection);
				}
			}
		} else if(get_editor().tool() == TOOL_SELECT && dragging_on_) {
			get_editor().clear_selection();
		}

		dragging_on_ = false;
		voxels_drawn_on_this_drag_.clear();

		break;
	}

	case SDL_MOUSEBUTTONDOWN: {
		const SDL_MouseButtonEvent& e = event.button;
		if(e.x >= x() && e.y >= y() &&
		   e.x <= x() + width() && e.y <= y() + height()) {
			switch(get_editor().tool()) {
			case TOOL_PENCIL:
			case TOOL_PENCIL_ABOVE: {
				if(e.button == SDL_BUTTON_LEFT) {
					pencil_voxel();
				} else if(e.button == SDL_BUTTON_RIGHT) {
					delete_voxel();
				}

				calculate_cursor(last_select_x_, last_select_y_);

				dragging_on_ = true;
				voxels_drawn_on_this_drag_.clear();

				if(get_editor().get_cursor()) {
					voxels_drawn_on_this_drag_.insert(normalize_pos(*get_editor().get_cursor()));
				}
				break;
			}

			case TOOL_FLOOD: {
				flood_voxel();
				break;
			}

			case TOOL_PICKER: {
				if(get_editor().get_cursor()) {
					auto voxel_itor = get_editor().voxels().find(*get_editor().get_cursor());
					if(voxel_itor != get_editor().voxels().end()) {
						const graphics::color color = voxel_itor->second.color;
						if(e.button == SDL_BUTTON_LEFT) {
							get_editor().get_ColorPicker().setPrimaryColor(color);
						} else if(e.button == SDL_BUTTON_RIGHT) {
							get_editor().get_ColorPicker().setSecondaryColor(color);
						}
					}
				}

				break;
			}
			case TOOL_SELECT: {
				dragging_on_ = true;
				anchor_drag_x_ = e.x;
				anchor_drag_y_ = e.y;
				if(get_editor().get_selection()) {
					starting_selection_.reset(new VoxelArea(*get_editor().get_selection()));
				} else {
					starting_selection_.reset();
				}
				break;
			}
			default:
				break;
			}
		} else {
			dragging_on_ = false;
			voxels_drawn_on_this_drag_.clear();
		}
		break;
	}

	case SDL_MOUSEMOTION: {
		const SDL_MouseMotionEvent& motion = event.motion;
		if(motion.x >= x() && motion.y >= y() &&
		   motion.x <= x() + width() && motion.y <= y() + height()) {
			focus_ = true;

			Uint8 button_state = input::sdl_get_mouse_state(nullptr, nullptr);

			if(!button_state) {
				selection_border_ = touching_selection_border(motion.x, motion.y);
			}

			const bool is_cursor_set = calculate_cursor(motion.x, motion.y);
			last_select_x_ = motion.x;
			last_select_y_ = motion.y;

			if(is_cursor_set) {
				switch(get_editor().tool()) {
				case TOOL_PENCIL_ABOVE:
				case TOOL_PENCIL: {
					if(button_state & SDL_BUTTON(SDL_BUTTON_LEFT) && dragging_on_) {
						if(voxels_drawn_on_this_drag_.count(normalize_pos(*get_editor().get_cursor())) == 0) {
							pencil_voxel();
							calculate_cursor(motion.x, motion.y);
							voxels_drawn_on_this_drag_.insert(normalize_pos(*get_editor().get_cursor()));
						}
					} else if(button_state & SDL_BUTTON(SDL_BUTTON_RIGHT) && dragging_on_) {
						if(voxels_drawn_on_this_drag_.count(normalize_pos(*get_editor().get_cursor())) == 0) {
							delete_voxel();
							calculate_cursor(motion.x, motion.y);
							voxels_drawn_on_this_drag_.insert(normalize_pos(*get_editor().get_cursor()));
						}
					}
					break;
				}

				case TOOL_SELECT: {
					if(dragging_on_ && selection_border_ == -1) {
						VoxelArea selection = calculate_selection(anchor_drag_x_, anchor_drag_y_, motion.x, motion.y);
						if(selection.top_left[0] == std::numeric_limits<int>::min()) {
							get_editor().clear_selection();
						} else {
							get_editor().setSelection(selection);
						}
					} else if(dragging_on_ && selection_border_ != -1 && starting_selection_ && get_editor().get_selection()) {
						const int xdelta = motion.x - anchor_drag_x_;
						const int ydelta = motion.y - anchor_drag_y_;

						int xdelta_voxel = xdelta/voxel_width_;
						int ydelta_voxel = ydelta/voxel_width_;

						int dim[2];
						int* p = dim;
						int dimensions[3] = {0, 2, 1};
						for(int n = 0; n != 3; ++n) {
							if(dimensions[n] != facing_) {
								*p++ = dimensions[n];
							}
						}

						if(dim[1] == 1) {
							ydelta_voxel *= -1;
						}

						VoxelArea area = *starting_selection_;
						if(selection_border_ == 0) {
							area.top_left[dim[1]] += ydelta_voxel;
						} else if(selection_border_ == 1) {
							area.bot_right[dim[0]] += xdelta_voxel;
						} else if(selection_border_ == 2) {
							area.bot_right[dim[1]] += ydelta_voxel;
						} else if(selection_border_ == 3) {
							area.top_left[dim[0]] += xdelta_voxel;
						}

						//Here we're dragging the edge of the selection so extrude.
						if(get_editor().get_selection()) {
							VoxelArea current_area = *get_editor().get_selection();

							VoxelMap extrusion, revert;

							//iterate over all areas of the selection.
							for(int x = area.top_left[0]; x < area.bot_right[0]; ++x) {
								for(int y = area.top_left[1]; y < area.bot_right[1]; ++y) {
									for(int z = area.top_left[2]; z < area.bot_right[2]; ++z) {
										VoxelPos pos(x,y,z);
										const VoxelPos original_pos = pos;
										if(current_area.voxel_in_area(pos)) {
											continue;
										}

										//this voxel isn't in the original selection. Find the
										//nearest voxel that is in the selection and make it
										//a copy of that voxel.

										bool found = false;

										for(int n = 0; n != 1000 && !found; ++n) {
											for(int dim = 0; dim != 3; ++dim) {
												pos[dim] += n;
												if(current_area.voxel_in_area(pos)) {
													found = true;
													break;
												}

												pos[dim] -= n*2;
												if(current_area.voxel_in_area(pos)) {
													found = true;
													break;
												}

												pos[dim] += n;
											}
										}

										if(found) {
											auto voxel_itor = get_editor().layer().map.find(pos);
											if(voxel_itor != get_editor().layer().map.end()) {
												extrusion[original_pos] = voxel_itor->second;
												voxel_itor = get_editor().layer().map.find(original_pos);
												if(voxel_itor != get_editor().layer().map.end()) {
													revert[original_pos] = voxel_itor->second;
												}
											}
										}
									}
								}
							}

							if(extrusion.empty() == false) {
								get_editor().executeCommand(
									[=]() {
										for(auto p : extrusion) {
											get_editor().set_voxel(p.first, p.second);
										}
									},
									[=]() {
										for(auto p : extrusion) {
											if(revert.count(p.first)) {
												get_editor().set_voxel(p.first, revert.find(p.first)->second);
											} else {
												get_editor().delete_voxel(p.first);
											}
										}
									}
								);
							}
						}

						get_editor().setSelection(area);
					}
					break;
				}

				default:
					break;
				}
			}

			break;
		} else {
			last_select_x_ = last_select_y_ = std::numeric_limits<int>::min();
			focus_ = false;
			selection_border_ = -1;
		}
	}
	}
	return widget::handleEvent(event, claimed);
}

void perspective_renderer::handleDraw() const
{
	const SDL_Rect clipArea = { x(), y(), width(), height() };
	const graphics::clip_scope clipping_scope(clipArea);

	gles2::manager gles2_manager(gles2::get_simple_col_shader());

	std::vector<GLfloat> varray, carray;

	const int cells_h = width()/voxel_width_ + 1;
	const int cells_v = height()/voxel_width_ + 1;
	for(int xpos = -cells_h/2; xpos <= cells_h/2; ++xpos) {
		const int left_side = x() + width()/2 + xpos*voxel_width_;
		if(left_side < x() || left_side + voxel_width_ > x() + width()) {
			continue;
		}

		varray.push_back(left_side);
		varray.push_back(y());
		varray.push_back(left_side);
		varray.push_back(y() + height());

		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(xpos == 0 ? 1.0 : 0.3);

		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(xpos == 0 ? 1.0 : 0.3);
	}

	for(int ypos = -cells_v/2; ypos <= cells_v/2; ++ypos) {
		const int top_side = y() + height()/2 + ypos*voxel_width_;
		if(top_side < y() || top_side + voxel_width_ > y() + height()) {
			continue;
		}

		varray.push_back(x());
		varray.push_back(top_side);
		varray.push_back(x() + width());
		varray.push_back(top_side);

		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(ypos == 0 ? 1.0 : 0.3);

		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(ypos == 0 ? 1.0 : 0.3);
	}

	if(get_editor().get_cursor()) {
		const VoxelPos cursor = normalize_pos(*get_editor().get_cursor());

		const int x1 = x() + width()/2 + cursor[0]*voxel_width_;
		const int y1 = y() + height()/2 + cursor[1]*voxel_width_*invert_y_;

		const int x2 = x1 + voxel_width_;
		const int y2 = y1 - voxel_width_;

		int vertexes[] = { x1, y1, x1, y2,
		                   x2, y1, x2, y2,
						   x1, y1, x2, y1,
						   x1, y2, x2, y2, };
		for(int n = 0; n != sizeof(vertexes)/sizeof(*vertexes); ++n) {
			varray.push_back(vertexes[n]);
			if(n%2 == 0) {
				carray.push_back(1.0);
				carray.push_back(0.0);
				carray.push_back(0.0);
				carray.push_back(1.0);
			}
		}
	}

	gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray[0]);
	gles2::active_shader()->shader()->color_array(4, GL_FLOAT, 0, 0, &carray[0]);
	glDrawArrays(GL_LINES, 0, varray.size()/2);

	varray.clear();
	carray.clear();

	std::vector<VoxelPair> voxels(get_editor().voxels().begin(), get_editor().voxels().end());
	if(is_flipped()) {
		std::reverse(voxels.begin(), voxels.end());
	}

	for(const VoxelPair& p : voxels) {
		const VoxelPos pos = normalize_pos(p.first);

		const int x1 = x() + width()/2 + pos[0]*voxel_width_;
		const int y1 = y() + height()/2 + pos[1]*voxel_width_*invert_y_;

		const int x2 = x1 + voxel_width_;
		const int y2 = y1 - voxel_width_;

		bool is_selected = get_editor().get_cursor() && normalize_pos(*get_editor().get_cursor()) == pos || get_editor().nhighlight_layer() >= 0 && get_editor().nhighlight_layer() == p.second.nlayer;

		graphics::color color = p.second.color;
		if(is_selected) {
			const int delta = sin(profile::get_tick_time()*0.01)*64;
			graphics::color_transform transform(delta, delta, delta, 0);
			graphics::color_transform new_color = graphics::color_transform(color) + transform;
			color = new_color.to_color();
		}

		int vertexes[] = { x1, y1,
		                   x1, y1, x1, y2,
		                   x2, y1, x2, y2,
						   x1, y1, x2, y1,
						   x1, y2, x2, y2,
						   x2, y2, };
		for(int n = 0; n != sizeof(vertexes)/sizeof(*vertexes); ++n) {
			varray.push_back(vertexes[n]);
			if(n%2 == 0) {
				carray.push_back(color.r()/255.0);
				carray.push_back(color.g()/255.0);
				carray.push_back(color.b()/255.0);
				carray.push_back(color.a()/255.0);
			}
		}
	}

	gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray[0]);
	gles2::active_shader()->shader()->color_array(4, GL_FLOAT, 0, 0, &carray[0]);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, varray.size()/2);

	varray.clear();
	carray.clear();

	//When voxels are adjacent but different height to each other from our
	//perspective, we represent the height difference by drawing black lines
	//between the voxels.
	for(const VoxelPair& p : voxels) {
		const VoxelPos pos = normalize_pos(p.first);

		const int x1 = x() + width()/2 + pos[0]*voxel_width_;
		const int y1 = y() + height()/2 + pos[1]*voxel_width_*invert_y_;

		const int x2 = x1 + voxel_width_;
		const int y2 = y1 - voxel_width_;

		VoxelPos actual_pos = get_editor().get_selected_voxel(p.first, facing_, vector_[facing_] < 0);
		if(actual_pos != p.first) {
			continue;
		}

		VoxelPos down = p.first;
		VoxelPos right = p.first;

		switch(facing_) {
			case 0: down[1]--; right[2]++; break;
			case 1: down[2]++; right[0]++; break;
			case 2: down[1]--; right[0]++; break;
		}

		if(get_editor().get_selected_voxel(down, facing_, vector_[facing_] < 0) != down) {
			varray.push_back(x1);
			varray.push_back(y1);
			varray.push_back(x2);
			varray.push_back(y1);

			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(1);
			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(1);
		}

		if(get_editor().get_selected_voxel(right, facing_, vector_[facing_] < 0) != right) {
			varray.push_back(x2);
			varray.push_back(y1);
			varray.push_back(x2);
			varray.push_back(y2);

			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(1);
			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(1);
		}
	}

	if(get_editor().get_selection()) {

		const VoxelPos tl = normalize_pos(get_editor().get_selection()->top_left);
		const VoxelPos br = normalize_pos(get_editor().get_selection()->bot_right);

		const int x1 = x() + width()/2 + tl[0]*voxel_width_;
		const int y1 = y() + height()/2 + (tl[1] - (invert_y_ < 0 ? 0 : 1))*voxel_width_*invert_y_;

		const int x2 = x() + width()/2 + br[0]*voxel_width_;
		const int y2 = y() + height()/2 + (br[1] - (invert_y_ < 0 ? 0 : 1))*voxel_width_*invert_y_;

		varray.push_back(x1);
		varray.push_back(y1);
		varray.push_back(x2);
		varray.push_back(y1);

		varray.push_back(x2);
		varray.push_back(y1);
		varray.push_back(x2);
		varray.push_back(y2);

		varray.push_back(x2);
		varray.push_back(y2);
		varray.push_back(x1);
		varray.push_back(y2);

		varray.push_back(x1);
		varray.push_back(y2);
		varray.push_back(x1);
		varray.push_back(y1);

		for(int n = 0; n != 8; ++n) {
			const bool selected = selection_border_ == n/2;
			carray.push_back(1);
			carray.push_back(1);
			carray.push_back(selected ? 0 : 1);
			carray.push_back(1);
		}
	}

	{
		glm::vec3 camera_vec = get_iso_renderer().camera().position();
		GLfloat camera_pos[2];
		GLfloat* camera_pos_ptr = camera_pos;
		int dimensions[3] = {0, 2, 1};
		for(int n = 0; n != 3; ++n) {
			if(dimensions[n] != facing_) {
				*camera_pos_ptr++ = camera_vec[dimensions[n]];
			}
		}

		varray.push_back(x() + width()/2);
		varray.push_back(y() + height()/2);
		varray.push_back(x() + width()/2 + camera_pos[0]*voxel_width_);
		varray.push_back(y() + height()/2 + camera_pos[1]*voxel_width_*invert_y_);

		carray.push_back(1);
		carray.push_back(0);
		carray.push_back(1);
		carray.push_back(0.5);
		carray.push_back(1);
		carray.push_back(0);
		carray.push_back(1);
		carray.push_back(0.5);
	}


	gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray[0]);
	gles2::active_shader()->shader()->color_array(4, GL_FLOAT, 0, 0, &carray[0]);
	glDrawArrays(GL_LINES, 0, varray.size()/2);
}

class perspective_widget : public gui::dialog
{
public:
	perspective_widget(const rect& area, int xdir, int ydir, int zdir);
	void init();
private:
	void flip();
	int xdir_, ydir_, zdir_;
	bool flipped_;

	ffl::IntrusivePtr<perspective_renderer> renderer_;
	gui::LabelPtr description_label_;
};

perspective_widget::perspective_widget(const rect& area, int xdir, int ydir, int zdir)
  : dialog(area.x(), area.y(), area.w(), area.h()),
    xdir_(xdir), ydir_(ydir), zdir_(zdir), flipped_(false)
{
	init();
}

void perspective_widget::init()
{
	clear();

	renderer_.reset(new perspective_renderer(xdir_, ydir_, zdir_));

	grid_ptr toolbar(new grid(4));

	std::string description;
	if(xdir_) { description = flipped_ ? "Reverse" : "Side"; }
	else if(ydir_) { description = flipped_ ? "Bottom" : "Top"; }
	else if(zdir_) { description = flipped_ ? "Back" : "Front"; }

	description_label_.reset(new label(description, 12));
	toolbar->addCol(description_label_);
	toolbar->addCol(new button(new label("Flip", graphics::color("antique_white").as_sdl_color(), 14, "Montaga-Regular"), std::bind(&perspective_widget::flip, this)));
	toolbar->addCol(new button(new label("+", graphics::color("antique_white").as_sdl_color(), 14, "Montaga-Regular"), std::bind(&perspective_renderer::zoomIn, renderer_.get())));
	toolbar->addCol(new button(new label("-", graphics::color("antique_white").as_sdl_color(), 14, "Montaga-Regular"), std::bind(&perspective_renderer::zoomOut, renderer_.get())));
	addWidget(toolbar);

	addWidget(renderer_);
	renderer_->setDim(width(), height() - renderer_->y());
};

void perspective_widget::flip()
{
	flipped_ = !flipped_;
	xdir_ *= -1;
	ydir_ *= -1;
	zdir_ *= -1;
	init();
}

voxel_editor::voxel_editor(const rect& r, const std::string& fname)
  : dialog(r.x(), r.y(), r.w(), r.h()), area_(r),
    current_layer_(0), highlight_layer_(-1),
    fname_(fname), tool_(TOOL_PENCIL), symmetric_(false)
{
	if(fname_.empty()) {
		layers_.push_back(Layer());
	} else {
		variant doc = json::parse_from_file(fname_);
		model_ = read_model(doc);

		for(const LayerType& layer_type : model_.layer_types) {
			auto itor = layer_type.variations.find(layer_type.last_edited_variation);
			if(itor == layer_type.variations.end()) {
				itor = layer_type.variations.begin();
			}

			assert(itor != layer_type.variations.end());

			layers_.push_back(itor->second);
		}
	}

	g_voxel_editor = this;
	init();
	build_voxels();
}

voxel_editor::~voxel_editor()
{
	if(g_voxel_editor == this) {
		g_voxel_editor = nullptr;
	}
}

std::vector<ffl::IntrusivePtr<perspective_widget> > g_perspectives;

void voxel_editor::init()
{
	clear();

	const int sidebar_padding = 200;
	const int between_padding = 10;
	const int widget_width = (area_.w() - sidebar_padding - between_padding)/2;
	const int widget_height = (area_.h() - between_padding)/2;

	rect perspective_areas[] = {
		rect(area_.x(), area_.y(), widget_width, widget_height),
		rect(area_.x() + widget_width + between_padding, area_.y(), widget_width, widget_height),
		rect(area_.x(), area_.y() + widget_height + between_padding, widget_width, widget_height),
	};

	if(g_perspectives.empty()) {
		g_perspectives.push_back(ffl::IntrusivePtr<perspective_widget>(new perspective_widget(perspective_areas[0], 1, 0, 0)));
		g_perspectives.push_back(ffl::IntrusivePtr<perspective_widget>(new perspective_widget(perspective_areas[1], 0, 1, 0)));
		g_perspectives.push_back(ffl::IntrusivePtr<perspective_widget>(new perspective_widget(perspective_areas[2], 0, 0, 1)));
	} else {
		for(int n = 0; n != 3; ++n) {
			g_perspectives[n]->setLoc(perspective_areas[n].x(), perspective_areas[n].y());
			g_perspectives[n]->setDim(perspective_areas[n].w(), perspective_areas[n].h());
			g_perspectives[n]->init();
		}
	}

	const bool iso_maximized = iso_renderer_ && iso_renderer_->maximized();

	if(!iso_maximized) {
		for(int n = 0; n != 3; ++n) {
			addWidget(g_perspectives[n], g_perspectives[n]->x(), g_perspectives[n]->y());
		}
	}

	rect iso_renderer_area(area_.x() + widget_width + between_padding, area_.y() + widget_height + between_padding, widget_width, widget_height);

	if(iso_maximized) {
		iso_renderer_area = rect(area_.x() + 5, area_.y() + 5, area_.w() - sidebar_padding - 10, area_.h() - 10);
	}

	if(!iso_renderer_) {
		iso_renderer_.reset(new iso_renderer(iso_renderer_area));
	} else {
		iso_renderer_->setLoc(iso_renderer_area.x(), iso_renderer_area.y());
		iso_renderer_->setDim(iso_renderer_area.w(), iso_renderer_area.h());
		iso_renderer_->init();
	}
	addWidget(iso_renderer_, iso_renderer_->x(), iso_renderer_->y());

	grid_ptr toolbar(new grid(3));

	toolbar->addCol(WidgetPtr(new button(new label("Save", graphics::color("antique_white").as_sdl_color(), 14, "Montaga-Regular"), std::bind(&voxel_editor::on_save, this))));
	toolbar->addCol(WidgetPtr(new button(new label("Undo", graphics::color("antique_white").as_sdl_color(), 14, "Montaga-Regular"), std::bind(&voxel_editor::undo, this))));
	toolbar->addCol(WidgetPtr(new button(new label("Redo", graphics::color("antique_white").as_sdl_color(), 14, "Montaga-Regular"), std::bind(&voxel_editor::redo, this))));
	addWidget(toolbar, area_.x2() - 190, area_.y() + 4);

	tool_borders_.clear();
	grid_ptr tools_grid(new grid(3));

	for(int n = 0; ToolIcons[n]; ++n) {
		assert(n < NUM_VOXEL_TOOLS);
		ButtonPtr tool_button(
		  new button(WidgetPtr(new GuiSectionWidget(ToolIcons[n], 26, 26)),
		      std::bind(&voxel_editor::select_tool, this, static_cast<VOXEL_TOOL>(n))));
		tool_borders_.push_back(new BorderWidget(tool_button, tool_ == n ? KRE::Color::colorWhite() : graphics::color_black()));
		tools_grid->addCol(WidgetPtr(tool_borders_.back()));
	}

	tools_grid->finishRow();

	addWidget(tools_grid);

	addWidget(WidgetPtr(new Checkbox(new label("Symmetric", graphics::color("antique_white").as_sdl_color(), 14, "Montaga-Regular"), symmetric_, std::bind(&voxel_editor::set_symmetric, this, _1))));

	if(model_.layer_types.empty() == false) {
		assert(model_.layer_types.size() == layers_.size());
		grid_ptr layers_grid(new grid(2));

		for(int n = 0; n != layers_.size(); ++n) {
			layers_grid->addCol(WidgetPtr(new label(model_.layer_types[n].name)));
			layers_grid->addCol(WidgetPtr(new button(layers_[n].name, std::bind(&voxel_editor::on_change_layer_button_clicked, this, n))));
		}

		layers_grid->allowSelection();
		layers_grid->set_draw_selection_highlight();
		layers_grid->set_default_selection(current_layer_);
		layers_grid->register_mouseover_callback(std::bind(&voxel_editor::mouseover_layer, this, _1));
		layers_grid->registerSelectionCallback(std::bind(&voxel_editor::select_layer, this, _1, layers_grid.get()));

		addWidget(layers_grid);
	}

	if(!ColorPicker_) {
		ColorPicker_.reset(new ColorPicker(rect(area_.x() + area_.w() - 190, area_.y() + 6, 180, 440)));
		ColorPicker_->setPrimaryColor(graphics::color(255, 0, 0));
	}
	addWidget(ColorPicker_);

	pos_label_.reset(new label("", 12));
	addWidget(pos_label_, area_.x() + area_.w() - pos_label_->width() - 100,
	                       area_.y() + area_.h() - pos_label_->height() - 30 );


}

void voxel_editor::set_voxel(const VoxelPos& pos, const Voxel& voxel)
{
	layer().map[pos] = voxel;
	if(symmetric_) {
		VoxelPos opposite_pos = pos;
		opposite_pos[0] = -1*opposite_pos[0] - 1;
		layer().map[opposite_pos] = voxel;
	}
	build_voxels();
}

void voxel_editor::delete_voxel(const VoxelPos& pos)
{
	layer().map.erase(pos);
	if(symmetric_) {
		VoxelPos opposite_pos = pos;
		opposite_pos[0] = -1*opposite_pos[0] - 1;
		layer().map.erase(opposite_pos);
	}
	build_voxels();
}

bool voxel_editor::setCursor(const VoxelPos& pos)
{
	if(cursor_ && *cursor_ == pos) {
		return false;
	}

	cursor_.reset(new VoxelPos(pos));
	if(pos_label_) {
		pos_label_->setText(formatter() << "(" << pos[0] << "," << pos[1] << "," << pos[2] << ")");
		pos_label_->setLoc(area_.x() + area_.w() - pos_label_->width() - 8,
		                    area_.y() + area_.h() - pos_label_->height() - 4);
	}

	return true;
}

VoxelPos voxel_editor::get_selected_voxel(const VoxelPos& pos, int facing, bool reverse)
{
	const int flip = reverse ? -1 : 1;
	VoxelPos result = pos;
	bool found = false;
	int best_value = 0;
	for(const VoxelPair& p : voxels_) {
		bool is_equal = true;
		for(int n = 0; n != 3; ++n) {
			if(n != facing && pos[n] != p.first[n]) {
				is_equal = false;
				break;
			}
		}

		if(!is_equal) {
			continue;
		}

		const int value = flip*p.first[facing];
		if(found == false || value >= best_value) {
			best_value = value;
			result = p.first;
			found = true;
		}
	}

	return result;
}

bool voxel_editor::handleEvent(const SDL_Event& event, bool claimed)
{
	if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
		video_resize(event);
		setDim(preferences::actual_screen_width(), preferences::actual_screen_height());
		init();
		return true;
	}
	return dialog::handleEvent(event, claimed);
}

void voxel_editor::on_color_changed(const graphics::color& color)
{
}

void voxel_editor::on_change_layer_button_clicked(int nlayer)
{
	if(nlayer < 0) {
		return;
	}

	assert(nlayer < model_.layer_types.size());

	LayerType& layer = model_.layer_types[nlayer];

	std::vector<std::pair<std::string,Layer> > variations(layer.variations.begin(), layer.variations.end());

	grid_ptr context_menu(new grid(2));
	context_menu->setHpad(10);

	for(const std::pair<std::string,Layer>& p : variations) {
		context_menu->addCol(p.first);
		context_menu->addCol("");
	}

	ffl::IntrusivePtr<TextEditorWidget> editor(new TextEditorWidget(100));
	context_menu->addCol(editor);
	context_menu->addCol("add");

	int result = show_grid_as_context_menu(context_menu, WidgetPtr(this));
	if(result < 0) {
		return;
	}

	if(result <= variations.size() && !editor->text().empty()) {
		std::string name = editor->text();
		int index = 0;
		for(index = 0; index < variations.size(); ++index) {
			if(variations[index].first == name) {
				break;
			}
		}

		if(index == variations.size()) {
			Layer new_layer;
			if(result < variations.size()) {
				new_layer = variations[result].second;
			}

			new_layer.name = name;

			layer.variations[name] = new_layer;
			layers_[nlayer] = layer.variations[name];
			build_voxels();
			init();
			return;
		}

		result = index;
	}

	if(result < variations.size()) {
		model_.layer_types[nlayer].variations[layers_[nlayer].name] = layers_[nlayer];
		layers_[nlayer] = variations[result].second;
		build_voxels();
		init();
	}
}

void voxel_editor::select_tool(VOXEL_TOOL tool)
{
	tool_ = tool;
	init();
}

void voxel_editor::set_symmetric(bool value)
{
	const bool old_value = symmetric_;
	symmetric_ = value;
	get_editor().executeCommand(
		[this, value]() { this->symmetric_ = value; },
		[this, old_value]() { this->symmetric_ = old_value; }
	);
}

void voxel_editor::mouseover_layer(int nlayer)
{
	highlight_layer_ = nlayer;
}

void voxel_editor::select_layer(int nlayer, grid* layer_grid)
{
	std::cerr << "SELECT LAYER: " << nlayer << "\n";
	if(nlayer != -1) {
		assert(nlayer >= 0 && nlayer < layers_.size());
		const int old_layer = current_layer_;

		executeCommand(
			[this, nlayer]() { this->current_layer_ = nlayer; },
			[this, old_layer]() { this->current_layer_ = old_layer; }
		);
	} else {
		layer_grid->set_default_selection(current_layer_);
	}
}

void voxel_editor::onSave()
{
	if(fname_.empty()) {
		std::cerr << "NO FILENAME. CANNOT SAVE\n";
		return;
	}

	assert(layers_.size() == model_.layer_types.size());

	for(int n = 0; n != layers_.size(); ++n) {
		model_.layer_types[n].variations[layers_[n].name] = layers_[n];
		model_.layer_types[n].last_edited_variation = layers_[n].name;
	}

	variant doc = write_model(model_);
	sys::write_file(fname_, doc.write_json());
}

void voxel_editor::undo()
{
	if(undo_.empty() == false) {
		Command cmd = undo_.back();
		undo_.pop_back();
		cmd.undo();
		redo_.push_back(cmd);
		init();
	}
}

void voxel_editor::redo()
{
	if(redo_.empty() == false) {
		Command cmd = redo_.back();
		redo_.pop_back();
		cmd.redo();
		undo_.push_back(cmd);
		init();
	}
}

void voxel_editor::handleProcess()
{
	VOXEL_TOOL current_tool = tool();
	for(int n = 0; n != tool_borders_.size(); ++n) {
		tool_borders_[n]->setColor(n == current_tool ? KRE::Color::colorWhite() : graphics::color_black());
	}

	dialog::handleProcess();
}

void voxel_editor::executeCommand(std::function<void()> redo, std::function<void()> undo)
{
	executeCommand(Command(redo, undo));
}

void voxel_editor::executeCommand(const Command& cmd)
{
	cmd.redo();
	undo_.push_back(cmd);
	redo_.clear();
}

void voxel_editor::build_voxels()
{
	voxels_.clear();
	int nlayer = 0;
	for(const Layer& layer : layers_) {
		for(VoxelPair p : layer.map) {
			p.second.nlayer = nlayer;
			voxels_.insert(p);
		}

		++nlayer;
	}
}

}

UTILITY(voxel_editor)
{
	std::deque<std::string> arguments(args.begin(), args.end());

	ASSERT_LOG(arguments.size() <= 1, "Unexpected arguments");

	std::string fname;
	if(arguments.empty() == false) {
		fname = module::map_file(arguments.front());
	}

	if(!fname.empty() && !sys::file_exists(fname)) {
		Model model;
		model.feet_position[0] = model.feet_position[1] = model.feet_position[2] = 0;
		model.scale = decimal::from_int(1);
		for(int n = 1; n <= 3; ++n) {
			LayerType layer;
			layer.name = formatter() << "layer" << n;
			layer.last_edited_variation = "default";
			layer.symmetric = false;
			layer.variations["default"].name = "default";
			model.layer_types.push_back(layer);
		}

		sys::write_file(fname, write_model(model).write_json());
	}
	
	ffl::IntrusivePtr<voxel_editor> editor(new voxel_editor(rect(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height()), fname));
	editor->showModal();
}

*/
