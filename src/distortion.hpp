/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include "geometry.hpp"

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

namespace raster_effects 
{
	//Class which represents distortions which affect blitting operations.
	//This is useful to generate 'waves' such as for water, heat, etc.
	class RasterDistortion : public game_logic::FormulaCallable
	{
	public:
		explicit RasterDistortion(const rect& r);
		virtual ~RasterDistortion();

		//function which map undistorted co-ordinates into their distorted equivalents.
		virtual void distortPoint(float* x, float* y) const = 0;

		//functions which determine the granularity of the distortion on each axis.
		//This represents the size of the edges of the rectangles that textures will
		//be divided into. The lower the value, the finer the granularity, and the
		//more expensive the operations.
		virtual int getGranularityX() const = 0;
		virtual int getGranularityY() const = 0;

		//the area that the raster distortion takes effect in.
		rect getArea() const { return area_; }
		void setArea(const rect& area) { area_ = area; }

		int getCycle() const { return cycle_; }
		void nextCycle() const { ++cycle_; }
		void setCycle(int n) { cycle_ = n; }
	private:
		DECLARE_CALLABLE(RasterDistortion);
		rect area_;
		mutable int cycle_;
	};

	typedef ffl::IntrusivePtr<RasterDistortion> RasterDistortionPtr;

	class WaterDistortion : public RasterDistortion
	{
	public:
		WaterDistortion(int offset, const rect& r);

		void distortPoint(float* x, float* y) const override;

		int getGranularityX() const override;
		int getGranularityY() const override;
	private:
		DECLARE_CALLABLE(WaterDistortion);
		int offset_;
	};

	class RadialDistortion : public RasterDistortion
	{
	public:
		RadialDistortion(int x, int y, int radius, int intensity=5);

		void distortPoint(float* x, float* y) const override;

		int getGranularityX() const override;
		int getGranularityY() const override;
	private:
		DECLARE_CALLABLE(RadialDistortion);
		int x_, y_;
		float radius_;
		float intensity_;
	};
}
