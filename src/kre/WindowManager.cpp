/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#pragma comment(lib, "SDL2")
#pragma comment(lib, "SDL2main")
#pragma comment(lib, "SDL2_image")

#include <cctype>
#include <sstream>

#include "asserts.hpp"
#include "DisplayDevice.hpp"
#include "SurfaceSDL.hpp"
#include "SDL.h"
#include "SDL_image.h"
#include "WindowManager.hpp"

namespace KRE
{
	namespace 
	{
		typedef std::shared_ptr<SDL_Window> SDL_WindowPtr;

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

		DisplayDevicePtr& current_display_device()
		{
			static DisplayDevicePtr res;
			return res;
		}
	}

	class SDLWindowManager : public WindowManager
	{
	public:
		SDLWindowManager(const std::string& title, const std::string& renderer_hint) 
			: WindowManager(title), 
			renderer_hint_(renderer_hint),
			renderer_(nullptr),
			context_(nullptr) {
			if(renderer_hint_.empty()) {
				renderer_hint_ = "opengl";
			}
			current_display_device() = display_ = DisplayDevice::factory(renderer_hint_);
			// XXX figure out a better way to pass this hint.
			SDL_SetHint(SDL_HINT_RENDER_DRIVER, renderer_hint_.c_str());
		}
		~SDLWindowManager() {
			//destroyWindow();
		}

		void doCreateWindow(int width, int height) override {
			logical_width_ = width_ = width;
			logical_height_ = height_ = height;

			Uint32 wnd_flags = 0;

			if(display_->ID() == DisplayDevice::DISPLAY_DEVICE_OPENGL) {
				// We need to do extra SDL set-up for an OpenGL context.
				// Since these parameter's need to be set-up before context
				// creation.
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
				SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
				SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
				if(use16bpp()) {
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
				if(useMultiSampling()) {
					if(SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1) != 0) {
						LOG_WARN("MSAA(" << multiSamples() << ") requested but mutlisample buffer couldn't be allocated.");
					} else {
						int msaa = next_pow2(multiSamples());
						if(SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa) != 0) {
							LOG_INFO("Requested MSAA of " << msaa << " but couldn't allocate");
						}
					}
				}
				wnd_flags |= SDL_WINDOW_OPENGL;
			}

			if(resizeable()) {
				wnd_flags |= SDL_WINDOW_RESIZABLE;
			}

			int x = SDL_WINDOWPOS_CENTERED;
			int y = SDL_WINDOWPOS_CENTERED;
			int w = width_;
			int h = height_;
			switch(fullscreenMode()) {
			case FullScreenMode::WINDOWED:		break;
			case FullScreenMode::FULLSCREEN_WINDOWED:
				x = y = SDL_WINDOWPOS_UNDEFINED;
				w = h = 0;
				wnd_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
				break;
			//case FullscreenMode::FULLSCREEN:
			//	x = y = SDL_WINDOWPOS_UNDEFINED;
			//	wnd_flags |= SDL_WINDOW_FULLSCREEN;
			//	break;
			}
			window_.reset(SDL_CreateWindow(getTitle().c_str(), x, y, w, h, wnd_flags), [&](SDL_Window* wnd){
				if(display_->ID() != DisplayDevice::DISPLAY_DEVICE_SDL) {
					SDL_DestroyRenderer(renderer_);
				}
				display_.reset();
				if(context_) {
					SDL_GL_DeleteContext(context_);
					context_ = nullptr;
				}
				SDL_DestroyWindow(wnd);
			});

			if(display_->ID() != DisplayDevice::DISPLAY_DEVICE_SDL) {
				Uint32 rnd_flags = SDL_RENDERER_ACCELERATED;
				if(vSync()) {
					rnd_flags |= SDL_RENDERER_PRESENTVSYNC;
				}
				renderer_ = SDL_CreateRenderer(window_.get(), -1, rnd_flags);
				ASSERT_LOG(renderer_ != nullptr, "Failed to create renderer: " << SDL_GetError());				
			}

