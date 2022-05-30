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

#include <boost/range/adaptor/reversed.hpp>
#include <functional>
#include <iostream>

#include "Canvas.hpp"
#include "ClipScope.hpp"
#include "WindowManager.hpp"

#include "controls.hpp"
#include "formula_callable_visitor.hpp"
#include "grid_widget.hpp"
#include "input.hpp"
#include "joystick.hpp"
#include "label.hpp"
#include "profile_timer.hpp"
#include "widget_factory.hpp"

namespace gui
{
	using std::placeholders::_1;

	Grid::Grid(int ncols)
	  : ncols_(ncols), col_widths_(ncols, 0),
		col_aligns_(ncols, ColumnAlign::LEFT), row_height_(0),
		selected_row_(-1), allow_selection_(false), must_select_(false),
		swallow_clicks_(false), hpad_(0), vpad_(0), show_background_(false),
		max_height_(-1), allow_highlight_(true), set_h_(0), set_w_(0),
		default_selection_(-1), draw_selection_highlight_(false)
	{
		setEnvironment();
		setDim(0,0);
	}

	Grid::Grid(const variant& v, game_logic::FormulaCallable* e)
		: ScrollableWidget(v, e), row_height_(v["row_height"].as_int(0)), selected_row_(-1),
		allow_selection_(false), must_select_(false),
		swallow_clicks_(false), hpad_(0), vpad_(0), show_background_(false),
		max_height_(-1), allow_highlight_(true), set_h_(0), set_w_(0),
		default_selection_(v["default_select"].as_int(-1)),
		draw_selection_highlight_(v["draw_selection_highlighted"].as_bool(false))
	{
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
		if(v.has_key("on_select")) {
			const variant on_select_value = v["on_select"];
			if(on_select_value.is_function()) {
				ASSERT_LOG(on_select_value.min_function_arguments() <= 1 && on_select_value.max_function_arguments() >= 1, "on_select grid function should take 1 argument: " << v.debug_location());
				static const variant fml("fn(selection)");
				ffl_on_select_.reset(new game_logic::Formula(fml));

				game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
				callable->add("fn", on_select_value);

				select_arg_.reset(callable);
			} else {
				ffl_on_select_ = getEnvironment()->createFormula(on_select_value);
			}
			on_select_ = std::bind(&Grid::selectDelegate, this, _1);
		}
		if(v.has_key("on_mouseover")) {
			allow_selection_ = true;
			on_mouseover_ = std::bind(&Grid::mouseoverDelegate, this, _1);
			const variant on_mouseover_value = v["on_mouseover"];
			if(on_mouseover_value.is_function()) {
				ASSERT_LOG(on_mouseover_value.min_function_arguments() <= 1 && on_mouseover_value.max_function_arguments() >= 1, "on_mouseover grid function should take 1 argument: " << v.debug_location());
				static const variant fml("fn(selection)");
				ffl_on_mouseover_.reset(new game_logic::Formula(fml));

				game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
				callable->add("fn", on_mouseover_value);

				mouseover_arg_.reset(callable);
			} else {
				ffl_on_mouseover_ = getEnvironment()->createFormula(v["on_mouseover"]);
			}
		}

		ncols_ = v["columns"].as_int(1);
		if(v.has_key("column_widths")) {
			if(v["column_widths"].is_list()) {
				ASSERT_LOG(v["column_widths"].num_elements() == ncols_, "List of column widths must have " << ncols_ << " elements");
				std::vector<int> li = v["column_widths"].as_list_int();
				col_widths_.assign(li.begin(), li.end());
			} else if(v["column_widths"].is_int()) {
				col_widths_.assign(ncols_, v["column_widths"].as_int());
			} else {
				ASSERT_LOG(false, "grid: column_widths must be an int or list of ints");
			}
		} else {
			col_widths_.assign(ncols_, 0);
		}
		col_aligns_.resize(ncols_);
		if(v.has_key("column_alignments")) {
			if(v["column_alignments"].is_list()) {
				// XXX this could be a list of strings as well.
				int col = 0;
				for(const variant& c : v["column_alignments"].as_list()) {
					if(c.is_int()) {
						setAlign(col, static_cast<ColumnAlign>(c.as_int()));
					} else if(c.is_string()) {
						const std::string& s = c.as_string();
						if(s == "center" || s == "centre") {
							setAlign(col, ColumnAlign::CENTER);
						} else if(s == "right") {
							setAlign(col, ColumnAlign::RIGHT);
						} else if(s == "left") {
							setAlign(col, ColumnAlign::LEFT);
						} else {
							ASSERT_LOG(false, "grid: column_alignments must be \"left\", \"right\" or \"center\"");
						}
					} else {
						ASSERT_LOG(false, "grid: column alignment members must be an integer or a string.");
					}
					col++;
				}
			} else if(v["column_alignments"].is_int()) {
				col_aligns_.assign(ncols_, static_cast<ColumnAlign>(v["column_alignments"].as_int()));
			} else if(v["column_alignments"].is_string()) {
				const std::string& s = v["column_alignments"].as_string();
				if(s == "center" || s == "centre") {
					col_aligns_.assign(ncols_, ColumnAlign::CENTER);
				} else if(s == "right") {
					col_aligns_.assign(ncols_, ColumnAlign::RIGHT);
				} else if(s == "left") {
					col_aligns_.assign(ncols_, ColumnAlign::LEFT);
				} else {
					ASSERT_LOG(false, "grid: column_alignments must be \"left\", \"right\" or \"center\"");
				}
			} else {
				ASSERT_LOG(false, "grid: column_alignments must be an int or list of ints");
			}
		} else {
			col_aligns_.assign(ncols_, ColumnAlign::LEFT);
		}

		allow_selection_ = v["allow_selection"].as_bool(allow_selection_);
		if(v.has_key("must_select")) {
			must_select_ = v["must_select_"].as_bool();
			if(v.has_key("must_select_row")) {
				selected_row_ = v["must_select_row"].as_int();
			}
		}
		if(v.has_key("swallow_clicks")) {
			swallow_clicks_ = v["swallow_clicks"].as_bool();
		}
		if(v.has_key("max_height")) {
			max_height_ = v["max_height"].as_int();
		}
		if(v.has_key("allow_draw_highlight")) {
			allow_highlight_ = v["allow_draw_highlight"].as_bool();
		}
		if(v.has_key("header_rows")) {
			if(v["header_rows"].is_int()) {
				setHeaderRow(v["header_rows"].as_int());
			} else if(v["header_rows"].is_list()) {
				header_rows_.assign(v["header_rows"].as_list_int().begin(), v["header_rows"].as_list_int().end());
			} else {
				ASSERT_LOG(false, "grid: header_rows must be an int or list of ints");
			}
		}
		if(v.has_key("horizontal_padding")) {
			setHpad(v["horizontal_padding"].as_int());
		}
		if(v.has_key("vertical_padding")) {
			vpad_ = v["vertical_padding"].as_int();
		}
		if(v.has_key("show_background")) {
			show_background_ = v["show_background"].as_bool();
		}

		if(v.has_key("children")) {
			// children is a list of lists or a list of single widgets, the outmost list being rows,
			// the inner list being the columns.
			resetContents(v["children"]);
		}

		set_h_ = height();
		set_w_ = width();

		if(v["scroll_to_bottom"].as_bool(false) && getVirtualHeight() > height()) {
			setYscroll(getVirtualHeight() - height());
		}

		if(on_select_ && default_selection_ >= 0) {
			on_select_(default_selection_);
		}

		if(!ffl_on_select_ && !ffl_on_mouseover_) {
			setClaimMouseEvents(v["claim_mouse_events"].as_bool(false));
		}
	}

