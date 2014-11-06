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
#ifndef RECTANGLE_ROTATOR_HPP_INCLUDED
#define RECTANGLE_ROTATOR_HPP_INCLUDED

#include "geometry.hpp"

void rotate_rect(GLshort center_x, GLshort center_y, float rotation, GLshort* rect_vertexes);
void rotate_rect(const rect& r, GLfloat angle, GLshort* output);
point rotate_point_around_origin_with_offset(int x1, int y1, float alpha, int u1, int v1);
point rotate_point_around_origin(int x1, int y1, float alpha);

#endif
