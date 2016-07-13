/*
	Copyright (C) 2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <functional>

#include "AttributeSet.hpp"
#include "geometry.hpp"
#include "SceneObject.hpp"
#include "WindowManagerFwd.hpp"
#include "event_listener.hpp"

namespace scrollable
{
	typedef std::function<void(int)> change_handler;

	class Scrollbar : public KRE::SceneObject, public event_listener
	{
	public:
		enum Direction {VERTICAL, HORIZONTAL};
		explicit Scrollbar(Direction d, change_handler onchange, const rect& loc, const point& offset=point());
		~Scrollbar();
		int getScrollPosition() const { return scroll_pos_; }
		void setRange(int minr, int maxr);
		int getMin() const { return min_range_; }
		int getMax() const { return max_range_; }
		// N.B. using this function doesn't trigger a change notification.
		void setScrollPosition(int pos);
		const Direction& getDirection() const { return dir_; }

		void setLocation(int x, int y);
		void setDimensions(int w, int h);
		void setRect(const rect& r);
		const point& getLocation() const { return loc_.top_left(); }
		const point& getDimensions() const { static point p; p = point(loc_.w(), loc_.h()); return p; }

		void setOnChange(change_handler onchange);

		void setPageSize(int ps) { page_size_.reset(new int(ps)); }
		void setLineSize(int ls) { line_size_.reset(new int(ls)); }

		bool isVisible() const { return visible_; }
		void setVisible(bool v = true) { visible_ = v; }

		void scrollLines(int lines);

		void preRender(const KRE::WindowPtr& wm) override;

		void triggerFadeIn();
		void triggerFadeOut();

		void enableFade(float in_time=0.5, float out_time=0.5, bool in_on_mouseenter=true, bool out_on_mouseleave=false);
		void disableFade() { fade_enabled_ = false; transition_ = 0; }
	private:
		bool handle_mouse_motion(bool claimed, const point& p, unsigned keymod) override;
		bool handle_mouse_button_up(bool claimed, const point& p, unsigned buttons, unsigned keymod) override;
		bool handle_mouse_button_down(bool claimed, const point& p, unsigned buttons, unsigned keymod) override;	
		bool handle_mouse_wheel(bool claimed, const point& p, const point& delta, int direction) override;

		void init();
		void updateColors();
		void computeThumbPosition();
		int getLineSize() { return line_size_ ? *line_size_ : 1; }
		int getPageSize() { return page_size_ ? *page_size_ : (max_range_ - min_range_) / 10; }
		change_handler on_change_;
		Direction dir_;
		int min_range_;
		int max_range_;
		int scroll_pos_;
		std::unique_ptr<int> page_size_;		// amount to jump for one page
		std::unique_ptr<int> line_size_;		// amount to jump for one line
		rect loc_;
		rect up_arrow_area_;
		rect down_arrow_area_;
		rect left_arrow_area_;
		rect right_arrow_area_;
		rect thumb_area_;
		rect background_loc_;
		bool visible_;
		KRE::Color thumb_color_;
		KRE::Color thumb_selected_color_;
		KRE::Color thumb_mouseover_color_;
		KRE::Color background_color_;
		std::shared_ptr<KRE::Attribute<KRE::vertex_texcoord>> vertices_arrows_;
		std::shared_ptr<KRE::Attribute<KRE::vertex_texcoord>> vertices_background_;
		std::shared_ptr<KRE::Attribute<KRE::vertex_texcoord>> vertices_thumb_;
		KRE::AttributeSetPtr attr_arrows_;
		KRE::AttributeSetPtr attr_background_;
		KRE::AttributeSetPtr attr_thumb_;
		// Set to true to re-calculate the attribute sets for drawing.
		bool changed_;
		bool thumb_dragging_;
		bool thumb_mouseover_;
		bool thumb_update_;
		bool mouse_in_scrollbar_;
		point drag_start_position_;
		// Offset to compensate for mouse position being different from draw location.
		point offset_;
		bool fade_enabled_;
		bool fade_triggered_;
		float fade_in_time_;
		float fade_out_time_;
		int transition_;
		int start_time_;
		int fade_out_start_;
		bool fade_in_on_mouseenter_;
		bool fade_out_on_mouseleave_;
		bool fading_in_;
		int start_alpha_;
		int alpha_;
		Scrollbar() = delete;
	};

	typedef std::shared_ptr<Scrollbar> ScrollbarPtr;
}
