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

#include "widget.hpp"
#include "framed_gui_element.hpp"

namespace gui 
{
	enum BUTTON_RESOLUTION { 
		BUTTON_SIZE_NORMAL_RESOLUTION, 
		BUTTON_SIZE_DOUBLE_RESOLUTION 
	};
	//"default" means a visually fat-edged button - the one that gets pressed 
	// by hitting enter.  This is standard gui lingo, it's what the dialogue 
	// "defaults" to doing when you press return.
	enum BUTTON_STYLE { 
		BUTTON_STYLE_NORMAL, 
		BUTTON_STYLE_DEFAULT 
	};

	//a button widget. Forwards to a given function whenever it is clicked.
	class Button : public Widget
	{
	public:
		struct SetColorSchemeScope {
			explicit SetColorSchemeScope(variant v);
			~SetColorSchemeScope();
			variant backup;
		};

		static variant getColorScheme();

		Button(const std::string& label, std::function<void ()> onclick);
		Button(WidgetPtr label, std::function<void ()> onclick, BUTTON_STYLE button_style = BUTTON_STYLE_NORMAL, BUTTON_RESOLUTION buttonResolution = BUTTON_SIZE_NORMAL_RESOLUTION);
		Button(const variant& v, game_logic::FormulaCallable* e);
		void setColorScheme(const variant& v);
		virtual WidgetPtr getWidgetById(const std::string& id) override;
		virtual ConstWidgetPtr getWidgetById(const std::string& id) const override;
		void setClickHandler(std::function<void ()> click_fun) { onclick_ = click_fun; }

		void setHPadding(int hpad);
		void setVPadding(int vpad);

		virtual void setFocus(bool f=true) override;

		virtual void doExecute() override;

		BUTTON_RESOLUTION buttonResolution() const { return button_resolution_; }

		std::vector<WidgetPtr> getChildren() const override;

		virtual WidgetPtr clone() const override;
	protected:
		void setLabel(WidgetPtr label);
		virtual void handleProcess() override;
		virtual variant handleWrite() override;
		virtual WidgetSettingsDialog* settingsDialog(int x, int y, int w, int h) override;

		void surrenderReferences(GarbageCollector* collector) override;

	private:
		DECLARE_CALLABLE(Button);
		virtual void visitValues(game_logic::FormulaCallableVisitor& visitor) override;

		void setup();

		void handleDraw() const override;
		bool handleEvent(const SDL_Event& event, bool claimed) override;
		void click();
		int vpadding_;
		int hpadding_;

		BUTTON_RESOLUTION button_resolution_;
		BUTTON_STYLE button_style_;
		WidgetPtr label_;
		std::function<void ()> onclick_;
		bool down_;
		variant mouseover_handler_, mouseoff_handler_;
		game_logic::FormulaPtr click_handler_;
		variant click_handler_fn_;
		game_logic::FormulaCallablePtr handler_arg_;
	
		ConstFramedGuiElementPtr normal_button_image_set_,depressed_button_image_set_,focus_button_image_set_,current_button_image_set_;

		KRE::Color normal_color_, depressed_color_, focus_color_;
		KRE::Color text_normal_color_, text_depressed_color_, text_focus_color_;
	};

	typedef ffl::IntrusivePtr<Button> ButtonPtr;

}
