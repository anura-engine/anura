/*
	Copyright (C) 2015-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "event_listener.hpp"
#include "AttributeSet.hpp"
#include "SceneObject.hpp"

namespace xhtml
{
	class Slider;
	typedef std::shared_ptr<Slider> SliderPtr;

	typedef std::function<void(float)> on_change_fn;

	class Slider : public KRE::SceneObject, public EventListener
	{
	public:
		Slider(const rect& area, on_change_fn change=nullptr);
		~Slider();
		void setRange(float mn, float mx);
		float getMin() const { return min_range_; }
		float getMax() const { return max_range_; }
		void setStep(float step) { step_ = step; }

		float getHandlePosition() const { return position_; }
		void setHandlePosition(float value);

		void setLoc(const point& p) {
			loc_ = rect(p.x, p.y, loc_.w(), loc_.h());
		}
		void setDimensions(int w, int h);
	private:
		void init();
		void preRender(const KRE::WindowPtr& wm) override;
		float positionFromPixelPos(int px);

		bool handleMouseMotion(bool claimed, const point& p, unsigned keymod, bool in_rect) override;
		bool handleMouseButtonUp(bool claimed, const point& p, unsigned buttons, unsigned keymod, bool in_rect) override;
		bool handleMouseButtonDown(bool claimed, const point& p, unsigned buttons, unsigned keymod, bool in_rect) override;	
		bool handleMouseWheel(bool claimed, const point& p, const point& delta, int direction, bool in_rect) override;
		
		bool handleKeyDown(bool claimed, const SDL_Keysym& keysym, bool repeat, bool pressed) override;
		bool handleKeyUp(bool claimed, const SDL_Keysym& keysym, bool repeat, bool pressed) override;
		float min_range_;
		float max_range_;
		float step_;
		float position_;
		on_change_fn on_change_;
		rect loc_;
		bool pos_changed_;
		bool dragging_;

		std::shared_ptr<KRE::Attribute<KRE::vertex_texcoord>> attr_;
		std::vector<rectf> tex_coords_;
	};
}
