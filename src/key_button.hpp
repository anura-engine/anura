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

#include "button.hpp"
#include "controls.hpp"
#include "widget.hpp"
#include "framed_gui_element.hpp"


namespace gui 
{
	//a key selection button widget. Does not derive from button as we don't need the onclick event.
	class KeyButton : public Widget
	{
	public:
		KeyButton(key_type key, BUTTON_RESOLUTION buttonResolution);
		KeyButton(const variant& v, game_logic::FormulaCallable* e);

		key_type get_key();

		WidgetPtr clone() const override;
		static std::string getKeyName(key_type key);
	private:
		DECLARE_CALLABLE(KeyButton);

		void handleDraw() const override;
		bool handleEvent(const SDL_Event& event, bool claimed) override;

		BUTTON_RESOLUTION button_resolution_;
		WidgetPtr label_;
		key_type key_;
		bool grab_keys_;

		ConstFramedGuiElementPtr normal_button_image_set_,depressed_button_image_set_,focus_button_image_set_,current_button_image_set_;
	};

	typedef ffl::IntrusivePtr<KeyButton> KeyButtonPtr;
}