			ASSERT_LOG(window_ != nullptr, "Failed to create window: " << SDL_GetError());
			if(display_->ID() == DisplayDevice::DISPLAY_DEVICE_OPENGL) {
				context_ = SDL_GL_CreateContext(window_.get());	
				ASSERT_LOG(context_ != nullptr, "Failed to GL Context: " << SDL_GetError());
			}

			display_->setClearColor(clear_color_);
			display_->printDeviceInfo();
			display_->init(width_, height_);
			display_->clear(ClearFlags::ALL);
			swap();
		}

		void doDestroyWindow() override {
			window_.reset();
		}

		void clear(ClearFlags f) override {
			display_->clear(ClearFlags::ALL);
		}

		void swap() override {
			// This is a little bit hacky -- ideally the display device should swap buffers.
			// But SDL provides a device independent way of doing it which is really nice.
			// So we use that.
			if(display_->ID() == DisplayDevice::DISPLAY_DEVICE_OPENGL) {
				SDL_GL_SwapWindow(window_.get());
			} else {
				// default to delegating to the display device.
				display_->swap();
			}
		}

		void setViewPort(int x, int y, int width, int height) override {
			display_->setViewPort(x, y, width, height);
		}

		unsigned getWindowID() const override {
			return SDL_GetWindowID(window_.get());
		}

		void setWindowIcon(const std::string& name) override {
			SurfaceSDL icon(name);
			SDL_SetWindowIcon(window_.get(), icon.get());
		}
		
		bool setWindowSize(int width, int height) override {
			SDL_SetWindowSize(window_.get(), width, height);
			width_ = width;
			height_ = height;
			return false;
		}

		bool autoWindowSize(int& width, int& height) override {
			// XXX
			return false;
		}

		void setWindowTitle(const std::string& title) override {
			ASSERT_LOG(window_ != nullptr, "Window is null");
			SDL_SetWindowTitle(window_.get(), title.c_str());		
		}

		virtual void render(const Renderable* r) const override {
			ASSERT_LOG(display_ != nullptr, "No display to render to.");
			display_->render(r);
		}

		std::vector<WindowMode> getWindowModes(std::function<bool(const WindowMode&)> mode_filter) override {
			std::vector<WindowMode> res;
			const int display_index = SDL_GetWindowDisplayIndex(window_.get());
			const int nmodes = SDL_GetNumDisplayModes(display_index);
			for(int n = 0; n != nmodes; ++n) {
				SDL_DisplayMode new_mode;
				const int nvalue = SDL_GetDisplayMode(display_index, n, &new_mode);
				if(nvalue != 0) {
					LOG_ERROR("QUERYING DISPLAY INFO: " << SDL_GetError());
					continue;
				}
				WindowMode mode = { new_mode.w, new_mode.h, std::make_shared<SDLPixelFormat>(new_mode.format), new_mode.refresh_rate };
				// filter modes based on pixel format here
				if(mode_filter(mode)) {
					res.push_back(mode);
				}
			}
			return res;
		}
	protected:
	private:
		void handleSetClearColor() const override {
			if(display_ != nullptr) {
				display_->setClearColor(clear_color_);
			}
		}
		void changeFullscreenMode() override {
			// XXX
		}
		bool handleLogicalWindowSizeChange() override {
			// XXX do nothing for now
			return true;
		}

		bool handlePhysicalWindowSizeChange() override {
			// XXX do nothing for now
			return true;
		}

