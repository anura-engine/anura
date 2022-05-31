/*
	This work by Ryan Muller released under the Creative Commons CC0 License
	http://creativecommons.org/publicdomain/zero/1.0/
*/
/**
	Spline interpolation of a parametric function.
 
	INPUT: std::vector<double> x
	A list of double values that represent sampled
	points. The array index of each point will be
	taken as the parameter t by which x will be
	represented as a function.
 
	OUTPUT: std::vector<cv::Vec4d> P
	A list of cv::Vec4d representing polynomials. To
	interpret segment [i]:
	x(t) = P0*a + P1*b + P2*(a^3-a)/6 + P3*(b^3-b)/6
	where a = t-i
	b = i-t+1
*/
/*
	Modified by Kristina Simpson in order to better fit with how we do things,
	also added the interpolate function.
*/

#pragma once

namespace geometry 
{
	void spline(const ImVec2* cps, const int maxpoints, float* z_prime_prime) 
	{
		// working variables
		float* u = new float[maxpoints];
		memset(u, 0, sizeof(float) * maxpoints);

		// set the second derivative to 0 at the ends
		z_prime_prime[0] = u[0] = 0;
		z_prime_prime[maxpoints-1] = 0;

		// decomposition loop
		for(int i = 1; i < maxpoints-1; i++) {
			const float sig = (cps[i].x-cps[i-1].x)/(cps[i+1].x-cps[i-1].x);
			const float p = sig * z_prime_prime[i-1] + 2.0f;
			z_prime_prime[i] = (sig-1.0f)/p;
			u[i] = (cps[i+1].y-cps[i].y)/(cps[i+1].x-cps[i].x) - (cps[i].y-cps[i-1].y)/(cps[i].x-cps[i-1].x);
			u[i] = (6.0f*u[i]/(cps[i+1].x-cps[i-1].x)-sig*u[i-1])/p;
		}

		// back-substitution loop
		for(int i = maxpoints - 1; i > 0; i--) {
			z_prime_prime[i-1] = z_prime_prime[i-1] * z_prime_prime[i] + u[i-1];
		}

		delete [] u;
	}

	float interpolate(float x, const ImVec2* cps, const int maxpoints, float* z_prime_prime)
	{
		int lo = 0;
		int hi = maxpoints - 1;
		float h, b, a;

		while(hi - lo > 1) {
				int k = (hi + lo) >> 1;
				if(cps[k].x > x) {
						hi = k;
				} else {
						lo = k;
				}
		}
		h = cps[hi].x - cps[lo].x;
		assert(h != 0.0f);
		//ASSERT_LOG(h != 0.0, "SPLINE: bad value in call to spline::interpolate.");
		a = (cps[hi].x - x)/h;
		b = (x - cps[lo].x)/h;
		return a*cps[lo].y + b*cps[hi].y + ((a*a*a-a)*z_prime_prime[lo] + (b*b*b-b)*z_prime_prime[hi])*(h*h)/6.0f;
	}
}
