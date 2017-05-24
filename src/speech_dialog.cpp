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

#include <stack>

#include <boost/tokenizer.hpp>

#include "Canvas.hpp"
#include "WindowManager.hpp"

#include "asserts.hpp"
#include "controls.hpp"
#include "draw_scene.hpp"
#include "entity.hpp"
#include "frame.hpp"
#include "framed_gui_element.hpp"
#include "graphical_font.hpp"
#include "gui_section.hpp"
#include "input.hpp"
#include "joystick.hpp"
#include "module.hpp"
#include "screen_handling.hpp"
#include "speech_dialog.hpp"

namespace 
{
#if defined(MOBILE_BUILD)
	const int OptionHeight = 70;
	const int OptionMinWidth = 200;
	const int OptionXPad = 20;
#else
	const int OptionHeight = 50;
	const int OptionMinWidth = 150;
	const int OptionXPad = 10;
#endif
	const int OptionsBorder = 20; // size of the border around the options window
}

SpeechDialog::SpeechDialog()
	: cycle_(0), 
	  left_side_speaking_(false), 
	  horizontal_position_(0), 
	  text_char_(0), 
	  option_selected_(0),
#if defined(MOBILE_BUILD)
      joystick_button_pressed_(false),
      joystick_up_pressed_(false),
      joystick_down_pressed_(false),
#else 
      joystick_button_pressed_(true),
      joystick_up_pressed_(true),
      joystick_down_pressed_(true),
#endif 
	  expiration_(-1)
{
#if defined(MOBILE_BUILD)
	option_selected_ = -1;
#endif
}

SpeechDialog::~SpeechDialog()
{
}

bool SpeechDialog::handleMouseMove(int x, int y)
{
	input::sdl_get_mouse_state(&x, &y);
	rect box(
		graphics::GameScreen::get().getVirtualWidth() - option_width_/2 - OptionsBorder*2,
		0,
		option_width_ + OptionsBorder*2, OptionHeight * static_cast<int>(options_.size()) + OptionsBorder*2
	);
	if(pointInRect(point(x, y), box)) {
		option_selected_ = (y-box.y())/OptionHeight;
		return true;
	} else {
		option_selected_ = -1;
		return false;
	}
}

void SpeechDialog::moveUp()
{
	--option_selected_;
	if(option_selected_ < 0) {
		option_selected_ = static_cast<int>(options_.size() - 1);
	}
}

void SpeechDialog::moveDown()
{
	++option_selected_;
	if(option_selected_ == options_.size()) {
		option_selected_ = 0;
	}
}

bool SpeechDialog::keyPress(const SDL_Event& event)
{
	static int last_mouse = 0;
	if(text_char_ == num_chars() && options_.empty() == false) {
		if(event.type == SDL_KEYDOWN) {
			if(event.key.keysym.sym == get_keycode(controls::CONTROL_UP)) {
				moveUp();
			} else if(event.key.keysym.sym == get_keycode(controls::CONTROL_DOWN)) {
				moveDown();
			} else if(event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE ||
				event.key.keysym.sym == get_keycode(controls::CONTROL_JUMP) ||
				event.key.keysym.sym == get_keycode(controls::CONTROL_TONGUE)) {
				return true;
			}
		}
		return false;
	} else if ((event.type != SDL_KEYDOWN && event.type != SDL_MOUSEBUTTONDOWN) || (event.type == SDL_KEYDOWN && event.key.repeat)) {
		return false; // only keydown and mousebuttondown should be handled by the rest of the function
	}

	return scrollText();
}

bool SpeechDialog::scrollText()
{
	if(text_char_ < num_chars()) {
		text_char_ = num_chars();
		return false;
	}

	if(text_.size() > 2) {
		markup_.erase(markup_.begin());
		text_.erase(text_.begin());
		text_char_ = static_cast<int>(text_.front().size());
		return false;
	}

	return true;
}

bool is_skipping_game();

