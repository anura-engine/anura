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
#include "graphics.hpp"

#include "asserts.hpp"
#include "camera.hpp"
#include "foreach.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "raster_distortion.hpp"
#include "rectangle_rotator.hpp"
#include "texture_frame_buffer.hpp"

#include "SDL.h"
#include "SDL_syswm.h"

#include <boost/shared_array.hpp>
#include <iostream>
#include <cmath>
#include <stack>

namespace graphics
{

namespace 
{
	bool g_flip_draws = false;

	PREF_INT(msaa, 0);

	uint32_t next_pow2(uint32_t v) 
	{
		--v;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		return ++v;
	}

	int g_msaa_set = 0;
}

int get_configured_msaa()
{
	return g_msaa_set;
}

flip_draw_scope::flip_draw_scope() : old_value(g_flip_draws)
{
	g_flip_draws = true;
}

flip_draw_scope::~flip_draw_scope()
{
	g_flip_draws = old_value;
}

void reset_opengl_state()
{
	glShadeModel(GL_SMOOTH);
	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
#if !defined(USE_SHADERS)
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#if defined(USE_SHADERS)
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glClearColor(0.0,0.0,0.0,0.0);
	gles2::init_default_shader();
#else
	glColor4ub(255,255,255,255);
#endif
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);
	glDepthRange(0.0f, 1.0f);
	glClearDepth(1.0);
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
namespace {
	SDL_Window* global_main_window = NULL;
	SDL_Renderer* global_renderer = NULL;

	GLuint fbo_framebuffer;
	GLuint fbo_texture;
	boost::scoped_ptr<graphics::texture> fbo_texture_obj;
	GLuint real_framebuffer;
	int fbo_framebuffer_width, fbo_framebuffer_height;
}

SDL_Window* get_window()
{
	ASSERT_LOG(global_main_window != NULL, "swap_buffers called on NULL window");
	return global_main_window;
}
#endif

namespace {
	int g_letterbox_horz, g_letterbox_vert;
}

void swap_buffers()
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	ASSERT_LOG(global_main_window != NULL, "swap_buffers called on NULL window");

	g_letterbox_horz = g_letterbox_vert = 0;
	if(fbo_framebuffer) {
		glBindFramebuffer(GL_FRAMEBUFFER, real_framebuffer);

		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		int width = 0, height = 0;
		SDL_GetWindowSize(global_main_window, &width, &height);

		if((fbo_framebuffer_width*1000)/fbo_framebuffer_height < (width*1000)/height) {
			//actual screen wider than the framebuffer, letterboxing
			//on the sides.
			const int normalized_width = (fbo_framebuffer_width*height)/fbo_framebuffer_height;

			g_letterbox_horz = width - normalized_width;
		} else if((fbo_framebuffer_width*1000)/fbo_framebuffer_height > (width*1000)/height) {
			//actual screen narrow than the framebuffer, letterboxing
			//on the top/bottom.
			const int normalized_height = (fbo_framebuffer_height*width)/fbo_framebuffer_width;
			g_letterbox_vert = height - normalized_height;
		}


		preferences::screen_dimension_override_scope dim_scope(width, height, width, height);
		prepare_raster();

		fbo_texture_obj->set_as_current_texture();

		graphics::blit_texture(*fbo_texture_obj, g_letterbox_horz/2, g_letterbox_vert/2, width - g_letterbox_horz, height - g_letterbox_vert, 0.0, 0.0, 1.0, 1.0, 0.0);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo_framebuffer);
	}

	SDL_GL_SwapWindow(global_main_window );
#else
	SDL_GL_SwapBuffers();
#endif
#if defined(__ANDROID__)
	graphics::reset_opengl_state();
#endif
}

bool set_video_mode(int w, int h)
{
#ifdef TARGET_OS_HARMATTAN
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	return set_video_mode(w,h,0,SDL_OPENGLES | SDL_FULLSCREEN);
#else
#if SDL_VERSION_ATLEAST(2, 0, 0)
	return set_video_mode(w,h,SDL_WINDOW_SHOWN|SDL_WINDOW_OPENGL|(preferences::resizable() ? SDL_WINDOW_RESIZABLE : 0)|(preferences::fullscreen() ? SDL_WINDOW_FULLSCREEN : 0)) != NULL;
#else
	return set_video_mode(w,h,0,SDL_OPENGL|(preferences::resizable() ? SDL_RESIZABLE : 0)|(preferences::fullscreen() ? SDL_FULLSCREEN : 0)) != NULL;
#endif
#endif
}

#if SDL_VERSION_ATLEAST(2, 0, 0)

