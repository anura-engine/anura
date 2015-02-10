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

#include <algorithm>
#include <sstream>

#include <boost/algorithm/string/replace.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include "Canvas.hpp"
#include "Font.hpp"

#include "dropdown_widget.hpp"
#include "filesystem.hpp"
#include "grid_widget.hpp"
#include "input.hpp"
#include "label.hpp"
#include "poly_line_widget.hpp"
#include "text_editor_widget.hpp"
#include "tree_view_widget.hpp"

namespace gui 
{
	TreeViewWidget::TreeViewWidget(int w, int h, const variant& tree)
		: tree_(tree), 
		hpad_(10), 
		col_size_(80), 
		font_size_(12), 
		selected_row_(-1), 
		char_height_(KRE::Font::charHeight(font_size_)), 
		allow_selection_(false),
		must_select_(false), 
		nrows_(0), 
		swallow_clicks_(false),
		max_height_(-1), 
		min_col_size_(20), 
		max_col_size_(80), 
		char_width_(KRE::Font::charWidth(font_size_)),
		persistent_highlight_(false), 
		highlight_color_(KRE::Color::colorBlue()), 
		highlighted_row_(-1)
	{
		row_height_ = KRE::Font::charHeight(font_size_);
		setEnvironment();
		Widget::setDim(w, h);
		init();
	}

	TreeViewWidget::TreeViewWidget(const variant& v, game_logic::FormulaCallable* e)
		: ScrollableWidget(v,e), 
		selected_row_(-1), 
		nrows_(0),
		min_col_size_(20), 
		max_col_size_(80),
		persistent_highlight_(false), 
		highlight_color_(KRE::Color::colorBlue()), 
		highlighted_row_(-1)
	{
		tree_ = v["child"];

		hpad_ = v["horizontal_padding"].as_int(10);
		col_size_ = v["column_size"].as_int(80);
		font_size_ = v["font_size"].as_int(12);
		allow_selection_ = v["allow_selection"].as_bool(false);
		must_select_ = v["must_select"].as_bool(false);
		max_height_ = v["max_height"].as_int(-1);

		char_height_ = KRE::Font::charHeight(font_size_);
		char_width_ = KRE::Font::charWidth(font_size_);
		row_height_ = KRE::Font::charHeight(font_size_);

		init();
	}

	void TreeViewWidget::init()
	{
		ASSERT_LOG(tree_.is_map() == true, "Tree passed to the TreeViewWidget must be a map object.");

		col_widths_.clear();
		col_widths_.push_back(col_size_/2);
		genTraverse(0, std::bind(&TreeViewWidget::calcColumnWidths, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), variant(), &tree_);

		LOG_INFO_NOLF("Column widths: ");
		for(int colw : col_widths_) {
			LOG_INFO_NOLF(colw << ", ");
		}
		LOG_INFO("");

		selection_map_.clear();
		widgets_.clear();
		last_coords_.clear();
		nrows_ = 0;
		traverse(0, col_size_/2, 0, &tree_, variant(), &tree_);
		recalculateDimensions();
	}

