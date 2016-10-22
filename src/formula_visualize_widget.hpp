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

#include "intrusive_ptr.hpp"

#include <vector>

#include "formula_function.hpp"
#include "widget.hpp"

namespace gui 
{
	class TextEditorWidget;

	class FormulaVisualizeWidget : public Widget
	{
	public:
		FormulaVisualizeWidget(game_logic::ExpressionPtr expr, int text_pos, int row, int col, int x, int y, int w, int h, TextEditorWidget* editor);
	private:
		void init(game_logic::ConstExpressionPtr expr=game_logic::ConstExpressionPtr());
		void handleDraw() const override;

		bool handleEvent(const SDL_Event& event, bool claimed) override;

		void onSelectExpression(game_logic::ConstExpressionPtr expr);

		void addExpression(game_logic::ConstExpressionPtr expr, int x, int y, int spacing, int depth=0, WidgetPtr parent=WidgetPtr());

		game_logic::ExpressionPtr expression_;
		int text_pos_;
		int row_, col_;

		std::vector<WidgetPtr> children_;

		std::vector<std::vector<WidgetPtr> > child_rows_;

		typedef std::pair<WidgetPtr, WidgetPtr> Edge;
		std::vector<Edge> edges_;
		TextEditorWidget* editor_;
	};

	typedef ffl::IntrusivePtr<FormulaVisualizeWidget> FormulaVisualizeWidgetPtr;
	typedef ffl::IntrusivePtr<const FormulaVisualizeWidget> ConstFormulaVisualizeWidgetPtr;
}
