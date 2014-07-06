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

#include "asserts.hpp"
#include "code_editor_widget.hpp"
#include "decimal.hpp"
#include "formatter.hpp"
#include "formula_tokenizer.hpp"
#include "json_parser.hpp"
#include "label.hpp"
#include "string_utils.hpp"
#include "utility_query.hpp"

#include <boost/regex.hpp>

#include <stack>

namespace gui
{
	code_editor_widget::code_editor_widget(int width, int height)
	  : TextEditorWidget(width, height),
	  row_slider_(0), begin_col_slider_(0), end_col_slider_(0),
	  slider_decimal_(false), slider_magnitude_(0), is_formula_(false)
	{
		setEnvironment();
	}

	code_editor_widget::code_editor_widget(const variant& v, game_logic::FormulaCallable* e) 
		: TextEditorWidget(v,e), row_slider_(0), begin_col_slider_(0), 
		end_col_slider_(0),	slider_decimal_(false), slider_magnitude_(0), is_formula_(false)
	{
	}

	void code_editor_widget::onMoveCursor(bool auto_shift)
	{
		TextEditorWidget::onMoveCursor(auto_shift);

		ObjectInfo info = get_current_object();
	}

	void code_editor_widget::onChange()
	{
		generate_tokens();

		bracket_match_.clear();
		colors_.clear();
		colors_.resize(colors_.size()+1);
		const std::string s = (is_formula_ ? "\"" : "") + text() + (is_formula_ ? "\"" : "");
		std::string::const_iterator i = s.begin();
		while(i != s.end()) {
			if(*i == '"') {
				std::vector<std::vector<std::pair<int, int> > > opening_brackets;

				if(!is_formula_) {
					colors_.back().push_back(KRE::Color(196, 196, 196));
				}
				++i;
				std::string::const_iterator end = i;
				while(end != s.end() && *end != '"') {
					if(*end == '\\') {
						++end;
					}

					++end;
				}

				if(end != s.end()) {
					while(i != end) {
						std::string::const_iterator begin = i;
						try {
							formula_tokenizer::token t = formula_tokenizer::get_token(i, end);

							bool error_color = false;
							switch(t.type) {
							case formula_tokenizer::TOKEN_LPARENS:
							case formula_tokenizer::TOKEN_LSQUARE:
							case formula_tokenizer::TOKEN_LBRACKET:
								opening_brackets.resize(opening_brackets.size()+1);
								opening_brackets.back().push_back(std::pair<int,int>(colors_.size()-1, colors_.back().size()));
								break;
							case formula_tokenizer::TOKEN_RPARENS:
							case formula_tokenizer::TOKEN_RSQUARE:
							case formula_tokenizer::TOKEN_RBRACKET:
								if(opening_brackets.empty()) {
									error_color = true;
								} else {
									opening_brackets.back().push_back(std::pair<int,int>(colors_.size()-1, colors_.back().size()));
									std::pair<int,int> key(colors_.size()-1, colors_.back().size());
									for(int n = 0; n != opening_brackets.back().size(); ++n) {
										bracket_match_[opening_brackets.back()[n]] = opening_brackets.back();
									}
									opening_brackets.pop_back();
								}
								break;
							case formula_tokenizer::TOKEN_COMMA:
								if(opening_brackets.empty() == false) {
									opening_brackets.back().push_back(std::pair<int,int>(colors_.size()-1, colors_.back().size()));
								}
								break;
							default:
								break;
							}

							if(t.type == formula_tokenizer::TOKEN_OPERATOR && util::c_isalpha(*t.begin)) {
								t.type = formula_tokenizer::TOKEN_KEYWORD;

							}
						
							while(begin != i) {
								if(*begin == '\n') {
									colors_.resize(colors_.size()+1);
								} else {
	static const KRE::Color TokenColors[] = {
		KRE::Color(128, 128, 255), //operator
		KRE::Color(64, 255, 64), //string literal
		KRE::Color(196, 196, 196), //const identifier
		KRE::Color(255, 255, 255), //identifier
		KRE::Color(255, 196, 196), //integer
		KRE::Color(255, 196, 196), //decimal
		KRE::Color(128, 128, 255), //lparens
		KRE::Color(128, 128, 255), //rparens
		KRE::Color(128, 128, 255), //lsquare
		KRE::Color(128, 128, 255), //rsquare
		KRE::Color(128, 128, 255), //lbracket
		KRE::Color(128, 128, 255), //rbracket
		KRE::Color(128, 128, 255), //comma
		KRE::Color(128, 128, 255), //semi
		KRE::Color(128, 128, 255), //colon
		KRE::Color(255, 255, 255), //whitespace
		KRE::Color(64, 255, 64), //keyword
		KRE::Color(64, 255, 64), //comment
		KRE::Color(255, 255, 255), //pointer
	};
									KRE::Color col(255, 255, 255);
									if(t.type >= 0 && t.type < sizeof(TokenColors)/sizeof(TokenColors[0])) {
										col = TokenColors[t.type];
									}

									if(error_color) {
										col = KRE::Color::colorRed();
									}

									colors_.back().push_back(col);
								}
								++begin;
							}

						} catch(formula_tokenizer::token_error&) {
							i = begin;
							break;
						}
					}

					for(int n = 0; n != opening_brackets.size(); ++n) {
						//any remaining brackets that weren't matched can be marked as errors.
						colors_[opening_brackets[n].front().first][opening_brackets[n].front().second] = KRE::Color::colorRed();
					}

					while(i != end) {
						//we might have bailed out of formula parsing early due to an error. Just treat
						//remaining text until the closing quotes as plain.
						if(*i == '\n') {
							colors_.resize(colors_.size()+1);
						} else {
							colors_.back().push_back(KRE::Color(196, 196, 196));
						}
						++i;
					}

					colors_.back().push_back(KRE::Color(196, 196, 196));
					i = end + 1;
				}


			} else if(*i == '\n') {
				colors_.resize(colors_.size()+1);
				++i;
			} else {
				colors_.back().push_back(KRE::Color::colorWhite());
				++i;
			}
		}

		TextEditorWidget::onChange();
	}

