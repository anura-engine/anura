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

#include <ctime>

#include "random.hpp"

namespace rng 
{
	namespace 
	{
		static unsigned int UninitSeed = 11483;
		static unsigned int next = UninitSeed;
	}

	int generate() 
	{
		if(next == UninitSeed) {
			next = static_cast<unsigned int>(time(NULL));
		}

		next = next * 1103515245 + 12345;
		const int result = ((unsigned int)(next/65536) % 32768);
		return result;
	}

	void set_seed(unsigned int seed) 
	{
		next = seed;
	}

	unsigned int get_seed() 
	{
		return next;
	}
}
