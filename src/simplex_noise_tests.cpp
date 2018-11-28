/*
 *  Copyright (C) 2003-2018 David White <davewx7@gmail.com>
 *                20??-2018 Kristina Simpson <sweet.kristas@gmail.com>
 *                2017-2018 galegosimpatico <galegosimpatico@outlook.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not
 *   claim that you wrote the original software. If you use this software
 *   in a product, an acknowledgement in the product documentation would be
 *   appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be
 *   misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any source
 *   distribution.
 */

#include <stdint.h>

#include "simplex_noise.hpp"

#include "asserts.hpp"
#include "unit_test.hpp"

UNIT_TEST(simplex_noise_0) {
	noise::simplex::init(0);
	const double noise1 = noise::simplex::noise1(0.0);
	CHECK_EQ(0, noise1);
}

UNIT_TEST(simplex_noise_1) {
	noise::simplex::init(0);
	float a2[2] = {0.0f, 0.0f};
	const float noise2 = noise::simplex::noise2(a2);
	CHECK_EQ(0, noise2);
}

UNIT_TEST(simplex_noise_2) {
	noise::simplex::init(0);
	float a3[3] = {0.0f, 0.0f, 0.0f};
	const float noise3 = noise::simplex::noise3(a3);
	CHECK_EQ(0, noise3);
}

UNIT_TEST(simplex_noise_3) {
	noise::simplex::init(0);
	const double noise1 = noise::simplex::noise1(0.9);
#ifdef __APPLE__  //   XXX
	ASSERT_LOG(-0.12016 < noise1, noise1);
	ASSERT_LOG(+0.12044 > noise1, noise1);
#endif  //   XXX
}

UNIT_TEST(simplex_noise_4) {
	noise::simplex::init(0);
	float a2[2] = {0.7f, 0.8f};
	const float noise2 = noise::simplex::noise2(a2);
#ifdef __APPLE__  //   XXX
	/** Results seem not to be evenly distributed. */
	/** Negative extremes are more frequent than positive extremes. */
	ASSERT_LOG(-0.47892 < noise2, noise2);
	ASSERT_LOG(+0.47766 > noise2, noise2);
#endif  //   XXX
}

UNIT_TEST(simplex_noise_5) {
	noise::simplex::init(0);
	float a3[3] = {0.7f, 0.8f, 0.9f};
	const float noise3 = noise::simplex::noise3(a3);
#ifdef __APPLE__  //   XXX
	/** Results seem not to be evenly distributed. */
	/** Negative extremes are more frequent than positive extremes. */
	ASSERT_LOG(-0.44993 < noise3, noise3);
	ASSERT_LOG(+0.45376 > noise3, noise3);
#endif  //   XXX
}
