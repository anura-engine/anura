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

#include <string>
#include <vector>

#include "SDL_events.h"

#include "Color.hpp"
#include "entity_fwd.hpp"
#include "gui_section.hpp"

class SpeechDialog
{
public:

	SpeechDialog();
	~SpeechDialog();

	bool detectJoystickPress();
	bool keyPress(const SDL_Event& e);
	bool process();
	void draw() const;
	void setSpeakerAndFlipSide(ConstEntityPtr e);
	void setSpeaker(ConstEntityPtr e, bool left_side=false);
	void setSide(bool left_side);
	void setText(const std::vector<std::string>& text);
	void setOptions(const std::vector<std::string>& options);
	void setExpiration(int time) { expiration_ = time; }

	int getOptionSelected() const { return option_selected_; }
	void setOptionSelected(int n) { option_selected_ = n; }
private:
	bool handleMouseMove(int x, int y);
	void moveUp();
	void moveDown();

	bool scrollText();

	int cycle_;
	ConstEntityPtr left_, right_;
	bool left_side_speaking_;
	int horizontal_position_;

	struct TextMarkup {
		size_t begin;
		std::shared_ptr<KRE::Color> color;
	};

	std::vector<std::vector<TextMarkup>> markup_;

	std::vector<std::string> text_;
	int text_char_;

	std::vector<std::string> options_;
	int option_selected_;
	int option_width_;

	bool joystick_button_pressed_, joystick_up_pressed_, joystick_down_pressed_;

	int expiration_;

	mutable rect pane_area_;

	int num_chars() const;

	SpeechDialog(const SpeechDialog&);
	void operator=(const SpeechDialog&);
};