SDL_DisplayMode set_video_mode_auto_select()
{
	SDL_DisplayMode mode;

	//uncomment out when support for SDL_GetWindowDisplayIndex stabilizes.
	const int display_index = 0; //SDL_GetWindowDisplayIndex(graphics::get_window());
	SDL_GetDesktopDisplayMode(display_index, &mode);

	std::cerr << "CURRENT MODE IS " << mode.w << "x" << mode.h << "\n";

	SDL_DisplayMode best_mode = mode;
	if(preferences::fullscreen() == false && mode.w > 1024 && mode.h > 768) {
		const int nmodes = SDL_GetNumDisplayModes(display_index);
		for(int n = 0; n != nmodes; ++n) {
			SDL_DisplayMode candidate_mode;
			const int nvalue = SDL_GetDisplayMode(display_index, n, &candidate_mode);
			if(nvalue != 0) {
				std::cerr << "ERROR QUERYING DISPLAY INFO: " << SDL_GetError() << "\n";
				continue;
			}

			const float MinReduction = 0.9;

			if(candidate_mode.w < mode.w && candidate_mode.h < mode.w && candidate_mode.w < mode.w*MinReduction && candidate_mode.h < mode.h*MinReduction && (candidate_mode.w >= best_mode.w && candidate_mode.h >= best_mode.h || best_mode.w == mode.w && best_mode.h == mode.h)) {
	std::cerr << "BETTER MODE IS " << candidate_mode.w << "x" << candidate_mode.h << "\n";
				best_mode = candidate_mode;
			} else {
	std::cerr << "REJECTED MODE IS " << candidate_mode.w << "x" << candidate_mode.h << "\n";
			}
		}
	}

	if(best_mode.w < 1024 || best_mode.h < 768) {
		best_mode.w = 1024;
		best_mode.h = 768;
	}

	const bool result = set_video_mode(best_mode.w, best_mode.h, SDL_WINDOW_OPENGL);
	ASSERT_LOG(result, "FAILED TO SET AUTO SELECT VIDEO MODE: " << best_mode.w << "x" << best_mode.h);
	
	return best_mode;
}

namespace {
PREF_INT(grab_fullscreen, 0);
}

SDL_Window* set_video_mode(int w, int h, int flags)
{
	static SDL_Window* wnd = NULL;
	static SDL_GLContext ctx = NULL;
	static int wnd_flags = 0;

	setup_fbo_rendering(0, 0);

	bool grab_fullscreen = false;
	int grab_fullscreen_w = w, grab_fullscreen_h = h;

	if((flags&SDL_WINDOW_FULLSCREEN) && !g_grab_fullscreen) {
		grab_fullscreen = true;
		const int display_index = 0; //SDL_GetWindowDisplayIndex(graphics::get_window());
		SDL_DisplayMode mode;
		SDL_GetDesktopDisplayMode(display_index, &mode);
		w = mode.w;
		h = mode.h;
	}

	if(wnd) {
		SDL_DisplayMode mode;
		if(SDL_GetWindowDisplayMode(wnd, &mode) == 0) {
			mode.w = w;
			mode.h = h;
			if(SDL_SetWindowDisplayMode(wnd, &mode) == 0) {
				SDL_SetWindowSize(wnd, w, h);

				if(grab_fullscreen) {
					SDL_SetWindowFullscreen(wnd, SDL_WINDOW_FULLSCREEN_DESKTOP);
					setup_fbo_rendering(grab_fullscreen_w, grab_fullscreen_h);
				} else {
					SDL_SetWindowFullscreen(wnd, flags&SDL_WINDOW_FULLSCREEN);
					SDL_SetWindowSize(wnd, w, h);
					SDL_SetWindowPosition(wnd, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
				}

				return wnd;
			} else {
				fprintf(stderr, "ERROR: Failed to set window display mode. Destroying window and creating a new one.\n");
			}

		} else {
			fprintf(stderr, "ERROR: Failed to get window display mode. Destroying window and creating a new one.\n");
		}
	}

	wnd_flags = flags;
	
	graphics::texture::unbuild_all();
#if defined(USE_SHADERS) 
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	if(g_msaa > 0) {
		if(SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1) != 0) {
			std::cerr << "MSAA(" << g_msaa << ") requested but mutlisample buffer couldn't be allocated." << std::endl;
		} else {
			size_t msaa = next_pow2(g_msaa);
			std::cerr << "Requesting MSAA of " << msaa;
			if(SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa) != 0) {
				std::cerr << " -- Failure, disabled.";
			}
			std::cerr << std::endl;
		}
	}
#endif
	if(global_renderer) {
		SDL_DestroyRenderer(global_renderer);
		global_renderer = NULL;
	}
	if(ctx) {
		SDL_GL_DeleteContext(ctx);
		ctx = NULL;
	}
	if(wnd) {
		SDL_DestroyWindow(wnd);
		global_main_window = wnd = NULL;		
	}
	if(!(flags & CLEANUP_WINDOW_CONTEXT)) {
		global_main_window = wnd = SDL_CreateWindow(module::get_module_pretty_name().c_str(), 
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
		ctx = SDL_GL_CreateContext(wnd);
		global_renderer = SDL_CreateRenderer(wnd, -1, SDL_RENDERER_ACCELERATED);
#if defined(__GLEW_H__)
	GLenum glew_status = glewInit();
	ASSERT_EQ(glew_status, GLEW_OK);
#endif
		
		reset_opengl_state();
		graphics::texture::rebuild_all();
		texture_frame_buffer::rebuild();
	}
#if defined(USE_SHADERS)
	int depth_size, stencil_size;
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depth_size);
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_size);
	std::cerr << "Depth buffer size: " << depth_size << std::endl;
	std::cerr << "Stenicl buffer size: " << stencil_size << std::endl;
	int depth;
	glGetIntegerv(GL_DEPTH_BITS, &depth);
	std::cerr << "Depth(from GL) buffer size: " << depth << std::endl;

	if(g_msaa > 0 && SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &g_msaa_set) == 0) {
		std::cerr << "Actual MSAA: " << g_msaa_set << std::endl; 
	}

	if(grab_fullscreen) {
		setup_fbo_rendering(grab_fullscreen_w, grab_fullscreen_h);
	}
