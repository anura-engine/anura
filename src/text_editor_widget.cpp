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

#include <boost/regex.hpp>

#include <algorithm>

#include "Blittable.hpp"
#include "Canvas.hpp"
#include "DisplayDevice.hpp"
#include "Font.hpp"
#include "WindowManager.hpp"

#include "asserts.hpp"
#include "clipboard.hpp"
#include "input.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "scoped_resource.hpp"
#include "string_utils.hpp"
#include "text_editor_widget.hpp"
#include "unit_test.hpp"

namespace gui 
{
	namespace 
	{
		const int BorderSize = 3;
		const int TabWidth = 4;
		const int TabAdjust = TabWidth - 1;

		typedef KRE::TexturePtr char_texture_ptr;
		std::vector<char_texture_ptr> char_textures;

		std::map<int, std::map<char, rectf>> all_char_to_area;

		std::string monofont()
		{
			return KRE::Font::get_default_monospace_font();
		}

		const rectf& get_char_area(int font_size, char c)
		{
			std::map<char, rectf>& char_to_area = all_char_to_area[font_size];
			auto i = char_to_area.find(c);
			if(i != char_to_area.end()) {
				return i->second;
			}

			const rectf& result = char_to_area[c];

			const int char_width = KRE::Font::charWidth(font_size, monofont());
			const int char_height = KRE::Font::charHeight(font_size, monofont());

			std::string str;
			int row = 0, col = 0;
			int nchars = 0;
			for(auto& i : char_to_area) {
				str.push_back(i.first);

				char_to_area[i.first] = rectf(static_cast<float>(col), 
					static_cast<float>(row), 
					static_cast<float>(char_width), 
					static_cast<float>(char_height));

				col += char_width;
				if(col >= 128 * char_width) {
					str += "\n";
					col = 0;
					row += char_height;
				}
			}

			auto char_texture = char_textures[font_size] = KRE::Font::getInstance()->renderText(str, KRE::Color::colorWhite(), font_size, true, monofont());

			for(auto& area : char_to_area) {
				area.second = rectf::from_coordinates(char_texture->getTextureCoordW(0, area.second.x1()),
					char_texture->getTextureCoordH(0, area.second.y1()),
					char_texture->getTextureCoordW(0, area.second.x2()),
					char_texture->getTextureCoordH(0, area.second.y2()));
			}

			return result;
		}

		void init_char_area(size_t font_size)
		{
			if(char_textures.size() <= font_size) {
				char_textures.resize(font_size+1);
			}

			if(char_textures[font_size].get()) {
				return;
			}

			std::map<char, rectf>& char_to_area = all_char_to_area[font_size];
			for(char c = 1; c < 127; ++c) {
				if(util::c_isprint(c) && c != 'a') {
					char_to_area[c] = rectf();
				}
			}

			get_char_area(font_size, 'a');
			ASSERT_LOG(char_textures[font_size].get(), "DID NOT INIT CHAR TEXTURE\n");
		}
	}

	TextEditorWidget::TextEditorWidget(int width, int height)
	  : last_op_type_(nullptr),
		font_size_(14),
		char_width_(KRE::Font::charWidth(font_size_, monofont())),
		char_height_(KRE::Font::charHeight(font_size_, monofont())),
		select_(0,0), 
		cursor_(0,0),
		nrows_((height - BorderSize*2)/char_height_),
		ncols_((width - 20 - BorderSize*2)/char_width_),
		scroll_pos_(0), 
		xscroll_pos_(0),
		begin_highlight_line_(-1), 
		end_highlight_line_(-1),
		has_focus_(false), 
		editable_(true), 
		is_dragging_(false),
		begin_enter_return_(true),
		last_click_at_(-1),
		consecutive_clicks_(0),
		text_color_(255, 255, 255, 255),
		in_event_(0),
		password_entry_(false),
		no_border_(false),
		clear_on_focus_(false)
	{
		setEnvironment();
		if(height == 0) {
			height = char_height_ + BorderSize*2;
			nrows_ = 1;
			ncols_ = (width - BorderSize*2)/char_width_;
			Widget::setDim(width, height);
		} else {
			Widget::setDim(width - 20, height);
		}

		text_.push_back("");

		init_clipboard();
	}

	TextEditorWidget::TextEditorWidget(const variant& v, game_logic::FormulaCallable* e)
		: ScrollableWidget(v,e), 
		  last_op_type_(nullptr), 
		  font_size_(14), 
		  select_(0,0), 
		  cursor_(0,0), 
		  scroll_pos_(0), 
		  xscroll_pos_(0),
		  begin_highlight_line_(-1), 
		  end_highlight_line_(-1),
		  has_focus_(v["focus"].as_bool(false)), 
		  editable_(v["editable"].as_bool(true)), 
		  is_dragging_(false),
		  begin_enter_return_(true),
		  last_click_at_(-1),
		  consecutive_clicks_(0),
		  text_color_(255, 255, 255, 255),
		  in_event_(0),
		  password_entry_(v["password"].as_bool(false)),
		  no_border_(v["no_border"].as_bool(false)),
		  clear_on_focus_(v["clear_on_focus"].as_bool(false))
	{
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");

		if(v.has_key("bg_color")) {
			bg_color_.reset(new KRE::Color(v["bg_color"]));
		}

		int width = v.has_key("width") ? v["width"].as_int() : 0;
		int height = v.has_key("height") ? v["height"].as_int() : 0;
		if(v.has_key("font_size")) { 
			font_size_ = v["font_size"].as_int(); 
		}
		if(v.has_key("color")) {
			text_color_ = KRE::Color(v["color"]);
		} else if(v.has_key("colour")) {
			text_color_ = KRE::Color(v["colour"]);
		}

		if(v.has_key("on_change")) {
			on_change_ = std::bind(&TextEditorWidget::changeDelegate, this);
			ffl_on_change_ = getEnvironment()->createFormula(v["on_change"]);
		}
		if(v.has_key("on_move_cursor")) {
			onMoveCursor_ = std::bind(&TextEditorWidget::moveCursorDelegate, this);
			ffl_onMoveCursor_ = getEnvironment()->createFormula(v["on_move_cursor"]);
		}
		if(v.has_key("on_enter")) {
			on_enter_ = std::bind(&TextEditorWidget::enterDelegate, this);
			ffl_on_enter_ = getEnvironment()->createFormula(v["on_enter"]);
		}
		if(v.has_key("on_tab")) {
			on_tab_ = std::bind(&TextEditorWidget::tabDelegate, this);
			ffl_on_tab_ = getEnvironment()->createFormula(v["on_tab"]);
		}
		if(v.has_key("on_escape")) {
			on_escape_ = std::bind(&TextEditorWidget::escapeDelegate, this);
			ffl_on_escape_ = getEnvironment()->createFormula(v["on_escape"]);
		}
		if(v.has_key("on_begin_enter")) {
			onBeginEnter_ = std::bind(&TextEditorWidget::beginEnterDelegate, this);
			ffl_onBeginEnter_ = getEnvironment()->createFormula(v["on_begin_enter"]);
		}
		if(v.has_key("on_change_focus")) {
			on_change_focus_ = std::bind(&TextEditorWidget::changeFocusDelgate, this, std::placeholders::_1);
			ffl_on_change_focus_ = getEnvironment()->createFormula(v["on_change_focus"]);
		}

		char_width_= KRE::Font::charWidth(font_size_, monofont());
		char_height_ = KRE::Font::charHeight(font_size_, monofont());
		nrows_ = (height - BorderSize*2)/char_height_;
		ncols_ = (width - 20 - BorderSize*2)/char_width_;

		if(height == 0) {
			height = char_height_ + BorderSize*2;
			nrows_ = 1;
			Widget::setDim(width - 20, height);
		} else {
			Widget::setDim(width - 20, height);
		}

		if(v.has_key("text") && v["text"].is_string()) {
			setText(v["text"].as_string());
		} else {
			text_.push_back("");
		}

		if(v["select_all"].as_bool(false)) {
			cursor_ = Loc(text_.size()-1, text_.back().size());
		}

		init_clipboard();
	}

