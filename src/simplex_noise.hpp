#pragma once

namespace noise
{
	namespace simplex
	{
		void init(uint32_t seed);
		double noise1(double arg);
		float noise2(float vec[2]);
		float noise3(float vec[3]);
	}
}