	void Grid::setBgColor(const KRE::Color& col)
	{
		bg_color_.reset(new KRE::Color(col));
	}

	void Grid::setFocusColor(const KRE::Color& col)
	{
		focus_color_.reset(new KRE::Color(col));
	}

	void Grid::setDim(int w, int h)
	{
		Widget::setDim(w,h);
		set_h_ = h;
		set_w_ = w;
	}

	void Grid::handleProcess()
	{
		for(WidgetPtr w : cells_) {
			if(w != nullptr) {
				w->process();
			}
		}
		Widget::handleProcess();
	}

	void Grid::addRow(const std::vector<WidgetPtr>& widgets)
	{
		assert(widgets.size() == ncols_);
		int index = 0;
		for(const WidgetPtr& widget : widgets) {
			cells_.push_back(widget);

			if(widget && widget->width()+hpad_ > col_widths_[index]) {
				col_widths_[index] = widget->width()+hpad_;
			}

			if(widget && widget->height() + vpad_*2 > row_height_) {
				row_height_ = widget->height() + vpad_*2;
			}

			++index;
		}

		recalculateDimensions();
	}

	Grid& Grid::addCol(const std::string& str) {
		return addCol(WidgetPtr(new Label(str, KRE::Color::colorWhite())));
	}

	Grid& Grid::addCol(const WidgetPtr& widget) {
		new_row_.push_back(widget);
		if(new_row_.size() == ncols_) {
			addRow(new_row_);
			new_row_.clear();
		}
		return *this;
	}