	int TreeViewWidget::traverse(int depth, int x, int y, variant* parent, const variant& key, variant* value)
	{
		std::vector<point> points;
		std::map<int,int>::iterator it = last_coords_.find(x);
		int last_y = y;
		if(it != last_coords_.end()) {
			last_y = it->second;
		}
		points.push_back(point(x/2, last_y));
		points.push_back(point(x/2, y+char_height_/2));
		points.push_back(point(x, y+char_height_/2 ));
		PolyLineWidgetPtr plw(new PolyLineWidget(points, KRE::Color::colorGray()));
		widgets_.push_back(plw);
		last_coords_[x] = y + char_height_/2;

		LabelPtr key_label;
		if(key.is_null() == false) {
			std::string str(key.as_string());
			// list or map don't need to trunate the key.
			if(!value->is_list() && !value->is_map()) {
				int max_chars = col_widths_[depth]/char_width_;
				if(str.length() > static_cast<unsigned>(max_chars && max_chars > 3)) {
					str = str.substr(0, max_chars-3) + "...";
				}
			}
			key_label.reset(new Label(str, KRE::Color::colorWhite(), font_size_));
			key_label->setLoc(x, y);
			key_label->setDim(col_widths_[depth], key_label->height());
			x += col_widths_[depth] + hpad_;
			widgets_.push_back(key_label);
		}
		if(value->is_null()) {
			LabelPtr null_label(new Label("<null>", KRE::Color::colorYellow(), font_size_));
			null_label->setLoc(x, y);
			null_label->setDim(col_widths_[depth], null_label->height());
			widgets_.push_back(null_label);
			y += widgets_.back()->height();
		} else if(value->is_int()) {
			std::stringstream ss;
			ss << value->as_int();
			LabelPtr int_label(new Label(ss.str(), KRE::Color::colorYellow(), font_size_));
			int_label->setLoc(x, y);
			int_label->setDim(col_widths_[depth], int_label->height());
			widgets_.push_back(int_label);
			y += widgets_.back()->height();
		} else if(value->is_decimal()) {
			std::stringstream ss;
			ss << value->as_decimal();
			LabelPtr decimal_label(new Label(ss.str(), KRE::Color::colorYellow(), font_size_));
			decimal_label->setLoc(x, y);
			decimal_label->setDim(col_widths_[depth], decimal_label->height());
			widgets_.push_back(decimal_label);
			y += widgets_.back()->height();
		} else if(value->is_string()) {
			std::string str(value->as_string());
			boost::replace_all(str, "\n", "\\n");
			int max_chars = (width()-x)/char_width_;
			if(str.length() > static_cast<unsigned>(max_chars) && max_chars > 3) {
				str = str.substr(0, max_chars-3) + "...";
			}
			LabelPtr string_label(new Label(str, KRE::Color::colorYellow(), font_size_));
			string_label->setLoc(x, y);
			string_label->setDim(col_widths_[depth], string_label->height());
			widgets_.push_back(string_label);
			y += widgets_.back()->height();
		} else if(value->is_list()) {
			if(key_label) {
				y += key_label->height();
				onTraverseElement(key, parent, value, nrows_);
			}
		
			for(int index = 0; index != value->as_list().size(); index++) {
				variant* item = value->get_index_mutable(index);
				y = traverse(depth+1, x, y, value, item->is_map() ? variant("<map>") : variant(), item);
			}
			last_coords_.erase(x);
		} else if(value->is_map()) {
			if(key_label) {
				y += key_label->height();
				onTraverseElement(key, parent, value, nrows_);
			}
			for(const variant& k : value->getKeys().as_list()) {
				y = traverse(depth+1, x, y, value, k, value->get_attr_mutable(k));
			}
			last_coords_.erase(x);
		} else if(value->is_bool()) {
			LabelPtr bool_label(new Label(value->as_bool() ? "true" : "false", KRE::Color::colorYellow(), font_size_));
			bool_label->setLoc(x, y);
			bool_label->setDim(col_widths_[depth], bool_label->height());
			widgets_.push_back(bool_label);
			y += widgets_.back()->height();
		}
		if(!value->is_map() && !value->is_list()) {
			onTraverseElement(key, parent, value, nrows_);
		}
		return y;
	}

	void TreeViewWidget::genTraverse(int depth, 
		std::function<void(int,const variant&,variant*)> fn, 
		const variant& key, 
		variant* value)
	{
		if(value->is_map()) {
			for(const variant& k : value->getKeys().as_list()) {
				genTraverse(depth + 1, fn, k, value->get_attr_mutable(k));
			}
		} else if(value->is_list()) {
			for(int index = 0; index != value->as_list().size(); index++) {
				genTraverse(depth + 1, fn, variant(), value->get_index_mutable(index));
			}
		} else if(value->is_string() || value->is_null() || value->is_int() || value->is_decimal() || value->is_bool()) {
			// skip so we just call fn() at the end.
		} else {
			LOG_INFO("genTraverse(): Ignored variant element: " << value->to_debug_string());
			return;
		}
		fn(depth, key, value);
	}

