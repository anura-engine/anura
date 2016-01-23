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

#pragma once

#include <string>

#include "Color.hpp"
#include "DisplayDevice.hpp"
#include "Texture.hpp"
#include "PixelFormat.hpp"
#include "Renderable.hpp"
#include "WindowManagerFwd.hpp"

namespace KRE
{
	enum class FullScreenMode {
		WINDOWED,
		FULLSCREEN_WINDOWED,
		FULLSCREEN_EXCLUSIVE,
	};

	struct WindowMode
	{
		int width;
		int height;
		PixelFormatPtr pf;
		int refresh;
	};

	inline bool operator==(const WindowMode& lhs, const WindowMode& rhs) {
		return lhs.width == rhs.width && lhs.height == rhs.height;
	}

	enum WindowSizeChangeFlags {
		NONE = 0,
		NOTIFY_CANVAS_ONLY = 1,
	};

	class Window : public std::enable_shared_from_this<Window>
	{
	public:
		Window(int width, int height, const variant& hints);
		virtual ~Window();

		virtual void createWindow() = 0;
		virtual void destroyWindow() = 0;

		bool setWindowSize(int width, int height, int flags=WindowSizeChangeFlags::NONE);
		virtual bool autoWindowSize(int& width, int& height) = 0;
		
		bool setLogicalWindowSize(int width, int height);

		void setWindowTitle(const std::string& title);
		
		virtual void setWindowIcon(const std::string& name) = 0;

		virtual unsigned getWindowID() const = 0;

		void render(const Renderable* r) const;

		virtual void swap() = 0;

		void mapMousePosition(int* x, int* y);

		void enable16bpp(bool bpp=true);
		void enableMultisampling(bool multi_sampling=true, int samples=4);
		void enableResizeableWindow(bool en=true);
		void setFullscreenMode(FullScreenMode mode);
		void enableVsync(bool en=true);

		bool use16bpp() const { return use_16bpp_; }
		bool useMultiSampling() const { return use_multi_sampling_; }
		int multiSamples() const { return samples_; }
		bool resizeable() const { return is_resizeable_; }
		bool borderless() const { return is_borderless_; }
		FullScreenMode fullscreenMode() const { return fullscreen_mode_; }
		bool vSync() const { return use_vsync_; }

		int width() const { return width_; }
		int height() const { return height_; }

		int logicalWidth() const { return logical_width_; }
		int logicalHeight() const { return logical_height_; }

		const std::string& getTitle() const { return title_; }

		void setClearColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255) const;
		void setClearColor(float r, float g, float b, float a=1.0f) const;
		void setClearColor(const Color& color) const;
		
		Color getClearColor() const { return clear_color_; }

		virtual void clear(ClearFlags f) = 0;

		void setViewPort(int x, int y, int width, int height);
		void setViewPort(const rect& vp);
		const rect& getViewPort() const { return view_port_; }

		std::string saveFrameBuffer(const std::string& filename);

		virtual std::vector<WindowMode> getWindowModes(std::function<bool(const WindowMode&)> mode_filter) const = 0;
		virtual WindowMode getDisplaySize() const = 0;

		void notifyNewWindowSize(int new_width, int new_height, int flags=WindowSizeChangeFlags::NONE);

		int registerSizeChangeObserver(std::function<void(int,int,int)> fn);
		bool registerSizeChangeObserver(int key, std::function<void(int,int,int)> fn);
		void unregisterSizeChangeObserver(int);

		DisplayDevicePtr getDisplayDevice() const { return display_; }
	protected:
		void updateDimensions(int w, int h, int flags=WindowSizeChangeFlags::NONE);
		void setDisplayDevice(DisplayDevicePtr display) { display_ = display; }
		mutable Color clear_color_;
	private:
		int width_;
		int height_;
		int logical_width_;
		int logical_height_;
		bool use_16bpp_;
		bool use_multi_sampling_;
		int samples_;
		bool is_resizeable_;
		bool is_borderless_;
		FullScreenMode fullscreen_mode_;
		std::string title_;
		bool use_vsync_;
		rect view_port_;

		DisplayDevicePtr display_;

		virtual void changeFullscreenMode() = 0;
		virtual void handleSetClearColor() const = 0;
		virtual bool handleLogicalWindowSizeChange() = 0;
		virtual bool handlePhysicalWindowSizeChange() = 0;
		virtual void handleSetWindowTitle() = 0;
		virtual void handleSetViewPort() = 0;

		std::map<int, std::function<void(int,int,int)>> dimensions_changed_observers_;
	};

	class WindowManager
	{
	public:
		WindowManager(const std::string& window_hint);
		// API creation function (1) All-in-one window creation based on the hints given.
		WindowPtr createWindow(int width, int height, const variant& hints=variant());

		// API creation functions (2) Allocate a window structure, allowing you to programmatically
		// set the parameters, then actually create the window.
		WindowPtr allocateWindow(const variant& hints=variant());
		void createWindow(WindowPtr wnd);
		
		static std::vector<WindowPtr> getWindowList();
		static WindowPtr getWindowFromID(unsigned id);
		static WindowPtr getMainWindow();
	private:
		WindowManager(const WindowManager&);
		std::string window_hint_;
	};
}