	Grid& Grid::finishRow()
	{
		while(!new_row_.empty()) {
			addCol();
		}

		return *this;
	}

	Grid& Grid::setColWidth(int col, int width)
	{
		assert(col >= 0 && col < ncols_);
		col_widths_[col] = width;
		recalculateDimensions();
		return *this;
	}

	Grid& Grid::setAlign(int col, ColumnAlign align)
	{
		assert(col >= 0 && col < ncols_);
		col_aligns_[col] = align;
		recalculateDimensions();
		return *this;
	}

	Grid& Grid::setHpad(int pad)
	{
		hpad_ = pad;
		return *this;
	}

	Grid& Grid::setVpad(int pad)
	{
		vpad_ = pad;
		return *this;
	}

	void Grid::resetContents(const variant& v)
	{
		cells_.clear();
		if(v.is_null()) {
			return;
		}
		bool check_end = false;
		for(const variant& row : v.as_list()) {
			if(row.is_list()) {
				for(const variant& col : row.as_list()) {
					addCol(widget_factory::create(col,getEnvironment()));
				}
				finishRow();
			} else {
				addCol(widget_factory::create(row,getEnvironment()));
					//.finishRow();
				check_end = true;
			}
		}
		if(check_end && v.num_elements() % ncols_) {
			finishRow();
		}
	}

	void Grid::registerMouseoverCallback(Grid::callback_type ptr)
	{
		on_mouseover_ = ptr;
	}

	void Grid::registerSelectionCallback(Grid::callback_type ptr)
	{
		on_select_ = ptr;
	}

	void Grid::registerRowSelectionCallback(std::function<void()> ptr)
	{
		row_callbacks_.push_back(ptr);
	}

	int Grid::getRowAt(int xpos, int ypos) const
	{
		if(row_height_ == 0) {
			return -1;
		} else if(inWidget(xpos, ypos)) {
			return (ypos + getYscroll() - getPos().y) / row_height_;
		} else {
			return -1;
		}
	}

