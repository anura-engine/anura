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
		void start(float t) { start_time_ = t + delay_; started_ = true; onStart(); }
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
		virtual void onStart() {}
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
		const KRE::Color& getStartColor() const { return start_color_; }
		const KRE::Color& getEndColor() const { return end_color_; }
		const KRE::ColorPtr& getColor() const { return mix_color_; }
		bool isEqual() const { return start_color_ == end_color_; }
	private:
		std::string handleToString() const override;
		void handleProcess(float dt, float outp) override;
		KRE::Color start_color_;
		KRE::Color end_color_;
		KRE::ColorPtr mix_color_;
	};

	class LengthTransition : public Transition
	{
	public:
		MAKE_FACTORY(LengthTransition);
		explicit LengthTransition(const TimingFunction& fn, float duration, float delay);
		void setStartLength(std::function<xhtml::FixedPoint()> fn);
		void setEndLength(std::function<xhtml::FixedPoint()> fn);
		xhtml::FixedPoint getLength() const { return mix_; }
		xhtml::FixedPoint getStartLength() const { return start_; }
		xhtml::FixedPoint getEndLength() const { return end_; }
		bool isEqual() const { return start_ == end_; }
	private:
		std::string handleToString() const override;
		void handleProcess(float dt, float outp) override;
		xhtml::FixedPoint start_;
		xhtml::FixedPoint end_;
		xhtml::FixedPoint mix_;
	};

	class WidthTransition : public Transition
	{
	public:
		MAKE_FACTORY(WidthTransition);
		explicit WidthTransition(const TimingFunction& fn, float duration, float delay);
		void setStartWidth(std::function<xhtml::FixedPoint()> fn);
		void setEndWidth(std::function<xhtml::FixedPoint()> fn);
		xhtml::FixedPoint getWidth() const { return mix_; }
		xhtml::FixedPoint getStartWidth() const { return start_; }
		xhtml::FixedPoint getEndWidth() const { return end_; }
		bool isEqual() const { return start_ == end_; }
	private:
		std::string handleToString() const override;
		void handleProcess(float dt, float outp) override;
		xhtml::FixedPoint start_;
		xhtml::FixedPoint end_;
		xhtml::FixedPoint mix_;
	};

	class FilterTransition : public Transition
	{
	public:
		MAKE_FACTORY(FilterTransition);
		explicit FilterTransition(const TimingFunction& fn, float duration, float delay);
		void setStartFilter(const std::shared_ptr<FilterStyle>& start);
		void setEndFilter(const std::shared_ptr<FilterStyle>& end) { end_ = end; }
		std::shared_ptr<FilterStyle> getFilter() const { return mix_filter_; }
	private:
		std::string handleToString() const override;
		void handleProcess(float dt, float outp) override;
		std::shared_ptr<FilterStyle> start_;
		std::shared_ptr<FilterStyle> end_;
		std::shared_ptr<FilterStyle> mix_filter_;
	};

	class TransformTransition : public Transition
	{
	public:
		MAKE_FACTORY(TransformTransition);
		explicit TransformTransition(const TimingFunction& fn, float duration, float delay);
		void setStart(const std::shared_ptr<TransformStyle>& start);
		void setEnd(const std::shared_ptr<TransformStyle>& end) { end_ = end; }
		std::shared_ptr<TransformStyle> getTransform() const { return mix_; }
	private:
		std::string handleToString() const override;
		void handleProcess(float dt, float outp) override;
		std::shared_ptr<TransformStyle> start_;
		std::shared_ptr<TransformStyle> end_;
		std::shared_ptr<TransformStyle> mix_;
	};

	typedef std::shared_ptr<ColorTransition> ColorTransitionPtr;
	typedef std::shared_ptr<LengthTransition> LengthTransitionPtr;
	typedef std::shared_ptr<WidthTransition> WidthTransitionPtr;
	typedef std::shared_ptr<FilterTransition> FilterTransitionPtr;
	typedef std::shared_ptr<TransformTransition> TransformTransitionPtr;
}
