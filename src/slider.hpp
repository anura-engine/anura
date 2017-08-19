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

#include <functional>

#include "image_widget.hpp"
#include "widget.hpp"
#include "gui_section.hpp"


namespace gui 
{
	typedef std::function<void(float)> ChangeFn;
	typedef std::function<void(float)> DragEndFn;

	//A Slider widget. Forwards to a given function whenever its value changes.
	class Slider : public Widget
	{
	public:
		explicit Slider(int width, ChangeFn onchange, float position=0.0f, int scale=2);
		explicit Slider(const variant& v, game_logic::FormulaCallable* e);
		float position() const {return position_;};
		void setPosition (float position) {position_ = position;};
		void setDragEnd(DragEndFn ondragend) { ondragend_ = ondragend; }
		WidgetPtr clone() const override;
	private:
		DECLARE_CALLABLE(Slider);

		bool inButton(int xloc, int yloc) const;

		void init() const;
		bool inSlider(int x, int y) const;
		void handleDraw() const override;
		bool handleEvent(const SDL_Event& event, bool claimed) override;
		void handleProcess() override;
		
		int width_;
		ChangeFn onchange_;
		DragEndFn ondragend_;
		bool dragging_;
		float position_;
		
		WidgetPtr slider_left_, slider_right_, slider_middle_, slider_button_;

		game_logic::FormulaPtr ffl_handler_;
		void changeDelegate(float);
		game_logic::FormulaPtr ffl_end_handler_;
		void dragEndDelegate(float);
	};
	
	typedef ffl::IntrusivePtr<Slider> SliderPtr;
}
