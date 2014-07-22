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

#include <boost/math/special_functions/round.hpp>

#include "kre/Geometry.hpp"

template<typename T>
Geometry::Point<T> rotate_point_around_origin(T x1, T y1, float alpha, bool round)
{
	Geometry::Point<T> beta;

	/*   //we actually don't need the initial theta and radius.  This is why:
	x2 = R * (cos(theta) * cos(alpha) + sin(theta) * sin(alpha))
	y2 = R * (sin(theta) * cos(alpha) + cos(theta) * sin(alpha));
	but
	R * (cos(theta)) = x1
	R * (sin(theta)) = x2
	this collapses the above to:  */

	float c1 = x1 * cos(alpha) - y1 * sin(alpha);
	float c2 = y1 * cos(alpha) + x1 * sin(alpha);

	beta.x = static_cast<T>(round ? boost::math::round(c1) : c1);
	beta.y = static_cast<T>(round ? boost::math::round(c2) : c2);

	return beta;
}

template<typename T>
Geometry::Point<T> rotate_point_around_origin_with_offset(T x1, T y1, float alpha, T u1, T v1, bool round=true)
{
	Geometry::Point<T> beta = rotate_point_around_origin(x1 - u1, y1 - v1, alpha, round);

	beta.x += u1;
	beta.y += v1;

	return beta;
}

void rotate_rect(short center_x, short center_y, float rotation, short* rect_vertexes);
void rotate_rect(float center_x, float center_y, float rotation, float* rect_vertexes);
void rotate_rect(const rect& r, float angle, short* output);
