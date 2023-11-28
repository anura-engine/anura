/*
    Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
    Copyright (C) 2015 by Kristina Simpson <sweet.kristas@gmail.com>

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

#include "CameraObject.hpp"
#include "WindowManager.hpp"

#include "asserts.hpp"
#include "screen_handling.hpp"
#include "preferences.hpp"

extern bool g_desktop_fullscreen; //test

namespace graphics
{
	PREF_INT(min_window_width, 934, "Minimum window width when auto-determining window size");
	PREF_INT(min_window_height, 700, "Minimum window height when auto-determining window size");
	PREF_INT(max_window_width, 10240, "Minimum window width when auto-determining window size");
	PREF_INT(max_window_height, 7680, "Minimum window height when auto-determining window size");
	PREF_INT(auto_size_ideal_width, 0, "");
	PREF_INT(auto_size_ideal_height, 0, "");

	GameScreen::GameScreen()
		: width_(0),
		  height_(0),
		  virtual_width_(0),
		  virtual_height_(0),
		  x_(0),
		  y_(0),
		  screen_clip_(),
		  cam_(),
		  last_cam_()
	{
	}

	GameScreen& GameScreen::get()
	{
		static GameScreen res;
		static int handle = KRE::WindowManager::getMainWindow()->registerSizeChangeObserver(std::bind(&GameScreen::windowSizeUpdated, &res, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		return res;
	}

	void GameScreen::setDimensions(int width, int height)
	{
		width_ = width;
		height_ = height;
	}

	void GameScreen::windowSizeUpdated(int width, int height, int flags)
	{
		if(!(flags & KRE::WindowSizeChangeFlags::NOTIFY_CANVAS_ONLY)) {
			setDimensions(width, height);
			setVirtualDimensions(width, height);
		}
	}

	void GameScreen::setVirtualDimensions(int vwidth, int vheight)
	{
		virtual_width_ = vwidth;
		virtual_height_ = vheight;
		cam_ = std::make_shared<KRE::Camera>("gs.cam", 0, virtual_width_, 0, virtual_height_);
	}

	void GameScreen::setLocation(int x, int y)
	{
		x_ = x;
		y_ = y;
		cam_ = std::make_shared<KRE::Camera>("gs.cam", x_, virtual_width_, y_, virtual_height_);
	}

	//should use the fullscreen mode from preferences.hpp
	void GameScreen::setFullscreen(KRE::FullScreenMode mode) {
		auto wnd = KRE::WindowManager::getMainWindow();
		auto& gs = graphics::GameScreen::get();

		if(mode == KRE::FullScreenMode::FULLSCREEN_WINDOWED)
		{
			LOG_DEBUG("Entering full-screen mode.");
			wnd->setFullscreenMode(KRE::FullScreenMode::FULLSCREEN_WINDOWED);

			if(preferences::auto_size_window() || g_desktop_fullscreen) {
				SDL_DisplayMode dm;
				if(SDL_GetDesktopDisplayMode(0, &dm) == 0) {
					preferences::adjust_virtual_width_to_match_physical(dm.w, dm.h);
					wnd->setWindowSize(dm.w, dm.h);
					gs.setDimensions(dm.w, dm.h);
					gs.setVirtualDimensions(preferences::requested_virtual_window_width(), preferences::requested_virtual_window_height());
				}

			}
		} else {
			LOG_DEBUG("Entering windowed mode.");
			wnd->setFullscreenMode(KRE::FullScreenMode::WINDOWED);

			if(preferences::auto_size_window() || g_desktop_fullscreen) {
				int width = 0, height = 0;

				if(preferences::requested_window_width() > 0 && preferences::requested_window_height() > 0) {
					width = preferences::requested_window_width();
					height = preferences::requested_window_height();
				} else {
					GameScreen::autoSelectResolution(wnd, width, height, true, false);
				}

				preferences::adjust_virtual_width_to_match_physical(width, height);

				wnd->setWindowSize(width, height);
				gs.setDimensions(width, height);
				gs.setVirtualDimensions(preferences::requested_virtual_window_width(), preferences::requested_virtual_window_height());
			}
		}
	}


	// Seemingly, this is to select the "next common resolution down" for windowed mode.
	// Takes a window, two out params for the best common w/h which will fit in the screen at 2x (?), and "reduce" (?).
	void GameScreen::autoSelectResolution(KRE::WindowPtr wm, int& width, int& height, bool reduce, bool isFullscreen)
	{
		auto mode = wm->getDisplaySize();
		auto best_mode = mode;
		bool found = false;

		if(isFullscreen) {
			LOG_INFO("RESOLUTION SET TO FULLSCREEN RESOLUTION " << mode.width << "x" << mode.height);

			width = mode.width;
			height = mode.height;

			return;
		}

		LOG_INFO("TARGET RESOLUTION IS " << mode.width << "x" << mode.height);

		const float MinReduction = reduce ? 0.9f : 2.0f;
		for(auto& candidate_mode : wm->getWindowModes([](const KRE::WindowMode&){ return true; })) {
			if(g_auto_size_ideal_width && g_auto_size_ideal_height) {
				if(found && candidate_mode.width < best_mode.width) {
					continue;
				}

				if(candidate_mode.width > mode.width * MinReduction) {
					LOG_INFO("REJECTED MODE IS " << candidate_mode.width << "x" << candidate_mode.height
						<< "; (width " << candidate_mode.width << " > " << mode.width * MinReduction << ")");
					continue;
				}

				int h = (candidate_mode.width * g_auto_size_ideal_height) / g_auto_size_ideal_width;
				if(h > mode.height * MinReduction) {
					continue;
				}

				best_mode = candidate_mode;
				best_mode.height = h;
				found = true;

				LOG_INFO("BETTER MODE IS " << best_mode.width << "x" << best_mode.height);

			} else
				if(    candidate_mode.width < mode.width * MinReduction
					&& candidate_mode.height < mode.height * MinReduction
					&& ((candidate_mode.width >= best_mode.width
						&& candidate_mode.height >= best_mode.height) || !found)
					) {
					found = true;
					LOG_INFO("BETTER MODE IS " << candidate_mode.width << "x" << candidate_mode.height << " vs " << best_mode.width << "x" << best_mode.height);
					best_mode = candidate_mode;
				} else {
					LOG_INFO("REJECTED MODE IS " << candidate_mode.width << "x" << candidate_mode.height);
				}
		}

		if (best_mode.width < g_min_window_width ||
			best_mode.height < g_min_window_height) {

			best_mode.width = g_min_window_width;
			best_mode.height = g_min_window_height;
		}

		if (best_mode.width > g_max_window_width) {
			best_mode.width = g_max_window_width;
		}

		if (best_mode.height > g_max_window_height) {
			best_mode.height = g_max_window_height;
		}

		LOG_INFO("CHOSEN MODE IS " << best_mode.width << "x" << best_mode.height);

		width = best_mode.width;
		height = best_mode.height;
	}

	void GameScreen::setupForDraw(KRE::WindowPtr wnd)
	{
		last_cam_ = KRE::DisplayDevice::getCurrent()->setDefaultCamera(cam_);
		//screen_clip_.reset(new KRE::Scissor::Manager(rect(x_, y_, width_, height_)));
		wnd->setViewPort(x_, y_, virtual_width_, virtual_height_);
	}

	void GameScreen::cleanupAfterDraw(KRE::WindowPtr wnd)
	{
		//screen_clip_.reset();
		wnd->setViewPort(0, 0, wnd->width(), wnd->height());
		KRE::DisplayDevice::getCurrent()->setDefaultCamera(last_cam_);
	}

	GameScreen::Manager::Manager(KRE::WindowPtr wnd)
		: wnd_(wnd)
	{
		GameScreen::get().setupForDraw(wnd_);
	}

	GameScreen::Manager::~Manager()
	{
		GameScreen::get().cleanupAfterDraw(wnd_);
	}

	extern bool g_desktop_fullscreen;
}
