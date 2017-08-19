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

#include <cmath>

#include "rectangle_rotator.hpp"
#include "unit_test.hpp"

void rotate_rect(short center_x, short center_y, float rotation, short* rect_vertexes)
{
	point p;
	
	float rotate_radians = static_cast<float>((rotation * M_PI)/180.0);
	
	//rect r(rect_vertexes[0],rect_vertexes[1],rect_vertexes[4]-rect_vertexes[0],rect_vertexes[5]-rect_vertexes[1]);
	
	p = rotate_point_around_origin_with_offset<int>(rect_vertexes[0], rect_vertexes[1], rotate_radians, center_x, center_y);
	rect_vertexes[0] = p.x;
	rect_vertexes[1] = p.y;
	
	p = rotate_point_around_origin_with_offset<int>(rect_vertexes[2], rect_vertexes[3], rotate_radians, center_x, center_y);
	rect_vertexes[2] = p.x;
	rect_vertexes[3] = p.y;
	
	p = rotate_point_around_origin_with_offset<int>(rect_vertexes[4], rect_vertexes[5], rotate_radians, center_x, center_y);
	rect_vertexes[4] = p.x;
	rect_vertexes[5] = p.y;
	
	p = rotate_point_around_origin_with_offset<int>(rect_vertexes[6], rect_vertexes[7], rotate_radians, center_x, center_y);
	rect_vertexes[6] = p.x;
	rect_vertexes[7] = p.y;
}

void rotate_rect(float center_x, float center_y, float rotation, float* rect_vertexes)
{
	pointf p;
	
	float rotate_radians = static_cast<float>((rotation * M_PI)/180.0);
	
	//rect r(rect_vertexes[0],rect_vertexes[1],rect_vertexes[4]-rect_vertexes[0],rect_vertexes[5]-rect_vertexes[1]);
	
	p = rotate_point_around_origin_with_offset(rect_vertexes[0], rect_vertexes[1], rotate_radians, center_x, center_y, false);
	rect_vertexes[0] = p.x;
	rect_vertexes[1] = p.y;
	
	p = rotate_point_around_origin_with_offset(rect_vertexes[2], rect_vertexes[3], rotate_radians, center_x, center_y, false);
	rect_vertexes[2] = p.x;
	rect_vertexes[3] = p.y;
	
	p = rotate_point_around_origin_with_offset(rect_vertexes[4], rect_vertexes[5], rotate_radians, center_x, center_y, false);
	rect_vertexes[4] = p.x;
	rect_vertexes[5] = p.y;
	
	p = rotate_point_around_origin_with_offset(rect_vertexes[6], rect_vertexes[7], rotate_radians, center_x, center_y, false);
	rect_vertexes[6] = p.x;
	rect_vertexes[7] = p.y;
}


void rotate_rect(const rect& r, float angle, short* output)
{
	point offset;
	offset.x = r.x() + r.w()/2;
	offset.y = r.y() + r.h()/2;

	point p;

	p = rotate_point_around_origin_with_offset( r.x(), r.y(), angle, offset.x, offset.y );
	output[0] = p.x;
	output[1] = p.y;

	p = rotate_point_around_origin_with_offset( r.x2(), r.y(), angle, offset.x, offset.y );
	output[2] = p.x;
	output[3] = p.y;

	p = rotate_point_around_origin_with_offset( r.x2(), r.y2(), angle, offset.x, offset.y );
	output[4] = p.x;
	output[5] = p.y;

	p = rotate_point_around_origin_with_offset( r.x(), r.y2(), angle, offset.x, offset.y );
	output[6] = p.x;
	output[7] = p.y;

}

//Calculate the bounding box for a rect which has been rotated and scaled.
rect rotated_scaled_rect_bounds(const rect& r, float angle, float scale)
{
	point offset;
	offset.x = r.x() + r.w()/2;
	offset.y = r.y() + r.h()/2;

	const point p1 = rotate_point_around_origin_with_offset( r.x(),  r.y(),  angle, offset.x, offset.y );
	const point p2 = rotate_point_around_origin_with_offset( r.x2(), r.y(),  angle, offset.x, offset.y );
	const point p3 = rotate_point_around_origin_with_offset( r.x2(), r.y2(), angle, offset.x, offset.y );
	const point p4 = rotate_point_around_origin_with_offset( r.x(),  r.y2(), angle, offset.x, offset.y );
	
	point min_bound = point(fmin(fmin(p1.x, p2.x), fmin(p3.x, p4.x)), fmin(fmin(p1.y, p2.y), fmin(p3.y, p4.y)));
	point max_bound = point(fmax(fmax(p1.x, p2.x), fmax(p3.x, p4.x)), fmax(fmax(p1.y, p2.y), fmax(p3.y, p4.y)));
	
	min_bound.x = (min_bound.x-offset.x)*scale + offset.x;
	min_bound.y = (min_bound.y-offset.y)*scale + offset.y; 
	max_bound.x = (max_bound.x-offset.x)*scale + offset.x;
	max_bound.y = (max_bound.y-offset.y)*scale + offset.y;
	
	return rect(min_bound.x, min_bound.y, max_bound.x, max_bound.y);
}


/*UNIT_TEST(rotate_test) {
	std::cerr << "rotating_a_point \n";
	std::cerr << rotate_point_around_origin( 1000, 1000, (M_PI/2)).to_string() << "\n";  //Should be -1000,1000 
	std::cerr << rotate_point_around_origin_with_offset( 11000, 1000, (M_PI/2), 10000,0).to_string() << "\n"; //Should be 9000,1000 
	
	GLshort myOutputData[8];
	rect r(10, 10, 20, 30);
	rotate_rect(r, (M_PI*2), myOutputData);
	
	std::cerr << "Outputting point list \n";
	for(int i=0;i<8;++i){
		std::cerr << myOutputData[i] << " ";
		if(i%2){ std::cerr << "\n";}
	}
}*/


BENCHMARK(rect_rotation) {
	rect r(10, 10, 20, 30);
	short output[8];
	BENCHMARK_LOOP {
		rotate_rect(r, 75, output);
	}
}
