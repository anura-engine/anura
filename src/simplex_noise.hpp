#pragma once

namespace noise
{
	namespace simplex
	{
		double noise1(double arg, uint32_t seed);
		float noise2(float vec[2], uint32_t seed);
		float noise3(float vec[3], uint32_t seed);
	}
}