	KRE::Color code_editor_widget::getCharacterColor(int row, int col) const
	{
		std::map<std::pair<int, int>, std::vector<std::pair<int, int> > >::const_iterator itor = bracket_match_.find(std::pair<int,int>(row,col));
		if(itor != bracket_match_.end()) {
			for(int n = 0; n != itor->second.size(); ++n) {
				const int match_row = itor->second[n].first;
				const int match_col = itor->second[n].second;
				if(cursorRow() == match_row) {
					if(cursorCol() == match_col+1 || colors_[match_row].size() == match_col+1 && cursorCol() > match_col+1) {
						return KRE::Color::colorRed();
					}
				}
			}
		}

		ASSERT_LOG(row >= 0 && static_cast<unsigned>(row) < colors_.size(), "Invalid row: " << row << " /" << colors_.size());
		ASSERT_LOG(col >= 0 && static_cast<unsigned>(col) < colors_[row].size(), "Invalid col: " << col << " /" << colors_[row].size());
		return colors_[row][col];
	}

	void code_editor_widget::selectToken(const std::string& row, int& begin_row, int& end_row, int& begin_col, int& end_col)
	{
		std::pair<int,int> key(begin_row, begin_col);
		if(bracket_match_.count(key)) {
			begin_row = bracket_match_.find(key)->second.front().first;
			begin_col = bracket_match_.find(key)->second.front().second;
			end_row = bracket_match_.find(key)->second.back().first;
			end_col = bracket_match_.find(key)->second.back().second+1;
			return;
		}

		TextEditorWidget::selectToken(row, begin_row, end_row, begin_col, end_col);

		std::string token(row.begin() + begin_col, row.begin() + end_col);
	
		boost::regex numeric_regex("-?\\d+(\\.\\d+)?", boost::regex::perl);
		std::cerr << "token: (" << token << ")\n";
		if(boost::regex_match(token.c_str(), numeric_regex)) {

			const decimal current_value(decimal::from_string(token));
			if(current_value <= 10000000 && current_value >= -10000000) {
				using std::placeholders::_1;
				slider_.reset(new Slider(200, std::bind(&code_editor_widget::onSliderMove, this, _1)));
				slider_decimal_ = std::count(token.begin(), token.end(), '.') ? true : false;
				slider_magnitude_ = (abs(current_value.as_int())+1) * 5;
	
				const decimal slider_value = (current_value - decimal::from_int(-slider_magnitude_)) / decimal::from_int(slider_magnitude_*2);
				slider_->setPosition(static_cast<float>(slider_value.as_float()));

				slider_range_.clear();
				slider_labels_.clear();
				if(current_value > 0) {
					slider_range_.push_back(SliderRange(0.0f, 0.1f, -current_value*5, -current_value));
					slider_range_.push_back(SliderRange(0.1f, 0.2f, -current_value, decimal(0)));
					slider_range_.push_back(SliderRange(0.2f, 0.3f, decimal(0), current_value));
					slider_range_.push_back(SliderRange(0.3f, 0.5f, decimal(0), current_value));
					slider_range_.push_back(SliderRange(0.5f, 0.7f, current_value, 2*current_value));
					slider_range_.push_back(SliderRange(0.7f, 0.9f, 2*current_value, 5*current_value));
					slider_range_.push_back(SliderRange(0.9f, 1.0f, 5*current_value, 10*current_value));
					slider_range_.push_back(SliderRange(1.0f, 2.0f, 10*current_value, 20*current_value));
					slider_->setPosition(0.5f);
				} else {
					slider_range_.push_back(SliderRange(0.0f, 0.5f, current_value*2, decimal(0)));
					slider_range_.push_back(SliderRange(0.5f, 1.0f, decimal(0), -current_value*2));
					slider_range_.push_back(SliderRange(1.0f, 2.0f, -current_value*2, -current_value*4));
					slider_->setPosition(0.25f);
				}

				std::pair<int,int> pos = charPositionOnScreen(begin_row, (begin_col+end_col)/2);

				row_slider_ = begin_row;
				begin_col_slider_ = begin_col;
				end_col_slider_ = end_col;

				int x = pos.second - slider_->width()/2;
				int y = pos.first + 20 - slider_->height();
				if(x < 10) {
					x = 10;
				}

				if(x > width() - slider_->width()) {
					x = width() - slider_->width();
				}

				if(y < 20) {
					y += 60;
				}

				if(y > height() - slider_->height()) {
					y = height() - slider_->height();
				}
	
				slider_->setLoc(x, y);

				for(SliderRange& r : slider_range_) {
					slider_labels_.push_back(WidgetPtr(new gui::Label(formatter() << r.target_begin, 10)));
					slider_labels_.back()->setLoc(static_cast<int>(x + slider_->width()*r.begin) - slider_labels_.back()->width()/2, y);
				}
			}
		}
	}

