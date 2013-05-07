#include "color_chart.hpp"
#include "color_utils.hpp"
#include "dialog.hpp"
#include "foreach.hpp"
#include "formula_visualize_widget.hpp"
#include "label.hpp"
#include "poly_line_widget.hpp"
#include "raster.hpp"
#include "string_utils.hpp"
#include "text_editor_widget.hpp"

namespace gui {

using namespace game_logic;

namespace {

class expression_widget : public gui::dialog
{
public:
	explicit expression_widget(game_logic::const_expression_ptr expression,
	                           int x, int y, int w, int h, bool focused,
							   text_editor_widget* editor,
							   boost::function<void()> on_click)
	  : dialog(x, y, w, h), expression_(expression), focused_(focused),
	    editor_(editor), on_click_(on_click)
	{
		init();
	}

	void init() {
		graphics::color text_color(focused_ ? "yellow" : "white");
		gui::label* label = new gui::label(expression_->name(), text_color.as_sdl_color());
		add_widget(gui::widget_ptr(label), width()/2 - label->width()/2, 10);

		label = new gui::label(expression_->query_variant_type()->to_string(), text_color.as_sdl_color());
		add_widget(gui::widget_ptr(label), width()/2 - label->width()/2, 26);

		std::string s = expression_->str();
		s.erase(std::remove_if(s.begin(), s.end(), util::c_isspace), s.end());
		if(s.size() > 13) {
			s.resize(10);
			s += "...";
		}

		label = new gui::label(s, text_color.as_sdl_color());
		add_widget(gui::widget_ptr(label), width()/2 - label->width()/2, 42);
	}

private:
	bool handle_event(const SDL_Event& event, bool claimed) {
		if(event.type == SDL_MOUSEMOTION) {
			const SDL_MouseMotionEvent& motion = event.motion;
			const bool in_widget = motion.x >= x() && motion.x <= x() + width() && motion.y >= y() && motion.y <= y() + height();
			if(in_widget) {
				PinpointedLoc loc;
				expression_->debug_pinpoint_location(&loc);
				editor_->highlight(text_editor_widget::Loc(loc.begin_line-1, loc.begin_col-1), text_editor_widget::Loc(loc.end_line-1, loc.end_col-1));
			}
		} else if(event.type == SDL_MOUSEBUTTONDOWN) {
			const SDL_MouseButtonEvent& ev = event.button;
			const bool in_widget = ev.x >= x() && ev.x <= x() + width() && ev.y >= y() && ev.y <= y() + height();
			if(in_widget) {
				on_click_();
				claimed = true;
				return true;
			}
		}

		return gui::dialog::handle_event(event, claimed);
	}

	void handle_draw() const {
		gui::dialog::handle_draw();
		const SDL_Rect r = {x(), y(), width(), height()};
		const SDL_Color col = {255, 255, 255, 255};
		graphics::draw_hollow_rect(r, col);
	}

