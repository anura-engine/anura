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

#include "Blend.hpp"
#include "Color.hpp"

namespace KRE
{
	class ScopeableValue
	{
	public:
		ScopeableValue()
			: color_(),
			  color_set_(false),
			  blend_eqn_(),
			  blend_equation_set_(false),
			  blend_mode_(),
			  blend_mode_set_(false)
		{
		}
		bool isBlendEquationSet() const { return blend_equation_set_; }
		const BlendEquation& getBlendEquation() const { return blend_eqn_; }
		void setBlendEquation(const BlendEquation& eqn) { blend_eqn_ = eqn; blend_equation_set_ = true; }
		void clearBlendEquation() { blend_equation_set_ = false; }

		bool isBlendModeSet() const { return blend_mode_set_; }
		const BlendMode& getBlendMode() const { return blend_mode_; }
		void setBlendMode(const BlendMode& bm) { blend_mode_ = bm; blend_mode_set_ = true; }
		void setBlendMode(BlendModeConstants src, BlendModeConstants dst) { blend_mode_.set(src, dst); blend_mode_set_ = true; }
		void clearBlendMode() { blend_mode_set_ = false; }

		void setColor(float r, float g, float b, float a=1.0);
		void setColor(int r, int g, int b, int a=255);
		void setColor(const Color& color);
		const Color& getColor() const { return color_; }
		bool isColorSet() const { return color_set_; }
		void clearColor() { color_set_ = false; }

	private:
		Color color_;
		bool color_set_;
		BlendEquation blend_eqn_;
		bool blend_equation_set_;
		BlendMode blend_mode_;
		bool blend_mode_set_;
	};
}