	void TreeViewWidget::calcColumnWidths(int depth, const variant& key, variant* value)
	{
		while(col_widths_.size() <= static_cast<unsigned>(depth)) {
			col_widths_.push_back(min_col_size_);
		}
		int str_chars = 0;
		if(value->is_string()) {
			str_chars = value->as_string().length();
		} else if(value->is_numeric()) {
			std::stringstream ss;
			ss << *value;
			str_chars = ss.str().length();
		} else if(value->is_null()) {
			str_chars = 6;  // "<null>"
		} else if(value->is_bool()) {
			str_chars = 5;	// length of "false".
		} else {
			str_chars = 999;	// Arbitrarily large value
		}
		if(key.is_null() == false) {
			if(int(key.as_string().length()*char_width_) > col_widths_[depth-1]) {
				col_widths_[depth-1] = key.as_string().length()*char_width_;
				if(col_widths_[depth-1] > max_col_size_) {
					col_widths_[depth-1] = max_col_size_;
				}
			}
			if(col_widths_[depth-1] < min_col_size_) {
				col_widths_[depth-1] = min_col_size_;
			}
			//col_widths_[depth-1] = std::min(min_col_size_, std::max(int(key.as_string().length()*char_width_), std::max(max_col_size_, col_widths_[depth-1])));
		}
		//col_widths_[depth] = std::min(min_col_size_, std::max(str_chars*char_width_, std::max(max_col_size_, col_widths_[depth])));
		if(str_chars*char_width_ > col_widths_[depth]) {
			col_widths_[depth] = str_chars*char_width_;
			if(col_widths_[depth] > max_col_size_) {
				col_widths_[depth] = max_col_size_;
			}
		}
		if(col_widths_[depth] < min_col_size_) {
			col_widths_[depth] = min_col_size_;
		}
	}

	void TreeViewWidget::onTraverseElement(const variant& key, variant* parent, variant* value, int row)
	{
		selection_map_[nrows_] = variant_pair(key, *value);
		nrows_++;
	}

	variant TreeViewWidget::get_selection_key(int selection) const
	{
		std::map<int, variant_pair>::const_iterator it = selection_map_.find(selection);
		if(it == selection_map_.end()) {
			LOG_INFO("Key not found for selection. " << selection);
			return variant("");
		}
		return it->second.first;
	}

	void TreeViewWidget::handleDraw() const
	{
		auto canvas = KRE::Canvas::getInstance();

		canvas->drawHollowRect(rect(x(), y(), width(), height()), KRE::Color::colorGray());

		const int offsx = (x()+2)&~1;
		int offsy = (y()+2)&~1;

		if(selected_row_ >= 0 && selected_row_ < getNRows()) {
			canvas->drawSolidRect(rect(offsx,row_height_*selected_row_ - getYscroll()+offsy,width()-2*offsx,row_height_-2*offsy), KRE::Color(255,0,0,128));
		}

		if(persistent_highlight_ && highlighted_row_ >= 0 && static_cast<int>(persistent_highlight_) < getNRows()) {
			canvas->drawSolidRect(rect(offsx,row_height_*highlighted_row_ - getYscroll()+offsy,width()-2*offsx,row_height_-2*offsy), highlight_color_);
		}
		offsy -= getYscroll() & ~1;

		for(const WidgetPtr& w : widgets_) {
			w->draw(offsx, offsy, getRotation(), getScale());
		}

		ScrollableWidget::draw();
	}

	int TreeViewWidget::getRowAt(int xpos, int ypos) const
	{
		if(row_height_ == 0) {
			return -1;
		} else if(xpos > x()+1 && xpos < x()-1 + width() &&
		   ypos > y()+1 && ypos < y()-1 + height()) {
			return (ypos + getYscroll() - y()-1) / row_height_;
		} else {
			return -1;
		}
	}

	void TreeViewWidget::recalculateDimensions()
	{
		int desired_height = row_height_*getNRows();
		setVirtualHeight(desired_height);
		setScrollStep(row_height_);
	
		if(max_height_ > 0 && desired_height > max_height_) {
			desired_height = max_height_;
			while(desired_height%row_height_) {
				--desired_height;
			}
		}

		for(const WidgetPtr& w : widgets_) {
			if(w->y() - getYscroll() >= 0 && w->y() + w->height() - getYscroll() < height()+2) {
				w->setVisible(true);
			} else {
				w->setVisible(false);
			}
		}
	
		updateScrollbar();
	}

	void TreeViewWidget::onSetYscroll(int old_value, int value)
	{
		recalculateDimensions();
	}


	bool TreeViewWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		claimed = ScrollableWidget::processEvent(event, claimed);

