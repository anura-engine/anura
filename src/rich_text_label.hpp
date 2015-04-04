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

#include <string>
#include <vector>

#include "scrollable_widget.hpp"

namespace gui
{
	class RichTextLabel : public ScrollableWidget
	{
	public:
		RichTextLabel(const variant& v, game_logic::FormulaCallable* e);

		std::vector<WidgetPtr> getChildren() const;

		WidgetPtr clone() const override;
	private:
		DECLARE_CALLABLE(RichTextLabel)
		void handleProcess() override;
		void handleDraw() const override;
		bool handleEvent(const SDL_Event& event, bool claimed) override;

		std::vector<std::vector<WidgetPtr>> children_;
	};
}