	void Grid::recalculateDimensions()
	{
		visible_cells_.clear();

		int w = 0;
		for(int width : col_widths_) {
			w += width;
		}

		int desired_height = row_height_*getNRows();
		setVirtualHeight(desired_height);
		setScrollStep(1);
		setArrowScrollStep(row_height_);

		if(max_height_ > 0 && desired_height > max_height_) {
			desired_height = max_height_;
		//	while(desired_height%row_height_) {
		//		--desired_height;
		//	}
		}

		if(set_h_ != 0 || set_w_ != 0) {
			Widget::setDim(set_w_ ? set_w_ : w, set_h_ ? set_h_ : desired_height);
		} else {
			Widget::setDim(w, desired_height);
		}

		int y = 0;
		for(int n = 0; n != getNRows(); ++n) {
			int x = 0;
			for(int m = 0; m != ncols_; ++m) {
				int align = 0;
				WidgetPtr widget = cells_[n*ncols_ + m];
				if(widget) {
					switch(col_aligns_[m]) {
					case ColumnAlign::LEFT:
						align = 0;
						break;
					case ColumnAlign::CENTER:
						align = (col_widths_[m] - widget->width())/2;
						break;
					case ColumnAlign::RIGHT:
						align = col_widths_[m] - widget->width();
						break;
					}

					widget->setLoc(x+align,y+row_height_/2 - widget->height()/2 - getYscroll());
					if(widget->y() + widget->height() > 0 && widget->y() < height()) {
						visible_cells_.push_back(widget);
						widget->setClipArea(rect(0, 0, width(), height()));
					}
					std::sort(visible_cells_.begin(), visible_cells_.end(), WidgetSortZOrder());
				}

				x += col_widths_[m];
			}

			y += row_height_;
		}

		updateScrollbar();
	}

	void Grid::visitValues(game_logic::FormulaCallableVisitor& visitor)
	{
		for(WidgetPtr& cell : cells_) {
			visitor.visit(&cell);
		}
	}

	void Grid::onSetYscroll(int old_value, int value)
	{
		recalculateDimensions();
	}

	void Grid::handleDraw() const
	{
		auto canvas = KRE::Canvas::getInstance();
		{
			//commented out until clipping is fixed.
			KRE::ClipScope::Manager clip_scope(rect(x() & ~1, y() & ~1, width(), height()), canvas->getCamera());

			if(show_background_) {
				canvas->drawSolidRect(rect(x(), y(), width(), height()), bg_color_ ? *bg_color_ : KRE::Color(50, 50, 50));
			}

			if(draw_selection_highlight_ && default_selection_ >= 0 && default_selection_ < getNRows()) {
				if(std::find(header_rows_.begin(), header_rows_.end(), default_selection_) == header_rows_.end()) {
					canvas->drawSolidRect(rect(x(),y()+row_height_*default_selection_ - getYscroll(),width(),row_height_),
						KRE::Color(0,0,255,128));
				}
			}

			if(allow_highlight_ && selected_row_ >= 0 && selected_row_ < getNRows()) {
				if(std::find(header_rows_.begin(), header_rows_.end(), selected_row_) == header_rows_.end()) {
					canvas->drawSolidRect(rect(x(),y()+row_height_*selected_row_ - getYscroll(),width(),row_height_),
						focus_color_ ? *focus_color_ : KRE::Color(255,0,0,128));
				}
			}

			for(const WidgetPtr& widget : visible_cells_) {
				if(widget) {
					widget->draw(x(), y());
				}
			}
		} //end of scope so clip_scope goes away.

		//KRE::ModelManager2D mm(x(), y(), getRotation(), getScale());
		ScrollableWidget::handleDraw();
	}

