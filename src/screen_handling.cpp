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

namespace graphics
{
	GameScreen::GameScreen()
		: width_(0),
		  height_(0),
		  virtual_width_(0),
		  virtual_height_(0),
		  x_(0),
		  y_(0),
		  screen_clip_()
	{
	}

	GameScreen& GameScreen::get()
	{
		static GameScreen res;
		return res;
	}

	void GameScreen::mapCoordsPtoV(int* x, int *y)
	{
		ASSERT_LOG(x != nullptr && y != nullptr, "Either x or y is null.");
		*x = static_cast<int>(static_cast<float>(*x) * getScaleW());
		*y = static_cast<int>(static_cast<float>(*y) * getScaleH());
	}

	void GameScreen::mapCoordsVtoP(int* x, int *y)
	{
		ASSERT_LOG(x != nullptr && y != nullptr, "Either x or y is null.");
		*x = static_cast<int>(static_cast<float>(*x) / getScaleW());
		*y = static_cast<int>(static_cast<float>(*y) / getScaleH());
	}

	void GameScreen::setDimensions(int width, int height)
	{
		width_ = width;
		height_ = height;
	}

	void GameScreen::setVirtualDimensions(int vwidth, int vheight)
	{
		virtual_width_ = vwidth;
		virtual_height_ = vheight;
	}

	void GameScreen::setLocation(int x, int y)
	{
		x_ = x;
		y_ = y;
	}

	void GameScreen::setupForDraw(KRE::WindowPtr wnd)
	{
		auto orthocam = std::make_shared<KRE::Camera>("orthocam", 0, width_, 0, height_);
		KRE::DisplayDevice::getCurrent()->setDefaultCamera(orthocam);
		screen_clip_.reset(new KRE::Scissor::Manager(rect(x_, y_, width_, height_)));
		wnd->setViewPort(0, 0, width_, height_);
	}

	void GameScreen::cleanupAfterDraw(KRE::WindowPtr wnd)
	{
		screen_clip_.reset();
		wnd->setViewPort(0, 0, wnd->width(), wnd->height());
		auto orthocam = std::make_shared<KRE::Camera>("orthocam", 0, wnd->width(), 0, wnd->height());
		KRE::DisplayDevice::getCurrent()->setDefaultCamera(orthocam);
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
}
