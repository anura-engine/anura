/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#include "graphics.hpp"
#include "rectangle_rotator.hpp"
#include <math.h>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "unit_test.hpp"

#if defined(_MSC_VER)
#include <boost/math/special_functions/round.hpp>
#define bmround	boost::math::round
#else
#define bmround	std::round
#endif

void rotate_rect(GLshort center_x, GLshort center_y, float rotation, GLshort* rect_vertexes){

	point p;
	
	float rotate_radians = (rotation * M_PI)/180;
	
	//rect r(rect_vertexes[0],rect_vertexes[1],rect_vertexes[4]-rect_vertexes[0],rect_vertexes[5]-rect_vertexes[1]);
	
	p = rotate_point_around_origin_with_offset( rect_vertexes[0], rect_vertexes[1], rotate_radians, center_x, center_y );
	rect_vertexes[0] = p.x;
	rect_vertexes[1] = p.y;
	
	p = rotate_point_around_origin_with_offset( rect_vertexes[2], rect_vertexes[3], rotate_radians, center_x, center_y );
	rect_vertexes[2] = p.x;
	rect_vertexes[3] = p.y;
	
	p = rotate_point_around_origin_with_offset( rect_vertexes[4], rect_vertexes[5], rotate_radians, center_x, center_y );
	rect_vertexes[4] = p.x;
	rect_vertexes[5] = p.y;
	
	p = rotate_point_around_origin_with_offset( rect_vertexes[6], rect_vertexes[7], rotate_radians, center_x, center_y );
	rect_vertexes[6] = p.x;
	rect_vertexes[7] = p.y;
	
}


void rotate_rect(const rect& r, GLfloat angle, GLshort* output){
	
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

point rotate_point_around_origin_with_offset(int x1, int y1, float alpha, int u1, int v1){
	
	point beta = rotate_point_around_origin(x1 - u1, y1 - v1, alpha);
	
	beta.x += u1;
	beta.y += v1;
	
	return beta;
}

point rotate_point_around_origin(int x1, int y1, float alpha){

	point beta;
	
	/*   //we actually don't need the initial theta and radius.  This is why:
	x2 = R * (cos(theta) * cos(alpha) + sin(theta) * sin(alpha))
	y2 = R * (sin(theta) * cos(alpha) + cos(theta) * sin(alpha));
	but
	R * (cos(theta)) = x1
	R * (sin(theta)) = x2
	this collapses the above to:  */

	beta.x = bmround(x1 * cos(alpha)) - bmround(y1 * sin(alpha));
	beta.y = bmround(y1 * cos(alpha)) + bmround(x1 * sin(alpha));

	return beta;
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
	GLshort output[8];
	BENCHMARK_LOOP {
		rotate_rect(r, 75, output);
	}
}
