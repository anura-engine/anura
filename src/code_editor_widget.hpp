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

#include <map>
#include <vector>

#include "json_tokenizer.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"

namespace gui 
{
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
