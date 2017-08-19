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
#include "gui_section.hpp"
#include "image_widget.hpp"
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
		void setFont(const std::string& font);
		void setFontSize(int size) { editor_->setFontSize(size); }
		void setText(const std::string& s) { editor_->setText(s); }

		WidgetPtr clone() const override;
	protected:
		void handleDraw() const override;
		bool handleEvent(const SDL_Event& event, bool claimed) override;
		void handleProcess() override;

		void init();
		void textEnter();
		void textChange();
	private:
		DECLARE_CALLABLE(DropdownWidget)
		bool handleMousedown(const SDL_MouseButtonEvent& event, bool claimed);
		bool handleMouseup(const SDL_MouseButtonEvent& event, bool claimed);
		bool handleMouseMotion(const SDL_MouseMotionEvent& event, bool claimed);
		void executeSelection(int selection);
		void mouseoverItem(int selection);
		void setLabel();

		int dropdown_height_;
		DropdownList list_;
		int current_selection_;
		DropdownType type_;
		TextEditorWidgetPtr editor_;
		GridPtr dropdown_menu_;
		std::vector<WidgetPtr> labels_;
		WidgetPtr label_;
		GuiSectionWidgetPtr dropdown_image_;
		
		std::function<void(const std::string&)> on_change_;
		std::function<void(int, const std::string&)> on_select_;
		
		std::string normal_image_, focus_image_;
		std::string font_;

		// delgate 
		void changeDelegate(const std::string& s);
		void selectDelegate(int selection, const std::string& s);

		void setColorScheme(const variant& v);

		// FFL formula
		game_logic::FormulaPtr change_handler_;
		game_logic::FormulaPtr select_handler_;

		KRE::ColorPtr normal_color_, depressed_color_, focus_color_;
		KRE::ColorPtr text_normal_color_, text_depressed_color_, text_focus_color_;

		std::function<WidgetPtr(const std::string&, int)> make_label_;

		bool in_widget_;
	};

	typedef ffl::IntrusivePtr<DropdownWidget> DropdownWidgetPtr;
	typedef ffl::IntrusivePtr<const DropdownWidget> ConstDropdownWidgetPtr;
}

#endif // NO_EDITOR