	bool Grid::handleEvent(const SDL_Event& event, bool claimed)
	{
		claimed = ScrollableWidget::handleEvent(event, claimed);
		if(claimed) {
			return claimed;
		}

		SDL_Event ev = event;
		//normalizeEvent(&ev);
		std::vector<WidgetPtr> cells = visible_cells_;
		for(WidgetPtr widget : boost::adaptors::reverse(cells)) {
			if(widget) {
				claimed = widget->processEvent(getPos(), ev, claimed);
			}
		}

		if(!claimed && ev.type == SDL_MOUSEWHEEL) {
			int mx, my;
			input::sdl_get_mouse_state(&mx, &my);
			if(inWidget(mx, my)) {
				if(ev.wheel.y > 0) {
					setYscroll(getYscroll() - 3*row_height_ < 0 ? 0 : getYscroll() - 3*row_height_);
					if(allow_selection_) {
						selected_row_ -= 3;
						if(selected_row_ < 0) {
							selected_row_ = 0;
						}
					}
				} else {
					int y3 = getYscroll() + 3*row_height_;
					setYscroll(getVirtualHeight() - y3 < height()
						? getVirtualHeight() - height()
						: y3);
					if(allow_selection_) {
						selected_row_ += 3;
						if(selected_row_ >= getNRows()) {
							selected_row_ = getNRows() - 1;
						}
					}
				}
				claimed = claimMouseEvents();
			}
		}

		if(!claimed && allow_selection_) {
			if(ev.type == SDL_MOUSEMOTION) {
				const SDL_MouseMotionEvent& e = ev.motion;
				int new_row = getRowAt(e.x,e.y);
				if(new_row != selected_row_) {
					selected_row_ = new_row;
					if(on_mouseover_) {
						on_mouseover_(new_row);
					}
				}
			} else if(ev.type == SDL_MOUSEBUTTONDOWN) {
				const SDL_MouseButtonEvent& e = ev.button;
				if(e.state == SDL_PRESSED) {
					const int row_index = getRowAt(e.x, e.y);
					LOG_INFO("SELECT ROW: " << row_index);
					if(row_index >= 0 && row_index < int(row_callbacks_.size()) &&
						row_callbacks_[row_index]) {
						LOG_INFO("ROW CB: " << row_index);
						row_callbacks_[row_index]();
					}

					default_selection_ = row_index;
					if(on_select_) {
						on_select_(row_index);
					}
				}
				if(swallow_clicks_) {
					LOG_INFO("SWALLOW CLICK");
					claimed = true;
				}
			}
		}

		if(!claimed && must_select_) {
			if(ev.type == SDL_KEYDOWN) {
				if(ev.key.keysym.sym == SDLK_UP) {
					setYscroll(getYscroll() - row_height_ < 0 ? 0 : getYscroll() - row_height_);
					if(selected_row_-- == 0) {
						selected_row_ = getNRows()-1;
						setYscroll(std::min(getVirtualHeight(),row_height_*getNRows()) - height());
					}
					claimed = true;
				} else if(ev.key.keysym.sym == SDLK_DOWN) {
					int y1 = getYscroll() + row_height_;
					setYscroll(std::min(getVirtualHeight(),row_height_*getNRows()) - y1 < height()
						? std::min(getVirtualHeight(),row_height_*getNRows()) - height()
						: y1);
					if(++selected_row_ == getNRows()) {
						setYscroll(0);
						selected_row_ = 0;
					}
					claimed = true;
				} else if(ev.key.keysym.sym == controls::get_keycode(controls::CONTROL_ATTACK)
					|| ev.key.keysym.sym == controls::get_keycode(controls::CONTROL_JUMP)) {
					if(on_select_) {
						on_select_(selected_row_);
					}
					claimed = true;
				}
			}
		}

		return claimed;
	}

	bool Grid::hasFocus() const
	{
		for(const WidgetPtr& w : cells_) {
			if(w && w->hasFocus()) {
				return true;
			}
		}

		return false;
	}

	void Grid::selectDelegate(int selection)
	{
		if(select_arg_) {
			using namespace game_logic;
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(select_arg_.get()));
			callable->add("selection", variant(selection));
			variant value = ffl_on_select_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else if(getEnvironment()) {
			game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable(getEnvironment());
			callable->add("selection", variant(selection));
			variant v(callable);
			variant value = ffl_on_select_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("Grid::selectDelegate() called without environment!");
		}
	}

	void Grid::mouseoverDelegate(int selection)
	{
		if(mouseover_arg_) {
			using namespace game_logic;
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(mouseover_arg_.get()));
			callable->add("selection", variant(selection));
			variant value = ffl_on_mouseover_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else if(getEnvironment()) {
			game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable(getEnvironment());
			callable->add("selection", variant(selection));
			variant v(callable);
			variant value = ffl_on_mouseover_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("Grid::mouseover_delegate() called without environment!");
		}
	}