bool SpeechDialog::process()
{
	if(is_skipping_game()) {
		return true;
	}

	++cycle_;

	if(text_char_ < num_chars()) {
		++text_char_;
	}

	const int ScrollSpeed = 20;
	if(left_side_speaking_) {
		if(horizontal_position_ > 0) {
			horizontal_position_ -= ScrollSpeed;
			if(horizontal_position_ < 0) {
				horizontal_position_ = 0;
			}
		}
	} else {
		const int width = GuiSection::get("speech_portrait_pane")->width();
		if(horizontal_position_ < width) {
			horizontal_position_ += ScrollSpeed;
			if(horizontal_position_ > width) {
				horizontal_position_ = width;
			}
		}
	}

	if(expiration_ <= 0) {
		joystick::update();

		if(!joystick_up_pressed_ && joystick::up()) {
			moveUp();
		}

		if(!joystick_down_pressed_ && joystick::down()) {
			moveDown();
		}
	}

	joystick_up_pressed_ = joystick::up();
	joystick_down_pressed_ = joystick::down();

	return cycle_ == expiration_;
}

bool SpeechDialog::detectJoystickPress()
{
	const bool new_press = joystick::button(0) || joystick::button(1);
	const bool is_pressed = new_press && !joystick_button_pressed_;
	joystick_button_pressed_ = new_press;

	if(is_pressed) {
		return scrollText();
	} else {
		return false;
	}
}

