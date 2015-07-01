/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <glm/glm.hpp>

#include "asserts.hpp"
#include "css_transition.hpp"
#include "profile_timer.hpp"
#include "unit_test.hpp"

namespace css
{
	namespace
	{
		inline bool flt_equal(const float t, const float value) 
		{
			return std::abs(t - value) < FLT_EPSILON;
		}

		float recurse_cubic_bezier(float x, const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3)
		{
			const float tolerance = 0.00005f;
			const glm::vec2 p01 = (p0 + p1) / 2.0f;
			const glm::vec2 p12 = (p1 + p2) / 2.0f;
			const glm::vec2 p23 = (p2 + p3) / 2.0f;
			const glm::vec2 p012 = (p01 + p12) / 2.0f;
			const glm::vec2 p123 = (p12 + p23) / 2.0f;
			const glm::vec2 p0123 = (p012 + p123) / 2.0f;

			if(x < p0123.x) {
				if(std::abs(x - p012.x) < tolerance) {
					return p012.y;
				} else if(std::abs(x - p0123.x) < tolerance) {
					return p0123.y;
				} else if(std::abs(x - p0.x) < tolerance) {
					return p0.y;
				} else if(std::abs(x - p01.x) < tolerance) {
					return p01.y;
				}
				return recurse_cubic_bezier(x, p0, p01, p012, p0123);
			}

			if(std::abs(x - p0123.x) < tolerance) {
				return p0123.y;
			} else if(std::abs(x - p123.x) < tolerance) {
				return p123.y;
			} else if(std::abs(x - p23.x) < tolerance) {
				return p23.y;
			} else if(std::abs(x - p3.x) < tolerance) {
				return p3.y;
			}
			return recurse_cubic_bezier(x, p0123, p123, p23, p3);
		}

		// This isn't a full-blown evaluator for a cubic bezier, it's to meet the
		// CSS requirements.
		float evaluate_cubic_bezier(float t, const glm::vec2& p1,const glm::vec2& p2)
		{
			static const glm::vec2 p0(0.0f);
			static const glm::vec2 p3(1.0f);
			if(flt_equal(t, 0.0f)) {
				return 0.0f;
			}
			if(flt_equal(t, 1.0f)) {
				return 1.0f;
			}
			
			return recurse_cubic_bezier(t, p0, p1, p2, p3);
		}

		float evaluate_step(float t, int nintervals, bool start)
		{
			ASSERT_LOG(t >= 0.0f && t <= 1.0f, "Time specfied not in interval [0,1]: " << t);
			if(nintervals == 1) {
				// slight optimisation for a common case.
				return start ? 1.0f : t < 1.0f ? 0.0f : 1.0f;
			}
			if(flt_equal(t, 0.0f)) {
				return (start ? 1.0f/nintervals : 0);
			}
			if(flt_equal(t, 1.0f)) {
				return 1.0f;
			}
			const float step_incr = 1.0f / static_cast<float>(nintervals);
			const int step = (start ? 1 : 0) + static_cast<int>(t /  step_incr);
			return (step > nintervals ? nintervals : step) * step_incr;
		}

		float mix(float a, float s, float e) 
		{
			return (1.0f - a) * s + a * e;
		}
	}

	Transition::Transition(const TimingFunction& fn, float duration, float delay)
		: ttfn_(fn),
		  started_(false),
		  stopped_(false),
		  duration_(duration),
		  delay_(delay),
		  start_time_(0)
	{
	}

	void Transition::process(float t)
	{
		if(started_ && !stopped_) {
			if(t > (start_time_ + duration_)) {
				handleProcess(t, 1.0f);
				stopped_ = true;
			} else if(t >= start_time_) {
				float frac = (t - start_time_) / duration_;
				if(frac > 1.0f) {
					handleProcess(t, 1.0f);
				} else {
					float outp = 0.0f;
					if(ttfn_.getFunction() == CssTransitionTimingFunction::STEPS) {
						// steps
						outp = evaluate_step(frac, ttfn_.getIntervals(), ttfn_.getStepChangePoint() == StepChangePoint::START);
					} else {
						// cubic bezier
						outp = evaluate_cubic_bezier(frac, ttfn_.getP1(), ttfn_.getP2());
					}
					handleProcess(t, outp);
				}
			}
		}
	}

	std::string Transition::toString() const
	{
		std::stringstream ss;
		ss  << handleToString()
			<< ", started: " << (started_ ? "true" : "false")
			<< ", stopped: " << (stopped_ ? "true" : "false")
			<< ", duration: " << duration_
			<< ", delay: " << delay_
			<< ", start_time: " << start_time_
			;
		return ss.str();
	}

	ColorTransition::ColorTransition(const TimingFunction& fn, float duration, float delay)
		: Transition(fn, duration, delay),
		  start_color_(),
		  end_color_(),
		  mix_color_(std::make_shared<KRE::Color>())
	{
	}

	std::string ColorTransition::handleToString() const
	{
		std::stringstream ss;
		ss  << "ColorTransition: StartColor: " << start_color_
			<< ", EndColor: " << end_color_
			<< ", Mix: " << *mix_color_
			;
		return ss.str();
	}

	void ColorTransition::handleProcess(float dt, float outp)
	{
		mix_color_->setRed(mix(outp, start_color_.r(), end_color_.r()));
		mix_color_->setGreen(mix(outp, start_color_.g(), end_color_.g()));
		mix_color_->setBlue(mix(outp, start_color_.b(), end_color_.b()));
		mix_color_->setAlpha(mix(outp, start_color_.a(), end_color_.a()));
	}
}

UNIT_TEST(cubic_bezier) 
{
	for(float x = 0.0f; x <= 1.0f; x += 0.1f) {
		profile::manager pman("css::evaluate_cubic_bezier");
		float y = css::evaluate_cubic_bezier(x, glm::vec2(0.25f, 0.1f), glm::vec2(0.25f, 1.0f));
		//LOG_DEBUG("'ease' x: " << x << ", y: " << y);
	}
}