		SDL_WindowPtr window_;
		SDL_GLContext context_;
		SDL_Renderer* renderer_;
		std::string renderer_hint_;
		SDLWindowManager(const SDLWindowManager&);
	};

	WindowManager::WindowManager(const std::string& title)
		: width_(0), 
		height_(0),
		logical_width_(0),
		logical_height_(0),
		use_16bpp_(false),
		use_multi_sampling_(false),
		samples_(4),
		is_resizeable_(false),
		title_(title),
		clear_color_(0.0f,0.0f,0.0f,1.0f)
	{
	}

	WindowManager::~WindowManager()
	{
	}

	void WindowManager::enable16bpp(bool bpp) {
		use_16bpp_ = bpp;
	}

	void WindowManager::enableMultisampling(bool multi_sampling, int samples) {
		use_multi_sampling_ = multi_sampling;
		samples_ = samples;
	}

	void WindowManager::enableResizeableWindow(bool en) {
		is_resizeable_ = en;
	}

	void WindowManager::setFullscreenMode(FullScreenMode mode)
	{
		bool modes_differ = fullscreen_mode_ != mode;
		fullscreen_mode_ = mode;
		if(modes_differ) {
			changeFullscreenMode();
		}
	}

	void WindowManager::enableVsync(bool en)
	{
		use_vsync_ = en;
	}

	void WindowManager::mapMousePosition(int* x, int* y) 
	{
		if(x) {
			*x = int(*x * double(logical_width_) / width_);
		}
		if(y) {
			*y = int(*y * double(logical_height_) / height_);
		}
	}

	bool WindowManager::setLogicalWindowSize(int width, int height)
	{
		logical_width_ = width;
		logical_height_ = height;
		return handleLogicalWindowSizeChange();
	}

	void WindowManager::setClearColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) const
	{
		clear_color_ = Color(r,g,b,a);
		handleSetClearColor();
	}

	void WindowManager::setClearColor(float r, float g, float b, float a) const
	{
		clear_color_ = Color(r,g,b,a);
		handleSetClearColor();
	}

	void WindowManager::setClearColor(const Color& color) const
	{
		clear_color_ = color;
		handleSetClearColor();
	}

	namespace
	{
		std::map<unsigned,WindowManagerPtr>& get_window_list()
		{
			static std::map<unsigned,WindowManagerPtr> res;
			return res;
		}

		WindowManagerPtr main_window = nullptr;
	}

	void WindowManager::createWindow(int width, int height)
	{
		doCreateWindow(width, height);
	}

	void WindowManager::destroyWindow()
	{
		if(!get_window_list().empty()) {
			for(auto it = get_window_list().begin(); it != get_window_list().end(); ) {
				if(it->second.get() == this) {
					get_window_list().erase(it++);
				} else {
					++it;
				}
			}
		}
		doDestroyWindow();
	}

	void WindowManager::notifyNewWindowSize(int new_width, int new_height)
	{
		width_ = new_width;
		height_ = new_height;
		handlePhysicalWindowSizeChange();
	}

	//! Save the current window display to a file
	void WindowManager::saveFrameBuffer(const std::string& filename)
	{
		auto surface = Surface::create(width_, height_, PixelFormat::PF::PIXELFORMAT_RGB24);
		std::vector<glm::u8vec3> pixels;
		if(display_->readPixels(0, 0, width_, height_, ReadFormat::RGB, AttrFormat::UNSIGNED_BYTE, pixels)) {
			surface->writePixels(&pixels[0]);
			surface->savePng(filename);
			LOG_INFO("Saved screenshot to: " << filename);
		} else {
			LOG_ERROR("Failed to save screenshot");
		}
	}

	WindowManagerPtr WindowManager::createInstance(const std::string& title, const std::string& wnd_hint, const std::string& rend_hint)
	{
		// We really only support one sub-class of the window manager
		// at the moment, so we just return it. We could use hint in the
		// future if we had more.
		WindowManagerPtr wm = std::make_shared<SDLWindowManager>(title, rend_hint);
		get_window_list()[wm->getWindowID()] = wm;
		// We consider the first window created the main one.
		if(main_window == nullptr) {
			main_window = wm;
		}
		LOG_DEBUG("Added window with id: " << wm->getWindowID());
		return wm;
	}

	std::vector<WindowManagerPtr> WindowManager::getWindowList()
	{
		std::vector<WindowManagerPtr> res;
		auto it = get_window_list().begin();
		for(auto w : get_window_list()) {
			res.push_back(w.second);
		}
		return res;
	}

	WindowManagerPtr WindowManager::getMainWindow()
	{
		return main_window;
	}

	WindowManagerPtr WindowManager::getWindowFromID(unsigned id)
	{
		auto it = get_window_list().find(id);
		//ASSERT_LOG(it != get_window_list().end(), "Couldn't get window from id: " << id);
		if(it == get_window_list().end()) {
			return nullptr;
		}
		return it->second;
	}
}