#endif
	return wnd;
}
#else
SDL_Surface* set_video_mode(int w, int h, int bitsperpixel, int flags)
{
	graphics::texture::unbuild_all();
	SDL_Surface* result = SDL_SetVideoMode(w,h,bitsperpixel,flags);
	reset_opengl_state();
	graphics::texture::rebuild_all();
	texture_frame_buffer::rebuild();

	return result;
}
#endif

	/* unavoidable global variable to store global clip
	 rectangle changes */
	std::vector<boost::shared_array<GLint> > clip_rectangles;
	
	std::vector<GLfloat>& global_vertex_array()
	{
		static std::vector<GLfloat> v;
		return v;
	}
	
	std::vector<GLfloat>& global_texcoords_array()
	{
		static std::vector<GLfloat> v;
		return v;
	}

	std::vector<GLbyte>& global_vertex_color_array()
	{
		static std::vector<GLbyte> v;
		return v;
	}
	
#if defined(GL_ES_VERSION_2_0) || defined(GL_VERSION_ES_CM_1_0) || defined(GL_VERSION_ES_CL_1_0) \
	|| defined(GL_VERSION_ES_CM_1_1) || defined(GL_VERSION_ES_CL_1_1)
#define glOrtho glOrthof
#endif

	void setup_fbo_rendering(int width, int height)
	{
		if(width == fbo_framebuffer_width && height == fbo_framebuffer_height) {
			return;
		}

		if(fbo_texture) {
			glDeleteFramebuffers(1, &fbo_framebuffer);
			glDeleteTextures(1, &fbo_texture);

			fbo_framebuffer = 0;
			fbo_texture = 0;
			fbo_texture_obj.reset();

			fbo_framebuffer_width = fbo_framebuffer_height = 0;

			glBindFramebuffer(GL_FRAMEBUFFER, real_framebuffer);
		}

		if(width == 0 || height == 0) {
			return;
		}

		glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*)&real_framebuffer);

		glGenTextures(1, &fbo_texture);
		glBindTexture(GL_TEXTURE_2D, fbo_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		fbo_texture_obj.reset(new graphics::texture(fbo_texture, width, height));
		fbo_framebuffer_width = width;
		fbo_framebuffer_height = height;

		glGenFramebuffers(1, &fbo_framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_framebuffer);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                       GL_TEXTURE_2D, fbo_texture, 0);
		int mywidth, myheight;
		SDL_GetWindowSize(global_main_window, &mywidth, &myheight);

		preferences::set_actual_screen_width(fbo_framebuffer_width);
		preferences::set_actual_screen_height(fbo_framebuffer_height);
	}

	void map_mouse_position(int* x, int* y)
	{
		if(!fbo_texture) {
			return;
		}

		if(x) {
			*x -= g_letterbox_horz/2;
		}

		if(y) {
			*y -= g_letterbox_vert/2;
		}

		int width = 0, height = 0;
		SDL_GetWindowSize(global_main_window, &width, &height);

		if(x) {
			*x *= fbo_framebuffer_width;
			*x /= width - g_letterbox_horz;
		}

		if(y) {
			*y *= fbo_framebuffer_height;
			*y /= height - g_letterbox_vert;
		}
	}
	
	void prepare_raster()
	{
		glViewport(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height());
		glShadeModel(GL_FLAT);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		
		if(preferences::screen_rotated()) {
			int top = screen_width();
			int bot = 0;
			if(g_flip_draws) {
				std::swap(top, bot);
			}
			glOrtho(0, screen_height(), top, bot, -1.0, 1.0);
		} else {
			int top = screen_height();
			int bot = 0;
			if(g_flip_draws) {
				std::swap(top, bot);
			}
			glOrtho(0, screen_width(), top, bot, -1.0, 1.0);
		}
		
		if(preferences::screen_rotated()) {
			// Rotate 90 degrees ccw, then move real_h pixels down
			// This has to be in opposite order since A(); B(); means A(B(x))
			glTranslatef(screen_height(), 0.0f, 0.0f);
			glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
		}
		
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		glDisable(GL_DEPTH_TEST);
#if !defined(USE_SHADERS)
		glDisable(GL_LIGHTING);
		glDisable(GL_LIGHT0);
#endif
		
		glColor4f(1.0, 1.0, 1.0, 1.0);
	}
	
	namespace {
		struct draw_detection_rect {
			rect area;
			char* buf;
		};
		
		std::vector<draw_detection_rect> draw_detection_rects_;
		rect draw_detection_rect_;
		char* draw_detection_buf_;
		
		std::vector<const raster_distortion*> distortions_;
	}
	
	void blit_texture(const texture& tex, int x, int y, GLfloat rotate)
	{
		if(!tex.valid()) {
			return;
		}

		x &= preferences::xypos_draw_mask;
		y &= preferences::xypos_draw_mask;
		
		int h = tex.height();
		int w = tex.width();
		const int w_odd = w % 2;
		const int h_odd = h % 2;
		h /= 2;
		w /= 2;
		
		glPushMatrix();
		
		glTranslatef(x+w,y+h,0.0);
		glRotatef(rotate,0.0,0.0,1.0);
		
		tex.set_as_current_texture();
		
		GLfloat varray[] = {
			(GLfloat)-w, (GLfloat)-h,
			(GLfloat)-w, (GLfloat)h+h_odd,
			(GLfloat)w+w_odd, (GLfloat)-h,
			(GLfloat)w+w_odd, (GLfloat)h+h_odd
		};
		GLfloat tcarray[] = {
			texture::get_coord_x(0.0), texture::get_coord_y(0.0),
			texture::get_coord_x(0.0), texture::get_coord_y(1.0),
			texture::get_coord_x(1.0), texture::get_coord_y(0.0),
			texture::get_coord_x(1.0), texture::get_coord_y(1.0)
		};
#if defined(USE_SHADERS)
		gles2::active_shader()->prepare_draw();
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
		gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0, tcarray);
