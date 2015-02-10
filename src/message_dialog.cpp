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

#include <ctype.h>
#include <iostream>

#include "Color.hpp"
#include "Canvas.hpp"
#include "Font.hpp"
#include "WindowManager.hpp"

#include "input.hpp"
#include "message_dialog.hpp"
#include "string_utils.hpp"

namespace 
{
	const int FontSize = 22;

	std::string::const_iterator get_end_of_word(std::string::const_iterator i1,
												std::string::const_iterator i2)
	{
		while(i1 != i2 && util::c_isspace(*i1)) {
			++i1;
		}

		while(i1 != i2 && !util::c_isspace(*i1)) {
			++i1;
		}

		return i1;
	}


	std::string::const_iterator get_line(std::string::const_iterator i1,
										 std::string::const_iterator i2,
										 int max_chars)
	{
		std::string::const_iterator begin = i1;
		i1 = get_end_of_word(i1, i2);
		if(i1 == i2) {
			return i1;
		}

		while(i1 != i2 && *i1 != '\n' &&  get_end_of_word(i1, i2) - begin < max_chars) {
			i1 = get_end_of_word(i1, i2);
		}

		return i1;
	}

	MessageDialog* current_dialog = NULL;
}

void MessageDialog::showModal(const std::string& text, const std::vector<std::string>* options)
{
	if(current_dialog) {
		clearModal();
	}

	const int Width = 650;
	const int Height = KRE::Font::charHeight(FontSize)*3;
	auto wnd = KRE::WindowManager::getMainWindow();
	current_dialog = new MessageDialog(text, rect(wnd->width()/2 - Width/2, wnd->height()/2 - Height/2, Width, Height), options);
}

void MessageDialog::clearModal()
{
	delete current_dialog;
	current_dialog = NULL;
}

MessageDialog* MessageDialog::get()
{
	return current_dialog;
}

MessageDialog::MessageDialog(const std::string& text, const rect& pos,
                             const std::vector<std::string>* options)
  : text_(text), pos_(pos), line_height_(0),
    cur_row_(0), cur_char_(0), cur_wait_(0),
	selected_option_(0)
{
	line_height_ = KRE::Font::charHeight(FontSize);
	viewable_lines_ = pos.h()/line_height_;
	if(viewable_lines_ < 1) {
		viewable_lines_ = 1;
	}

	const int max_chars_on_line = std::max<int>(1, pos.w()/KRE::Font::charWidth(FontSize));
	std::string::const_iterator i1 = text.begin();
	std::string::const_iterator i2 = i1;
	while(i2 != text.end()) {
		i2 = get_line(i2, text.end(), max_chars_on_line);
		if(i2 == i1) {
			break;
		}

		while(i1 != i2 && util::c_isspace(*i1)) {
			++i1;
		}

		lines_.push_back(KRE::Font::getInstance()->renderText(std::string(i1, i2), KRE::Color::colorBlack(), FontSize));
		i1 = i2;
	}

	if(options != NULL) {
		for(const std::string& option : *options) 
		{
			options_.emplace_back(KRE::Font::getInstance()->renderText(option, KRE::Color::colorBlack(), FontSize));
		}
	}
}

namespace 
{
	void draw_frame(const rect& r)
	{
		// XXX Having the color's below fixed is pretty meh and wouldn't nescessarily fit
		// with another color widget set color scheme.
		const int Border = 4;
		const int Padding = 10;
		auto canvas = KRE::Canvas::getInstance();
		canvas->drawSolidRect(rect(r.x() - Padding - Border, r.y() - Padding - Border, r.w() + + Padding*2 + Border*2, r.h() + Padding*2 + Border*2),
			KRE::Color(0xa2, 0x64, 0x76));
		canvas->drawSolidRect(rect(r.x() - Padding, r.y() - Padding, r.w() + Padding*2, r.h() + Padding*2), 
			KRE::Color(0xbe, 0xa2, 0x8f));
	}
}

void MessageDialog::draw() const
{
	auto canvas = KRE::Canvas::getInstance();
	draw_frame(pos_);

	for(unsigned n = 0; n <= cur_row_ && n < lines_.size(); ++n) {
		if(n != cur_row_) {
			canvas->blitTexture(lines_[n], 0, rect(pos_.x(), pos_.y() + n*line_height_));
		} else {
			const int width = cur_char_*KRE::Font::charWidth(FontSize);
			const int height = lines_[n]->height();
			canvas->blitTexture(lines_[n], rect(0,0,width,height), 0, rect(pos_.x(), pos_.y() + n*line_height_));
		}
	}

	if(cur_row_ >= lines_.size() && !options_.empty()) {
		const int CursorWidth = 8;

		unsigned width = 0;
		unsigned height = 0;
		for(const KRE::TexturePtr& t : options_) {
			if(t->width() > width) {
				width = t->width();
			}

			height += t->height();
		}

		rect r(pos_.x2() - 100, pos_.y2(), width + CursorWidth, height);
		draw_frame(r);

		for(int n = 0; n != options_.size(); ++n) {
			canvas->blitTexture(options_[n], 0, rect(r.x() + CursorWidth, r.y() + n*line_height_));

			if(n == selected_option_) {
				int xpos = r.x() + CursorWidth;
				const int ypos = r.y() + n*line_height_ + line_height_/2;
				int height = 1;
				while(xpos > r.x()) {
					canvas->drawSolidRect(rect(xpos, ypos - height, 1, height*2), KRE::Color::colorBlack());
					--xpos;
					++height;
				}
			}
		}
	}
}

void MessageDialog::process()
{
	SDL_Event event;
	while(input::sdl_poll_event(&event)) {
		switch(event.type) {
			case SDL_KEYDOWN:
				if(options_.empty() == false) {
					if(event.key.keysym.sym == SDLK_DOWN) {
						selected_option_++;
						if(selected_option_ == options_.size()) {
							selected_option_ = 0;
						}
						break;
					}
					if(event.key.keysym.sym == SDLK_UP) {
						selected_option_--;
						if(selected_option_ == -1) {
							selected_option_ = options_.size() - 1;
						}
						break;
					}
				}
				if(cur_row_ >= lines_.size()) {
					clearModal();
					return;
				}

				break;
		}
	}

	if(cur_row_ < lines_.size()) {
		int num_keys;
		const Uint8* keys = SDL_GetKeyboardState(&num_keys);
		const unsigned WaitTime = std::count(keys, keys + num_keys, 0)  == num_keys ? 3 : 1;

		const unsigned nchars = lines_[cur_row_]->width()/KRE::Font::charWidth(FontSize);

		++cur_wait_;
		if(cur_wait_ >= WaitTime) {
			cur_wait_ = 0;
			++cur_char_;
			if(cur_char_ >= nchars) {
				cur_char_ = 0;
				++cur_row_;
			}
		}
	}
}
