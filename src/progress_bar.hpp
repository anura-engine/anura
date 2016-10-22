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

#include "framed_gui_element.hpp"
#include "widget.hpp"

namespace gui 
{
	class ProgressBar : public Widget
	{
	public:
		ProgressBar(int progress=0, int minv=0, int maxv=100, const std::string& gui_set="default_button");
		explicit ProgressBar(const variant& v, game_logic::FormulaCallable* e);

		int getMinValue() const { return min_; }
		int getMaxValue() const { return max_; }
		void setMinValue(int min_val);
		void setMaxValue(int max_val);
		int progress() const { return progress_; }
		void setProgress(int value);
		void updateProgress(int delta);
		void setCompletionHandler(std::function<void ()> oncompletion);
		void reset();
		WidgetPtr clone() const override;
	private:
		DECLARE_CALLABLE(ProgressBar)
		void complete();
		void handleDraw() const override;

		KRE::Color color_;
		int hpad_;
		int vpad_;
		int min_;
		int max_;
		int progress_;
		bool completion_called_;
		std::function<void ()> oncompletion_;
		game_logic::FormulaPtr completion_handler_;

		bool upscale_;
		ConstFramedGuiElementPtr frame_image_set_;
	};

	typedef ffl::IntrusivePtr<ProgressBar> ProgressBarPtr;
	typedef ffl::IntrusivePtr<const ProgressBar> ConstProgressBarPtr;
}
