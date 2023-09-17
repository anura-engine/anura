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

#include <cctype>
#include <sstream>

#include "asserts.hpp"
#include "DisplayDevice.hpp"
#include "SurfaceSDL.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "WindowManager.hpp"

#ifdef USE_IMGUI
#include "imgui.h"
#include "imgui_impl_sdl_gl3.h"
#endif

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

		typedef std::map<unsigned,std::weak_ptr<Window>> window_list_type;
		window_list_type& get_window_list()
		{
			static window_list_type res;
			return res;
		}

		std::weak_ptr<Window>& get_main_window()
		{
			static std::weak_ptr<Window> res;
			return res;
		}

	}

	class SDLWindow : public Window
	{
	public:
		explicit SDLWindow(int width, int height, const variant& hints)
			: Window(width, height, hints),
			  renderer_hint_(),
			  context_(nullptr),
			  nonfs_width_(width),
			  nonfs_height_(height),
			  request_major_version_(2),
			  request_minor_version_(1),
			  profile_(ProfileValue::COMPAT),
			  new_frame_(0)
		{
			if(hints.has_key("renderer")) {
				if(hints["renderer"].is_string()) {
					renderer_hint_.emplace_back(hints["renderer"].as_string());
				} else {
					renderer_hint_ = hints["renderer"].as_list_string();
				}
			} else {
				renderer_hint_.emplace_back("opengl");
			}

			// XXX figure out a better way to pass this hint.
			SDL_SetHint(SDL_HINT_RENDER_DRIVER, renderer_hint_.front().c_str());

			auto dpi_aware = hints["dpi_aware"].as_bool(false);
			if(dpi_aware) {
				SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
			} else {
				SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "1");
			}

			if(hints.has_key("version")) {
				const int version = hints["version"].as_int32();
				request_major_version_ = version / 100;
				request_minor_version_ = version % 100;
			}
			if(hints.has_key("profile")) {
				const std::string profile = hints["profile"].as_string();
				if(profile == "compatibility" || profile == "compat") {
					profile_ = ProfileValue::COMPAT;
				} else if(profile == "core") {
					profile_ = ProfileValue::CORE;
				} else if(profile == "es" || profile == "ES") {
					profile_ = ProfileValue::ES;
				} else {
					LOG_ERROR("Unrecognized profile setting '" << profile << "' defaulting to compatibility.");
				}
			}
		}

		void createWindow() override {
			Uint32 wnd_flags = 0;

			for(auto rh : renderer_hint_) {
				setDisplayDevice(DisplayDevice::factory(rh, shared_from_this()));
				current_display_device() = getDisplayDevice();
				if(getDisplayDevice() != nullptr) {
					break;
				}
			}
			ASSERT_LOG(getDisplayDevice() != nullptr, "No display driver was created.");

			if(getDisplayDevice()->ID() == DisplayDevice::DISPLAY_DEVICE_OPENGL) {
				// We need to do extra SDL set-up for an OpenGL context.
				// Since these parameter's need to be set-up before context
				// creation.
				int err;
				err = SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, request_major_version_);
				if(err) {
					LOG_ERROR("Setting major OpenGL context version to " << request_major_version_ << ": " << SDL_GetError());
				}
				err = SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, request_minor_version_);
				if(err) {
					LOG_ERROR("Setting minor OpenGL context version to " << request_minor_version_ << ": " << SDL_GetError());
				}
				SDL_GLprofile prof;
				switch(profile_) {
					case ProfileValue::COMPAT:	prof = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY; break;
					case ProfileValue::CORE:	prof = SDL_GL_CONTEXT_PROFILE_CORE; break;
					case ProfileValue::ES:		prof = SDL_GL_CONTEXT_PROFILE_ES; break;
					default: prof = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
				}
				err = SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, prof);
				if(err) {
					LOG_ERROR("Setting major OpenGL profile mask to " << prof << ": " << SDL_GetError());
				}
				err = SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
				if(err) {
					LOG_ERROR("Setting major OpenGL depth to 24: " << SDL_GetError());
				}
				err = SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
				if(err) {
					LOG_ERROR("Setting major OpenGL stencil size to 8: " << SDL_GetError());
				}
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
				wnd_flags |= SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
			} else if(getDisplayDevice()->ID() == DisplayDevice::DISPLAY_DEVICE_OPENGLES) {
				// We need to do extra SDL set-up for an OpenGL context.
				// Since these parameter's need to be set-up before context
				// creation.
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

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

			if(borderless()) {
				wnd_flags |= SDL_WINDOW_BORDERLESS;
			}

			int x = SDL_WINDOWPOS_CENTERED;
			int y = SDL_WINDOWPOS_CENTERED;
			int w = width();
			int h = height();
			switch(fullscreenMode()) {
			case FullScreenMode::WINDOWED:		break;
			case FullScreenMode::FULLSCREEN_WINDOWED:
				x = y = SDL_WINDOWPOS_UNDEFINED;
				//w = h = 0;
				wnd_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
				break;
			case FullScreenMode::FULLSCREEN_EXCLUSIVE:
				x = y = SDL_WINDOWPOS_UNDEFINED;
				wnd_flags |= SDL_WINDOW_FULLSCREEN;
				break;
			}
			window_.reset(SDL_CreateWindow(getTitle().c_str(), x, y, w, h, wnd_flags), [&](SDL_Window* wnd){
#ifdef USE_IMGUI
				ImGui_ImplSdlGL3_Shutdown();
#endif
				getDisplayDevice().reset();
				if(context_) {
					SDL_GL_DeleteContext(context_);
					context_ = nullptr;
				}
				SDL_DestroyWindow(wnd);
			});

#ifdef USE_IMGUI
			ImGui_ImplSdlGL3_Init(window_.get());
#endif

			ASSERT_LOG(window_.get() != nullptr, "Could not create window: " << x << ", " << y << ", " << w << ", " << h << " / wnd_flags = " << wnd_flags);

			if(getDisplayDevice()->ID() != DisplayDevice::DISPLAY_DEVICE_SDL) {
				Uint32 rnd_flags = SDL_RENDERER_ACCELERATED;
				if(vSync()) {
					rnd_flags |= SDL_RENDERER_PRESENTVSYNC;
				}
			}

			ASSERT_LOG(window_ != nullptr, "Failed to create window: " << SDL_GetError());
			if(getDisplayDevice()->ID() == DisplayDevice::DISPLAY_DEVICE_OPENGL ||getDisplayDevice()->ID() == DisplayDevice::DISPLAY_DEVICE_OPENGLES) {
				context_ = SDL_GL_CreateContext(window_.get());
				ASSERT_LOG(context_ != nullptr, "Failed to GL Context: " << SDL_GetError());
			}

			getDisplayDevice()->init(width(), height());
			getDisplayDevice()->printDeviceInfo();

			clear(ClearFlags::ALL);
			//getDisplayDevice()->setClearColor(clear_color_);
			//getDisplayDevice()->clear(ClearFlags::ALL);
			swap();
		}

		void destroyWindow() override {
			window_.reset();
		}

		void setVisible(bool visible) override {
			if(visible) {
				SDL_ShowWindow(window_.get());
			} else {
				SDL_HideWindow(window_.get());
			}
		}

		void clear(ClearFlags f) override {
			// N.B. Clear color is global GL state, so we need to re-configure it everytime we clear.
			// Since it may have changed by some sneaky render target user.
			getDisplayDevice()->setClearColor(clear_color_);
			getDisplayDevice()->clear(f);
#ifdef USE_IMGUI
			if(new_frame_ == 0) {
				ImGui_ImplSdlGL3_NewFrame(window_.get());
				++new_frame_;
			}
#endif
		}

		void swap() override {
			// This is a little bit hacky -- ideally the display device should swap buffers.
			// But SDL provides a device independent way of doing it which is really nice.
			// So we use that.
#ifdef USE_IMGUI
			ImGui::Render();
			if(--new_frame_ < 0) {
				new_frame_ = 0;
			}
#endif
			if(getDisplayDevice()->ID() == DisplayDevice::DISPLAY_DEVICE_OPENGL || getDisplayDevice()->ID() == DisplayDevice::DISPLAY_DEVICE_OPENGLES) {
				SDL_GL_SwapWindow(window_.get());
			} else {
				// default to delegating to the display device.
				getDisplayDevice()->swap();
			}
		}

		unsigned getWindowID() const override {
			return SDL_GetWindowID(window_.get());
		}

		void setWindowIcon(const std::string& name) override {
			SurfaceSDL icon(name);
			SDL_SetWindowIcon(window_.get(), icon.get());
		}

		bool autoWindowSize(int& width, int& height) override {
			std::vector<WindowMode> res;
			int num_displays = SDL_GetNumVideoDisplays();
			if(num_displays < 0) {
				LOG_ERROR("Error enumerating number of displays: " << std::string(SDL_GetError()));
				return false;
			}
			for(int n = 0; n != num_displays; ++n) {
				SDL_Rect display_bounds;
				SDL_GetDisplayBounds(n, &display_bounds);
				int num_modes = SDL_GetNumDisplayModes(n);
				if(num_modes < 0) {
					LOG_ERROR("Error enumerating number of display modes for display " << n << ": " << std::string(SDL_GetError()));
					return false;
				}
				for(int m = 0; m != num_modes; ++m) {
					SDL_DisplayMode mode;
					int err = SDL_GetDisplayMode(n, m, &mode);
					if(err < 0) {
						LOG_ERROR("Couldn't get display mode information for display: " << n << " mode: " << m << " : " << std::string(SDL_GetError()));
					} else {
						WindowMode new_mode = { mode.w, mode.h, std::make_shared<SDLPixelFormat>(mode.format), mode.refresh_rate };
						res.emplace_back(new_mode);
						LOG_DEBUG("added mode w: " << mode.w << ", h: " << mode.h << ", refresh: " << mode.refresh_rate);
					}
				}
			}

			if(!res.empty()) {
				width = static_cast<int>(res.front().width * 0.8f);
				height = static_cast<int>(res.front().height * 0.8f);
				return true;
			}
			return false;
		}

		void handleSetWindowTitle() override {
			if(window_ != nullptr) {
				SDL_SetWindowTitle(window_.get(), getTitle().c_str());
			}
		}

		WindowMode getDisplaySize() const override {
			SDL_DisplayMode new_mode;
			int display_index = 0;
			if(window_ != nullptr) {
				display_index = SDL_GetWindowDisplayIndex(window_.get());
			}
			SDL_GetDesktopDisplayMode(display_index, &new_mode);
			WindowMode mode = { new_mode.w, new_mode.h, std::make_shared<SDLPixelFormat>(new_mode.format), new_mode.refresh_rate };
			return mode;
		}

		std::vector<WindowMode> getWindowModes(std::function<bool(const WindowMode&)> mode_filter) const override {
			std::vector<WindowMode> res;
			int display_index = 0;
			if(window_ != nullptr) {
				display_index = SDL_GetWindowDisplayIndex(window_.get());
			}
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
					res.emplace_back(mode);
				}
			}
			return res;
		}

	private:
		void handleSetClearColor() const override {
			if(getDisplayDevice() != nullptr) {
				getDisplayDevice()->setClearColor(clear_color_);
			}
		}
		void changeFullscreenMode() override {
			if(fullscreenMode() == FullScreenMode::FULLSCREEN_EXCLUSIVE) {
				nonfs_width_ = width();
				nonfs_height_ = height();
				if(SDL_SetWindowFullscreen(window_.get(), SDL_WINDOW_FULLSCREEN) != 0) {
					LOG_WARN("Unable to set fullscreen mode at " << width() << " x " << height());
					return;
				}
			} else if(fullscreenMode() == FullScreenMode::FULLSCREEN_WINDOWED) {
				nonfs_width_ = width();
				nonfs_height_ = height();

				if(SDL_SetWindowFullscreen(window_.get(), SDL_WINDOW_FULLSCREEN_DESKTOP) != 0) {
					LOG_WARN("Unable to set windowed fullscreen mode at " << width() << " x " << height());
					return;
				}
				SDL_SetWindowSize(window_.get(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED);
				SDL_SetWindowPosition(window_.get(), 0, 0);

			} else {
				if(SDL_SetWindowFullscreen(window_.get(), 0) != 0) {
					LOG_WARN("Unable to set windowed mode at " << width() << " x " << height());
					return;
				}
				SDL_SetWindowSize(window_.get(), nonfs_width_, nonfs_height_);
				SDL_SetWindowPosition(window_.get(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
			}
			int w, h;
			SDL_GetWindowSize(window_.get(), &w, &h);
			// update viewport
			setViewPort(0, 0, w, h);
			// update width_ and height_ and notify observers
			updateDimensions(w, h);
		}
		bool handleLogicalWindowSizeChange() override {
			// XXX do nothing for now
			return true;
		}

		bool handlePhysicalWindowSizeChange() override {
			if(window_) {
				SDL_SetWindowSize(window_.get(), width(), height());
				setViewPort(0, 0, width(), height());
				return true;
			}
			return false;
		}

		void handleSetViewPort() override {
			getDisplayDevice()->setViewPort(getViewPort());
		}

		SDL_WindowPtr window_;
		SDL_GLContext context_;
		std::vector<std::string> renderer_hint_;

		// Width of the window before changing to full-screen mode
		int nonfs_width_;
		// Height of the window before changing to full-screen mode
		int nonfs_height_;

		int request_major_version_;
		int request_minor_version_;

		enum class ProfileValue {
			COMPAT,
			CORE,
			ES,
		};
		ProfileValue profile_;

		int new_frame_;

		SDLWindow(const SDLWindow&);
	};

	Window::Window(int width, int height, const variant& hints)
		: width_(hints["width"].as_int32(width)),
		  height_(hints["height"].as_int32(height)),
		  logical_width_(hints["logical_width"].as_int32(width_)),
		  logical_height_(hints["logical_height"].as_int32(height_)),
		  use_16bpp_(hints["use_16bpp"].as_bool(false)),
		  use_multi_sampling_(hints["use_multisampling"].as_bool(false)),
		  samples_(hints["samples"].as_int32(4)),
		  is_resizeable_(hints["resizeable"].as_bool(false)),
		  is_borderless_(hints["borderless"].as_bool(false)),
		  fullscreen_mode_(hints["fullscreen"].as_bool(false) ? FullScreenMode::FULLSCREEN_WINDOWED : FullScreenMode::WINDOWED),
		  title_(hints["title"].as_string_default("")),
		  use_vsync_(hints["use_vsync"].as_bool(false)),
		  clear_color_(0.0f,0.0f,0.0f,1.0f),
		  view_port_(0, 0, width_, height_),
		  display_(nullptr)
	{
		if(hints.has_key("clear_color")) {
			clear_color_ = Color(hints["clear_color"]);
		}
	}

	Window::~Window()
	{
	}

	void Window::render(const Renderable* r) const
	{
		ASSERT_LOG(display_ != nullptr, "display was null");
		display_->render(r);
	}

	void Window::enable16bpp(bool bpp) {
		use_16bpp_ = bpp;
	}

	void Window::enableMultisampling(bool multi_sampling, int samples) {
		use_multi_sampling_ = multi_sampling;
		samples_ = samples;
	}

	void Window::enableResizeableWindow(bool en) {
		is_resizeable_ = en;
	}
	
	//Use `graphics::GameScreen::get().setFullscreen(fullscreenCheckbox->checked()`,
	//instead of this function, like `KRE::WindowManager::getMainWindow()->setFullscreenMode(fullscreenCheckbox->checked()`.
	//This function is a lower-level function called from setFullscreen - this one
	//*only* changes the window size, and doesn't recalculate the internal scaling.
	void Window::setFullscreenMode(FullScreenMode mode)
	{
		bool modes_differ = fullscreen_mode_ != mode;
		fullscreen_mode_ = mode;
		if(modes_differ) {
			changeFullscreenMode();
		}
	}

	void Window::enableVsync(bool en)
	{
		use_vsync_ = en;
	}

	void Window::mapMousePosition(int* x, int* y)
	{
		if(x) {
			*x = int(*x * double(logical_width_) / width_);
		}
		if(y) {
			*y = int(*y * double(logical_height_) / height_);
		}
	}

	bool Window::setWindowSize(int width, int height, int flags)
	{
		width_ = width;
		height_ = height;
		bool result = handlePhysicalWindowSizeChange();
		if(result) {
			for(auto& observer : dimensions_changed_observers_) {
				observer.second(width_, height_, flags);
			}
		}
		return result;
	}

	void Window::updateDimensions(int w, int h, int flags)
	{
		width_ = w;
		height_ = h;
		for(auto& observer : dimensions_changed_observers_) {
			observer.second(width_, height_, flags);
		}
	}

	bool Window::setLogicalWindowSize(int width, int height)
	{
		logical_width_ = width;
		logical_height_ = height;
		return handleLogicalWindowSizeChange();
	}

	void Window::setClearColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) const
	{
		clear_color_ = Color(r,g,b,a);
		handleSetClearColor();
	}

	void Window::setClearColor(float r, float g, float b, float a) const
	{
		clear_color_ = Color(r,g,b,a);
		handleSetClearColor();
	}

	void Window::setClearColor(const Color& color) const
	{
		clear_color_ = color;
		handleSetClearColor();
	}

	void Window::notifyNewWindowSize(int new_width, int new_height, int flags)
	{
		width_ = new_width;
		height_ = new_height;
		handlePhysicalWindowSizeChange();

		for(auto& observer : dimensions_changed_observers_) {
			observer.second(new_width, new_height, flags);
		}
	}

	void Window::setWindowTitle(const std::string& title)
	{
		title_ = title;
		handleSetWindowTitle();
	}

	//! Save the current window display to a file
	std::string Window::saveFrameBuffer(const std::string& filename)
	{
		auto surface = Surface::create(width_, height_, PixelFormat::PF::PIXELFORMAT_RGB24);
		int stride = surface->rowPitch();
		std::vector<uint8_t> pixels;
		pixels.resize(stride * height_);
		if(display_->readPixels(0, 0, width_, height_, ReadFormat::RGB, AttrFormat::UNSIGNED_BYTE, pixels, stride)) {
			surface->writePixels(&pixels[0], height_ * stride);
			return surface->savePng(filename);
		} else {
			LOG_ERROR("Failed to save screenshot");
		}
		return std::string();
	}

	int Window::registerSizeChangeObserver(std::function<void(int,int,int)> fn)
	{
		static int counter = 0;
		dimensions_changed_observers_[counter] = fn;
		return counter++;
	}

	bool Window::registerSizeChangeObserver(int key, std::function<void(int,int,int)> fn)
	{
		auto it = dimensions_changed_observers_.find(key);
		if(it == dimensions_changed_observers_.end()) {
			dimensions_changed_observers_[key] = fn;
			return false;
		}
		it->second = fn;
		return true;
	}

	void Window::unregisterSizeChangeObserver(int index)
	{
		auto it = dimensions_changed_observers_.find(index);
		ASSERT_LOG(it != dimensions_changed_observers_.end(), "Unable to remove observer with id: " << index);
		dimensions_changed_observers_.erase(it);
	}

	void Window::setViewPort(int x, int y, int w, int h)
	{
		view_port_ = rect(x, y, w, h);
		handleSetViewPort();
	}

	void Window::setViewPort(const rect& vp)
	{
		view_port_ = vp;
		handleSetViewPort();
	}

	WindowPtr WindowManager::createWindow(int width, int height, const variant& hints)
	{
		// We really only support one sub-class of the window
		// at the moment, so we just return it. We could use window_hint_ in the
		// future if we had more.
		WindowPtr wp = std::make_shared<SDLWindow>(width, height, hints);
		createWindow(wp);
		return wp;
	}

	WindowPtr WindowManager::allocateWindow(const variant& hints)
	{
		WindowPtr wp = std::make_shared<SDLWindow>(0, 0, hints);
		return wp;
	}

	void WindowManager::createWindow(WindowPtr wnd)
	{
		wnd->createWindow();

		// Add a weak_ptr to our window to the list of id versus windows.
		get_window_list()[wnd->getWindowID()] = wnd;
		// We consider the first window created the main one.
		if(get_main_window().lock() == nullptr) {
			get_main_window() = wnd;
		}
	}

	WindowManager::WindowManager(const std::string& window_hint)
		: window_hint_(window_hint)
	{
	}

	std::vector<WindowPtr> WindowManager::getWindowList()
	{
		std::vector<WindowPtr> res;
		auto it = get_window_list().begin();
		for(auto& w : get_window_list()) {
			res.emplace_back(w.second.lock());
		}
		return res;
	}

	WindowPtr WindowManager::getMainWindow()
	{
		return get_main_window().lock();
	}

	WindowPtr WindowManager::getWindowFromID(unsigned id)
	{
		auto it = get_window_list().find(id);
		if(it == get_window_list().end()) {
			return nullptr;
		}
		return it->second.lock();
	}
}