	TextEditorWidget::~TextEditorWidget()
	{
	}

	std::string TextEditorWidget::text() const
	{
		std::string result;
		for(const std::string& line : text_) {
			result += line;
			result += "\n";
		}

		result.resize(result.size()-1);
		return result;
	}

	void TextEditorWidget::setRowContents(int row, const std::string& value)
	{
		ASSERT_LOG(row >= 0 && size_t(row) < text_.size(), "ILLEGAL ROW SET: " << row << " / " << text_.size());
		text_[row] = value;
		refreshScrollbar();
		onChange();
	}

	void TextEditorWidget::highlight(Loc begin, Loc end)
	{
		search_matches_.clear();

		for(int n = begin.row; n <= end.row && static_cast<unsigned>(n) < text_.size(); ++n) {
			int begin_col = 0;
			if(n == begin.row) {
				begin_col = begin.col;
			}

			int end_col = text_[n].size();
			if(n == end.row) {
				end_col = end.col;
			}

			Loc a(n, begin_col);
			Loc b(n, end_col);

			search_matches_.push_back(std::pair<Loc,Loc>(a, b));
		}
	}

	void TextEditorWidget::setText(const std::string& value, bool resetCursor)
	{
		const int current_in_event = in_event_;
		util::scope_manager event_recorder(
			[this]() { this->in_event_ = 0; },
			[this, current_in_event]() { this->in_event_ = current_in_event; }
		);

		std::string txt = value;
		txt.erase(std::remove(txt.begin(), txt.end(), '\r'), txt.end());
		text_ = util::split(txt, '\n', 0 /*don't remove empties or strip spaces*/);
		if(text_.empty()) {
			text_.push_back("");
		}

		if(resetCursor) {
			select_ = cursor_ = Loc(0,0);
			xscroll_pos_ = scroll_pos_ = 0;
		} else {
			if(static_cast<unsigned>(select_.row) >= text_.size()) {
				select_.row = text_.size() - 1;
			}

			if(static_cast<unsigned>(cursor_.row) >= text_.size()) {
				cursor_.row = text_.size() - 1;
			}
		}

		refreshScrollbar();
		onChange();
	}

	void TextEditorWidget::setFontSize(int font_size)
	{
		if(font_size < 6) {
			font_size = 6;
		} else if(font_size > 28) {
			font_size = 28;
		}

		font_size_ = font_size;

		char_width_ = KRE::Font::charWidth(font_size_, monofont());
		char_height_ = KRE::Font::charHeight(font_size_, monofont());
		nrows_ = (height() - BorderSize*2)/char_height_;
		ncols_ = (width() - BorderSize*2)/char_width_;

		refreshScrollbar();
	}

	void TextEditorWidget::changeFontSize(int amount)
	{
		setFontSize(font_size_ + amount);
	}

	void TextEditorWidget::setDim(int w, int h)
	{
		Widget::setDim(w - 20, h);

		nrows_ = (height() - BorderSize*2)/char_height_;
		ncols_ = (width() - BorderSize*2)/char_width_;

		refreshScrollbar();
	}

	namespace 
	{
		struct RectDraw {
			rect area;
			KRE::Color col;

			bool merge(RectDraw& o) {
				if(o.col != col) {
					return false;
				}

				if(o.area.y() != area.y() || o.area.x() > area.x() + area.w()) {
					return false;
				}

				area = rect(area.x(), area.y(), area.w() + o.area.w(), area.h());
				return true;
			}
		};
	}