	void code_editor_widget::onSliderMove(float value)
	{
		if(recordOp("slider")) {
			saveUndoState();
		}

		std::ostringstream s;

		decimal new_value;
		for(const SliderRange& r : slider_range_) {
			if(value <= r.end) {
				const float pos = (value - r.begin)/(r.end - r.begin);
				new_value = decimal(r.target_begin.as_float() + (r.target_end.as_float() - r.target_begin.as_float())*pos);
				break;
			}
		}

		if(slider_decimal_) {
			s << new_value;
		} else {
			s << new_value.as_int();
		}

		std::string new_string = s.str();

		ASSERT_LOG(row_slider_ >= 0 && static_cast<unsigned>(row_slider_) < getData().size(), "Illegal row value for Slider: " << row_slider_ << " / " << getData().size());
		std::string row = getData()[row_slider_];

		row.erase(row.begin() + begin_col_slider_, row.begin() + end_col_slider_);
		row.insert(row.begin() + begin_col_slider_, new_string.begin(), new_string.end());

		const int old_end = end_col_slider_;
		end_col_slider_ = begin_col_slider_ + new_string.size();

		if(cursorRow() == row_slider_ && cursorCol() == old_end) {
			setCursor(cursorRow(), end_col_slider_);
		}

		setRowContents(row_slider_, row);
	}

	void code_editor_widget::handleDraw() const
	{
		TextEditorWidget::handleDraw();

		if(slider_) {
			slider_->draw();
			for(WidgetPtr w : slider_labels_) {
				w->draw();
			}
		}
	}

	bool code_editor_widget::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(slider_) {
			if(slider_->processEvent(event, claimed)) {
				return true;
			}
		}

		if(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_KEYDOWN) {
			slider_.reset();
		}

