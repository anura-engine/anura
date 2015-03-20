/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <time.h>

#include "random.hpp"

namespace rng {

namespace {
boost::random::mt19937 state;
boost::random::uniform_int_distribution<> generator(0,0xFFFFFF);
bool rng_init = false;
}

int generate() {
	if(!rng_init) {
		seed_from_int(time(NULL));
	}
	return generator(state);
}

void seed_from_int(unsigned int seed) {
	rng_init = true;
	state = boost::random::mt19937(seed);
}

void set_seed(const Seed& seed) {
	state = seed;
}

Seed get_seed() {
	return state;
}

}
