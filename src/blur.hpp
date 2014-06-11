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

#include <deque>

class frame;

//class which represents the blur information for a single object.
//a blur contains three parameters:
// - alpha: the initial alpha value of the blurred vision of the object.
// - fade: the rate at which the alpha fades each frame
// - granularity: the number of copies of the object that are made
//                every cycle.
class BlurInfo
{
public:
	BlurInfo(double alpha, double fade, int granularity);

	//function to copy settings into another BlurInfo instance. This will
	//keep our BlurFrames as they are, but copy in the alpha/fade/granularity
	//settings and so change our blur behavior from then on.
	void copySettings(const BlurInfo& info);

	//function to progress to the next frame. We are given starting and
	//ending position of the object, along with its drawing settings.
	//
	//'granularity' copies of the object's image will be made, linearly
	//interpolated between start_x,start_y and end_x,end_y.
	void nextFrame(int start_x, int start_y, int end_x, int end_y,
	                const frame* f, int time_in_frame, bool facing,
					bool upside_down, float start_rotate, float rotate);

	void draw() const;

	//returns true iff our granularity is now 0 and we have no BlurFrames.
	bool destroyed() const;

private:
	struct BlurFrame {
		const frame* object_frame;
		int time_in_frame;
		double x, y;
		bool facing, upside_down;
		float rotate;
		double fade;
	};

	double alpha_;
	double fade_;
	int granularity_;
	std::deque<BlurFrame> frames_;
};
