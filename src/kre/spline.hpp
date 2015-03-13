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

#include <vector>

namespace geometry 
{
	typedef std::pair<double,double> control_point;
	typedef std::vector<control_point> control_point_vector;

	struct vec4
	{
		double a, b, c, d;;
	};

	class spline
	{
		public:
			spline(const control_point_vector& cps) : control_points_(cps) {
				// spline size
				size_t n = cps.size();
 
				// loop counter
				size_t i;
 
				// working variables
				std::vector<double> u;
 
				u.resize(n);
				z_prime_prime_.resize(n);
 
				// set the second derivative to 0 at the ends
				z_prime_prime_[0] = u[0] = 0;
				z_prime_prime_[n-1] = 0;
 
				// decomposition loop
				for(i = 1; i < n-1; i++) {
					double sig = (cps[i].first-cps[i-1].first)/(cps[i+1].first-cps[i-1].first);
					double p = sig * z_prime_prime_[i-1] + 2.0;
					z_prime_prime_[i] = (sig-1.0)/p;
					u[i] = (cps[i+1].second-cps[i].second)/(cps[i+1].first-cps[i].first) - (cps[i].second-cps[i-1].second)/(cps[i].first-cps[i-1].first);
					u[i] = (6.0*u[i]/(cps[i+1].first-cps[i-1].first)-sig*u[i-1])/p;
				}
 
				// back-substitution loop
				for(i = n - 1; i > 0; i--) {
					z_prime_prime_[i] = z_prime_prime_[i] * z_prime_prime_[i+1] + u[i];
				}
			}
			double interpolate(double x)
			{
				size_t lo = 0;
				size_t hi = z_prime_prime_.size()-1;
				double h, b, a;

				while(hi - lo > 1) {
						size_t k = (hi + lo) >> 1;
						if(control_points_[k].first > x) {
								hi = k;
						} else {
								lo = k;
						}
				}
				h = control_points_[hi].first - control_points_[lo].first;
				ASSERT_LOG(h != 0.0, "SPLINE: bad value in call to spline::interpolate.");
				a = (control_points_[hi].first - x)/h;
				b = (x - control_points_[lo].first)/h;
				return a*control_points_[lo].second + b*control_points_[hi].second + ((a*a*a-a)*z_prime_prime_[lo] + (b*b*b-b)*z_prime_prime_[hi])*(h*h)/6.0;
			}
		private:
			control_point_vector control_points_;
			// array of second derivatives
			std::vector<double> z_prime_prime_;
		
			spline();
			spline(const spline&);
	};
}