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

#include "distortion.hpp"

namespace raster_effects
{
	RasterDistortion::RasterDistortion(const rect& r)
		: area_(r),
		cycle_(0)
	{
	}

	RasterDistortion::~RasterDistortion()
	{
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(RasterDistortion)
		DEFINE_FIELD(cycle, "int")
			return variant(obj.cycle_);
		DEFINE_SET_FIELD
			obj.setCycle(value.as_int());
		DEFINE_FIELD(area, "[int,int,int,int]")
			return obj.area_.write();
		DEFINE_SET_FIELD
			obj.setArea(rect(value));
		DEFINE_FIELD(granularity, "[int,int]")
			std::vector<variant> v;
			v.emplace_back(obj.getGranularityX());
			v.emplace_back(obj.getGranularityY());
			return variant(&v);
	END_DEFINE_CALLABLE(RasterDistortion)

	WaterDistortion::WaterDistortion(int offset, const rect& r)
		: RasterDistortion(r),
		offset_(offset)
	{
	}

	void WaterDistortion::distortPoint(float* x, float* y) const
	{
		*x = *x + 8.0f*sin((offset_ + *x)/20.0f) - 5.0f*sin((offset_/4 + *x * 3)/20.0f);
	}

	int WaterDistortion::getGranularityX() const
	{
		return 20;
	}

	int WaterDistortion::getGranularityY() const
	{
		return 10000;
	}

	BEGIN_DEFINE_CALLABLE(WaterDistortion, RasterDistortion)
		DEFINE_FIELD(offset, "int")
			return variant(obj.offset_);
		DEFINE_SET_FIELD
			obj.offset_ = value.as_int();
	END_DEFINE_CALLABLE(WaterDistortion)

	RadialDistortion::RadialDistortion(int x, int y, int radius, int intensity)
		: RasterDistortion(rect(x - radius, y - radius, radius*2, radius*2)),
		x_(x),
		y_(y),
		radius_(static_cast<float>(radius)),
		intensity_(static_cast<float>(intensity))
	{
	}

	void RadialDistortion::distortPoint(float* x, float* y) const
	{
		if(*x == x_ && *y == y_) {
			return;
		}

		const float vector_x = *x - x_;
		const float vector_y = *y - y_;
		const float distance = sqrt(vector_x*vector_x + vector_y*vector_y);
		if(distance > radius_) {
			return;
		}

		const float unit_vector_x = vector_x/distance;
		const float unit_vector_y = vector_y/distance;

		const float distort = sin(distance + getCycle()*0.2f)*intensity_*((radius_ - distance)/radius_);
		*x += unit_vector_x*distort;
		*y += unit_vector_y*distort;
	}

	int RadialDistortion::getGranularityX() const
	{
		return 10;
	}

	int RadialDistortion::getGranularityY() const
	{
		return 10;
	}

	BEGIN_DEFINE_CALLABLE(RadialDistortion, RasterDistortion)
		DEFINE_FIELD(radius, "decimal")
			return variant(obj.radius_ * 1000.0);
		DEFINE_SET_FIELD
			obj.radius_ = value.as_int()/1000.0f;
			obj.setArea(rect(static_cast<int>(obj.x_ - obj.radius_), static_cast<int>(obj.y_ - obj.radius_), static_cast<int>(obj.radius_*2), static_cast<int>(obj.radius_*2)));
	END_DEFINE_CALLABLE(RadialDistortion)
}
