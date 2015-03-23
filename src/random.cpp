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

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>


#include "random.hpp"

namespace rng 
{
	namespace 
	{
		boost::random::mt19937 state;
		boost::random::uniform_int_distribution<> generator(0,0xFFFFFF);
		bool rng_init = false;
	}

	int generate() 
	{
		if(!rng_init) {
			seed_from_int(time(NULL));
		}
		return generator(state);
	}

	void seed_from_int(unsigned int seed) 
	{
		rng_init = true;
		state = boost::random::mt19937(seed);
	}

	void set_seed(const Seed& seed) 
	{
		state = seed;
	}

	Seed get_seed() 
	{
		return state;
	}
}
