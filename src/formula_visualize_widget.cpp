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

#include "Canvas.hpp"

#include "dialog.hpp"
#include "formula_visualize_widget.hpp"
#include "label.hpp"
#include "poly_line_widget.hpp"
#include "string_utils.hpp"
#include "text_editor_widget.hpp"

namespace gui 
{
	using namespace game_logic;

	namespace 
	{
		class ExpressionWidget : public gui::Dialog
		{
		public:
			explicit ExpressionWidget(game_logic::ConstExpressionPtr expression,
									   int x, int y, int w, int h, bool focused,
									   TextEditorWidget* editor,
									   std::function<void()> onClick)
			  : Dialog(x, y, w, h), 
				expression_(expression), 
				focused_(focused),
				editor_(editor), 
				on_click_(onClick)
			{
				init();
			}

			void init() {
				KRE::Color text_color(focused_ ? "yellow" : "white");
				gui::Label* Label = new gui::Label(expression_->name(), text_color);
				addWidget(gui::WidgetPtr(Label), width()/2 - Label->width()/2, 10);

				Label = new gui::Label(expression_->queryVariantType()->to_string(), text_color);
				addWidget(gui::WidgetPtr(Label), width()/2 - Label->width()/2, 26);

				std::string s = expression_->str();
				s.erase(std::remove_if(s.begin(), s.end(), util::c_isspace), s.end());
				if(s.size() > 13) {
					s.resize(10);
					s += "...";
				}

				Label = new gui::Label(s, text_color);
				addWidget(gui::WidgetPtr(Label), width()/2 - Label->width()/2, 42);
			}

		private:
			bool handleEvent(const SDL_Event& event, bool claimed) override {
				if(event.type == SDL_MOUSEMOTION) {
					const SDL_MouseMotionEvent& motion = event.motion;
					const bool inWidget = motion.x >= x() && motion.x <= x() + width() && motion.y >= y() && motion.y <= y() + height();
					if(inWidget) {
						PinpointedLoc loc;
						expression_->debugPinpointLocation(&loc);
						editor_->highlight(TextEditorWidget::Loc(loc.begin_line-1, loc.begin_col-1), TextEditorWidget::Loc(loc.end_line-1, loc.end_col-1));
					}
				} else if(event.type == SDL_MOUSEBUTTONDOWN) {
					const SDL_MouseButtonEvent& ev = event.button;
					const bool inWidget = ev.x >= x() && ev.x <= x() + width() && ev.y >= y() && ev.y <= y() + height();
					if(inWidget) {
						on_click_();
						claimed = true;
						return true;
					}
				}

				return gui::Dialog::handleEvent(event, claimed);
			}

			void handleDraw() const override {
				gui::Dialog::handleDraw();
				auto canvas = KRE::Canvas::getInstance();
				canvas->drawHollowRect(rect(x(), y(), width(), height()), KRE::Color::colorWhite());
			}

			game_logic::ConstExpressionPtr expression_;
			bool focused_;
			TextEditorWidget* editor_;
			std::function<void()> on_click_;
		};

	}

	FormulaVisualizeWidget::FormulaVisualizeWidget(
	  game_logic::ExpressionPtr expr, int text_pos, int row, int col, int x, int y, int w, int h, TextEditorWidget* editor)
		: expression_(expr), text_pos_(text_pos), row_(row), col_(col),
		  editor_(editor)
	{
		setLoc(x, y);
		setDim(w, h);
		init();
	}