void SpeechDialog::draw() const
{
	static const ConstGuiSectionPtr top_corner = GuiSection::get("speech_dialog_top_corner");
	static const ConstGuiSectionPtr bottom_corner = GuiSection::get("speech_dialog_bottom_corner");
	static const ConstGuiSectionPtr top_edge = GuiSection::get("speech_dialog_top_edge");
	static const ConstGuiSectionPtr bottom_edge = GuiSection::get("speech_dialog_bottom_edge");
	static const ConstGuiSectionPtr side_edge = GuiSection::get("speech_dialog_side_edge");
	static const ConstGuiSectionPtr arrow = GuiSection::get("speech_dialog_arrow");

	ConstGraphicalFontPtr font = GraphicalFont::get("default");

	auto canvas = KRE::Canvas::getInstance();

	const int TextAreaHeight = 80;

	const int TextBorder = 10;

	int speaker_xpos = std::numeric_limits<int>::max();
	int speaker_ypos = std::numeric_limits<int>::max();
	
	const int vw = graphics::GameScreen::get().getVirtualWidth();
	const int vh = graphics::GameScreen::get().getVirtualHeight();

	ConstEntityPtr speaker = left_side_speaking_ ? left_ : right_;
	if(speaker) {
		const screen_position& pos = last_draw_position();
		const int screen_x = static_cast<int>(pos.x/100.0f + (vw / 2) * (1.0f - 1.0f / pos.zoom));
		const int screen_y = static_cast<int>(pos.y/100.0f + (vh / 2) * (1.0f - 1.0f / pos.zoom));

		speaker_xpos = static_cast<int>((speaker->getFeetX() - screen_x)*pos.zoom - 36);
		speaker_ypos = static_cast<int>((speaker->getFeetY() - screen_y)*pos.zoom - 10);
	}

	if(pane_area_.w() == 0) {
		pane_area_ = rect(
		  top_corner->width(),
		  vh - TextAreaHeight + TextBorder,
		  vw - top_corner->width()*2,
		  TextAreaHeight - bottom_corner->height());
		if(speaker_ypos < 100) {
			pane_area_ = rect(pane_area_.x(), top_corner->height() + 50, pane_area_.w(), pane_area_.h());
		}
	}

	const rect text_area(pane_area_.x()-30, pane_area_.y()-30, pane_area_.w()+60, pane_area_.h()+60);

	if(module::get_speech_dialog_bg_color()) {
		canvas->drawSolidRect(pane_area_, *module::get_speech_dialog_bg_color());
	} else {
		canvas->drawSolidRect(pane_area_, KRE::Color(85, 53, 53, 255));
	}
	top_corner->blit(pane_area_.x() - top_corner->width(), pane_area_.y() - top_corner->height());
	top_corner->blit(pane_area_.x2()-1, pane_area_.y() - top_corner->height(), -top_corner->width(), top_corner->height());

	top_edge->blit(pane_area_.x(), pane_area_.y() - top_edge->height(), pane_area_.w(), top_edge->height());

	bottom_corner->blit(pane_area_.x() - bottom_corner->width(), pane_area_.y2());
	bottom_corner->blit(pane_area_.x2()-1, pane_area_.y2(), -bottom_corner->width(), bottom_corner->height());

	bottom_edge->blit(pane_area_.x(), pane_area_.y2(), pane_area_.w(), bottom_edge->height());

	side_edge->blit(pane_area_.x() - side_edge->width(), pane_area_.y(), side_edge->width(), pane_area_.h());
	side_edge->blit(pane_area_.x2()-1, pane_area_.y(), -side_edge->width(), pane_area_.h());

	if(speaker) {
		//if the arrow to the speaker is within reasonable limits, then
		//blit it.
		if(speaker_xpos > top_corner->width() && speaker_xpos < vw - top_corner->width() - arrow->width()) {
			arrow->blit(speaker_xpos, pane_area_.y() - arrow->height() - 32);
		}
	}


	//we center our text. Create a vector of the left edge of the text.
	std::vector<int> text_left_align;

	int total_height = 0;
	for(unsigned n = 0; n < text_.size(); ++n) {
		rect area = font->dimensions(text_[n]);

		if(n < 2) {
			total_height += area.h();
		}

		const int width = area.w();
		const int left = text_area.x() + text_area.w()/2 - width/2;
		text_left_align.push_back(left);
	}

	int ypos = text_area.y() + (text_area.h() - total_height)/2;
	int nchars = text_char_;
	for(unsigned n = 0; n < 2 && n < text_.size() && nchars > 0; ++n) {
		std::string str(text_[n].begin(), text_[n].begin() +
		                  std::min<int>(nchars, static_cast<int>(text_[n].size())));
		//move the first line slightly up so that accents don't mess up centering
		rect area = font->dimensions(str);
		area = rect(text_left_align[n], ypos - 2, area.w(), area.h());

		//draw the font by chunks of markup.
		int xadj = 0;
		const std::vector<TextMarkup>& markup = markup_[n];
		for(int m = 0; m != markup.size(); ++m) {
			const auto begin_index = markup[m].begin;
			const auto end_index = std::min<int>(static_cast<int>(str.size()), m+1 == markup.size() ? static_cast<int>(str.size()) : static_cast<int>(markup[m+1].begin));
			if(begin_index >= end_index) {
				continue;
			}

			KRE::Color draw_color(markup[m].color ? *markup[m].color : KRE::Color(255,187,10,255));
			const rect r = font->draw(text_left_align[n] + xadj, ypos - 2, std::string(str.begin() + begin_index, str.begin() + end_index), 2, draw_color);
			xadj += r.w();
		}
		//add some space between the lines
		ypos = area.y2() + 4;
		nchars -= static_cast<int>(text_[n].size());
	}

	if(text_.size() > 2 && text_char_ == num_chars() && (cycle_&16)) {
		ConstGuiSectionPtr down_arrow = GuiSection::get("speech_text_down_arrow");
		down_arrow->blit(text_area.x2() - down_arrow->width() - 10, text_area.y2() - down_arrow->height() - 10);
		
	}

	if(text_char_ == num_chars() && options_.empty() == false) {
		//ConstGuiSectionPtr options_panel = GuiSection::get("speech_portrait_pane");
		ConstFramedGuiElementPtr options_panel = FramedGuiElement::get("regular_window");
		int xpos = vw/2 - option_width_/2 - OptionsBorder*2;
		int ypos = 0;
		options_panel->blit(xpos, ypos, OptionsBorder*4 + option_width_, OptionsBorder*2 + static_cast<int>(OptionHeight*options_.size()), true);

		xpos += OptionsBorder + OptionXPad;
		ypos += OptionsBorder;

		KRE::Canvas::ColorManager cm(KRE::Color(255, 187, 10, 255));
		int index = 0;
		for(const std::string& option : options_) {
			rect area = font->dimensions(option);
			area = font->draw(xpos, ypos+(OptionHeight/3-area.h()/4), option);

			if(index == option_selected_) {
				KRE::Canvas::ColorManager cm(KRE::Color::colorWhite());
				ConstGuiSectionPtr cursor = GuiSection::get("cursor");
				cursor->blit(area.x2(), area.y());
			}
			ypos += OptionHeight;
			++index;
		}
	}
}

