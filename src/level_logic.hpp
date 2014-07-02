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

class Level;

// find out if [xpos + deltax, ypos] is over a drop-off from [xpos, ypos].
// [xpos, ypos] must be on the ground. deltax must not be greater than
// the tile size.
bool cliff_edge_within(const Level& lvl, int xpos, int ypos, int deltax);

// find out how far the nearest cliff is from [xpos, ypos]
int distance_to_cliff(const Level& lvl, int xpos, int ypos, int facing);

// given a position, will return the xpos of the ground level closest to this
// position. Will search downwards if (xpos,ypos) is not solid, and upwards
// if (xpos,ypos) is solid. Will return std::numeric_limits<int>::min() on failure to find a result.
int find_ground_level(const Level& lvl, int xpos, int ypos, int max_search=20);
