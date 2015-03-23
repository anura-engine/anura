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

#include <cstdlib>
#include <limits>

#include "level.hpp"
#include "level_logic.hpp"

bool cliff_edge_within(const Level& lvl, int xpos, int ypos, int deltax)
{
	const int FeetWidth = 5;
	return !lvl.standable(xpos + deltax, ypos) &&
	       !lvl.standable(xpos + deltax, ypos + std::abs(deltax) + FeetWidth);
}

int distance_to_cliff(const Level& lvl, int xpos, int ypos, int facing)
{
	const int max_search = 1000;
	const int cliff_face = 5;
	const int cliff_drop = 2;

	bool found = false;
	
	//search for up to three pixels below us to try to get a starting
	//position which is standable.
	for(int n = 0; n != 3; ++n) {
		if(lvl.standable_tile(xpos, ypos)) {
			found = true;
			break;
		}

		++ypos;
	}

	if(!found) {
		return max_search;
	}

	//make sure we are at the surface.
	while(lvl.standable_tile(xpos, ypos-1)) {
		--ypos;
	}

	int result = 0;
	for(; result < max_search; xpos += facing, ++result) {
		if(lvl.standable_tile(xpos, ypos) || lvl.standable_tile(xpos, ypos-1)) {
			int ydiff = 0;
			while(lvl.standable_tile(xpos, ypos-1) && ydiff < cliff_face) {
				--ypos;
				++ydiff;
			}

			if(ydiff == cliff_face) {
				return max_search;
			}
		} else {
			int ydiff = 0;
			while(!lvl.standable_tile(xpos, ypos) && ydiff < cliff_drop) {
				++ypos;
				++ydiff;
			}

			if(ydiff == cliff_drop) {
				return result;
			}
		}
	}

	return result;
}

int find_ground_level(const Level& lvl, int xpos, int ypos, int max_search)
{
	if(lvl.standable(xpos, ypos)) {
		--ypos;
		while(lvl.standable(xpos, ypos) && --max_search > 0) {
			--ypos;
		}

		if(!max_search) {
			return std::numeric_limits<int>::min();
		}

		return ypos + 1;
	} else {
		//search both up and down, since in the case of a platform the
		//ground may be above us.
		for(int n = 1; n < max_search; ++n) {
			if(lvl.standable(xpos, ypos + n)) {
				return ypos + n - 1;
			}

			if(lvl.standable(xpos, ypos - n) && !lvl.standable(xpos, ypos - n - 1)) {
				return ypos - n;
			}
		}

		return std::numeric_limits<int>::min();
	}
}
