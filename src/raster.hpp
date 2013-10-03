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
#ifndef RASTER_HPP_INCLUDED
#define RASTER_HPP_INCLUDED

#include <boost/scoped_ptr.hpp>
#include <boost/shared_array.hpp>

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "graphics.hpp"

#include "camera.hpp"
#include "color_chart.hpp"
#include "color_utils.hpp"
#include "geometry.hpp"
#include "texture.hpp"

namespace preferences
{
extern int xypos_draw_mask;
}

namespace graphics
{

struct vbo_deleter
{
	vbo_deleter(int n) : n_(n)
	{}

	void operator()(GLuint* d) 
	{
		glDeleteBuffers(n_, d);
		delete[] d;
	}

	int n_;
};

struct shader_save_context
{
	shader_save_context()
	{
		glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
	}

	~shader_save_context()
	{
		glUseProgram(current_program);
	}

	GLint current_program;
};


typedef boost::shared_array<GLuint> vbo_array;

class stencil_scope
{
public:
	explicit stencil_scope(bool enabled, int write_mask, GLenum func, GLenum ref, GLenum ref_mask, GLenum sfail, GLenum dpfail, GLenum dppass);
	~stencil_scope();
	void apply_settings();
	void revert_settings();
private:
	bool init_;
};

struct blend_mode
{
	GLenum sfactor, dfactor;
};

struct flip_draw_scope
{
	flip_draw_scope();
	~flip_draw_scope();

	bool old_value;
};

bool set_video_mode(int w, int h);
void reset_opengl_state();
SDL_DisplayMode set_video_mode_auto_select();
SDL_Window* set_video_mode(int w, int h, int flags);
SDL_Window* get_window();
void swap_buffers();

std::vector<GLfloat>& global_vertex_array();
std::vector<GLfloat>& global_texcoords_array();
std::vector<GLbyte>& global_vertex_color_array();

//function to map a mouse position in native window co-ordinates.
//this will modify the mouse position if we're using an fbo to
//present a fake framebuffer.
void map_mouse_position(int* x, int* y);

void prepare_raster();
void blit_texture(const texture& tex, int x=0, int y=0, GLfloat rotate=0.0);

//Function to blit a texture to the screen. Parameters:
//x, y: target on-screen location.
//w, h: dimensions of the on-screen area that will be filled by the
//blit.
//rotate: the number of degrees to rotate by when blitting
//x1, y1, x2, y2: the area of the texture to blit onto the screen. The
//defaults are to blit the entire texture. Note that these values can be
// < 0.0 or > 1.0 and the texture will wrap, but *only* if the texture's
//dimensions are powers of two. Otherwise they must be in the range [0,1]
void blit_texture(const texture& tex, int x, int y, int w, int h, GLfloat rotate=0.0, GLfloat x1=0.0, GLfloat y1=0.0, GLfloat x2=1.0, GLfloat y2=1.0);

void queue_blit_texture(const texture& tex, int x, int y, int w, int h,
                        GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void queue_blit_texture(const texture& tex, int x, int y, int w, int h, GLfloat rotate,
						GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void queue_blit_texture_3d(const texture& tex, GLfloat x, GLfloat y, GLfloat z, int w, int h,
                        GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void flush_blit_texture();
void flush_blit_texture_3d();

class blit_queue
{
public:
	blit_queue() : texture_(0)
	{}
	void set_texture(GLuint id) {
		texture_ = id;
	}
	GLuint texture() const { return texture_; }
	void clear();
	bool empty() const { return vertex_.empty(); }
	size_t size() const { return vertex_.size(); }
	void do_blit() const;
	void do_blit_range(short begin, short end) const;
	void add(GLshort x, GLshort y, GLfloat u, GLfloat v) {
		vertex_.push_back(x&preferences::xypos_draw_mask);
		vertex_.push_back(y&preferences::xypos_draw_mask);
		uv_.push_back(u);
		uv_.push_back(v);
	}

	void repeat_last() {
		if(!vertex_.empty()) {
			vertex_.push_back(vertex_[vertex_.size()-2]);
			vertex_.push_back(vertex_[vertex_.size()-2]);
			uv_.push_back(uv_[uv_.size()-2]);
			uv_.push_back(uv_[uv_.size()-2]);
		}
	}

	short position() const { return vertex_.size(); }

	bool merge(const blit_queue& q, short begin, short end);

	void reserve(size_t n) {
		vertex_.reserve(n);
		uv_.reserve(n);
	}
private:
	GLuint texture_;
	std::vector<GLshort> vertex_;
	std::vector<GLfloat> uv_;
};

//function which sets a rectangle where we want to detect if pixels are written.
//buf must point to a buffer with a size of rect.w*rect.h. Whenever a pixel
//is blitted within rect, the corresponding pixel in buf will be set. buf
//must remain valid until another call to set_draw_detection_rect() or a
//call to clear_draw_detection_rect().
void set_draw_detection_rect(const rect& rect, char* buf);
void clear_draw_detection_rect();

class raster_distortion;
void add_raster_distortion(const raster_distortion* distortion);
void remove_raster_distortion(const raster_distortion* distortion);
void clear_raster_distortion();

//a class that translates distortions within its scope.
class distortion_translation {
	int x_, y_;
public:
	distortion_translation();
	~distortion_translation();
	void translate(int x, int y);
};

void draw_rect(const SDL_Rect& rect, const SDL_Color& color,
               unsigned char alpha=0xFF);
void draw_rect(const rect& rect, const graphics::color& color);
void draw_hollow_rect(const SDL_Rect& rect, const SDL_Color& color,
               unsigned char alpha=0xFF);
void draw_hollow_rect(const rect& rect, const graphics::color& color);
void draw_circle(int x, int y, int radius);
int screen_width();
int screen_height();
void zoom_in();
void zoom_out();
void zoom_default();

void push_clip(const SDL_Rect& rect);
void pop_clip();

struct clip_scope {
	clip_scope(const SDL_Rect& rect);

	~clip_scope();

	void apply(const SDL_Rect& r);
	void reapply();

	clip_scope* parent_;
	SDL_Rect area_;
	GLfloat matrix_[16];
	boost::scoped_ptr<stencil_scope> stencil_;
};

int get_configured_msaa();

}

#endif
