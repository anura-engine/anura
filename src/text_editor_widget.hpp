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

#include <boost/intrusive_ptr.hpp>

#include "scrollable_widget.hpp"

namespace gui 
{
	class TextEditorWidget;
	typedef boost::intrusive_ptr<TextEditorWidget> TextEditorWidgetPtr;

	class TextEditorWidget : public ScrollableWidget
	{
	public:
		TextEditorWidget(int width, int height=0);
		TextEditorWidget(const variant& v, game_logic::FormulaCallable* e);
		~TextEditorWidget();

		std::string text() const;
		void setText(const std::string& value, bool resetCursor=true);

		int getFontSize() const { return font_size_; }
		void setFontSize(int font_size);
		void changeFontSize(int amount);

		virtual void setDim(int w, int h);

		void undo();
		void redo();

		struct Loc {
			Loc(int r, int c) : row(r), col(c)
			{}
			bool operator==(const Loc& o) const { return row == o.row && col == o.col; }
			bool operator!=(const Loc& o) const { return !(*this == o); }
			bool operator<(const Loc& o) const { return row < o.row || row == o.row && col < o.col; }
			bool operator>(const Loc& o) const { return o < *this; }
			bool operator<=(const Loc& o) const { return operator==(o) || operator<(o); }
			bool operator>=(const Loc& o) const { return o <= *this; }
			int row, col;
		};

		const std::vector<std::string>& getData() const { return text_; }

		void setSearch(const std::string& term);
		void nextSearchMatch();
		bool hasSearchMatches() const { return search_matches_.empty() == false; }

		void replace(const std::string& replace_with);

		void setOnChangeHandler(std::function<void()> fn) { on_change_ = fn; }
		void setOnUserChangeHandler(std::function<void()> fn) { on_user_change_ = fn; }
		void setOnMoveCursorHandler(std::function<void()> fn) { onMoveCursor_ = fn; }
		void setOnEnterHandler(std::function<void()> fn) { on_enter_ = fn; }
		void setOnBeginEnterHandler(std::function<bool()> fn) { onBeginEnter_ = fn; }
		void setOnTabHandler(std::function<void()> fn) { on_tab_ = fn; }
		void setOnEscHandler(std::function<void()> fn) { on_escape_ = fn; }
		void setOnChangeFocusHandler(std::function<void(bool)> fn) { on_change_focus_ = fn; }

		bool hasFocus() const { return has_focus_; }
		void setFocus(bool value);

		int cursorRow() const { return cursor_.row; }
		int cursorCol() const { return cursor_.col; }

		void setCursor(int row, int col, bool move_selection=true);

		//convert a row/col cursor position to a position within the text()
		//string that is returned.
		int rowColToTextPos(int row, int col) const;

		std::pair<int,int> text_pos_to_row_col(int pos) const;

		void setHighlightLines(int begin, int end);
		void clearHighlightLines();

		std::pair<int, int> charPositionOnScreen(int row, int col) const;

		void setRowContents(int row, const std::string& value);

		void highlight(Loc begin, Loc end);

	protected:
		virtual void selectToken(const std::string& row, int& begin_row, int& end_row, int& begin_col, int& end_col);

		virtual void onChange();

		void handleDraw() const override;
		bool handleEvent(const SDL_Event& event, bool claimed) override;

		void saveUndoState();
		bool recordOp(const char* type=nullptr);

		std::pair<int, int> mousePositiontoRowCol(int x, int y) const;

		virtual void onMoveCursor(bool auto_shift=false);

	private:
		DECLARE_CALLABLE(TextEditorWidget);

		bool handleMouseButtonDown(const SDL_MouseButtonEvent& event);
		bool handleMouseButtonUp(const SDL_MouseButtonEvent& event);
		bool handleMouseMotion(const SDL_MouseMotionEvent& event);
		bool handleKeyPress(const SDL_KeyboardEvent& key);
		bool handleMouseWheel(const SDL_MouseWheelEvent& event);
		bool handleTextInput(const SDL_TextInputEvent& event);
		bool handleTextInputInternal(const char* text);
		bool handleTextEditing(const SDL_TextEditingEvent& event);

		void handlePaste(std::string txt);
		void handleCopy(bool mouse_based=false);

		virtual KRE::Color getCharacterColor(int row, int col) const;

		void deleteSelection();

		void onPageUp();
		void onPageDown();

		int findEquivalentCol(int old_col, int old_row, int new_row) const;

		void onSetYscroll(int old_pos, int new_pos);

		void refreshScrollbar();

		virtual TextEditorWidgetPtr clone() const;
		virtual void restore(const TextEditorWidget* state);

		const char* last_op_type_;

		std::vector<TextEditorWidgetPtr> undo_, redo_;

		std::vector<std::string> text_;

		size_t font_size_;
		int char_width_, char_height_;

		Loc select_, cursor_;

		int nrows_, ncols_;
		int scroll_pos_;

		//scroll pos for when we have a single row widget.
		int xscroll_pos_;

		int begin_highlight_line_, end_highlight_line_;
	
	bool editable_;
		bool has_focus_;
		bool is_dragging_;

		int last_click_at_, consecutive_clicks_;

		KRE::Color text_color_;

		std::string search_;
		std::vector<std::pair<Loc, Loc> > search_matches_;
		void calculateSearchMatches();

		void truncateColPosition();

		std::function<void()> on_change_, on_user_change_, onMoveCursor_, on_enter_, on_tab_, on_escape_;
		std::function<void(bool)> on_change_focus_;
		std::function<bool()> onBeginEnter_;

		void changeDelegate();
		void moveCursorDelegate();
		void enterDelegate();
		void tabDelegate();
		void escapeDelegate();
		void changeFocusDelgate(bool new_focus_value);
		bool beginEnterDelegate();

		game_logic::FormulaPtr ffl_on_change_;
		game_logic::FormulaPtr ffl_onMoveCursor_;
		game_logic::FormulaPtr ffl_on_enter_;
		game_logic::FormulaPtr ffl_on_tab_;
		game_logic::FormulaPtr ffl_on_escape_;
		game_logic::FormulaPtr ffl_on_change_focus_;
		game_logic::FormulaPtr ffl_onBeginEnter_;

		bool begin_enter_return_;

		int in_event_;

		bool password_entry_;
		bool no_border_;
		bool clear_on_focus_;

		KRE::ColorPtr bg_color_;
	};

}
