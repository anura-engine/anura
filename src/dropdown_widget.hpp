/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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
#ifndef NO_EDITOR

#include <vector>

#include "border_widget.hpp"
#include "label.hpp"
#include "grid_widget.hpp"
#include "text_editor_widget.hpp"
#include "widget.hpp"

namespace gui 
{
	typedef std::vector<std::string> DropdownList;

	enum class DropdownType {
		LIST,
		COMBOBOX,
	};

	class DropdownWidget : public Widget
	{
	public:
		DropdownWidget(const DropdownList& list, int width, int height=0, DropdownType type=DropdownType::LIST);
		DropdownWidget(const variant& v, game_logic::FormulaCallable* e);

		void setOnChangeHandler(std::function<void(const std::string&)> fn) { on_change_ = fn; }
		void setOnSelectHandler(std::function<void(int,const std::string&)> fn) { on_select_ = fn; }
		void setSelection(int selection);
		int getMaxHeight() const;
		void setDropdownHeight(int h);
		void setFontSize(int size) { editor_->setFontSize(size); }
		void setText(const std::string& s) { editor_->setText(s); }
	protected:
		virtual void handleDraw() const override;
		virtual bool handleEvent(const SDL_Event& event, bool claimed) override;
		virtual void handleProcess() override;

		void init();
		void textEnter();
		void textChange();
	private:
		DECLARE_CALLABLE(DropdownWidget)
		bool handleMousedown(const SDL_MouseButtonEvent& event, bool claimed);
		bool handleMouseup(const SDL_MouseButtonEvent& event, bool claimed);
		bool handleMouseMotion(const SDL_MouseMotionEvent& event, bool claimed);
		void executeSelection(int selection);

		int dropdown_height_;
		DropdownList list_;
		int current_selection_;
		DropdownType type_;
		TextEditorWidgetPtr editor_;
		GridPtr dropdown_menu_;
		LabelPtr label_;
		WidgetPtr dropdown_image_;
		std::function<void(const std::string&)> on_change_;
		std::function<void(int, const std::string&)> on_select_;

		// delgate 
		void changeDelegate(const std::string& s);
		void selectDelegate(int selection, const std::string& s);
		// FFL formula
		game_logic::FormulaPtr change_handler_;
		game_logic::FormulaPtr select_handler_;
	};

	typedef boost::intrusive_ptr<DropdownWidget> DropdownWidgetPtr;
	typedef boost::intrusive_ptr<const DropdownWidget> ConstDropdownWidgetPtr;
}

#endif // NO_EDITOR
