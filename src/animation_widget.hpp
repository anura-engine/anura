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
#include <boost/intrusive_ptr.hpp>

#include "frame.hpp"
#include "kre/Geometry.hpp"
#include "label.hpp"
#include "widget.hpp"

namespace gui 
{
	class AnimationWidget : public Widget
	{
	public:
		AnimationWidget(int w, int h, const variant& node);
		AnimationWidget(const variant& v, game_logic::FormulaCallable* e);

		void setSequencePlayCount(int count) { max_sequence_plays_ = count; }
	private:
		DECLARE_CALLABLE(AnimationWidget);

		virtual void handleDraw() const override;
		void init();

		std::vector<variant> nodes_;
		mutable LabelPtr label_;
		mutable std::unique_ptr<Frame> frame_;
		mutable int cycle_;
		mutable int play_sequence_count_;
		// Number of times to repeat play each animation sequence.
		int max_sequence_plays_;
		mutable std::vector<variant>::const_iterator current_anim_;
	};

	typedef boost::intrusive_ptr<AnimationWidget> AnimationWidgetPtr;
}