void SpeechDialog::setSpeakerAndFlipSide(ConstEntityPtr e)
{
	left_side_speaking_ = !left_side_speaking_;
	setSpeaker(e, left_side_speaking_);
}

void SpeechDialog::setSpeaker(ConstEntityPtr e, bool left_side)
{
	if(left_side) {
		left_ = e;
	} else {
		right_ = e;
	}

	pane_area_ = rect();
}

void SpeechDialog::setSide(bool left_side)
{
	left_side_speaking_ = left_side;
}

void SpeechDialog::setText(const std::vector<std::string>& text)
{
	text_.clear();
	markup_.clear();
	for (unsigned i = 0; i < text.size(); i++) {
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		//split text[i] at newline characters, add each line separately
		tokenizer lines(text[i], boost::char_separator<char> ("\n"));
		for (tokenizer::iterator iter = lines.begin(); iter != lines.end(); ++iter) {
			std::string txt = *iter;
			std::vector<TextMarkup> markup;
			TextMarkup m;
			m.begin = 0;
			markup.push_back(m);
			static const std::string BeginEmStr = "<em>";
			static const std::string EndEmStr = "</em>";
			std::string::iterator begin_em = std::search(txt.begin(), txt.end(), BeginEmStr.begin(), BeginEmStr.end());
			while(begin_em != txt.end()) {
				const auto begin_index = begin_em - txt.begin();
				txt.erase(begin_em, begin_em + BeginEmStr.size());

				TextMarkup m;
				m.begin = begin_index;
				m.color.reset(new KRE::Color(KRE::Color::colorWhite()));
				markup.push_back(m);

				std::string::iterator end_em = std::search(txt.begin(), txt.end(), EndEmStr.begin(), EndEmStr.end());
				if(end_em != txt.end()) {
					const auto end_index = end_em - txt.begin();
					txt.erase(end_em, end_em + EndEmStr.size());

					TextMarkup m;
					m.begin = end_index;
					markup.push_back(m);
				}

				begin_em = std::search(txt.begin(), txt.end(), BeginEmStr.begin(), BeginEmStr.end());
			}

			text_.push_back(txt);
			markup_.push_back(markup);
		}
	}
	text_char_ = 0;
}

void SpeechDialog::setOptions(const std::vector<std::string>& options)
{
	options_ = options;
#if defined(MOBILE_BUILD)
	option_selected_ = -1;
#else
	option_selected_ = 0;
#endif
	option_width_ = OptionMinWidth;
	ConstGraphicalFontPtr font = GraphicalFont::get("default");
	for(const std::string& option : options_) {
		rect area = font->dimensions(option);
		if (area.w()+OptionXPad*2 > option_width_)
			option_width_ = area.w()+OptionXPad*2;
	}
}

int SpeechDialog::num_chars() const
{
	int res = 0;
	if(text_.size() >= 1) {
		res += static_cast<int>(text_[0].size());
	}

	if(text_.size() >= 2) {
		res += static_cast<int>(text_[1].size());
	}

	return res;
}
