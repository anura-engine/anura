#pragma once

#include <vector>

typedef std::pair<float,float> control_point;
typedef std::vector<control_point> control_point_vector;
class spline
{
	public:
		spline(const control_point_vector& cps, float yp0, float ypn_1)
		{
			std::vector<float> u;
			u.resize(cps.size());
			second_derivatives_.resize(cps.size());
			if(yp0 > 0.99e30) {
				second_derivatives_[0] = 0.0f;
				u[0] = 0.0f;
			} else {
                second_derivatives_[0] = -0.5f;
                u[0]=(3.0/(cps[1].first-cps[0].first))*((cps[1].second-cps[0].second)/(cps[1].first-cps[0].first)-ypn_1);
			}
			for(size_t i = 1; i < cps.size(); ++i) {
				float sig = (cps[i].first-cps[i-1].first)/(cps[i+1].first-cps[i-1].first);
				float p = sig*second_derivatives_[i-1]+2.0f;
				second_derivatives_[i] = (sig-1.0f)/p;
				u[i] = (cps[i+1].second-cps[i].second)/(cps[i+1].first-cps[i].first) - (cps[i].second-cps[i-1].second)/(cps[i].first-cps[i-1].first);
				u[i] = (6.0*u[i]/(cps[i+1].first-cps[i-1].first)-sig*u[i-1])/p;
			}
			float qn = 0.0f;
			float un = 0.0f;
			if (ypn_1 <= 0.99e30) {
				qn = 0.5f;
				un = (3.0/(cps.back().first - cps[cps.size()-2].first)) * (ypn_1-(cps.back().second - cps[cps.size()-2].second)/(cps.back().first-cps[cps.size()-2].first));
			}
			second_derivatives_[cps.size()-2] = (un - qn * u[cps.size()-2]) / (qn * second_derivatives_[cps.size()-2] + 1.0f);
			for(size_t k=cps.size()-2; k >= 0; --k) {
				second_derivatives_[k] = second_derivatives_[k] * second_derivatives_[k+1] + u[k];
			}
		}
		float interpolate(float x)
		{
			size_t klo = 0;
			size_t khi = second_derivatives_.size()-1;
			size_t k;
			float h, b, a;

			while(khi - klo > 1) {
				k = (khi + klo) >> 1;
				if(control_points_[k].first > x) {
					khi = k;
				} else {
					klo = k;
				}
			}
			h = control_points_[khi].first - control_points_[klo].first;
			ASSERT_LOG(h != 0.0f, "FATAL: bad value in call to spline::interpolate.")
			a = (control_points_[khi].first - x)/h;
			b = (x - control_points_[klo].first)/h;
			return a*control_points_[klo].second + b*control_points_[khi].second + ((a*a*a-a)*second_derivatives_[klo] + (b*b*b-b)*second_derivatives_[khi])*(h*h)/6.0;
		}
	private:
		control_point_vector control_points_;
		std::vector<float> second_derivatives_;
		
		spline();
		spline(const spline&);
};
