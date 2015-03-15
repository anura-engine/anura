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

#pragma once

#include "WindowManagerFwd.hpp"

namespace graphics
{
	class GameScreen
	{
	public:
		int x() const { return x_; }
		int y() const { return y_; }
		int getWidth() const { return width_; }
		int getHeight() const { return height_; }
		int getArea() const { return width_ * height_; }
		float getAspectRatio() const { return static_cast<float>(width_) / static_cast<float>(height_); }
		int getVirtualWidth() const { return virtual_width_; }
		int getVirtualHeight() const { return virtual_height_; }
		float getScaleW() const { return static_cast<float>(virtual_width_) / static_cast<float>(width_); }
		float getScaleH() const { return static_cast<float>(virtual_height_) / static_cast<float>(height_); }
		void mapCoordsPtoV(int* x, int* y);
		void mapCoordsVtoP(int* x, int* y);

		void setLocation(int x, int y);
		void setDimensions(int width, int height);
		void setVirtualDimensions(int vwidth, int vheight);

		void setupForDraw(KRE::WindowPtr wnd);

		static GameScreen& get();
	private:
		GameScreen();
		int width_;
		int height_;
		int virtual_width_;
		int virtual_height_;
		int x_;
		int y_;

		GameScreen(const GameScreen&);
		void operator=(const GameScreen&);
	};
}
