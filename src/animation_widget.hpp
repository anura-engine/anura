/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include <boost/scoped_ptr.hpp>
#include "intrusive_ptr.hpp"

#include "frame.hpp"
#include "geometry.hpp"
#include "label.hpp"
#include "widget.hpp"

namespace gui 
{
	class AnimationWidget : public Widget
	{
	public:
		AnimationWidget(int w, int h, const variant& node);
		AnimationWidget(const variant& v, game_logic::FormulaCallable* e);
		AnimationWidget(const AnimationWidget& a);

		void setSequencePlayCount(int count) { max_sequence_plays_ = count; }
	
		WidgetPtr clone() const override;
	protected:
		void surrenderReferences(GarbageCollector* collector) override;
	private:
		DECLARE_CALLABLE(AnimationWidget);
		
		void handleDraw() const override;
		void handleProcess() override;
		void init();

		std::string anim_name_;
		std::string type_;
		std::vector<variant> nodes_;

		LabelPtr label_;
		FramePtr frame_;
		int cycle_;
		int play_sequence_count_;
		// Number of times to repeat play each animation sequence.
		int max_sequence_plays_;
		std::vector<variant>::const_iterator current_anim_;
	};

	typedef ffl::IntrusivePtr<AnimationWidget> AnimationWidgetPtr;
}