	void TextEditorWidget::handleDraw() const
	{
		auto canvas = KRE::Canvas::getInstance();

		init_char_area(font_size_);

		std::vector<RectDraw> rects;
		std::map<uint32_t, std::vector<KRE::vertex_texcoord>> chars;

		int begin_build = profile::get_tick_time();

		const int xpos = x() + BorderSize;
		const int ypos = y() + BorderSize;

		const int begin_draw = profile::get_tick_time();

		if(bg_color_.get() != nullptr) {
			canvas->drawSolidRect(rect(x(), y(), width(), height()), *bg_color_);

		}

		for(const RectDraw& r : rects) {
			canvas->drawSolidRect(r.area, r.col);
		}

		int r = 0;
		for(int n = scroll_pos_; static_cast<unsigned>(n) < text_.size() && r < nrows_; ++n, ++r) {
			if(n >= begin_highlight_line_ && n <= end_highlight_line_) {
				RectDraw rect_draw = { rect(xpos, ypos + r*char_height_, width(), char_height_), KRE::Color(255, 255, 255, 32) };
				rects.push_back(rect_draw);
			}

			int c = 0;
			std::vector<std::pair<Loc, Loc> >::const_iterator search_itor = std::lower_bound(search_matches_.begin(), search_matches_.end(), std::pair<Loc,Loc>(Loc(n,0),Loc(n,0)));
			for(int m = xscroll_pos_; static_cast<unsigned>(m) < text_[n].size(); ++m, ++c) {
				if(c >= ncols_) {
					++r;
					c -= ncols_;
					if(r == nrows_) {
						break;
					}
				}

				const char ch = password_entry_ && !clear_on_focus_ ? '*' : text_[n][m];
				const int char_size = ch == '\t' ? 4 : 1;
				Loc pos(n, m);

				Loc begin_select = select_;
				Loc end_select = cursor_;
				if(end_select < begin_select) {
					std::swap(begin_select, end_select);
				}

				KRE::Color col = getCharacterColor(n, m);


				if(pos >= begin_select && pos < end_select) {
					RectDraw rect_draw = { rect(xpos + c*char_width_, ypos + r*char_height_, char_width_*char_size, char_height_), col };

					if(rects.empty() || !rects.back().merge(rect_draw)) {
						rects.push_back(rect_draw);
					}

					col = KRE::Color::colorBlack();
				} else {
					for(std::vector<std::pair<Loc,Loc> >::const_iterator i = search_itor; i != search_matches_.end() && i->first <= pos; ++i) {
						if(pos >= i->first && pos < i->second) {
							RectDraw rect_draw = { rect(xpos + c*char_width_, ypos + r*char_height_, char_width_*char_size, char_height_), KRE::Color(255,255,0,128) };
							if(rects.empty() || !rects.back().merge(rect_draw)) {
								rects.push_back(rect_draw);
							}

							col = KRE::Color::colorBlack();
						}
					}
				}

				if(!util::c_isspace(ch) && util::c_isprint(ch)) {
					const rectf& area = get_char_area(font_size_, ch);

					const int x1 = (xpos + c*char_width_) & preferences::xypos_draw_mask;
					const int y1 = (ypos + r*char_height_) & preferences::xypos_draw_mask;
					const int x2 = (x1 + char_width_) & preferences::xypos_draw_mask;
					const int y2 = (y1 + char_height_) & preferences::xypos_draw_mask;

					auto& queue = chars[col.asRGBA()];
					queue.emplace_back(glm::vec2(x1, y1), glm::vec2(area.x1(), area.y1()));
					queue.emplace_back(glm::vec2(x2, y1), glm::vec2(area.x2(), area.y1()));
					queue.emplace_back(glm::vec2(x2, y2), glm::vec2(area.x2(), area.y2()));

					queue.emplace_back(glm::vec2(x2, y2), glm::vec2(area.x2(), area.y2()));
					queue.emplace_back(glm::vec2(x1, y1), glm::vec2(area.x1(), area.y1()));
					queue.emplace_back(glm::vec2(x1, y2), glm::vec2(area.x1(), area.y2()));
					
					//canvas->drawSolidRect(rect::from_coordinates(x1, y1, x2, y2), KRE::Color::colorBlue());
					//canvas->blitTexture(char_textures[font_size_], area.as_type<int>(), 0, rect::from_coordinates(x1, y1, x2, y2), col);
				}

				if(cursor_.row == n && cursor_.col == m &&
				   (profile::get_tick_time()%500 < 350 || !has_focus_) &&
				   !clear_on_focus_) {
					RectDraw rect_draw = { rect(xpos + c*char_width_+1, ypos + r*char_height_, 1, char_height_), KRE::Color::colorWhite() };
					rects.push_back(rect_draw);
				}

				if(ch == '\t') {
					c += TabAdjust;
				}
			}

			if(has_focus_ && cursor_.row == n && static_cast<unsigned>(cursor_.col) >= text_[n].size() && profile::get_tick_time()%500 < 350) {
				RectDraw rect_draw = { rect(xpos + c*char_width_+1, ypos + r*char_height_, 1, char_height_), KRE::Color::colorWhite() };
				rects.push_back(rect_draw);
			}
		}

		if(no_border_ == false) {
			canvas->drawHollowRect(rect(x()+1, y()+1, width()-2, height()-2), has_focus_ ? KRE::Color::colorWhite() : KRE::Color::colorGray());
		}

		for(auto& c : chars) {
			if(c.second.size() > 0) {
				canvas->blitTexture(char_textures[font_size_], c.second, 0, KRE::Color(c.first));
			}
		}

		KRE::Canvas::ModelManager mm(x(), y(), getRotation(), getScale());
		ScrollableWidget::handleDraw();
	}

	bool TextEditorWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		util::scope_manager event_recorder(
			[this]() { this->in_event_++; },
			[this]() { this->in_event_--; }
		);

		if(!claimed) {
			claimed = clipboard_handleEvent(event);
		}

		claimed = ScrollableWidget::handleEvent(event, claimed) || claimed;

		switch(event.type) {
		case SDL_KEYDOWN:
			return handleKeyPress(event.key) || claimed;
		case SDL_MOUSEBUTTONDOWN:
			return handleMouseButtonDown(event.button) || claimed;
		case SDL_MOUSEBUTTONUP:
			return handleMouseButtonUp(event.button) || claimed;
		case SDL_MOUSEMOTION:
			return handleMouseMotion(event.motion) || claimed;
		case SDL_MOUSEWHEEL:
			return handleMouseWheel(event.wheel) || claimed;
		case SDL_TEXTINPUT:
			return handleTextInput(event.text) || claimed;
		case SDL_TEXTEDITING:
			return handleTextEditing(event.edit) || claimed;
		}

