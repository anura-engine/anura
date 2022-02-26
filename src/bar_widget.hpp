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

#include "widget.hpp"

namespace gui
{
	class BarWidget : public Widget
	{
	public:
		BarWidget(const variant& v, game_logic::FormulaCallable* e);
		virtual ~BarWidget();
		void init();

		void setRotation(float rotate);

		WidgetPtr clone() const override;
	private:
	DECLARE_CALLABLE(BarWidget);
	protected:
		virtual void handleDraw() const override;
		virtual void handleProcess() override;
		virtual bool handleEvent(const SDL_Event& event, bool claimed) override;
		void drawTicks(float x_offset, int segments, const KRE::Color& color) const;

		struct bar_section
		{
			KRE::TexturePtr texture;
			rect area;
		};

		void initBarSection(const variant&v, bar_section* b);

		bar_section bar_;
		bar_section left_cap_;
		bar_section right_cap_;

		KRE::Color bar_color_;
		KRE::Color drained_bar_color_;
		KRE::Color tick_mark_color_;
		KRE::Color drained_tick_mark_color_;

		float rotate_;
		float scale_;

		float animation_end_point_unscaled_;
		float animation_current_position_;

		// Rate (in frames/segment) that the bar goes removes
		// segments when drained_segments_ is increased or decreased.
		double drain_rate_;

		// Whether we are animating a transition or not.
		bool animating_;

		// Segments is a reflection of the number of ticks in the bar.
		// One segment = no tick marks
		// two segments = 1 tick
		// and so on
		int segments_;

		// Segments drawn with alternative color
		int drained_segments_;

		// Length of segments, in pixels basically.
		int segment_length_;

		// Width of tick mark in pixels.
		int tick_width_;

		// Calculated total length of the bar
		int total_bar_length_;
		// Calculated active length of the bar (i.e. segments-drained_segments)
		int active_bar_length_;
		// Calculated length of the drained bar
		int drained_bar_length_;
		// Calculated left_cap_width
		int left_cap_width_;
		// Calculated right_cap_width
		int right_cap_width_;
		// Calculated distance between ticks
		double tick_distance_;

		// Saved bar_height and width;
		int bar_height_;
		int bar_max_width_;

		int drained_segments_after_anim_;
	};
}