		rect r(x(), y(), width(), height());
		if(!claimed && allow_selection_) {
			if(event.type == SDL_MOUSEMOTION) {
				const SDL_MouseMotionEvent& e = event.motion;
				if(pointInRect(point(e.x, e.y), r)) {
					int new_row = getRowAt(e.x,e.y);
					if(new_row != selected_row_) {
						selected_row_ = new_row;
					}
				}
			} else if(event.type == SDL_MOUSEWHEEL) {
				int mx, my;
				input::sdl_get_mouse_state(&mx, &my);
				point p(mx, my);
				if(pointInRect(p, r)) {
					if(event.wheel.y < 0 ) {
						setYscroll(getYscroll() - 3*row_height_ < 0 ? 0 : getYscroll() - 3*row_height_);
						selected_row_ -= 3;
						if(selected_row_ < 0) {
							selected_row_ = 0;
						}
					} else {
						int y3 = getYscroll() + 3*row_height_;
						setYscroll(getVirtualHeight() - y3 < height() 
							? getVirtualHeight() - height()
							: y3);
						selected_row_ += 3;
						if(selected_row_ >= getNRows()) {
							selected_row_ = getNRows() - 1;
						}
					}
					claimed = claimMouseEvents();
				}
			} else if(event.type == SDL_MOUSEBUTTONDOWN) {
				const SDL_MouseButtonEvent& e = event.button;
				const int row_index = getRowAt(e.x, e.y);
				onSelect(e.button, row_index);
				if(swallow_clicks_) {
					claimed = true;
				}
			}
		}

		if(!claimed && must_select_) {
			if(event.type == SDL_KEYDOWN) {
				if(event.key.keysym.sym == SDLK_UP) {
					if(selected_row_-- == 0) {
						selected_row_ = getNRows()-1;
					}
					claimed = true;
				} else if(event.key.keysym.sym == SDLK_DOWN) {
					if(++selected_row_ == getNRows()) {
						selected_row_ = 0;
					}
					claimed = true;
				}
			}
		}

		SDL_Event ev = event;
		normalizeEvent(&ev);
		for(const WidgetPtr& widget : boost::adaptors::reverse(widgets_)) {
			if(widget) {
				if(widget->processEvent(ev, claimed)) {
					return true;
				}
			}
		}

