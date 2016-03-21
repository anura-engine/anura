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
			  blend_mode_set_(false),
			  blend_enabled_(true),
              blend_state_set_(false),
			  depth_enabled_(false),
			  is_depth_enable_set_(false),
			  use_depth_write_(false),
			  is_use_depth_write_set_(false),
			  use_lighting_(false),
			  is_use_lighting_set_(false)
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

		bool isBlendEnabled() const { return blend_enabled_; }
		bool isBlendStateSet() const { return blend_state_set_; }
		void setBlendState(bool en = true) { blend_enabled_ = en; blend_state_set_ = true; }
		void clearblendState() { blend_state_set_ = false; blend_enabled_ = true; }

		void setColor(float r, float g, float b, float a=1.0) { color_ = Color(r, g, b, a); color_set_ = true; }
		void setColor(int r, int g, int b, int a=255) { color_ = Color(r, g, b, a); color_set_ = true; }
		void setColor(const Color& color) { color_ = color; color_set_ = true; }
		const Color& getColor() const { return color_; }
		bool isColorSet() const { return color_set_; }
		void clearColor() { color_set_ = false; }

		bool isDepthEnabled() const { return depth_enabled_; }
		bool isDepthEnableStateSet() const { return is_depth_enable_set_; }
		void setDepthEnable(bool en = true) { depth_enabled_ = en; is_depth_enable_set_ = true; }
		void clearDepthEnableState() { is_depth_enable_set_ = false; depth_enabled_ = false; }

		bool isDepthWriteEnable() const { return use_depth_write_; }
		bool isDepthWriteStateSet() const { return is_use_depth_write_set_; }
		void setDepthWrite(bool en = true) { use_depth_write_ = en; is_use_depth_write_set_ = true; }
		void clearDepthWrite() { use_depth_write_ = false; is_use_depth_write_set_ = false; }

		bool useLighting() const { return use_lighting_; }
		bool isLightingStateSet() const { return is_use_lighting_set_; }
		void enableLighting(bool en = true) { use_lighting_ = en; is_use_lighting_set_ = true; }
		void disbleLighting(bool en = true) { use_lighting_ = !en; is_use_lighting_set_ = true; }
		void clearLighting() { is_use_lighting_set_ = false; use_lighting_ = false; }

		void clear() { 
			color_set_ = false; 
			blend_equation_set_ = false;
			blend_mode_set_ = false;
			blend_enabled_ = true;
			blend_state_set_ = false;
			depth_enabled_ = false;
			is_depth_enable_set_ = false;
			use_depth_write_ = false;
			is_use_depth_write_set_ = false;
			use_lighting_ = false;
			is_use_lighting_set_ = false;
		}
	private:
		Color color_;
		bool color_set_;
		BlendEquation blend_eqn_;
		bool blend_equation_set_;
		BlendMode blend_mode_;
		bool blend_mode_set_;
		bool blend_enabled_;
		bool blend_state_set_;
		bool depth_enabled_;
		bool is_depth_enable_set_;
		bool use_depth_write_;
		bool is_use_depth_write_set_;
		bool use_lighting_;
		bool is_use_lighting_set_;
	};
}