		return false;
	}

	bool TextEditorWidget::handleMouseWheel(const SDL_MouseWheelEvent& event)
	{
		int mx, my;
		input::sdl_get_mouse_state(&mx, &my);
		if(mx >= x() && mx < x() + width() && my >= y() && my < y() + height()) {
			if(event.y > 0) {
				if(cursor_.row > 2) {
					cursor_.row -= 3;
					scroll_pos_ -= 3;
					if( scroll_pos_ < 0 ){ 
						scroll_pos_ = 0; 
					}
					cursor_.col = findEquivalentCol(cursor_.col, cursor_.row+3, cursor_.row);
					onMoveCursor();
				}
				return true;
			} else {
				if(text_.size() > 2 && static_cast<unsigned>(cursor_.row) < text_.size()-3) {
					cursor_.row += 3;
					scroll_pos_ += 3;
					if(static_cast<unsigned>(scroll_pos_) > text_.size()){ 
						scroll_pos_ = text_.size(); 
					}
					cursor_.col = findEquivalentCol(cursor_.col, cursor_.row-3, cursor_.row);
					onMoveCursor();
				}
				return true;
			}
		}
		return false;
	}

	void TextEditorWidget::setFocus(bool value)
	{
		if(has_focus_ != value && on_change_focus_) {
			on_change_focus_(value);
		}
		has_focus_ = value;

		if(clear_on_focus_) {
			setText("");
			clear_on_focus_ = false;
		}

		if(nrows_ == 1 && value) {
			cursor_ = Loc(0, text_.front().size());
			select_ = Loc(0, 0);
			onMoveCursor();
		}
	}

	void TextEditorWidget::setCursor(int row, int col, bool move_selection)
	{
		if(row < 0) {
			row = 0;
		}

		if(col < 0) {
			col = 0;
		}

		if(static_cast<unsigned>(row) >= text_.size()) {
			row = text_.size() - 1;
		}

		if(static_cast<unsigned>(col) > text_[row].size()) {
			col = text_[row].size();
		}

		cursor_ = Loc(row, col);
	
		if(move_selection) {
			select_ = cursor_;
		}

		onMoveCursor();
	}

	int TextEditorWidget::rowColToTextPos(int row, int col) const
	{
		if(static_cast<unsigned>(col) > text_[row].size()) {
			col = text_[row].size();
		}

		int result = 0;
		for(unsigned n = 0; n != row; ++n) {
			result += text_[n].size() + 1;
		}

		return result + col;
	}

	std::pair<int,int> TextEditorWidget::text_pos_to_row_col(int pos) const
	{
		int nrow = 0;
		while(static_cast<unsigned>(pos) > text_[nrow].size()+1) {
			pos -= text_[nrow].size()+1;
			++nrow;
		}

		return std::pair<int,int>(nrow, pos);
	}

	void TextEditorWidget::setHighlightLines(int begin, int end)
	{
		begin_highlight_line_ = begin;
		end_highlight_line_ = end;
	}

	void TextEditorWidget::clearHighlightLines()
	{
		setHighlightLines(-1, -1);
	}

	bool TextEditorWidget::handleMouseButtonDown(const SDL_MouseButtonEvent& event)
	{
		recordOp();
		if(inWidget(event.x, event.y)) {
			setFocus(true);
			std::pair<int, int> pos = mousePositiontoRowCol(event.x, event.y);
			if(pos.first != -1) {
				cursor_.row = pos.first;
				cursor_.col = pos.second;
				onMoveCursor();
			}

			if(last_click_at_ != -1 && profile::get_tick_time() - last_click_at_ < 500) {
				++consecutive_clicks_;

				const int nclicks = consecutive_clicks_%3;

				if(nclicks == 1) {
					select_ = cursor_;
					selectToken(text_[cursor_.row], select_.row, cursor_.row, select_.col, cursor_.col);
				} else if(nclicks == 2) {
					select_ = Loc(cursor_.row, 0);
					cursor_.col = text_[cursor_.row].size();
				}

				if(select_ != cursor_) {
					//a mouse-based copy for X-style copy/paste
					handleCopy(true);
				}
			} else {
				consecutive_clicks_ = 0;

				if(event.button == SDL_BUTTON_MIDDLE && clipboard_has_mouse_area()) {
					std::string txt = copy_from_clipboard(true);
					handlePaste(txt);
				}
			}

			last_click_at_ = profile::get_tick_time();

			is_dragging_ = true;
			return claimMouseEvents();
		}

		if(has_focus_ != false && on_change_focus_) {
			on_change_focus_(false);
		}

		is_dragging_ = false;
		has_focus_ = false;

		return false;
	}

	bool TextEditorWidget::handleMouseButtonUp(const SDL_MouseButtonEvent& event)
	{
		recordOp();
		is_dragging_ = false;
	
		return false;
	}

	bool TextEditorWidget::handleMouseMotion(const SDL_MouseMotionEvent& event)
	{
		int mousex, mousey;
		if(is_dragging_ && has_focus_ && input::sdl_get_mouse_state(&mousex, &mousey)) {
			std::pair<int, int> pos = mousePositiontoRowCol(event.x, event.y);
			if(pos.first != -1) {
				cursor_.row = pos.first;
				cursor_.col = pos.second;
				onMoveCursor(true /*don't check for shift, assume it is*/);
			}

			if(mousey >= y() + height() && scroll_pos_ < int(text_.size())-2) {
				++scroll_pos_;
				int end = scroll_pos_ + nrows_ - 1;
				if(static_cast<unsigned>(end) >= text_.size()) {
					end = text_.size() - 1;
				}

				cursor_ = Loc(end, text_[end].size());
				onMoveCursor(true /*don't check for shift, assume it is*/);
				refreshScrollbar();
			} else if(mousey <= y() && scroll_pos_ > 0) {
				--scroll_pos_;
				cursor_ = Loc(scroll_pos_, 0);
				onMoveCursor(true /*don't check for shift, assume it is*/);

				refreshScrollbar();
			}
		}

		return false;
	}

	bool TextEditorWidget::handleKeyPress(const SDL_KeyboardEvent& event)
	{
		if(!has_focus_) {
			return false;
		}

		if(event.keysym.sym == SDLK_a && (event.keysym.mod&KMOD_CTRL)) {
			recordOp();
			cursor_.row = text_.size()-1;
			cursor_.col = text_[cursor_.row].size();
			onMoveCursor();
			select_ = Loc(0, 0);
			if(select_ != cursor_) {
				//a mouse-based copy for X-style copy/paste
				handleCopy(true);
			}
			return true;
		}

		if(editable_ && event.keysym.sym == SDLK_z && (event.keysym.mod&KMOD_CTRL)) {
			recordOp();
			undo();
			return true;
		}

		if(editable_ && event.keysym.sym == SDLK_y && (event.keysym.mod&KMOD_CTRL)) {
			recordOp();
			redo();
			return true;
		}

		if((event.keysym.sym == SDLK_c || event.keysym.sym == SDLK_x) && (event.keysym.mod&KMOD_CTRL)) {

			recordOp();
			handleCopy();

			if(editable_ && event.keysym.sym == SDLK_x) {
				saveUndoState();
				deleteSelection();
				onChange();
			}

			return true;
		} else if(editable_ && event.keysym.sym == SDLK_v && (event.keysym.mod&KMOD_CTRL)) {
			handlePaste(copy_from_clipboard(false));

			return true;
		}

		if(editable_ && (event.keysym.mod&KMOD_CTRL)) {
			if(event.keysym.sym == SDLK_BACKSPACE) {
				if(select_ == cursor_) {
					//We delete the current word behind us. 
					truncateColPosition();

					if(cursor_.col > 0) {
						saveUndoState();
					}

					const std::string& line = text_[select_.row];
					int col = select_.col;
					while(col > 0 && !(util::c_isalnum(line[col-1]) || line[col-1] == '_')) {
						--col;
					}

					while(col > 0 && (util::c_isalnum(line[col-1]) || line[col-1] == '_')) {
						--col;
					}

					select_.col = col;
					deleteSelection();
					recordOp();
					return true;
				}
			} else if(event.keysym.sym == SDLK_DELETE) {
				if(select_ == cursor_) {
					//We delete until end of line.
					truncateColPosition();

					if(static_cast<unsigned>(cursor_.col) < text_[select_.row].size()) {
						saveUndoState();
					}

					select_ = Loc(select_.row, text_[select_.row].size());
					deleteSelection();
					recordOp();
					return true;
				}
			} else { 
				recordOp();
				return false;
			}
		}

		if(event.keysym.sym == SDLK_ESCAPE && on_escape_) {
			on_escape_();
			return true;
		}

		switch(event.keysym.sym) {
		case SDLK_LEFT:
			recordOp();

			if(cursor_ != select_ && !(SDL_GetModState()&KMOD_SHIFT)) {
				//pressing left without shift while we have a selection moves us to the beginning of the selection
				if(cursor_ < select_) {
					select_ = cursor_;
				} else {
					cursor_ = select_;
				}
			} else {

				if(static_cast<unsigned>(cursor_.col) > text_[cursor_.row].size()) {
					cursor_.col = text_[cursor_.row].size();
				}

				--cursor_.col;
				if(cursor_.col < 0) {
					if(cursor_.row == 0) {
						cursor_.col = 0;
					} else {
						--cursor_.row;
						cursor_.col = text_[cursor_.row].size();
					}
				}
			}

			onMoveCursor();
			break;
		case SDLK_RIGHT:
			recordOp();

			if(cursor_ != select_ && !(SDL_GetModState()&KMOD_SHIFT)) {
				//pressing right without shift while we have a selection moves us to the end of the selection
				if(cursor_ < select_) {
					cursor_ = select_;
				} else {
					select_ = cursor_;
				}
			} else {
				++cursor_.col;
				if(static_cast<unsigned>(cursor_.col) > text_[cursor_.row].size()) {
					if(cursor_.row == text_.size()-1) {
						--cursor_.col;
					} else if(static_cast<unsigned>(cursor_.row) < text_.size()-1) {
						++cursor_.row;
						cursor_.col = 0;
					} else {
						--cursor_.col;
					}
				}
			}
			onMoveCursor();
			break;
		case SDLK_UP:
			recordOp();
			if(cursor_.row > 0) {
				--cursor_.row;
				cursor_.col = findEquivalentCol(cursor_.col, cursor_.row+1, cursor_.row);
			}
			onMoveCursor();

			break;
		case SDLK_DOWN:
			recordOp();
			if(static_cast<unsigned>(cursor_.row) < text_.size()-1) {
				++cursor_.row;
				cursor_.col = findEquivalentCol(cursor_.col, cursor_.row-1, cursor_.row);
			}
			onMoveCursor();

			break;
		case SDLK_PAGEUP: {
			recordOp();
			onPageUp();
			bool move_cursor = false;
			while(cursor_.row > scroll_pos_ && charPositionOnScreen(cursor_.row, cursor_.col).first == -1) {
				--cursor_.row;
				cursor_.col = findEquivalentCol(cursor_.col, cursor_.row+1, cursor_.row);
				move_cursor = true;
			}

			if(move_cursor) {
				onMoveCursor();
			}

			if(!(SDL_GetModState()&KMOD_SHIFT)) {
				select_ = cursor_;
			}
			break;
		}

		case SDLK_PAGEDOWN: {
			recordOp();
			onPageDown();
			bool move_cursor = false;
			while(cursor_.row < scroll_pos_ && charPositionOnScreen(cursor_.row, cursor_.col).first == -1) {
				++cursor_.row;
				cursor_.col = findEquivalentCol(cursor_.col, cursor_.row-1, cursor_.row);
				move_cursor = true;
			}

			if(move_cursor) {
				onMoveCursor();
			}

			if(!(SDL_GetModState()&KMOD_SHIFT)) {
				select_ = cursor_;
			}
			break;
		}
		case SDLK_HOME:
			recordOp();
	#ifdef __APPLE__
			cursor_.row = 0;
	#endif
			if((SDL_GetModState()&KMOD_CTRL)) {
				cursor_.row = 0;
			}

			cursor_.col = 0;
			onMoveCursor();
			break;
		case SDLK_END:
			recordOp();
	#ifdef __APPLE__
			cursor_.row = text_.size()-1;
	#endif
			if((SDL_GetModState()&KMOD_CTRL)) {
				cursor_.row = text_.size()-1;
			}

			cursor_.col = text_[cursor_.row].size();
			onMoveCursor();
			break;
		case SDLK_DELETE:
		case SDLK_BACKSPACE:
			if(!editable_) {
				break;
			}
			if(recordOp("delete")) {
				saveUndoState();

			}
			if(cursor_ == select_) {

				if(event.keysym.sym == SDLK_BACKSPACE) {
					//backspace is like delete but we move to the left first.
					if(static_cast<unsigned>(cursor_.col) > text_[cursor_.row].size()) {
						cursor_.col = text_[cursor_.row].size();
					}

					if(cursor_.row == 0 && cursor_.col == 0) {
						break;
					}

					--cursor_.col;
					if(cursor_.col < 0) {
						--cursor_.row;
						cursor_.col = text_[cursor_.row].size();
					}

					onMoveCursor();
				}

				if(static_cast<unsigned>(cursor_.col) >= text_[cursor_.row].size()) {
					if(text_.size() > static_cast<unsigned>(cursor_.row+1)) {
						cursor_.col = text_[cursor_.row].size();
						text_[cursor_.row] += text_[cursor_.row+1];
						text_.erase(text_.begin() + cursor_.row + 1);
					}
				} else {
					text_[cursor_.row].erase(text_[cursor_.row].begin() + cursor_.col);
				}
			} else {
				deleteSelection();
			}

			refreshScrollbar();
			onChange();
			break;

		case SDLK_RETURN: {
			if(!editable_) {
				break;
			}
			if(recordOp("enter")) {
				saveUndoState();

			}
			if(nrows_ == 1) {
				if(on_enter_) {
					on_enter_();
				}
				break;
			}

			if(onBeginEnter_) {
				if(!onBeginEnter_()) {
					break;
				}
			}

			deleteSelection();
			truncateColPosition();
		
			std::string new_line(text_[cursor_.row].begin() + cursor_.col, text_[cursor_.row].end());
			text_[cursor_.row].erase(text_[cursor_.row].begin() + cursor_.col, text_[cursor_.row].end());

			std::string::iterator indent = text_[cursor_.row].begin();
			while(indent != text_[cursor_.row].end() && strchr(" \t", *indent)) {
				++indent;
			}

			new_line.insert(new_line.begin(), text_[cursor_.row].begin(), indent);

			cursor_.col = indent - text_[cursor_.row].begin();

			text_.insert(text_.begin() + cursor_.row + 1, new_line);
			++cursor_.row;
			select_ = cursor_;

			refreshScrollbar();
			onChange();
			onMoveCursor();

			if(on_enter_) {
				on_enter_();
			}
		
			break;
		}
		case SDLK_TAB: {
			if(on_tab_) {
				on_tab_();
			} else if(nrows_ == 1) {
				return false;
			} else if(editable_) {
				handleTextInputInternal("\t");
			}
		}
		default: return true;
		}

		return true;
	}

	bool TextEditorWidget::handleTextInput(const SDL_TextInputEvent& event)
	{
		return handleTextInputInternal(event.text);
	}

	bool TextEditorWidget::handleTextInputInternal(const char* text)
	{
		if(!has_focus_ || !editable_) {
			return false;
		}

		if(recordOp("chars")) {
			saveUndoState();
		}
		deleteSelection();
		if(static_cast<unsigned>(cursor_.col) > text_[cursor_.row].size()) {
			cursor_.col = text_[cursor_.row].size();
		}
		for(const char* c = text; *c != 0; ++c) {
			text_[cursor_.row].insert(text_[cursor_.row].begin() + cursor_.col, *c);
			++cursor_.col;
		}
		select_ = cursor_;
		if(nrows_ == 1) {
			onMoveCursor();
		}

		refreshScrollbar();
		onChange();
		return true;
	}

	bool TextEditorWidget::handleTextEditing(const SDL_TextEditingEvent& event)
	{
		if(!has_focus_) {
			return false;
		}
		return false;
	}

	void TextEditorWidget::handlePaste(std::string txt)
	{
		if(!editable_) {
			return;
		}
		recordOp();
		saveUndoState();
		deleteSelection();

		txt.erase(std::remove(txt.begin(), txt.end(), '\r'), txt.end());
		std::vector<std::string> lines = util::split(txt, '\n', 0 /*don't remove empties or strip spaces*/);

		truncateColPosition();

		if(lines.size() == 1) {
			text_[cursor_.row].insert(text_[cursor_.row].begin() + cursor_.col, lines.front().begin(), lines.front().end());
			cursor_.col += lines.front().size();
			refreshScrollbar();
			select_ = cursor_;
		} else if(lines.size() >= 2) {
			text_.insert(text_.begin() + cursor_.row + 1, lines.back() + std::string(text_[cursor_.row].begin() + cursor_.col, text_[cursor_.row].end()));
			text_[cursor_.row] = std::string(text_[cursor_.row].begin(), text_[cursor_.row].begin() + cursor_.col) + lines.front();
			text_.insert(text_.begin() + cursor_.row + 1, lines.begin()+1, lines.end()-1);
			cursor_ = select_ = Loc(cursor_.row + lines.size() - 1, lines.back().size());
		}

		onChange();
	}

	void TextEditorWidget::handleCopy(bool mouse_based)
	{
		LOG_INFO("HANDLE COPY...");
		if(mouse_based && !clipboard_has_mouse_area()) {
			return;
		}

		Loc begin = cursor_;
		Loc end = select_;

		if(static_cast<unsigned>(begin.col) > text_[begin.row].size()) {
			begin.col = text_[begin.row].size();
		}

		if(static_cast<unsigned>(end.col) > text_[end.row].size()) {
			end.col = text_[end.row].size();
		}

		if(end < begin) {
			std::swap(begin, end);
		}


		std::string str;
		if(begin.row == end.row) {
			str = std::string(text_[begin.row].begin() + begin.col, text_[begin.row].begin() + end.col);
		} else {
			str = std::string(text_[begin.row].begin() + begin.col, text_[begin.row].end());
			while(++begin.row < end.row) {
				str += "\n" + text_[begin.row];
			}

			str += "\n" + std::string(text_[end.row].begin(), text_[end.row].begin() + end.col);
		}

		LOG_INFO("COPY TO CLIPBOARD: " << str << " " << mouse_based);

		copy_to_clipboard(str, mouse_based);
	}

	void TextEditorWidget::deleteSelection()
	{
		if(cursor_.col == select_.col && cursor_.row == select_.row) {
			return;
		}

		if(static_cast<unsigned>(cursor_.col) > text_[cursor_.row].size()) {
			cursor_.col = text_[cursor_.row].size();
		}

		if(static_cast<unsigned>(select_.col) > text_[select_.row].size()) {
			select_.col = text_[select_.row].size();
		}

		if(select_ < cursor_) {
			std::swap(cursor_, select_);
		}

		std::string& cursor_line = text_[cursor_.row];
		std::string& select_line = text_[select_.row];
		if(cursor_.row == select_.row) {
			cursor_line.erase(cursor_line.begin() + cursor_.col, cursor_line.begin() + select_.col);
		} else {
			cursor_line = std::string(cursor_line.begin(), cursor_line.begin() + cursor_.col) + std::string(select_line.begin() + select_.col, select_line.end());

			text_.erase(text_.begin() + cursor_.row + 1, text_.begin() + select_.row + 1);
		}

		select_ = cursor_;
	}

	KRE::Color TextEditorWidget::getCharacterColor(int row, int col) const
	{
		return text_color_;
	}

	std::pair<int, int> TextEditorWidget::mousePositiontoRowCol(int xpos, int ypos) const
	{
		const int xloc = x() + BorderSize;
		const int yloc = y() + BorderSize;

		int r = 0;
		for(unsigned n = scroll_pos_; n < text_.size() && r < nrows_; ++n, ++r) {
			int c = 0;
			bool matches_row = ypos >= yloc + r*char_height_ && ypos < yloc + (r+1)*char_height_;
			for(size_t m = xscroll_pos_; m < text_[n].size(); ++m, ++c) {
				if(c >= ncols_) {
					if(matches_row) {
						break;
					}
					++r;
					c -= ncols_;
					matches_row = ypos >= yloc + r*char_height_ && ypos < yloc + (r+1)*char_height_;
					if(r == nrows_) {
						break;
					}
				}

				const int char_size = text_[n][m] == '\t' ? TabWidth : 1;

				if(matches_row && xpos >= xloc + c*char_width_ && xpos < xloc + (c+char_size)*char_width_) {
					return std::pair<int, int>(n, m);
				}

				if(text_[n][m] == '\t') {
					c += TabAdjust;
					continue;
				}
			}

			if(matches_row) {
				return std::pair<int, int>(n, text_[n].size());
			}
		}

		return std::pair<int, int>(-1,-1);
	}

	std::pair<int, int> TextEditorWidget::charPositionOnScreen(int row, int col) const
	{
		if(row < scroll_pos_) {
			return std::pair<int, int>(-1, -1);
		}

		int r = 0;
		for(size_t n = scroll_pos_; n < text_.size() && r < nrows_; ++n, ++r) {
			int c = 0;
			size_t m;
			for(m = 0; m < text_[n].size(); ++m, ++c) {
				if(c >= ncols_) {
					++r;
					c -= ncols_;
					if(r == nrows_) {
						break;
					}
				}

				if(row == n && col == m) {
					return std::pair<int, int>(BorderSize + r*char_height_, BorderSize + c*char_width_);
				}

				if(text_[n][m] == '\t') {
					c += TabAdjust;
					continue;
				}
			}

			if(row == n && m == text_[n].size()) {
				return std::pair<int, int>(BorderSize + r*char_height_, BorderSize + c*char_width_);
			}
		}

		return std::pair<int, int>(-1,-1);
	}

	void TextEditorWidget::onPageUp()
	{
		int leap = nrows_ - 1;
		while(scroll_pos_ > 0 && leap > 0) {
			--scroll_pos_;
			--leap;

			for(int n = int(text_[scroll_pos_].size()) - ncols_; n > 0; n -= ncols_) {
				--leap;
			}
		}

		refreshScrollbar();
	}

	void TextEditorWidget::onPageDown()
	{
		int leap = nrows_ - 1;
		while(scroll_pos_ < int(text_.size())-2 && leap > 0) {
			++scroll_pos_;
			--leap;

			for(int n = int(text_[scroll_pos_].size()) - ncols_; n > 0; n -= ncols_) {
				--leap;
			}
		}

		refreshScrollbar();
	}

	void TextEditorWidget::onMoveCursor(bool auto_shift)
	{
		const int start_pos = scroll_pos_;
		if(cursor_.row < scroll_pos_) {
			scroll_pos_ = cursor_.row;
		} else {
			while(scroll_pos_ < cursor_.row && charPositionOnScreen(cursor_.row, cursor_.col).first == -1) {
				++scroll_pos_;
			}
		}

		if(nrows_ == 1) {
			if(cursor_.col < xscroll_pos_) {
				xscroll_pos_ = std::max<int>(0, cursor_.col - 4);
			} else if(cursor_.col >= xscroll_pos_ + ncols_) {
				xscroll_pos_ = cursor_.col + 4 - ncols_;
			}
		}

		if(start_pos != scroll_pos_) {
			refreshScrollbar();
		}

		if(!auto_shift && !(SDL_GetModState()&KMOD_SHIFT)) {
			select_ = cursor_;
		}

		ScrollableWidget::setYscroll(scroll_pos_*char_height_);

		if(select_ != cursor_) {
			//a mouse-based copy for X-style copy/paste
			handleCopy(true);
		}

		if(onMoveCursor_) {
			onMoveCursor_();
		}
	}

	int TextEditorWidget::findEquivalentCol(int old_col, int old_row, int new_row) const
	{
		unsigned actual_pos = old_col + std::count(text_[old_row].begin(), text_[old_row].end(), '\t')*TabAdjust;
		for(unsigned n = 0; n < actual_pos; ++n) {
			if(n < text_[new_row].size() && text_[new_row][n] == '\t') {
				actual_pos -= TabAdjust;
			}
		}

		return actual_pos;
	}

	void TextEditorWidget::onSetYscroll(int old_pos, int new_pos)
	{
		scroll_pos_ = new_pos/char_height_;
	}

	void TextEditorWidget::refreshScrollbar()
	{
		int total_rows = 0;
		//See if it can all fit without a scrollbar.
		for(int n = 0; n != text_.size(); ++n) {
			const int rows = 1 + text_[n].size()/ncols_;
			total_rows += rows;
			if(total_rows > nrows_) {
				break;
			}
		}

		if(total_rows <= nrows_ || nrows_ == 1) {
			//no scrollbar needed.
			setVirtualHeight(height());
			updateScrollbar();
			return;
		}

		setVirtualHeight(text_.size()*char_height_ + height() - char_height_);
		setScrollStep(char_height_);
		setArrowScrollStep(char_height_);

		setYscroll(scroll_pos_*char_height_);

		updateScrollbar();
	}

	void TextEditorWidget::selectToken(const std::string& row, int& begin_row, int& end_row, int& begin_col, int& end_col)
	{
		if(util::c_isdigit(row[begin_col]) || (row[begin_col] == '.' && static_cast<unsigned>(begin_col+1) < row.size() && util::c_isdigit(row[begin_col+1]))) {
			while(begin_col >= 0 && (util::c_isdigit(row[begin_col]) || row[begin_col] == '.')) {
				--begin_col;
			}

			if(begin_col < 0 || row[begin_col] != '-') {
				++begin_col;
			}

			while(static_cast<unsigned>(end_col) < row.size() && (util::c_isdigit(row[end_col]) || row[end_col] == '.')) {
				++end_col;
			}
		} else if(util::c_isalnum(row[begin_col]) || row[begin_col] == '_') {
			while(begin_col >= 0 && (util::c_isalnum(row[begin_col]) || row[begin_col] == '_')) {
				--begin_col;
			}

			++begin_col;

			while(static_cast<unsigned>(end_col) < row.size() && (util::c_isalnum(row[end_col]) || row[end_col] == '_')) {
				++end_col;
			}
		} else if(static_cast<unsigned>(end_col) < row.size()) {
			++end_col;
		}
	}

	TextEditorWidgetPtr TextEditorWidget::clone() const
	{
		TextEditorWidgetPtr result = new TextEditorWidget(*this);
		result->last_op_type_ = nullptr;
		return result;
	}

	void TextEditorWidget::restore(const TextEditorWidget* state)
	{
		*this = *state;
	}

	void TextEditorWidget::saveUndoState()
	{
		redo_.clear();
		undo_.push_back(TextEditorWidgetPtr(clone()));
	}

	bool TextEditorWidget::recordOp(const char* type)
	{
		if(type == nullptr || type != last_op_type_) {
			last_op_type_ = type;
			return true;
		} else {
			return false;
		}
	}

	void TextEditorWidget::undo()
	{
		if(undo_.empty()) {
			return;
		}

		std::vector<TextEditorWidgetPtr> redo_state = redo_;
		saveUndoState();
		redo_state.push_back(undo_.back());
		undo_.pop_back();

		//Save the state before restoring it so it doesn't get cleaned up
		//while we're in the middle of the restore call.
		TextEditorWidgetPtr state = undo_.back();
		restore(state.get());

		redo_ = redo_state;

		onChange();
	}

	void TextEditorWidget::redo()
	{
		if(redo_.empty()) {
			return;
		}

		std::vector<TextEditorWidgetPtr> redo_state = redo_;
		redo_state.pop_back();

		//Save the state before restoring it so it doesn't get cleaned up
		//while we're in the middle of the restore call.
		TextEditorWidgetPtr state = redo_.back();
		restore(state.get());

		redo_ = redo_state;

		onChange();
	}

	void TextEditorWidget::truncateColPosition()
	{
		if(static_cast<unsigned>(cursor_.col) > text_[cursor_.row].size()) {
			cursor_.col = text_[cursor_.row].size();
		}

		if(static_cast<unsigned>(select_.col) > text_[select_.row].size()) {
			select_.col = text_[select_.row].size();
		}
	}

	void TextEditorWidget::setSearch(const std::string& term)
	{
		search_ = term;
		calculateSearchMatches();
		if(search_matches_.empty()) {
			return;
		}

		std::vector<std::pair<Loc, Loc> >::const_iterator search_itor =
		   std::lower_bound(search_matches_.begin(), search_matches_.end(),
							std::pair<Loc,Loc>(cursor_, cursor_));
		if(search_itor == search_matches_.end()) {
			search_itor = search_matches_.begin();
		}

		select_ = cursor_ = search_itor->first;

		onMoveCursor();
	}

	void TextEditorWidget::nextSearchMatch()
	{
		if(search_matches_.empty()) {
			return;
		}

		cursor_.col++;
		select_ = cursor_;
		setSearch(search_);
	}

	void TextEditorWidget::calculateSearchMatches()
	{
		search_matches_.clear();
		if(search_.empty()) {
			return;
		}

		try {
			boost::regex re(search_, boost::regex::perl|boost::regex::icase);
			for(int n = 0; n != text_.size(); ++n) {
				boost::cmatch match;
				const char* ptr = text_[n].c_str();
				while(boost::regex_search(ptr, match, re)) {
					const int base = ptr - text_[n].c_str();
					const Loc begin(n, base + match.position());
					const Loc end(n, base + match.position() + match.length());
					search_matches_.push_back(std::pair<Loc,Loc>(begin,end));
	
					const int advance = match.position() + match.length();
					if(advance == 0) {
						break;
					}
	
					ptr += advance;
				}
			}
		} catch(boost::regex_error&) {
		}
	}

	void TextEditorWidget::replace(const std::string& replace_with)
	{
		recordOp();
		saveUndoState();
	
		//we have to get the end itor here because some compilers don't
		//support comparing a const and non-const reverse iterator
		const std::vector<std::pair<Loc, Loc> >::const_reverse_iterator end_itor = search_matches_.rend();
		for(std::vector<std::pair<Loc, Loc> >::const_reverse_iterator i = search_matches_.rbegin(); i != end_itor; ++i) {
			const Loc& begin = i->first;
			const Loc& end = i->second;
			if(begin.row != end.row) {
				continue;
			}

			text_[begin.row].erase(text_[begin.row].begin() + begin.col, text_[begin.row].begin() + end.col);
			text_[begin.row].insert(text_[begin.row].begin() + begin.col, replace_with.begin(), replace_with.end());
		}

		onChange();
	}
	
	void TextEditorWidget::onChange()
	{
		if(on_change_) {
			on_change_();
		}

		if(on_user_change_ && in_event_) {
			on_user_change_();
		}

		calculateSearchMatches();
	}

	BEGIN_DEFINE_CALLABLE(TextEditorWidget, Widget)
		DEFINE_FIELD(text, "string")
			return variant(obj.text());
		DEFINE_SET_FIELD
			obj.setText(value.as_string());
		DEFINE_FIELD(begin_enter, "bool")
			return variant::from_bool(obj.begin_enter_return_);
		DEFINE_SET_FIELD
			obj.begin_enter_return_ = value.as_bool();
		DEFINE_FIELD(color, "string")
			return variant("");
		DEFINE_SET_FIELD
			obj.text_color_ = KRE::Color(value);
		DEFINE_FIELD(has_focus, "bool")
			return variant::from_bool(obj.has_focus_);
		DEFINE_SET_FIELD
			obj.has_focus_ = value.as_bool();
			if(obj.clear_on_focus_ && obj.has_focus_) {
				obj.setText("");
				obj.clear_on_focus_ = false;
			}
	END_DEFINE_CALLABLE(TextEditorWidget)

	void TextEditorWidget::changeDelegate()
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("text", variant(text()));
			variant value = ffl_on_change_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("TextEditorWidget::changeDelegate() called without environment!");
		}
	}

	void TextEditorWidget::moveCursorDelegate()
	{
		if(getEnvironment()) {
			variant value = ffl_onMoveCursor_->execute(*getEnvironment());
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("TextEditorWidget::moveCursorDelegate() called without environment!");
		}
	}

	void TextEditorWidget::enterDelegate()
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("text", variant(text()));
			variant value = ffl_on_enter_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("TextEditorWidget::enterDelegate() called without environment!");
		}
	}

	void TextEditorWidget::escapeDelegate()
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("text", variant(text()));
			variant value = ffl_on_escape_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("TextEditorWidget::escapeDelegate() called without environment!");
		}
	}

	void TextEditorWidget::tabDelegate()
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("text", variant(text()));
			variant value = ffl_on_tab_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("TextEditorWidget::tabDelegate() called without environment!");
		}
	}

	bool TextEditorWidget::beginEnterDelegate()
	{
		if(getEnvironment()) {
			variant value = ffl_onBeginEnter_->execute(*getEnvironment());
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("TextEditorWidget::beginEnterDelegate() called without environment!");
		}
		// XXX Need some way of doing the return value here.
		return begin_enter_return_;
	}

	void TextEditorWidget::changeFocusDelgate(bool new_focus_value)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("focus", variant::from_bool(new_focus_value));
			callable->add("text", variant(text()));
			variant value = ffl_on_change_focus_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("TextEditorWidget::tabDelegate() called without environment!");
		}
	}

}