		return claimed;
	}

	void TreeViewWidget::onSelect(Uint8 button, int selection)
	{
		if(persistent_highlight_) {
			highlighted_row_ = selection;
		}

		if(button == SDL_BUTTON_LEFT) {
			LOG_INFO("TREEVIEW SELECT ROW(edit): " << selection);
		if(on_select_) {
			on_select_(selection_map_[selection].first, selection_map_[selection].second);
		}
		} else if(button == SDL_BUTTON_RIGHT) {
			LOG_INFO("TREEVIEW SELECT ROW(context): " << selection);
		}
	}

	WidgetPtr TreeViewWidget::getWidgetById(const std::string& id)
	{
		for(const WidgetPtr& w : widgets_) {
			WidgetPtr wx = w->getWidgetById(id);
			if(wx) {
				return wx;
			}
		}
		return Widget::getWidgetById(id);
	}

	ConstWidgetPtr TreeViewWidget::getWidgetById(const std::string& id) const
	{
		for(const WidgetPtr& w : widgets_) {
			WidgetPtr wx = w->getWidgetById(id);
			if(wx) {
				return wx;
			}
		}
		return Widget::getWidgetById(id);
	}

	BEGIN_DEFINE_CALLABLE(TreeViewWidget, ScrollableWidget)
		DEFINE_FIELD(dummy, "null")
			return variant();
	END_DEFINE_CALLABLE(TreeViewWidget)

	TreeEditorWidget::TreeEditorWidget(int w, int h, const variant& tree)
		: TreeViewWidget(w,h,tree)
	{
		init();
	}

	TreeEditorWidget::TreeEditorWidget(const variant& v, game_logic::FormulaCallable* e)
		: TreeViewWidget(v,e)
	{
	}

	void TreeEditorWidget::onTraverseElement(const variant& key, variant* parent, variant* value, int row)
	{
		row_map_[row] = std::pair<variant*, variant*>(parent, value);
		TreeViewWidget::onTraverseElement(key, parent, value, row);
	}

	void TreeEditorWidget::handleDraw() const
	{
		TreeViewWidget::draw();

		if(context_menu_) {
			context_menu_->draw();
		}
		if(edit_menu_) {
			edit_menu_->draw();
		}
	}

	bool TreeEditorWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(edit_menu_ && edit_menu_->processEvent(event, claimed)) {
			return true;
		}

		if(context_menu_ && context_menu_->processEvent(event, claimed)) {
			return claimMouseEvents();
		}

		if(claimed || TreeViewWidget::processEvent(event, claimed)) {
			return claimMouseEvents();
		}
		return claimed;
	}

	void TreeEditorWidget::init()
	{
		row_map_.clear();
		context_menu_.reset();
		edit_menu_.reset();
		TreeViewWidget::init();
		if(tree_.is_map() && tree_.num_elements() == 0) {
			row_map_[0] = std::pair<variant*, variant*>((variant*)NULL, &tree_);
		}
	}

	void TreeEditorWidget::onSelect(Uint8 button, int selection)
	{
		if(button == SDL_BUTTON_RIGHT) {
			if(selection != -1) {
				std::map<int, std::pair<variant*, variant*> >::const_iterator it 
					= row_map_.find(selection);
				if(it == row_map_.end()) {
					selection = row_map_.end()->first;
				}
				variant* v = row_map_[selection].second;
				variant* parent_container = row_map_[selection].first;

				// Create a menu to select how to edit the item.
				Grid* g = new Grid(1);
				g->setShowBackground(true);
				g->allowSelection(true);
				g->swallowClicks(true);
				g->allowDrawHighlight(true);
				std::vector<std::string> choices;
				if(parent_container != NULL) {
					choices.push_back("Edit");
					if(parent_container && parent_container->is_map()) {
						choices.push_back("Edit Key");
					}
					choices.push_back("----------------");
					choices.push_back("Edit As: Integer");
					choices.push_back("Edit As: Decimal");
					choices.push_back("Edit As: Boolean");
					choices.push_back("Edit As: String");
					choices.push_back("----------------");
				}
				choices.push_back("Add Integer");
				choices.push_back("Add Decimal");
				choices.push_back("Add Boolean");
				choices.push_back("Add String");
				choices.push_back("Add List");
				choices.push_back("Add Map");
				if(parent_container != NULL) {
					choices.push_back("----------------");
					choices.push_back("Delete");
				}

				for(const std::string& str : choices) {
					g->addCol(LabelPtr(new Label(str)));
				}
				g->registerSelectionCallback(std::bind(&TreeEditorWidget::contextMenuHandler, this, selection, choices, std::placeholders::_1));
				int mousex, mousey;
				input::sdl_get_mouse_state(&mousex, &mousey);
				mousex -= x();
				mousey -= y();
				context_menu_.reset(g);
				int posy = y() + row_height_ * selection - getYscroll();
				if(posy + g->height() > y() + height()) {
					posy = y() + height() - posy + g->height();
				}
				context_menu_->setLoc(mousex, posy);
			}
		}
		TreeViewWidget::onSelect(button, selection);
	}

	void TreeEditorWidget::contextMenuHandler(int tree_selection, const std::vector<std::string>& choices, int menu_selection)
	{
		if(menu_selection < 0 || size_t(menu_selection) >= choices.size()) {
			if(context_menu_) {
				context_menu_.reset();
			}
			return;
		}
		LOG_INFO("Tree selection: " << tree_selection);
	
		// Menu seperators have a '-' character in the first position.
		if(choices[menu_selection][0] == '-') {
			return;
		}

		variant* v = row_map_[tree_selection].second;
		variant* parent_container = row_map_[tree_selection].first;

		if(choices[menu_selection] == "Delete") {
			if(parent_container->is_map()) {
				parent_container->remove_attr(get_selection_key(tree_selection));
			} else if(parent_container->is_list()) {
				std::vector<variant> new_list = parent_container->as_list();
				new_list.erase(std::remove(new_list.begin(), new_list.end(), *v), new_list.end());
			}
			init();
		} else if(choices[menu_selection] == "Edit Key") {
			gui::Grid* grid = new gui::Grid(1);
			grid->setShowBackground(true);
			grid->allowSelection(true);
			grid->swallowClicks(false);
			grid->allowDrawHighlight(false);
			TextEditorWidgetPtr editor = new TextEditorWidget(200, 28);
			editor->setFontSize(14);
			editor->setOnEnterHandler(std::bind(&TreeEditorWidget::executeKeyEditEnter, this, editor, parent_container, get_selection_key(tree_selection), v));
			editor->setOnTabHandler(std::bind(&TreeEditorWidget::executeKeyEditEnter, this, editor, parent_container, get_selection_key(tree_selection), v));
			editor->setOnEscHandler(std::bind(&TreeEditorWidget::init, this));
			editor->setText(get_selection_key(tree_selection).as_string());
			editor->setFocus(true);
			grid->addCol(editor);
			grid->registerSelectionCallback(std::bind(&TreeEditorWidget::executeKeyEditSelect, this, std::placeholders::_1));
			int mousex, mousey;
			input::sdl_get_mouse_state(&mousex, &mousey);
			mousex -= x();
			mousey -= y();
			edit_menu_.reset(grid);
			edit_menu_->setLoc(mousex, y() + row_height_ * tree_selection - getYscroll());
		} else if(choices[menu_selection].substr(0, 4) == "Edit") {
			std::string choice_type = choices[menu_selection].length() > 4 ? choices[menu_selection].substr(9) : "";
			if(choice_type == "Integer") {
				*v = variant(0);
			} else if(choice_type == "Decimal") {
				*v = variant(0.0);
			} else if(choice_type == "Boolean") {
				*v = variant::from_bool(false);
			} else if(choice_type == "String") {
				*v = variant("");
			}
			editField(tree_selection, v);
		} else if(choices[menu_selection].substr(0, 4) == "Add ") {
			if(v->is_list() || v->is_map()) {
				parent_container = v;
			}
			std::string choice_type = choices[menu_selection].length() > 4 ? choices[menu_selection].substr(4) : "";
			std::vector<variant> new_list;
			if(choice_type == "Integer") {
				if(parent_container->is_map()) {
					parent_container->add_attr(variant("a_new_integer"), variant(0));
				} else {
					new_list.push_back(variant(0));
					*parent_container = *parent_container + variant(&new_list);
				}
			} else if(choice_type == "Decimal") {
				if(parent_container->is_map()) {
					parent_container->add_attr(variant("a_new_decimal"), variant(0.0));
				} else {
					new_list.push_back(variant(0.0));
					*parent_container = *parent_container + variant(&new_list);
				}
			} else if(choice_type == "Boolean") {
				if(parent_container->is_map()) {
					parent_container->add_attr(variant("a_new_boolean"), variant::from_bool(false));
				} else {
					new_list.push_back(variant::from_bool(false));
					*parent_container = *parent_container + variant(&new_list);
				}
			} else if(choice_type == "String") {
				if(parent_container->is_map()) {
					parent_container->add_attr(variant("a_new_string"), variant(""));
				} else {
					new_list.push_back(variant("a string"));
					*parent_container = *parent_container + variant(&new_list);
				}
			} else if(choice_type == "List") {
				std::vector<variant> inner_list;
				inner_list.push_back(variant("a string"));
				new_list.push_back(variant(&inner_list));
				if(parent_container->is_map()) {
					parent_container->add_attr(variant("a_new_list"), variant(&new_list));
				} else {
					*parent_container = *parent_container + variant(&new_list);
				}
			} else if(choice_type == "Map") {
				std::map<variant, variant> new_map;
				new_map[variant("a_key")] = variant("a string");
				if(parent_container->is_map()) {
					parent_container->add_attr(variant("a_new_map"), variant(&new_map));
				} else {
					new_list.push_back(variant(&new_map));
					*parent_container = *parent_container + variant(&new_list);
				}
			}
			init();
		}
	}

	void TreeEditorWidget::editField(int row, variant* v)
	{
		if(context_menu_) {
			context_menu_.reset();
		}
		if(edit_menu_) {
			edit_menu_.reset();
		}

		std::map<variant::TYPE, WidgetPtr>::iterator it = ex_editor_map_.find(v->type());
		if(it != ex_editor_map_.end()) {
			if(on_editor_select_) {
				on_editor_select_(v, std::bind(&TreeEditorWidget::externalEditorSave, this, v, std::placeholders::_1));
			}
			edit_menu_ = it->second;
			return;
		}

		if(v->is_map() 
			|| v->is_list() 
			|| v->is_function() 
			|| v->is_callable() 
			|| v->is_null()) {
			return;
		}

		Grid* grid = new Grid(1);
		grid->setShowBackground(true);
		grid->allowSelection(true);
		grid->swallowClicks(false);
		grid->allowDrawHighlight(false);

		if(v->is_numeric() || v->is_string()) {
			TextEditorWidgetPtr editor = new TextEditorWidget(200, 28);
			editor->setFontSize(14);
			editor->setOnEnterHandler(std::bind(&TreeEditorWidget::executeEditEnter, this, editor, v));
			editor->setOnTabHandler(std::bind(&TreeEditorWidget::executeEditEnter, this, editor, v));
			editor->setOnEscHandler(std::bind(&TreeEditorWidget::init, this));
			std::stringstream ss;
			if(v->is_int()) {
				ss << v->as_int();
			} else if(v->is_decimal()) {
				ss << v->as_decimal();
			} else if(v->is_string()) {
				ss << v->as_string();
			}
			editor->setText(ss.str());
			editor->setFocus(true);
			grid->addCol(editor);
		} else if(v->is_bool()) {
			std::vector<std::string> bool_list;
			bool_list.push_back("false");
			bool_list.push_back("true");
			DropdownWidgetPtr bool_dd(new DropdownWidget(bool_list, 100, 30));
			bool_dd->setSelection(v->as_bool());
			bool_dd->setOnSelectHandler(std::bind(&TreeEditorWidget::onBoolChange, this, v, std::placeholders::_1, std::placeholders::_2));
			grid->addCol(bool_dd);
		}
		grid->registerSelectionCallback(std::bind(&TreeEditorWidget::executeEditSelect, this, std::placeholders::_1));

		int mousex, mousey;
		input::sdl_get_mouse_state(&mousex, &mousey);
		mousex -= x();
		mousey -= y();
		edit_menu_.reset(grid);
		edit_menu_->setLoc(mousex, y() + row_height_ * row - getYscroll());
	}

	void TreeEditorWidget::executeEditEnter(const TextEditorWidgetPtr editor, variant* value)
	{
		if(edit_menu_) {
			edit_menu_.reset();
		}

		if(editor->text().empty() == false) {
			std::stringstream ss(editor->text());
			if(value->is_int()) {
				int i;
				ss >> i;
				*value = variant(i);
			} else if(value->is_decimal()) {
				double f;
				ss >> f;
				*value = variant(f);
			} else if(value->is_string()) {
				*value = variant(editor->text());
			}
		}
		init();
	}

	void TreeEditorWidget::executeKeyEditEnter(const TextEditorWidgetPtr editor, variant* parent, const variant& key, variant* value)
	{
		if(editor->text().empty() == false) {
			if(edit_menu_) {
				edit_menu_.reset();
			}
			parent->add_attr(variant(editor->text()), (*parent)[key]);
			parent->remove_attr(key);

			init();
		}
	}

	void TreeEditorWidget::executeKeyEditSelect(int selection)
	{
		if(selection == -1 && edit_menu_) {
			edit_menu_.reset();
		}
	}

	void TreeEditorWidget::externalEditorSave(variant* v, const variant &new_value)
	{
		if(edit_menu_) {
			edit_menu_.reset();
		}
		*v = new_value;
		init();
	}

	void TreeEditorWidget::executeEditSelect(int selection)
	{
		if(selection == -1 && edit_menu_) {
			edit_menu_.reset();
		}
	}

	void TreeEditorWidget::onBoolChange(variant* v, int selection, const std::string& s)
	{
		if(edit_menu_) {
			edit_menu_.reset();
		}

		if(selection < 0 || selection > 1) {
			return;
		}
		*v = variant::from_bool(selection != 0);
		init();
	}

	BEGIN_DEFINE_CALLABLE(TreeEditorWidget, TreeViewWidget)
		DEFINE_FIELD(dummy, "null")
			return variant();
	END_DEFINE_CALLABLE(TreeEditorWidget)
}