#else
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glTexCoordPointer(2, GL_FLOAT, 0, tcarray);
#endif
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		
		glPopMatrix();
	}
	
	namespace {
		
		//function which marks the draw detection buffer with pixels drawn.
		void detect_draw(const texture& tex, int x, int y, int orig_w, int orig_h, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
		{
			if(draw_detection_rects_.empty()) {
				return;
			}
			
			rect draw_rect(x, y, std::abs(orig_w), std::abs(orig_h));
			
			foreach(const draw_detection_rect& detect, draw_detection_rects_) {
				if(rects_intersect(draw_rect, detect.area)) {
					rect r = intersection_rect(draw_rect, detect.area);
					for(int ypos = r.y(); ypos != r.y2(); ++ypos) {
						for(int xpos = r.x(); xpos != r.x2(); ++xpos) {
							const GLfloat u = (GLfloat(draw_rect.x2() - xpos)*x1 + GLfloat(xpos - draw_rect.x())*x2)/GLfloat(draw_rect.w());
							const GLfloat v = (GLfloat(draw_rect.y2() - ypos)*y1 + GLfloat(ypos - draw_rect.y())*y2)/GLfloat(draw_rect.h());
							const int texture_x = u*tex.width();
							const int texture_y = v*tex.height();
							ASSERT_GE(texture_x, 0);
							ASSERT_GE(texture_y, 0);
							ASSERT_LOG(texture_x < tex.width(), texture_x << " < " << tex.width() << " " << r.x() << " " << r.x2() << " " << xpos << " x: " << x1 << " x2: " << x2 << " u: " << u << "\n");
							ASSERT_LT(texture_x, tex.width());
							ASSERT_LT(texture_y, tex.height());
							const bool alpha = tex.is_alpha(texture_x, texture_y);
							if(!alpha) {
								const int buf_x = xpos - detect.area.x();
								const int buf_y = ypos - detect.area.y();
								const int buf_index = buf_y*detect.area.w() + buf_x;
								ASSERT_LOG(buf_index >= 0, xpos << ", " << ypos << " -> " << buf_x << ", " << buf_y << " -> " << buf_index << " in " << detect.area << "\n");
								ASSERT_GE(buf_index, 0);
								ASSERT_LT(buf_index, detect.area.w()*detect.area.h());
								detect.buf[buf_index] = true;
							}
						}
					}
				}
			}
		}
		
		void blit_texture_internal(const texture& tex, int x, int y, int w, int h, GLfloat rotate, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
		{
			if(!tex.valid()) {
				return;
			}
			
			const int w_odd = w % 2;
			const int h_odd = h % 2;
			
			w /= 2;
			h /= 2;
			glPushMatrix();
			tex.set_as_current_texture();
			glTranslatef(x+abs(w),y+abs(h),0.0);
			glRotatef(rotate,0.0,0.0,1.0);
			GLfloat varray[] = {
				(GLfloat)-w, (GLfloat)-h,
				(GLfloat)-w, (GLfloat)h+h_odd,
				(GLfloat)w+w_odd, (GLfloat)-h,
				(GLfloat)w+w_odd, (GLfloat)h+h_odd
			};
			GLfloat tcarray[] = {
				texture::get_coord_x(x1), texture::get_coord_y(y1),
				texture::get_coord_x(x1), texture::get_coord_y(y2),
				texture::get_coord_x(x2), texture::get_coord_y(y1),
				texture::get_coord_x(x2), texture::get_coord_y(y2)
			};
#if defined(USE_SHADERS)
			gles2::active_shader()->prepare_draw();
			gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
			gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0, tcarray);
#else
			glVertexPointer(2, GL_FLOAT, 0, varray);
			glTexCoordPointer(2, GL_FLOAT, 0, tcarray);
#endif
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			glPopMatrix();
		}
		
		void blit_texture_with_distortion(const texture& tex, int x, int y, int w, int h, GLfloat rotate, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2, const raster_distortion& distort)
		{
			const rect& area = distort.area();
			if(x < area.x()) {
				const int new_x = area.x();
				const GLfloat new_x1 = (x1*(x + w - new_x) + x2*(new_x - x))/w;
				
				blit_texture(tex, x, y, new_x - x, h, rotate, x1, y1, new_x1, y2);
				
				x1 = new_x1;
				w -= new_x - x;
				x = new_x;
			}
			
			if(y < area.y()) {
				const int new_y = area.y();
				const GLfloat new_y1 = (y1*(y + h - new_y) + y2*(new_y - y))/h;
				
				blit_texture(tex, x, y, w, new_y - y, rotate, x1, y1, x2, new_y1);
				
				y1 = new_y1;
				h -= new_y - y;
				y = new_y;
			}
			
			if(x + w > area.x2()) {
				const int new_w = area.x2() - x;
				const int new_xpos = x + new_w;
				const GLfloat new_x2 = (x1*(x + w - new_xpos) + x2*(new_xpos - x))/w;
				
				blit_texture(tex, new_xpos, y, x + w - new_xpos, h, rotate, new_x2, y1, x2, y2);
				
				x2 = new_x2;
				w = new_w;
			}
			
			if(y + h > area.y2()) {
				const int new_h = area.y2() - y;
				const int new_ypos = y + new_h;
				const GLfloat new_y2 = (y1*(y + h - new_ypos) + y2*(new_ypos - y))/h;
				
				blit_texture(tex, x, new_ypos, w, y + h - new_ypos, rotate, x1, new_y2, x2, y2);
				
				y2 = new_y2;
				h = new_h;
			}
			
			tex.set_as_current_texture();
			
			const int xdiff = distort.granularity_x();
			const int ydiff = distort.granularity_y();
			for(int xpos = 0; xpos < w; xpos += xdiff) {
				const int xbegin = x + xpos;
				const int xend = std::min<int>(x + w, xbegin + xdiff);
				
				const GLfloat u1 = (x1*(x+w - xbegin) + x2*(xbegin - x))/w;
				const GLfloat u2 = (x1*(x+w - xend) + x2*(xend - x))/w;
				for(int ypos = 0; ypos < h; ypos += ydiff) {
					const int ybegin = y + ypos;
					const int yend = std::min<int>(y + h, ybegin + ydiff);
					
					const GLfloat v1 = (y1*(y+h - ybegin) + y2*(ybegin - y))/h;
					const GLfloat v2 = (y1*(y+h - yend) + y2*(yend - y))/h;
					
					GLfloat points[8] = { (GLfloat)xbegin, (GLfloat)ybegin, (GLfloat)xend, (GLfloat)ybegin, (GLfloat)xbegin, (GLfloat)yend, (GLfloat)xend, (GLfloat)yend };
					GLfloat uv[8] = { u1, v1, u2, v1, u1, v2, u2, v2 };
					
					for(int n = 0; n != 4; ++n) {
						distort.distort_point(&points[n*2], &points[n*2 + 1]);
					}
					
#if defined(USE_SHADERS)
					gles2::active_shader()->prepare_draw();
					gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, points);
					gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0, uv);
#else
					glVertexPointer(2, GL_FLOAT, 0, points);
					glTexCoordPointer(2, GL_FLOAT, 0, uv);
#endif
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				}
			}
		}
		
		int blit_texture_translate_x = 0;
		int blit_texture_translate_y = 0;
		
	}  // namespace
	
	distortion_translation::distortion_translation()
	: x_(0), y_(0)
	{
	}
	
	distortion_translation::~distortion_translation()
	{
		if(x_ || y_) {
			foreach(const raster_distortion* distort, distortions_) {
				rect r = distort->area();
				r = rect(r.x() + x_, r.y() + y_, r.w(), r.h());
				const_cast<raster_distortion*>(distort)->set_area(r);
			}
		}
	}
	
	void distortion_translation::translate(int x, int y)
	{
		x_ += x;
		y_ += y;
		
		foreach(const raster_distortion* distort, distortions_) {
			rect r = distort->area();
			r = rect(r.x() - x, r.y() - y, r.w(), r.h());
			const_cast<raster_distortion*>(distort)->set_area(r);
		}
	}
	
	void blit_texture(const texture& tex, int x, int y, int w, int h, GLfloat rotate, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
	{
		x &= preferences::xypos_draw_mask;
		y &= preferences::xypos_draw_mask;

		if(w < 0) {
			std::swap(x1, x2);
			w *= -1;
		}
		
		if(h < 0) {
			std::swap(y1, y2);
			h *= -1;
		}
		
		detect_draw(tex, x, y, w, h, x1, y1, x2, y2);
		
		for(std::vector<const raster_distortion*>::const_iterator i = distortions_.begin(); i != distortions_.end() && rotate == 0.0; ++i) {
			const raster_distortion& distort = **i;
			if(rects_intersect(rect(x, y, w, h), distort.area())) {
				blit_texture_with_distortion(tex, x, y, w, h, rotate, x1, y1, x2, y2, distort);
				return;
			}
		}
		blit_texture_internal(tex, x, y, w, h, rotate, x1, y1, x2, y2);
	}

namespace {
const texture* blit_current_texture;
std::vector<GLfloat> blit_tcqueue;
std::vector<GLshort> blit_vqueue;
}

void queue_blit_texture(const texture& tex, int x, int y, int w, int h,
                        GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	x &= preferences::xypos_draw_mask;
	y &= preferences::xypos_draw_mask;

	if(&tex != blit_current_texture) {
		flush_blit_texture();
		blit_current_texture = &tex;
	}

	x1 = tex.translate_coord_x(x1);
	y1 = tex.translate_coord_y(y1);
	x2 = tex.translate_coord_x(x2);
	y2 = tex.translate_coord_y(y2);

	if(w < 0) {
		std::swap(x1, x2);
		w *= -1;
	}
		
	if(h < 0) {
		std::swap(y1, y2);
		h *= -1;
	}
	
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y2);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y2);
	
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y + h);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y + h);
}

