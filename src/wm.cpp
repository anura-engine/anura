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
#include "asserts.hpp"
#include "fbo.hpp"
#include "graphics.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "surface_cache.hpp"
#include "texture.hpp"
#include "texture_frame_buffer.hpp"
#include "wm.hpp"

#include "level.hpp"

// Defined in video_selections.cpp
extern int g_vsync;

namespace preferences {
	void tweak_virtual_screen(int awidth, int aheight);
}

namespace graphics
{
	namespace
	{
		PREF_INT(msaa, 0, "Amount of multi-sampled AA to use in rendering");
		PREF_INT(min_window_width, 1024, "Minimum window width when auto-determining window size");
		PREF_INT(min_window_height, 768, "Minimum window height when auto-determining window size");

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

		SDL_DisplayMode mode_auto_select()
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

					const float MinReduction = 0.9f;

					if(candidate_mode.w < mode.w && candidate_mode.h < mode.w 
						&& candidate_mode.w < mode.w*MinReduction 
						&& candidate_mode.h < mode.h*MinReduction 
						&& (candidate_mode.w >= best_mode.w 
						&& candidate_mode.h >= best_mode.h 
						|| best_mode.w == mode.w && best_mode.h == mode.h)) {
						std::cerr << "BETTER MODE IS " << candidate_mode.w << "x" << candidate_mode.h << "\n";
						best_mode = candidate_mode;
					} else {
						std::cerr << "REJECTED MODE IS " << candidate_mode.w << "x" << candidate_mode.h << "\n";
					}
				}
			}

			if(best_mode.w < g_min_window_width || best_mode.h < g_min_window_height) {
				best_mode.w = g_min_window_width;
				best_mode.h = g_min_window_height;
			}

			return best_mode;
		}

	}

	window_manager::window_manager() 
		: width_(0), height_(0), gl_context_(0), msaa_set_(0), 
		letterbox_horz_(0), letterbox_vert_(0)
	{
#if !defined(__native_client__)
		Uint32 sdl_init_flags = SDL_INIT_VIDEO;
#if defined(_MSC_VER) || TARGET_OS_IPHONE
		sdl_init_flags |= SDL_INIT_TIMER;
#endif
		sdl_.reset(new SDL(sdl_init_flags));
		LOG( "After SDL_Init" );
#endif

#ifdef _MSC_VER
	freopen("CON", "w", stderr);
#endif
	}

	window_manager::~window_manager()
	{
	}

	void window_manager::notify_new_window_size()
	{
		SDL_GetWindowSize(sdl_window_.get(), &width_, &height_);
		screen_fbo_.reset(new fbo(0, 0, width_, height_, preferences::virtual_screen_width(), preferences::virtual_screen_height(), gles2::get_tex_shader()));
	}

	void window_manager::create_window(int width, int height)
	{
		if(preferences::auto_size_window()) {
			const SDL_DisplayMode mode = mode_auto_select();
			width_ = mode.w;
			height_ = mode.h;
		} else {
			width_ = width;
			height_ = height;
		}

#if defined(USE_SHADERS) 
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

		if(preferences::use_16bpp_textures()) {
			SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
			SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
			SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
			SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 1);
		} else {
			SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
		}

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
		int x = SDL_WINDOWPOS_CENTERED;
		int y = SDL_WINDOWPOS_CENTERED;
		int w = width_;
		int h = height_;
		Uint32 flags = SDL_WINDOW_OPENGL | (preferences::resizable() ? SDL_WINDOW_RESIZABLE : 0);

		switch(preferences::fullscreen()) {
			case preferences::FULLSCREEN_WINDOWED: {
				x = y = SDL_WINDOWPOS_UNDEFINED;
				w = h = 0;
				flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
				break;
			}
			case preferences::FULLSCREEN: {
				x = y = SDL_WINDOWPOS_UNDEFINED;
				flags |= SDL_WINDOW_FULLSCREEN;
				break;
			}
			case preferences::FULLSCREEN_NONE: // fallthrough -- default case no extra handling needed
			default: break;
		}

		sdl_window_.reset(SDL_CreateWindow(module::get_module_pretty_name().c_str(), x, y, w, h, flags), [&](SDL_Window* wnd){
			SDL_DestroyRenderer(sdl_renderer_);
			sdl_renderer_ = NULL;
			SDL_GL_DeleteContext(gl_context_);
			gl_context_ = NULL;
			SDL_DestroyWindow(wnd);
		});
		ASSERT_LOG(sdl_window_ != NULL, "FATAL: Failed to create window: " << SDL_GetError());
		gl_context_ = SDL_GL_CreateContext(sdl_window_.get());
		ASSERT_LOG(gl_context_ != NULL, "FATAL: Failed to GL Context: " << SDL_GetError());
		sdl_renderer_ = SDL_CreateRenderer(sdl_window_.get(), -1, SDL_RENDERER_ACCELERATED);
		ASSERT_LOG(sdl_renderer_ != NULL, "FATAL: Failed to create renderer: " << SDL_GetError());

		surface wm_icon = graphics::surface_cache::get_no_cache("window-icon.png");
		if(!wm_icon.null()) {
			SDL_SetWindowIcon(sdl_window_.get(), wm_icon.get());
		}

		if(preferences::fullscreen() == preferences::FULLSCREEN_WINDOWED
			|| preferences::fullscreen() == preferences::FULLSCREEN) {
			SDL_GetWindowSize(sdl_window_.get(), &width_, &height_);

			preferences::set_actual_screen_width(width);
			preferences::set_actual_screen_height(height);
			//preferences::set_virtual_screen_width(800);
			//preferences::set_virtual_screen_height(600);
		}
		std::cerr << "INFO: real window size: " << width << "," << height << std::endl;
		std::cerr << "INFO: actual screen size: " << width_ << "," << height_ << std::endl;

		if(preferences::fullscreen() == preferences::FULLSCREEN_NONE) {
			preferences::set_actual_screen_width(width_);
			preferences::set_actual_screen_height(height_);
			if(preferences::auto_size_window()) {
				preferences::set_virtual_screen_width(width_);
				preferences::set_virtual_screen_height(height_);
			} else {
				preferences::set_virtual_screen_width(width);
				preferences::set_virtual_screen_height(height);
				//preferences::tweak_virtual_screen(width_, height_);
			}
		}
		std::cerr << "INFO: virtual screen size: " << preferences::virtual_screen_width() << "," << preferences::virtual_screen_height() << std::endl;

		// Initialise glew library if headers have been included.
