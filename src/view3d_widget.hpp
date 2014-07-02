/*
	Copyright (C) 2003-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "button.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "grid_widget.hpp"
#include "variant.hpp"
#include "widget.hpp"

/// XXX This needs a serious amount of rethinking.

namespace gui
{
	class View3DWidget : public Widget
	{
	public:
		View3DWidget(int x, int y, int width, int height);
		View3DWidget(const variant& v, game_logic::FormulaCallable* e);
		virtual ~View3DWidget();
	private:
		DECLARE_CALLABLE(View3DWidget);

		void init();
		void resetContents(const variant& v);

		void handleProcess() override;
		void handleDraw() const override;
		bool handleEvent(const SDL_Event& event, bool claimed) override;

		std::vector<WidgetPtr> children_;

		View3DWidget();
		View3DWidget(const View3DWidget&);
	};

	typedef boost::intrusive_ptr<View3DWidget> View3DWidgetPtr;
}