void queue_blit_texture(const texture& tex, int x, int y, int w, int h, GLfloat rotate,
						GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	x &= preferences::xypos_draw_mask;
	y &= preferences::xypos_draw_mask;
	
	if(&tex != blit_current_texture) {
		flush_blit_texture();
		blit_current_texture = &tex;
	}
	
	x1 = tex.translate_coord_x(x1);
	y1 = tex.translate_coord_y(y1);
	x2 = tex.translate_coord_x(x2);
	y2 = tex.translate_coord_y(y2);
	
	if(w < 0) {
		std::swap(x1, x2);
		w *= -1;
	}
	
	if(h < 0) {
		std::swap(y1, y2);
		h *= -1;
	}
	
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y2);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y2);
	
	
	
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y + h);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y + h);

	rect r(x,y,w,h);
	GLshort* varray = &blit_vqueue[blit_vqueue.size()-8];
	rotate_rect(x+(w/2), y+(h/2), rotate, varray); 

}

void queue_blit_texture_3d(const texture& tex, 
	GLfloat x, GLfloat y, GLfloat z, 
	int w, int h, 
	GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	x1 = tex.translate_coord_x(x1);
	y1 = tex.translate_coord_y(y1);
	x2 = tex.translate_coord_x(x2);
	y2 = tex.translate_coord_y(y2);
	
	if(w < 0) {
		std::swap(x1, x2);
		w *= -1;
	}
	
	if(h < 0) {
		std::swap(y1, y2);
		h *= -1;
	}
	
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y2);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y2);
	
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(z);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(z);
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y + h);
	blit_vqueue.push_back(z);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y + h);
	blit_vqueue.push_back(z);
}