	game_logic::const_expression_ptr expression_;
	bool focused_;
	text_editor_widget* editor_;
	boost::function<void()> on_click_;
};

}

formula_visualize_widget::formula_visualize_widget(
  game_logic::expression_ptr expr, int text_pos, int row, int col, int x, int y, int w, int h, text_editor_widget* editor)
	: expression_(expr), text_pos_(text_pos), row_(row), col_(col),
	  editor_(editor)
{
	set_loc(x, y);
	set_dim(w, h);
	init();
}

void formula_visualize_widget::init(game_logic::const_expression_ptr expr)
{
	if(!expr) {
		expr = expression_;
	}

	children_.clear();

	int spacing = (width()*3)/4;

	add_expression(expr, x() + width()/2, y(), spacing);

	std::map<widget_ptr, widget_ptr> parents;

	foreach(const widget_ptr& w, children_) {
		foreach(const Edge& edge, edges_) {
			if(edge.second == w) {
				parents[w] = edge.first;
				break;
			}
		}
	}

	foreach(const std::vector<widget_ptr>& row, child_rows_) {
		bool needs_rebalance = false;
		for(int n = 1; n < row.size(); ++n) {
			if(row[n-1]->x() + row[n-1]->width() >= row[n]->x() - 10) {
				needs_rebalance = true;
			}
		}

		if(needs_rebalance) {
			for(int n = 0; n != row.size(); ++n) {
				row[n]->set_loc(n*110, row[n]->y());
			}
		}
	}

	bool adjustment = true;
	while(adjustment) {
		adjustment = false;

		foreach(const std::vector<widget_ptr>& row, child_rows_) {
			for(int n = 0; n < row.size(); ++n) {
				widget_ptr parent = parents[row[n]];
				if(!parent || parent->x() == row[n]->x()) {
					continue;
				}

				if(row[n]->x() < parent->x()) {
					if(n+1 == row.size()) {
						adjustment = true;
						row[n]->set_loc(parent->x(), row[n]->y());
					} else {
						if(row[n]->x() + row[n]->width() < row[n+1]->x()-10) {
							adjustment = true;
							row[n]->set_loc(row[n]->x()+1, row[n]->y());
						}
					}
				} else {
					if(n == 0) {
						adjustment = true;
						row[n]->set_loc(parent->x(), row[n]->y());
					} else {
						if(row[n]->x() > row[n-1]->x()+row[n-1]->width()+10) {
							adjustment = true;
							row[n]->set_loc(row[n]->x()-1, row[n]->y());
						}
					}
				}
			}
		}
	}

	foreach(const Edge& edge, edges_) {
		widget_ptr parent = edge.first;
		widget_ptr child = edge.second;
		children_.push_back(widget_ptr(new poly_line_widget(point(parent->x() + parent->width()/2, parent->y() + parent->height()), point(child->x() + child->width()/2, child->y()), graphics::color_white())));
	}
}

void formula_visualize_widget::on_select_expression(game_logic::const_expression_ptr expr)
{
	std::cerr << "SELECT EXPR\n";
	children_.clear();
	child_rows_.clear();
	edges_.clear();
	init(expr);
}

void formula_visualize_widget::add_expression(game_logic::const_expression_ptr expr, int x, int y, int spacing, int depth, widget_ptr parent)
{
	const bool focused = text_pos_ >= expr->debug_loc_in_file().first && text_pos_ <= expr->debug_loc_in_file().second;
	boost::function<void()> on_click_expr = boost::bind(&formula_visualize_widget::on_select_expression, this, expr);
	children_.push_back(widget_ptr(new expression_widget(expr, x, y, 100, 80, focused, editor_, on_click_expr)));
	if(child_rows_.size() <= depth) {
		child_rows_.resize(depth+1);
	}

	child_rows_[depth].push_back(children_.back());

	if(parent) {
		edges_.push_back(std::pair<widget_ptr, widget_ptr>(parent, children_.back()));
	}

	parent = children_.back();
	const std::vector<const_expression_ptr>& children = expr->query_children();
	for(int n = 0; n != children.size(); ++n) {
		const int xpos = children.size() == 1 ? x : (x - spacing/2 + (spacing*n)/(children.size()-1));
		add_expression(children[n], xpos, y + 100, spacing/children.size(), depth+1, parent);
	}
}

void formula_visualize_widget::handle_draw() const
{
	graphics::draw_rect(rect(x(), y(), width(), height()), graphics::color(128, 128, 128, 128));
	foreach(widget_ptr w, children_) {
		w->draw();
	}
}

bool formula_visualize_widget::handle_event(const SDL_Event& event, bool claimed)
{
	if(!claimed) {
		foreach(const widget_ptr& child, children_) {
			claimed = child->process_event(event, claimed) || claimed;
			if(claimed) {
				//we MUST break here as when claiming an event it might
				//modify children_.
				break;
			}
		}
	}

	return claimed;
}

}
