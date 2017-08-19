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

#include <vector>

#include "geometry.hpp"
#include "random.hpp"
#include "widget.hpp"

namespace gui
{
	class PolyLineWidget : public Widget
	{
	public:
		PolyLineWidget(const std::vector<point>& points, const KRE::Color& c, float width=1.0);
		PolyLineWidget(const point& p1, const point& p2, const KRE::Color& c, float width=1.0);
		PolyLineWidget(const variant& v, game_logic::FormulaCallable* e);
		virtual ~PolyLineWidget();
		void addPoint(const glm::vec2& p);
		WidgetPtr clone() const override;
	private:
		DECLARE_CALLABLE(PolyLineWidget);

		bool handleEvent(const SDL_Event& event, bool claimed) override;
		void handleDraw() const override;

		void calcCoords();
		
		KRE::Color color_;
		float width_;
		std::vector<glm::vec2> points_;	
	};
	typedef ffl::IntrusivePtr<PolyLineWidget> PolyLineWidgetPtr;
}
