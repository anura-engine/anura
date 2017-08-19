/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <set>
#include "widget.hpp"

namespace gui 
{
	class LayoutWidget : public Widget
	{
	public:
		enum class LayoutType {
			ABSOLUTE,
			RELATIVE,
		};

		LayoutWidget(const variant& v, game_logic::FormulaCallable* e);
		virtual ~LayoutWidget();

		std::vector<WidgetPtr> getChildren() const override;

		void reflowChildren();

		WidgetPtr clone() const override;
	protected:
		variant handleWrite() override;
		void recalcLoc() override;
		void handleDraw() const override;
		bool handleEvent(const SDL_Event& event, bool claimed) override;
	private:
		DECLARE_CALLABLE(LayoutWidget);

		void visitValues(game_logic::FormulaCallableVisitor& visitor) override;

		LayoutType layout_type_;

		// If width is specified then we keep a track of it here.
		int fixed_width_;
		// If height is specified then we keep a track of it here.
		int fixed_height_;

		typedef std::set<WidgetPtr, WidgetSortZOrder> WidgetList;
		WidgetList children_;
	};
	typedef ffl::IntrusivePtr<LayoutWidget> LayoutWidgetPtr;
}
