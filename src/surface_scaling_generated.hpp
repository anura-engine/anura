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
if(matrix[5] == matrix[6] &&
matrix[5] == matrix[12] &&
matrix[5] == matrix[18] &&
true) {
	{
	PixelUnion pu;
	int red = 0, green = 0, blue = 0, count = 0;
	pu.value = matrix[6];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[7];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[8];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[11];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[12];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[13];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	if(count > 0) {
		red /= count;
		green /= count;
		blue /= count;
		pu.rgba[0] = red; pu.rgba[1] = green; pu.rgba[2] = blue; pu.rgba[3] = 255;
		out1 = pu.value;
	}
}
	}
if(matrix[8] == matrix[9] &&
matrix[8] == matrix[12] &&
matrix[8] == matrix[16] &&
true) {
	{
	PixelUnion pu;
	int red = 0, green = 0, blue = 0, count = 0;
	pu.value = matrix[6];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[7];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[8];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[11];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[12];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[13];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	if(count > 0) {
		red /= count;
		green /= count;
		blue /= count;
		pu.rgba[0] = red; pu.rgba[1] = green; pu.rgba[2] = blue; pu.rgba[3] = 255;
		out0 = pu.value;
	}
}
	}
if(matrix[6] == matrix[12] &&
matrix[6] == matrix[18] &&
matrix[6] == matrix[19] &&
true) {
	{
	PixelUnion pu;
	int red = 0, green = 0, blue = 0, count = 0;
	pu.value = matrix[11];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[12];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[13];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[16];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[17];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[18];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	if(count > 0) {
		red /= count;
		green /= count;
		blue /= count;
		pu.rgba[0] = red; pu.rgba[1] = green; pu.rgba[2] = blue; pu.rgba[3] = 255;
		out2 = pu.value;
	}
}
	}
if(matrix[8] == matrix[12] &&
matrix[8] == matrix[15] &&
matrix[8] == matrix[16] &&
true) {
	{
	PixelUnion pu;
	int red = 0, green = 0, blue = 0, count = 0;
	pu.value = matrix[11];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[12];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[13];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[16];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[17];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	pu.value = matrix[18];
	red += pu.rgba[0]*1*pu.rgba[3];
	green += pu.rgba[1]*1*pu.rgba[3];
	blue += pu.rgba[2]*1*pu.rgba[3];
	count += pu.rgba[3]* 1;
	if(count > 0) {
		red /= count;
		green /= count;
		blue /= count;
		pu.rgba[0] = red; pu.rgba[1] = green; pu.rgba[2] = blue; pu.rgba[3] = 255;
		out3 = pu.value;
	}
}
	}