void flush_blit_texture_3d()
{
	if(!blit_current_texture) {
		return;
	}
	blit_current_texture->set_as_current_texture();
#if defined(USE_SHADERS)
	gles2::active_shader()->shader()->vertex_array(3, GL_SHORT, 0, 0, &blit_vqueue.front());
	gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0,  &blit_tcqueue.front());
#else
	glVertexPointer(3, GL_SHORT, 0, &blit_vqueue.front());
	glTexCoordPointer(2, GL_FLOAT, 0, &blit_tcqueue.front());
#endif
	glDrawArrays(GL_TRIANGLE_STRIP, 0, blit_tcqueue.size()/2);

	blit_current_texture = NULL;
	blit_tcqueue.clear();
	blit_vqueue.clear();
}


void flush_blit_texture()
{
	if(!blit_current_texture) {
		return;
	}

	blit_current_texture->set_as_current_texture();
#if defined(USE_SHADERS)
	gles2::active_shader()->prepare_draw();
	gles2::active_shader()->shader()->vertex_array(2, GL_SHORT, 0, 0, &blit_vqueue.front());
	gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0,  &blit_tcqueue.front());
#else
	glVertexPointer(2, GL_SHORT, 0, &blit_vqueue.front());
	glTexCoordPointer(2, GL_FLOAT, 0, &blit_tcqueue.front());
#endif
	glDrawArrays(GL_TRIANGLE_STRIP, 0, blit_tcqueue.size()/2);

	blit_current_texture = NULL;
	blit_tcqueue.clear();
	blit_vqueue.clear();
}

void blit_queue::clear()
{
	texture_ = 0;
	vertex_.clear();
	uv_.clear();
}

void blit_queue::do_blit() const
{
	if(vertex_.empty()) {
		return;
	}

	texture::set_current_texture(texture_);

#if defined(USE_SHADERS)
	gles2::active_shader()->prepare_draw();
	gles2::active_shader()->shader()->vertex_array(2, GL_SHORT, 0, 0, &vertex_.front());
	gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0,  &uv_.front());
#else
	glVertexPointer(2, GL_SHORT, 0, &vertex_.front());
	glTexCoordPointer(2, GL_FLOAT, 0, &uv_.front());
#endif
	glDrawArrays(GL_TRIANGLE_STRIP, 0, uv_.size()/2);
}

void blit_queue::do_blit_range(short begin, short end) const
{
	if(vertex_.empty()) {
		return;
	}

	texture::set_current_texture(texture_);

#if defined(USE_SHADERS)
	gles2::active_shader()->prepare_draw();
	gles2::active_shader()->shader()->vertex_array(2, GL_SHORT, 0, 0, &vertex_[begin]);
	gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0,  &uv_[begin]);
#else
	glVertexPointer(2, GL_SHORT, 0, &vertex_[begin]);
	glTexCoordPointer(2, GL_FLOAT, 0, &uv_[begin]);