#include "code_editor_widget.hpp"
#include "dialog.hpp"
#include "filesystem.hpp"

namespace 
{
	void on_change_search(const gui::TextEditorWidgetPtr search_entry, gui::TextEditorWidgetPtr editor)
	{
		editor->setSearch(search_entry->text());
	}
}

UTILITY(textedit)
{
	using namespace gui;
	if(args.size() != 1) {
		LOG_INFO("textedit usage: <filename>");
		return;
	}

	std::string file = args[0];
	std::string contents = sys::read_file(file);
	if(contents.empty()) {
		LOG_INFO("Could not read file (" << file << ")");
		return;
	}

	TextEditorWidgetPtr entry = new TextEditorWidget(120);

	TextEditorWidgetPtr editor = new code_editor_widget(600, 400);
	editor->setText(contents);

	entry->setOnChangeHandler(std::bind(on_change_search, entry, editor));
	entry->setOnEnterHandler(std::bind(&TextEditorWidget::nextSearchMatch, editor));

	auto wnd = KRE::WindowManager::getMainWindow();
	Dialog d(0, 0, wnd->width(), wnd->height());
	d.addWidget(WidgetPtr(entry), 10, 10);
	d.addWidget(WidgetPtr(editor), 10, 30);
	d.showModal();
}

UNIT_TEST(test_regex)
{
	std::string searching = "abcdefg";
	boost::regex re("cde");
	boost::cmatch matches;
	const char* ptr = searching.c_str();
	if(boost::regex_search(ptr, matches, re)) {
		CHECK_EQ(matches.size(), 1);
		CHECK_EQ(matches.position(), 2);
		CHECK_EQ(matches.length(), 3);
	}
}