	void FormulaVisualizeWidget::init(game_logic::ConstExpressionPtr expr)
	{
		if(!expr) {
			expr = expression_;
		}

		children_.clear();

		int spacing = (width()*3)/4;

		addExpression(expr, x() + width()/2, y(), spacing);

		std::map<WidgetPtr, WidgetPtr> parents;

		for(const WidgetPtr& w : children_) {
			for(const Edge& edge : edges_) {
				if(edge.second == w) {
					parents[w] = edge.first;
					break;
				}
			}
		}

		for(const std::vector<WidgetPtr>& row : child_rows_) {
			bool needs_rebalance = false;
			for(unsigned n = 1; n < row.size(); ++n) {
				if(row[n-1]->x() + row[n-1]->width() >= row[n]->x() - 10) {
					needs_rebalance = true;
				}
			}

			if(needs_rebalance) {
				for(int n = 0; n != row.size(); ++n) {
					row[n]->setLoc(n*110, row[n]->y());
				}
			}
		}

		bool adjustment = true;
		while(adjustment) {
			adjustment = false;

			for(const std::vector<WidgetPtr>& row : child_rows_) {
				for(unsigned n = 0; n < row.size(); ++n) {
					WidgetPtr parent = parents[row[n]];
					if(!parent || parent->x() == row[n]->x()) {
						continue;
					}

					if(row[n]->x() < parent->x()) {
						if(n+1 == row.size()) {
							adjustment = true;
							row[n]->setLoc(parent->x(), row[n]->y());
						} else {
							if(row[n]->x() + row[n]->width() < row[n+1]->x()-10) {
								adjustment = true;
								row[n]->setLoc(row[n]->x()+1, row[n]->y());
							}
						}
					} else {
						if(n == 0) {
							adjustment = true;
							row[n]->setLoc(parent->x(), row[n]->y());
						} else {
							if(row[n]->x() > row[n-1]->x()+row[n-1]->width()+10) {
								adjustment = true;
								row[n]->setLoc(row[n]->x()-1, row[n]->y());
							}
						}
					}
				}
			}
		}

		for(const Edge& edge : edges_) {
			WidgetPtr parent = edge.first;
			WidgetPtr child = edge.second;
			children_.push_back(WidgetPtr(new PolyLineWidget(point(parent->x() + parent->width()/2, parent->y() + parent->height()), point(child->x() + child->width()/2, child->y()), KRE::Color::colorWhite())));
		}
	}

	void FormulaVisualizeWidget::onSelectExpression(game_logic::ConstExpressionPtr expr)
	{
		LOG_INFO("SELECT EXPR");
		children_.clear();
		child_rows_.clear();
		edges_.clear();
		init(expr);
	}

	void FormulaVisualizeWidget::addExpression(game_logic::ConstExpressionPtr expr, int x, int y, int spacing, int depth, WidgetPtr parent)
	{
		const bool focused = text_pos_ >= expr->debugLocInFile().first && text_pos_ <= expr->debugLocInFile().second;
		std::function<void()> on_click_expr = std::bind(&FormulaVisualizeWidget::onSelectExpression, this, expr);
		children_.push_back(WidgetPtr(new ExpressionWidget(expr, x, y, 100, 80, focused, editor_, on_click_expr)));
		if(child_rows_.size() <= static_cast<unsigned>(depth)) {
			child_rows_.resize(depth+1);
		}

		child_rows_[depth].push_back(children_.back());

		if(parent) {
			edges_.push_back(std::pair<WidgetPtr, WidgetPtr>(parent, children_.back()));
		}

		parent = children_.back();
		const std::vector<ConstExpressionPtr>& children = expr->queryChildren();
		for(int n = 0; n != children.size(); ++n) {
			const int xpos = children.size() == 1 ? x : (x - spacing/2 + (spacing*n)/(static_cast<int>(children.size())-1));
			addExpression(children[n], xpos, y + 100, spacing/static_cast<int>(children.size()), depth+1, parent);
		}
	}

	void FormulaVisualizeWidget::handleDraw() const
	{
		auto canvas = KRE::Canvas::getInstance();
		canvas->drawSolidRect(rect(x(), y(), width(), height()), KRE::Color(128,128,128,128));
		for(WidgetPtr w : children_) {
			w->draw();
		}
	}

	bool FormulaVisualizeWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(!claimed) {
			for(const WidgetPtr& child : children_) {
				claimed = child->processEvent(getPos(), event, claimed) || claimed;
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