#if defined(__GLEW_H__)
		GLenum glew_status = glewInit();
		ASSERT_EQ(glew_status, GLEW_OK);
#endif
		print_gl_info();

		// Does all the GL setup and configuration stuff needed
		init_gl_context();

		graphics::texture::rebuild_all();
		texture_frame_buffer::rebuild();

		if(SDL_GL_MakeCurrent(sdl_window_.get(), gl_context_) != 0) {
			std::cerr << "WARNING: Unable to make open GL context current: " << SDL_GetError() << std::endl;
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

		if(g_msaa > 0 && SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &msaa_set_) == 0) {
			std::cerr << "Actual MSAA: " << msaa_set_ << std::endl; 
		}

#if defined(USE_SHADERS)
#if !defined(GL_ES_VERSION_2_0)
		GLfloat min_pt_sz;
		glGetFloatv(GL_POINT_SIZE_MIN, &min_pt_sz);
		GLfloat max_pt_sz;
		glGetFloatv(GL_POINT_SIZE_MAX, &max_pt_sz);
		std::cerr << "Point size range: " << min_pt_sz << " < size < " << max_pt_sz << std::endl;
		glEnable(GL_POINT_SPRITE);
		glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif
#endif

		init_shaders();

		screen_fbo_.reset(new fbo(0, 0, width_, height_, preferences::virtual_screen_width(), preferences::virtual_screen_height(), gles2::get_tex_shader()));