	ConstWidgetPtr Grid::getWidgetById(const std::string& id) const
	{
		for(WidgetPtr w : cells_) {
			if(w) {
				WidgetPtr wx = w->getWidgetById(id);
				if(wx) {
					return wx;
				}
			}
		}
		return Widget::getWidgetById(id);
	}

	WidgetPtr Grid::getWidgetById(const std::string& id)
	{
		for(WidgetPtr w : cells_) {
			if(w) {
				WidgetPtr wx = w->getWidgetById(id);
				if(wx) {
					return wx;
				}
			}
		}
		return Widget::getWidgetById(id);
	}

	std::vector<WidgetPtr> Grid::getChildren() const
	{
		return cells_;
	}

	int show_grid_as_context_menu(GridPtr grid, WidgetPtr draw_widget)
	{
		std::vector<WidgetPtr> v;
		v.push_back(draw_widget);
		return show_grid_as_context_menu(grid, v);
	}

	int show_grid_as_context_menu(GridPtr grid, const std::vector<WidgetPtr> draw_widgets)
	{
		grid->setShowBackground(true);
		grid->allowSelection();
		grid->swallowClicks();
		int result = -1;
		bool quit = false;
		grid->registerSelectionCallback(
		  [&](int nitem) {
			result = nitem;
			quit = true;
		  }
		);

		int mousex = 0, mousey = 0;
		input::sdl_get_mouse_state(&mousex, &mousey);

		auto wnd = KRE::WindowManager::getMainWindow();
		const int max_x = wnd->width() - grid->width() - 6;
		const int max_y = wnd->height() - grid->height() - 6;

		grid->setLoc(std::min<int>(max_x, mousex), std::min<int>(max_y, mousey));

		while(!quit) {
			SDL_Event event;
			while(input::sdl_poll_event(&event)) {
				bool claimed = grid->processEvent(point(), event, false);

				if(claimed) {
					continue;
				}

				switch(event.type) {
				case SDL_KEYDOWN:
					if(event.key.keysym.sym != SDLK_ESCAPE) {
						break;
					}
				case SDL_MOUSEBUTTONDOWN:
				case SDL_QUIT:
					quit = true;
					break;
				}
			}

			for(WidgetPtr w : draw_widgets) {
				w->draw();
			}

			grid->draw();

			gui::draw_tooltip();

			wnd->swap();

			// XXX If the framerate is externally set this values needs to change.
			profile::delay(20);
		}

		return result;
	}

	WidgetPtr Grid::clone() const
	{
		Grid* g = new Grid(*this);
		g->cells_.clear();
		g->visible_cells_.clear();
		g->new_row_.clear();
		for(const auto& w : cells_) {
			if(w) {
				g->addCol(w->clone());
			}
		}
		return WidgetPtr(g);
	}

	BEGIN_DEFINE_CALLABLE(Grid, Widget)
		DEFINE_FIELD(children, "[builtin widget]")
			std::vector<variant> v;
			for(WidgetPtr w : obj.cells_) {
				v.emplace_back(w.get());
			}
			return variant(&v);
		DEFINE_SET_FIELD_TYPE("list")
			obj.resetContents(value);
			obj.finishRow();
			obj.recalculateDimensions();

		DEFINE_FIELD(child, "null")
			return variant();
		DEFINE_SET_FIELD_TYPE("map")
			obj.addCol(widget_factory::create(value, obj.getEnvironment())).finishRow();
			obj.recalculateDimensions();

		DEFINE_FIELD(selected_row, "int")
			return variant(obj.selected_row_);
	END_DEFINE_CALLABLE(Grid)

	void Grid::surrenderReferences(GarbageCollector* collector)
	{
		Widget::surrenderReferences(collector);

		for(WidgetPtr& w : cells_ ){
			collector->surrenderPtr(&w);
		}

		for(WidgetPtr& w : visible_cells_ ){
			collector->surrenderPtr(&w);
		}

		for(WidgetPtr& w : new_row_ ){
			collector->surrenderPtr(&w);
		}
	}
}
