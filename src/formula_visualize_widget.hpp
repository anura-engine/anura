#ifndef FORMULA_VISUALIZE_WIDGET_HPP_INCLUDED
#define FORMULA_VISUALIZE_WIDGET_HPP_INCLUDED

#include <boost/intrusive_ptr.hpp>

#include <vector>

#include "formula_function.hpp"
#include "widget.hpp"

namespace gui {
	
class TextEditorWidget;

class formula_visualize_widget : public gui::widget
{
public:
	formula_visualize_widget(game_logic::expression_ptr expr, int text_pos, int row, int col, int x, int y, int w, int h, TextEditorWidget* editor);
private:
	void init(game_logic::const_expression_ptr expr=game_logic::const_expression_ptr());
	void handleDraw() const override;

	bool handleEvent(const SDL_Event& event, bool claimed) override;

	void on_select_expression(game_logic::const_expression_ptr expr);

	void add_expression(game_logic::const_expression_ptr expr, int x, int y, int spacing, int depth=0, WidgetPtr parent=WidgetPtr());

	game_logic::expression_ptr expression_;
	int text_pos_;
	int row_, col_;

	std::vector<WidgetPtr> children_;

	std::vector<std::vector<WidgetPtr> > child_rows_;

	typedef std::pair<WidgetPtr, WidgetPtr> Edge;
	std::vector<Edge> edges_;
	TextEditorWidget* editor_;
};

typedef boost::intrusive_ptr<formula_visualize_widget> formula_visualize_WidgetPtr;
typedef boost::intrusive_ptr<const formula_visualize_widget> const_formula_visualize_WidgetPtr;

}

#endif