#endif
		prepare_raster();

		if(g_vsync >= -1 && g_vsync <= 1) {
			if(SDL_GL_SetSwapInterval(g_vsync) != 0) {
				if(g_vsync == -1) {
					if(SDL_GL_SetSwapInterval(1) != 0) {
						std::cerr << "WARNING: Unable to set swap interval of 'late sync' or 'sync' " << SDL_GetError() << std::endl;
					}
				} else {
					std::cerr << "WARNING: Unable to set swap interval of: " << g_vsync << " " << SDL_GetError() << std::endl;
				}
			}
		} else {
			std::cerr << "Resetting unknown 'vsync' value of " << g_vsync << " to 0" << std::endl;
			g_vsync = 0;
			SDL_GL_SetSwapInterval(0);
		}
	}

	void window_manager::init_shaders() 
	{
#if defined(USE_SHADERS)
		if(glCreateShader == NULL) {
			const GLubyte* glstrings;
			if(glGetString != NULL && (glstrings = glGetString(GL_VERSION)) != NULL) {
				std::cerr << "OpenGL version: " << reinterpret_cast<const char *>(glstrings) << std::endl;
			}
			std::cerr << ""
				<< "an OpenGL version >= 2. Exiting." << std::endl;
			ASSERT_LOG(false, "glCreateShader is NULL. Check that your current video card drivers support "
				"an OpenGL version >= 2. Exiting.");
		}

		// Has to happen after the call to glewInit().
		gles2::init_default_shader();
#endif
	}
	
	void window_manager::print_gl_info()
	{
		std::cerr << std::endl;
		const GLubyte* glstrings;
		if((glstrings = glGetString(GL_VENDOR)) != NULL) {
			std::cerr << "OpenGL vendor: " << reinterpret_cast<const char *>(glstrings) << std::endl;
		} else {
			GLenum err = glGetError();
			std::cerr << "Error in vendor string: " << std::hex << err << std::endl;
		}
		if((glstrings = glGetString(GL_VERSION)) != NULL) {
			std::cerr << "OpenGL version: " << reinterpret_cast<const char *>(glstrings) << std::endl;
		} else {
			GLenum err = glGetError();
			std::cerr << "Error in version string: " << std::hex << err << std::endl;
		}
		if((glstrings = glGetString(GL_EXTENSIONS)) != NULL) {
			std::cerr << "OpenGL extensions: " << reinterpret_cast<const char *>(glstrings) << std::endl;
		} else {
			GLenum err = glGetError();
			std::cerr << "Error in extensions string: " << std::hex << err << std::endl;
		}
#ifdef GL_SHADING_LANGUAGE_VERSION
		if((glstrings = glGetString(GL_SHADING_LANGUAGE_VERSION)) != NULL) {
			std::cerr << "GLSL Version: " << reinterpret_cast<const char *>(glstrings) << std::endl;
		} else {
			GLenum err = glGetError();
			std::cerr << "Error in GLSL string: " << std::hex << err << std::endl;
		}
#endif
		std::cerr << std::endl;
	}

	void window_manager::init_gl_context()
	{
#if defined(USE_SHADERS)
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glClearColor(0.0f,0.0f,0.0f,1.0f);
#else
		glShadeModel(GL_SMOOTH);
		glEnable(GL_TEXTURE_2D);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4ub(255,255,255,255);
#endif

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);
		glDepthRange(0.0f, 1.0f);
		glClearDepth(1.0);

		glViewport(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height());
	}

	void window_manager::destroy_window()
	{
		graphics::texture::unbuild_all();

		sdl_window_.reset();
	}

	void window_manager::prepare_raster()
	{
		// XXX todo: burn this project/modelview matrix stuff in fire
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		if(preferences::screen_rotated()) {
			camera_.reset(new camera_callable(camera_callable::ORTHOGONAL_CAMERA, 0, screen_height(), 0, screen_width()));
			glLoadMatrixf(camera_->projection());
		} else {
			camera_.reset(new camera_callable(camera_callable::ORTHOGONAL_CAMERA, 0, screen_width(), 0, screen_height()));
			glLoadMatrixf(camera_->projection());
		}
		
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		if(preferences::screen_rotated()) {
			// Rotate 90 degrees ccw, then move real_h pixels down
			// This has to be in opposite order since A(); B(); means A(B(x))
			glTranslatef(float(screen_height()), 0.0f, 0.0f);
			glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
		}
			
		glDisable(GL_DEPTH_TEST);
#if !defined(USE_SHADERS)
		glDisable(GL_LIGHTING);
		glDisable(GL_LIGHT0);
#endif
		glColor4f(1.0, 1.0, 1.0, 1.0);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	bool window_manager::set_window_size(int width, int height)
	{
		width_	= width;
		height_ = height;
		switch(preferences::fullscreen()) {
			case preferences::FULLSCREEN_NONE: {
				if(SDL_SetWindowFullscreen(sdl_window_.get(), 0) != 0) {
					std::cerr << "WARNING: Unable to set windowed mode at " << width << " x " << height << std::endl;
					return false;
				}
				SDL_SetWindowSize(sdl_window_.get(), width, height);
				SDL_SetWindowPosition(sdl_window_.get(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

				std::cerr << "set_window_size: SDL_GetWindowSize = " << width << "," << height << " (" << width << "," << height << ")" << std::endl;
				preferences::set_actual_screen_width(width);
				preferences::set_actual_screen_height(height);
				preferences::set_virtual_screen_width(width);
				preferences::set_virtual_screen_height(height);
				screen_fbo_.reset(new fbo(0, 0, width_, height_, preferences::virtual_screen_width(), preferences::virtual_screen_height(), gles2::get_tex_shader()));
				break;
			}
			case preferences::FULLSCREEN_WINDOWED: {
				if(SDL_SetWindowFullscreen(sdl_window_.get(), SDL_WINDOW_FULLSCREEN_DESKTOP) != 0) {
					std::cerr << "WARNING: Unable to set windowed fullscreen mode at " << width << " x " << height << std::endl;
					return false;
				}
				SDL_SetWindowSize(sdl_window_.get(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED);
				SDL_SetWindowPosition(sdl_window_.get(), 0, 0);

				int w, h;
				SDL_GetWindowSize(sdl_window_.get(), &w, &h);
				std::cerr << "set_window_size: SDL_GetWindowSize = " << w << "," << h << " (" << width << "," << height << ")" << std::endl;
				preferences::set_actual_screen_width(width);
				preferences::set_actual_screen_height(height);
				preferences::set_virtual_screen_width(width);
				preferences::set_virtual_screen_height(height);
				screen_fbo_.reset(new fbo(0, 0, w, h, preferences::virtual_screen_width(), preferences::virtual_screen_height(), gles2::get_tex_shader()));
				break;
			}
			case preferences::FULLSCREEN: {
				SDL_SetWindowSize(sdl_window_.get(), width, height);
				if(SDL_SetWindowFullscreen(sdl_window_.get(), SDL_WINDOW_FULLSCREEN) != 0) {
					std::cerr << "WARNING: Unable to set fullscreen mode at " << width << " x " << height << std::endl;
					return false;
				}
				screen_fbo_.reset(new fbo(0, 0, width_, height_, preferences::virtual_screen_width(), preferences::virtual_screen_height(), gles2::get_tex_shader()));
				break;
			}

			default: break;
		}
		return true;
	}

	bool window_manager::auto_window_size(int& width, int& height)
	{
		const SDL_DisplayMode mode = mode_auto_select();
		width = mode.w;
		height = mode.h;
		return true;
	}

	void window_manager::swap()
	{
		if(screen_fbo_) {
			screen_fbo_->draw_end();
			screen_fbo_->render_to_screen();
		}

		ASSERT_LOG(sdl_window_ != NULL, "FATAL: swap called on NULL window.");
		SDL_GL_SwapWindow(sdl_window_.get());

		if(screen_fbo_) {
			screen_fbo_->draw_begin();
		}
	}

	// Function to map a mouse position in native window co-ordinates.
	// this will modify the mouse position if we're using an fbo to
	// present a fake framebuffer.
	void window_manager::map_mouse_position(int* x, int* y)
	{
		if(screen_fbo_ == NULL) {
			return;
		}

		if(x) {
			*x -= screen_fbo_->letterbox_width()/2;
			*x *= preferences::actual_screen_width();
			*x /= screen_fbo_->width() - screen_fbo_->letterbox_width();
		}

		if(y) {
			*y -= screen_fbo_->letterbox_height()/2;
			*y *= preferences::actual_screen_height();
			*y /= screen_fbo_->height() - screen_fbo_->letterbox_height();
		}
	}

	void window_manager::set_window_title(const std::string& title) 
	{
		ASSERT_LOG(sdl_window_ != NULL, "Window is null");
		SDL_SetWindowTitle(sdl_window_.get(), title.c_str());		
	}
}

/*
		int width = 0, height = 0;
		SDL_GetWindowSize(global_main_window, &width, &height);

		if((framebuffer_fbo->width()*1000)/framebuffer_fbo->height() < (width*1000)/height) {
			//actual screen wider than the framebuffer, letterboxing
			//on the sides.
			const int normalized_width = (framebuffer_fbo->width()*height)/framebuffer_fbo->height();

			g_letterbox_horz = width - normalized_width;
		} else if((framebuffer_fbo->width()*1000)/framebuffer_fbo->height() > (width*1000)/height) {
			//actual screen narrow than the framebuffer, letterboxing
			//on the top/bottom.
			const int normalized_height = (framebuffer_fbo->height()*width)/framebuffer_fbo->width();
			g_letterbox_vert = height - normalized_height;
		}
*/
