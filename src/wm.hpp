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
#pragma once

#include <boost/intrusive_ptr.hpp>
#include <memory>

class camera_callable;
typedef boost::intrusive_ptr<camera_callable> camera_callable_ptr;

namespace graphics
{
	class fbo;
	class lighting;
	typedef boost::intrusive_ptr<lighting> lighting_ptr;

	class init_error : public std::exception
	{
	public:
		init_error() : exception(), msg_(SDL_GetError())
		{}
		init_error(const std::string& msg) : exception(), msg_(msg)
		{}
		virtual ~init_error() throw()
		{}
		virtual const char* what() const throw() { return msg_.c_str(); }
	private:
		std::string msg_;
	};

	class SDL
	{
	public:
		SDL(Uint32 flags = SDL_INIT_VIDEO)
		{
			if (SDL_Init(flags) < 0) {
				std::stringstream ss;
				ss << "Unable to initialize SDL: " << SDL_GetError() << std::endl;
				throw init_error(ss.str());
			}
		}

		virtual ~SDL()
		{
			SDL_Quit();
		}
	};

	class window_manager
	{
	public:
		window_manager();
		virtual ~window_manager();

		void notify_new_window_size();
		
		void create_window(int width, int height);
		void destroy_window();
		
		bool set_window_size(int width, int height);
		bool auto_window_size(int& width, int& height);

		void set_window_title(const std::string& title);

		SDL_Window* sdl_window() { return sdl_window_.get(); }
		SDL_Renderer* renderer() { return sdl_renderer_; }
		
		void init_gl_context();
		void init_shaders();
		void print_gl_info();

		int get_configured_msaa() const { return msaa_set_; }

		void map_mouse_position(int* x, int* y);

		void prepare_raster();

		camera_callable_ptr camera() { return camera_; }
		lighting_ptr lighting() { return lighting_; }

		camera_callable_ptr camera() const { return camera_; }
		lighting_ptr lighting() const { return lighting_; }

		void swap();
	private:
		std::shared_ptr<SDL_Window> sdl_window_;
		SDL_GLContext gl_context_;
		SDL_Renderer* sdl_renderer_;

		camera_callable_ptr camera_;
		lighting_ptr lighting_;

		std::shared_ptr<fbo> screen_fbo_;

		int width_;
		int height_;

		int msaa_set_;

		int letterbox_horz_;
		int letterbox_vert_;

		std::shared_ptr<SDL> sdl_;
		
		window_manager(const window_manager&);
	};

	typedef std::shared_ptr<graphics::window_manager> window_manager_ptr;
}
