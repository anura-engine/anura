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

#include <vector>

#include "widget.hpp"

namespace gui
{
	typedef std::pair<std::string, WidgetPtr> SelectorPair;
	typedef std::vector<SelectorPair> SelectorList;

	class SelectorWidget : public Widget
	{
	public:
		explicit SelectorWidget(const std::vector<std::string>& list);
		explicit SelectorWidget(const SelectorList& list);
		explicit SelectorWidget(const variant& v, game_logic::FormulaCallable* e);
		virtual ~SelectorWidget() {}

		void setOnChangeHandler(std::function<void(const std::string&)> fn) { on_change_ = fn; }
		void setOnSelectHandler(std::function<void(const std::string&)> fn) { on_select_ = fn; }
		void setSelection(const std::string& sel);
		void setSelection(size_t sel);
		std::string get_selection();
	private:
		DECLARE_CALLABLE(SelectorWidget);

		virtual void handleDraw() const override;
		virtual bool handleEvent(const SDL_Event& event, bool claimed) override;

		void init();

		bool handleMousedown(const SDL_MouseButtonEvent& event, bool claimed);
		bool handleMouseup(const SDL_MouseButtonEvent& event, bool claimed);
		bool handleMouseMotion(const SDL_MouseMotionEvent& event, bool claimed);
		void selectLeft(size_t n=1);
		void selectRight(size_t n=1);

		SelectorList list_;
		size_t current_selection_;
		std::function<void(const std::string&)> on_change_;
		std::function<void(const std::string&)> on_select_;

		WidgetPtr left_arrow_;
		WidgetPtr right_arrow_;

		// delgate 
		void changeDelegate(const std::string& s);
		void selectDelegate(const std::string& s);
		// FFL formula
		game_logic::FormulaPtr change_handler_;
		game_logic::FormulaPtr select_handler_;
	};

	typedef boost::intrusive_ptr<SelectorWidget> SelectorWidgetPtr;
	typedef boost::intrusive_ptr<const SelectorWidget> ConstSelectorWidgetPtr;
}
