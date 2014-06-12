/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef CODE_EDITOR_WIDGET_HPP_INCLUDED
#define CODE_EDITOR_WIDGET_HPP_INCLUDED

#include <map>
#include <vector>

#include "json_tokenizer.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"

namespace gui {

class code_editor_widget : public TextEditorWidget
{
public:
	code_editor_widget(int width, int height);
	code_editor_widget(const variant& v, game_logic::FormulaCallable* e);
	void onSliderMove(float value);

	const std::string& currentText() const { return currentText_; }
	struct ObjectInfo {
		int begin, end;
		variant obj;
		std::vector<json::Token> tokens;
	};

	ObjectInfo get_current_object() const;
	void set_highlight_current_object(bool value);

	//modifies the currently selected object to be equal to this new value.
	void modify_current_object(variant new_obj);

	void set_formula(bool val=true) { is_formula_ = true; }

private:
	ObjectInfo get_object_at(int row, int col) const;

	virtual void handleDraw() const override;
	virtual bool handleEvent(const SDL_Event& event, bool claimed) override;
	void selectToken(const std::string& row, int& begin_row, int& end_row, int& begin_col, int& end_col);
	void onChange();
	void onMoveCursor(bool auto_shift=false);
	KRE::Color getCharacterColor(int row, int col) const;

	std::vector<std::vector<KRE::Color>> colors_;

	//maps a location (a bracket or comma) to matching locations.
	std::map<std::pair<int, int>, std::vector<std::pair<int, int> > > bracket_match_;

	mutable SliderPtr slider_;
	int row_slider_, begin_col_slider_, end_col_slider_;
	bool slider_decimal_;
	int slider_magnitude_;

	struct SliderRange {
		SliderRange(float b, float e, decimal tb, decimal te)
		  : begin(b), end(e), target_begin(tb), target_end(te)
		{}
		float begin, end;
		decimal target_begin, target_end;
	};

	std::vector<SliderRange> slider_range_;
	std::vector<WidgetPtr> slider_labels_;

	void generate_tokens();
	std::string currentText_;
	variant current_obj_;
	std::vector<json::Token> tokens_;

	bool is_formula_;
};

typedef boost::intrusive_ptr<code_editor_widget> code_editor_WidgetPtr;

}

#endif
