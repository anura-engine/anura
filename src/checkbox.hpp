/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
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

#include <string>

namespace gui 
{
	class Checkbox : public Button
	{
	public:
		Checkbox(const std::string& label, bool checked, std::function<void(bool)> onclick, BUTTON_RESOLUTION buttonResolution=BUTTON_SIZE_NORMAL_RESOLUTION);
		Checkbox(WidgetPtr label, bool checked, std::function<void(bool)> onclick, BUTTON_RESOLUTION buttonResolution=BUTTON_SIZE_NORMAL_RESOLUTION);
		Checkbox(const variant& v, game_logic::FormulaCallable* e);
		bool checked() const { return checked_; }
		WidgetPtr clone() const override;
	private:
		DECLARE_CALLABLE(Checkbox);
		void onClick();

		std::string label_;
		WidgetPtr label_widget_;
		std::function<void(bool)> onclick_;
		bool checked_;
		game_logic::FormulaPtr click_handler_;
		void click(bool checked);
		int hpadding_;
	};

	typedef ffl::IntrusivePtr<Checkbox> CheckboxPtr;
}
