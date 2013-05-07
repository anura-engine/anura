#ifndef FORMULA_VISUALIZE_WIDGET_HPP_INCLUDED
#define FORMULA_VISUALIZE_WIDGET_HPP_INCLUDED

#include <boost/intrusive_ptr.hpp>

#include <vector>

#include "formula_function.hpp"
#include "widget.hpp"

namespace gui {
	
class text_editor_widget;

class formula_visualize_widget : public gui::widget
{
public:
	formula_visualize_widget(game_logic::expression_ptr expr, int text_pos, int row, int col, int x, int y, int w, int h, text_editor_widget* editor);
private:
	void init(game_logic::const_expression_ptr expr=game_logic::const_expression_ptr());
	void handle_draw() const;

	bool handle_event(const SDL_Event& event, bool claimed);

	void on_select_expression(game_logic::const_expression_ptr expr);

	void add_expression(game_logic::const_expression_ptr expr, int x, int y, int spacing, int depth=0, widget_ptr parent=widget_ptr());

	game_logic::expression_ptr expression_;
	int text_pos_;
	int row_, col_;

	std::vector<widget_ptr> children_;

	std::vector<std::vector<widget_ptr> > child_rows_;

	typedef std::pair<widget_ptr, widget_ptr> Edge;
	std::vector<Edge> edges_;
	text_editor_widget* editor_;
};

typedef boost::intrusive_ptr<formula_visualize_widget> formula_visualize_widget_ptr;
typedef boost::intrusive_ptr<const formula_visualize_widget> const_formula_visualize_widget_ptr;

}

#endif
