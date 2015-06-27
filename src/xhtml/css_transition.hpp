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

#pragma once

#include <memory>

#include <Color.hpp>

#include "css_styles.hpp"

namespace css
{
	class Transition
	{
	public:
		explicit Transition(const TimingFunction& fn, float duration, float delay);
		virtual ~Transition() {}
		void start(float t) { start_time_ = t + delay_; started_ = true; }
		void stop() { stopped_ = true; }
		void process(float t);
		bool isStarted() const { return started_ && !stopped_; }
		bool isStopped() const { return stopped_; }
		void reset() { started_ = false; stopped_ = false; }
		std::string toString() const;
	private:
		TimingFunction ttfn_;
		bool started_;
		bool stopped_;
		float duration_;
		float delay_;
		float start_time_;

		virtual std::string handleToString() const = 0;
		virtual void handleProcess(float dt, float outp) = 0;
		Transition() = delete;
		Transition(const Transition&) = delete;
		void operator=(const Transition&) = delete;
	};

	typedef std::shared_ptr<Transition> TransitionPtr;

	class ColorTransition : public Transition
	{
	public:
		MAKE_FACTORY(ColorTransition);
		explicit ColorTransition(const TimingFunction& fn, float duration, float delay);
		void setStartColor(const KRE::Color& start) { start_color_ = start; *mix_color_ = start; }
		void setEndColor(const KRE::Color& end) { end_color_ = end; }
		const KRE::ColorPtr& getColor() const { return mix_color_; }
	private:
		std::string handleToString() const override;
		void handleProcess(float dt, float outp) override;
		KRE::Color start_color_;
		KRE::Color end_color_;
		KRE::ColorPtr mix_color_;
	};

	typedef std::shared_ptr<ColorTransition> ColorTransitionPtr;
}
