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

#include <vector>

#include "grid_widget_fwd.hpp"
#include "scrollable_widget.hpp"
#include "widget.hpp"

namespace gui 
{
	enum class ColumnAlign {LEFT, CENTER, RIGHT };

	class Grid : public ScrollableWidget
	{
	public:
		typedef std::function<void (int)> callback_type;

		explicit Grid(int ncols);
		explicit Grid(const variant& v, game_logic::FormulaCallable* e);
		virtual ~Grid() {}
		Grid& setShowBackground(bool val) {
			show_background_ = val;
			return *this;
		}
		virtual void setDim(int w, int h) override;
		void addRow(const std::vector<WidgetPtr>& widgets);

		void setBgColor(const KRE::Color& col);
		void setFocusColor(const KRE::Color& col);

		Grid& addCol(const std::string& str);
		Grid& addCol(const WidgetPtr& widget=WidgetPtr());

		Grid& finishRow();

		Grid& setColWidth(int col, int width);
		Grid& setAlign(int col, ColumnAlign align);
		Grid& setHpad(int pad);
		void resetContents(const variant&);
		Grid& setVpad(int pad);
		void setHeaderRow(int row) { header_rows_.push_back(row); }

		void setDrawSelectionHighlight(bool val=true) { draw_selection_highlight_ = val; }
		void setDefaultSelection(int value) { default_selection_ = value; }
		int getDefaultSelection() const { return default_selection_; }
		void allowSelection(bool val=true) { allow_selection_ = val; }
		void mustSelect(bool val=true, int nrow=0) { must_select_ = val; selected_row_ = nrow; }
		bool hasMustSelect() const { return must_select_; }
		void swallowClicks(bool val=true) { swallow_clicks_ = val; }
		int selection() const { return selected_row_; }
		void registerMouseoverCallback(callback_type cb);
		void registerSelectionCallback(callback_type cb);
		void registerRowSelectionCallback(std::function<void()> cb);

		void setMaxHeight(int amount) { max_height_ = amount; }

		void onSetYscroll(int old_value, int value) override;

		void allowDrawHighlight(bool val=true) { allow_highlight_ = val; }

		bool hasFocus() const override;
		virtual WidgetPtr getWidgetById(const std::string& id) override;
		virtual ConstWidgetPtr getWidgetById(const std::string& id) const override;

		virtual std::vector<WidgetPtr> getChildren() const override;
		
		WidgetPtr clone() const override;
	protected:
		bool handleEvent(const SDL_Event& event, bool claimed) override;
		void handleDraw() const override;
		void handleProcess() override;

		void surrenderReferences(GarbageCollector* collector) override;

	private:
		DECLARE_CALLABLE(Grid);

		int getRowAt(int x, int y) const;
		void recalculateDimensions();

		void visitValues(game_logic::FormulaCallableVisitor& visitor) override;

		int getNRows() const { return static_cast<int>(cells_.size())/ncols_; }
		int ncols_;
		std::vector<WidgetPtr> cells_;
		std::vector<WidgetPtr> visible_cells_;
		std::vector<int> col_widths_;
		std::vector<ColumnAlign> col_aligns_;
		std::vector<int> header_rows_;
		int row_height_;
		int selected_row_;
		bool allow_selection_;
		bool must_select_;
		bool swallow_clicks_;
		bool allow_highlight_;
		int default_selection_;
		bool draw_selection_highlight_;

		// Explicitly set dimensions
		int set_w_;
		int set_h_;

		std::vector<WidgetPtr> new_row_;
		std::vector<std::function<void()> > row_callbacks_;
		callback_type on_mouseover_;
		callback_type on_select_;
		int hpad_, vpad_;
		bool show_background_;

		KRE::ColorPtr bg_color_, focus_color_;

		int max_height_;

		void selectDelegate(int selection);
		void mouseoverDelegate(int selection);

		game_logic::FormulaPtr ffl_on_select_;
		game_logic::FormulaPtr ffl_on_mouseover_;
		game_logic::FormulaCallablePtr select_arg_;
		game_logic::FormulaCallablePtr mouseover_arg_;
	};

	typedef ffl::IntrusivePtr<Grid> GridPtr;
	typedef ffl::IntrusivePtr<const Grid> ConstGridPtr;

	int show_grid_as_context_menu(GridPtr grid, WidgetPtr draw_widget);
	int show_grid_as_context_menu(GridPtr grid, const std::vector<WidgetPtr> draw_widgets);
}