#endif
	glDrawArrays(GL_TRIANGLE_STRIP, 0, (end - begin)/2);
}

bool blit_queue::merge(const blit_queue& q, short begin, short end)
{
	if(vertex_.empty()) {
		texture_ = q.texture_;
		vertex_.insert(vertex_.end(), q.vertex_.begin()+begin, q.vertex_.begin()+end);
		uv_.insert(uv_.end(), q.uv_.begin()+begin, q.uv_.begin()+end);
		return true;
	}

	if(texture_ != q.texture_) {
		return false;
	}

	repeat_last();
	vertex_.push_back(q.vertex_[begin]);
	vertex_.push_back(q.vertex_[begin+1]);
	uv_.push_back(q.uv_[begin]);
	uv_.push_back(q.uv_[begin+1]);

	vertex_.insert(vertex_.end(), q.vertex_.begin()+begin, q.vertex_.begin()+end);
	uv_.insert(uv_.end(), q.uv_.begin()+begin, q.uv_.begin()+end);

	return true;
}
	
	void set_draw_detection_rect(const rect& rect, char* buf)
	{
		draw_detection_rect new_rect = { rect, buf };
		draw_detection_rects_.push_back(new_rect);
	}
	
	void clear_draw_detection_rect()
	{
		draw_detection_rects_.clear();
	}
	
	void add_raster_distortion(const raster_distortion* distortion)
	{
//TODO: distortions currently disabled
//		distortion->next_cycle();
//		distortions_.push_back(distortion);
	}
	
	void remove_raster_distortion(const raster_distortion* distortion)
	{
//		distortions_.erase(std::remove(distortions_.begin(), distortions_.end(), distortion), distortions_.end());
	}
	
	void clear_raster_distortion()
	{
		distortions_.clear();
	}
	
	void draw_rect(const SDL_Rect& r, const SDL_Color& color,
				   unsigned char alpha)
	{
		GLfloat varray[] = {
			(GLfloat)r.x, (GLfloat)r.y,
			(GLfloat)r.x+r.w, (GLfloat)r.y,
			(GLfloat)r.x, (GLfloat)r.y+r.h,
			(GLfloat)r.x+r.w, (GLfloat)r.y+r.h
		};
#if defined(USE_SHADERS)
		glColor4ub(color.r,color.g,color.b,alpha);
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glColor4f(1.0, 1.0, 1.0, 1.0);
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glColor4ub(color.r,color.g,color.b,alpha);
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		//glRecti(r.x,r.y,r.x+r.w,r.y+r.h);
		glColor4ub(255, 255, 255, 255);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif
	}
	
	void draw_rect(const rect& r, const graphics::color& color)
	{
		GLfloat varray[] = {
			(GLfloat)r.x(), (GLfloat)r.y(),
			(GLfloat)r.x()+r.w(), (GLfloat)r.y(),
			(GLfloat)r.x(), (GLfloat)r.y()+r.h(),
			(GLfloat)r.x()+r.w(), (GLfloat)r.y()+r.h()
		};
#if defined(USE_SHADERS)
		glColor4ub(color.r(),color.g(),color.b(),color.a());
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glColor4f(1.0, 1.0, 1.0, 1.0);
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glColor4ub(color.r(),color.g(),color.b(),color.a());
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		//glRecti(r.x(),r.y(),r.x()+r.w(),r.y()+r.h());
		glColor4ub(255, 255, 255, 255);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif
	}
	
	
	void draw_hollow_rect(const SDL_Rect& r, const SDL_Color& color,
						  unsigned char alpha)
	{
		GLfloat varray[] = {
			(GLfloat)r.x, (GLfloat)r.y,
			(GLfloat)r.x + r.w, (GLfloat)r.y,
			(GLfloat)r.x + r.w, (GLfloat)r.y + r.h,
			(GLfloat)r.x, (GLfloat)r.y + r.h
		};
#if defined(USE_SHADERS)
		glColor4ub(color.r, color.g, color.b, alpha);
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
		glDrawArrays(GL_LINE_LOOP, 0, sizeof(varray)/sizeof(GLfloat)/2);
		glColor4f(1.0, 1.0, 1.0, 1.0);
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glColor4ub(color.r, color.g, color.b, alpha);
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_LINE_LOOP, 0, sizeof(varray)/sizeof(GLfloat)/2);
		glColor4ub(255, 255, 255, 255);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif
	}

	void draw_hollow_rect(const rect& r, const graphics::color& color)
	{
		GLfloat varray[] = {
			(GLfloat)r.x(), (GLfloat)r.y(),
			(GLfloat)r.x() + r.w(), (GLfloat)r.y(),
			(GLfloat)r.x() + r.w(), (GLfloat)r.y() + r.h(),
			(GLfloat)r.x(), (GLfloat)r.y() + r.h()
		};
#if defined(USE_SHADERS)
		glColor4ub(color.r(), color.g(), color.b(), color.a());
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
		glDrawArrays(GL_LINE_LOOP, 0, sizeof(varray)/sizeof(GLfloat)/2);
		glColor4f(1.0, 1.0, 1.0, 1.0);
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glColor4ub(color.r(), color.g(), color.b(), color.a());
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_LINE_LOOP, 0, sizeof(varray)/sizeof(GLfloat)/2);
		glColor4ub(255, 255, 255, 255);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif
	}


	void draw_circle(int x, int y, int radius)
	{
		static std::vector<GLfloat> varray;
		varray.clear();
		varray.push_back(x);
		varray.push_back(y);
		for(double angle = 0; angle < 3.1459*2.0; angle += 0.1) {
			const double xpos = x + radius*cos(angle);
			const double ypos = y + radius*sin(angle);
			varray.push_back(xpos);
			varray.push_back(ypos);
		}

		//repeat the first coordinate to complete the circle.
		varray.push_back(varray[2]);
		varray.push_back(varray[3]);

#if defined(USE_SHADERS)
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray.front());
		glDrawArrays(GL_TRIANGLE_FAN, 0, varray.size()/2);
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glVertexPointer(2, GL_FLOAT, 0, &varray.front());
		glDrawArrays(GL_TRIANGLE_FAN, 0, varray.size()/2);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif
	}

	namespace {
	struct stencil_buffer_settings {
		bool enabled;
		GLint mask;
		GLenum func;
		GLint ref;
		GLuint ref_mask;

		GLenum sfail, dpfail, dppass;
	};

	std::stack<stencil_buffer_settings> stencil_buffer_stack;

	}

	stencil_scope::stencil_scope(bool enabled, int mask, GLenum func, GLenum ref, GLenum ref_mask, GLenum sfail, GLenum dpfail, GLenum dppass)
	  : init_(true)
	{
		stencil_buffer_settings settings = { enabled, mask, func, ref, ref_mask, sfail, dpfail, dppass };
		stencil_buffer_stack.push(settings);
		apply_settings();
	}

	stencil_scope::~stencil_scope() {
		if(init_) {
			revert_settings();
		}
	}

	void stencil_scope::apply_settings() {
		assert(!stencil_buffer_stack.empty());
		const stencil_buffer_settings& settings = stencil_buffer_stack.top();
		if(settings.enabled) {
			glEnable(GL_STENCIL_TEST);
		} else {
			glDisable(GL_STENCIL_TEST);
		}

		glStencilMask(settings.mask);
		glStencilFunc(settings.func, settings.ref, settings.ref_mask);
		glStencilOp(settings.sfail, settings.dpfail, settings.dppass);
	}
	
	void stencil_scope::revert_settings() {
		assert(!stencil_buffer_stack.empty());
		stencil_buffer_stack.pop();
		if(stencil_buffer_stack.empty()) {
			glDisable(GL_STENCIL_TEST);
			glStencilMask(0x0);
		} else {
			apply_settings();
		}
	}

	clip_scope* current_clip_scope = NULL;

	clip_scope::clip_scope(const SDL_Rect& r)
	  : parent_(current_clip_scope), area_(r)
	{
		glGetFloatv(GL_MODELVIEW_MATRIX, matrix_);
		apply(r);
		current_clip_scope = this;
	};
	
	void clip_scope::apply(const SDL_Rect& r)
	{
		{
		stencil_scope stencil_settings(true, 0x01, GL_NEVER, 0x01, 0xff, GL_REPLACE, GL_KEEP, GL_KEEP);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glClear(GL_STENCIL_BUFFER_BIT);
		
		GLfloat varray[] = {
			(GLfloat)r.x, (GLfloat)r.y,
			(GLfloat)r.x+r.w, (GLfloat)r.y,
			(GLfloat)r.x, (GLfloat)r.y+r.h,
			(GLfloat)r.x+r.w, (GLfloat)r.y+r.h
		};
#if defined(USE_SHADERS)
		glColor4f(1.0f,1.0f,1.0f,1.0f);
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
 		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#else
		glColor4ub(255, 255, 255, 255);
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif
		}

		stencil_.reset(new stencil_scope(true, 0x0, GL_EQUAL, 0x1, 0x1, GL_KEEP, GL_KEEP, GL_KEEP));

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}

	void clip_scope::reapply()
	{
		glPushMatrix();
		glLoadMatrixf(matrix_);
		apply(area_);
		glPopMatrix();
	}
	
	clip_scope::~clip_scope() {
		stencil_.reset();
		if(parent_) {
			parent_->reapply();
		}

		current_clip_scope = parent_;
	}
	
	namespace {
		int zoom_level = 1;
	}
	
	int screen_width()
	{
		return preferences::virtual_screen_width()*zoom_level;
		/*
		 SDL_Surface* surf = SDL_GetVideoSurface();
		 if(surf) {
		 return SDL_GetVideoSurface()->w;
		 } else {
		 return 1024;
		 }*/
	}
	
	int screen_height()
	{
		return preferences::virtual_screen_height()*zoom_level;
		/*
		 SDL_Surface* surf = SDL_GetVideoSurface();
		 if(surf) {
		 return SDL_GetVideoSurface()->h;
		 } else {
		 return 768;
		 }*/
	}
	
	void zoom_in()
	{
		--zoom_level;
		if(zoom_level < 1) {
			zoom_level = 1;
		}
	}
	
	void zoom_out()
	{
		++zoom_level;
		if(zoom_level > 5) {
			zoom_level = 5;
		}
	}
	
	void zoom_default()
	{
		zoom_level = 1;
	}

}