		return TextEditorWidget::handleEvent(event, claimed) || claimed;
	}

	void code_editor_widget::generate_tokens()
	{
		currentText_ = text();

		try {
			current_obj_ = json::parse(currentText_);
		} catch(...) {
		}

		tokens_.clear();
		const char* begin = currentText_.c_str();
		const char* end = begin + currentText_.size();

		try {
			json::Token token = json::get_token(begin, end);
			while(token.type != json::Token::NUM_TYPES) {
				tokens_.push_back(token);
				token = json::get_token(begin, end);
			}
		} catch(json::TokenizerError& e) {
			std::cerr << "Tokenizer error: " << e.msg << "\n";
		}
	}

	namespace 
	{
		variant get_map_editing(int row, int col, variant item)
		{
			if(!item.get_debug_info()) {
				return variant();
			}

			const int begin_row = item.get_debug_info()->line;
			const int begin_col = item.get_debug_info()->column;
			const int end_row = item.get_debug_info()->end_line;
			const int end_col = item.get_debug_info()->end_column;

			typedef TextEditorWidget::Loc Loc;

			if(Loc(row,col) < Loc(begin_row,begin_col) ||
			   Loc(row,col) > Loc(end_row,end_col)) {
				return variant();
			}

			if(item.is_list()) {
				for(variant v : item.as_list()) {
					variant result = get_map_editing(row, col, v);
					if(result.is_null() == false) {
						return result;
					}
				}
			} else if(item.is_map()) {
				for(const variant_pair& p : item.as_map()) {
					variant result = get_map_editing(row, col, p.second);
					if(result.is_null() == false) {
						return result;
					}
				}

				return item;
			}

			return variant();
		}
	}

	code_editor_widget::ObjectInfo code_editor_widget::get_object_at(int row, int col) const
	{
		const int pos = rowColToTextPos(row, col);
		const char* ptr = currentText_.c_str() + pos;
		ASSERT_LOG(pos >= 0 && static_cast<unsigned>(pos) <= currentText_.size(), "Unexpected position in code editor widget: " << pos << " / " << currentText_.size());
		const json::Token* begin_token = NULL;
		const json::Token* end_token = NULL;
		std::stack<const json::Token*> begin_stack;
		int nbracket = 0;
		for(const json::Token& token : tokens_) {
			if(token.type == json::Token::TYPE_LCURLY) {
				begin_stack.push(&token);
			}

			if(token.type == json::Token::TYPE_RCURLY) {
				if(begin_stack.empty()) {
					return ObjectInfo();
				}

				if(begin_stack.top()->begin <= ptr && token.begin >= ptr) {
					begin_token = begin_stack.top();
					end_token = &token;
					break;
				} else {
					begin_stack.pop();
				}
			}
		}

		if(!begin_token || !end_token) {
			return ObjectInfo();
		}

		ObjectInfo result;
		result.begin = begin_token->begin - currentText_.c_str();
		result.end = end_token->end - currentText_.c_str();
		result.tokens = std::vector<json::Token>(begin_token, end_token+1);
		try {
			result.obj = get_map_editing(row, col, current_obj_);
		} catch(json::parse_error&) {
			std::cerr << "json parse error: " << std::string(begin_token->begin, end_token->end) << "\n";
			return result;
		}

		return result;
	}

	code_editor_widget::ObjectInfo code_editor_widget::get_current_object() const
	{
		return get_object_at(cursorRow(), cursorCol());
	}

	void code_editor_widget::set_highlight_current_object(bool value)
	{
		if(!value) {
			clearHighlightLines();
			return;
		}

		ObjectInfo info = get_current_object();
		if(info.obj.is_null() == false) {
			setHighlightLines(text_pos_to_row_col(info.begin).first,
								text_pos_to_row_col(info.end).first);
		} else {
			clearHighlightLines();
		}
	}

	void code_editor_widget::modify_current_object(variant new_obj)
	{
		ObjectInfo info = get_current_object();
		if(info.obj.is_null() || info.tokens.empty()) {
			return;
		}

		saveUndoState();


		const std::string str(currentText_.begin() + info.begin, currentText_.begin() + info.end);

		//calculate the indentation this object has based on the first attribute.
		std::string indent;
		std::string::const_iterator end_line = std::find(str.begin(), str.end(), '\n');
		if(end_line != str.end()) {
			++end_line;
			std::string::const_iterator end_indent = end_line;
			while(end_indent != str.end() && util::c_isspace(*end_indent)) {
				if(*end_indent == '\n') {
					end_line = end_indent+1;
				}
				++end_indent;
			}

			indent = std::string(end_line, end_indent);
		}

		const std::string new_str = modify_variant_text(str, info.obj, new_obj, info.obj.get_debug_info()->line, info.obj.get_debug_info()->column, indent);
		currentText_ = std::string(currentText_.begin(), currentText_.begin() + info.begin) + new_str + std::string(currentText_.begin() + info.end, currentText_.end());
		setText(currentText_, false /*don't move cursor*/);
	}
}
