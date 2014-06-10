/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
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
#ifndef NO_EDITOR

#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>

#include "frame.hpp"
#include "label.hpp"
#include "variant.hpp"
#include "widget.hpp"

namespace gui 
{
	class AnimationPreviewWidget : public Widget
	{
	public:
		static bool is_animation(variant obj);

		explicit AnimationPreviewWidget(variant obj);
		explicit AnimationPreviewWidget(const variant& v, game_logic::FormulaCallable* e);
		void init();
		void setObject(variant obj);

		void process();

		void setRectHandler(boost::function<void(rect)>);
		void setPadHandler(boost::function<void(int)>);
		void setNumFramesHandler(boost::function<void(int)>);
		void setFramesPerRowHandler(boost::function<void(int)>);
		void setSolidHandler(boost::function<void(int,int)>);

	private:
		DECLARE_CALLABLE(AnimationPreviewWidget);
		
		void handleDraw() const;
		bool handleEvent(const SDL_Event& event, bool claimed);

		void zoomIn();
		void zoomOut();
		void resetRect();

		point mousePointToImageLoc(const point& p) const;

		variant obj_;

		boost::scoped_ptr<frame> frame_;
		mutable int cycle_;

		std::vector<WidgetPtr> widgets_;
		mutable Label* zoom_label_;
		Label* pos_label_;

		mutable int scale_;
		void update_zoom_label() const;

		mutable rect src_rect_, dst_rect_;

		//anchors for mouse dragging events.
		int anchor_x_, anchor_y_;
		rect anchor_area_;
		int anchor_pad_;
		bool has_motion_;

		mutable rect locked_focus_;

		mutable int dragging_sides_bitmap_;
		enum { LEFT_SIDE = 1, RIGHT_SIDE = 2, TOP_SIDE = 4, BOTTOM_SIDE = 8, PADDING = 16 };

		mutable rect solid_rect_;
		bool moving_solid_rect_;
		int anchor_solid_x_, anchor_solid_y_;

		boost::function<void(rect)> rect_handler_;
		boost::function<void(int)> pad_handler_;
		boost::function<void(int)> num_frames_handler_;
		boost::function<void(int)> frames_per_row_handler_;
		boost::function<void(int,int)> solid_handler_;

		void rectHandlerDelegate(rect r);
		void padHandlerDelegate(int pad);
		void numFramesHandlerDelegate(int frames);
		void framesPerRowHandlerDelegate(int frames_per_row);
		void solidHandlerDelegate(int x, int y);

		game_logic::formula_ptr ffl_rect_handler_;
		game_logic::formula_ptr ffl_pad_handler_;
		game_logic::formula_ptr ffl_num_frames_handler_;
		game_logic::formula_ptr ffl_frames_per_row_handler_;
		game_logic::formula_ptr ffl_solid_handler_;
	};

	typedef boost::intrusive_ptr<AnimationPreviewWidget> AnimationPreviewWidgetPtr;
}

#endif // !NO_EDITOR
