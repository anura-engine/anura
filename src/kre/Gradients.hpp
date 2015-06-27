/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include "Color.hpp"
#include "SceneObject.hpp"

namespace KRE
{
	struct ColorStop
	{
		ColorStop() : color(nullptr), length(0) {}
		explicit ColorStop(const KRE::Color& col, float len) : color(col), length(len) {}
		Color color;
		// Proportion of length from starting point (0.0) to ending point (1.0).
		float length;
	};

	class LinearGradient
	{
	public:
		LinearGradient() : angle_(0.0f), color_stops_() {}
		void setAngle(float a) { angle_ = a; }
		void addColorStop(const KRE::Color& col, float len) { color_stops_.emplace_back(col, len); }
		SceneObjectPtr createRenderable();
		TexturePtr createAsTexture(int width, int height);
	private:
		// angle of gradient line, 0 degrees is straight up, 90 degrees is to the right.
		float angle_;
		std::vector<ColorStop> color_stops_;
	};
}
