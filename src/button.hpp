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

#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>

#include "widget.hpp"
#include "framed_gui_element.hpp"

namespace gui 
{
	enum BUTTON_RESOLUTION { BUTTON_SIZE_NORMAL_RESOLUTION, BUTTON_SIZE_DOUBLE_RESOLUTION };
	enum BUTTON_STYLE { BUTTON_STYLE_NORMAL, BUTTON_STYLE_DEFAULT };	//"default" means a visually fat-edged button - the one that gets pressed by hitting enter.  This is standard gui lingo, it's what the dialogue "defaults" to doing when you press return.

	//a button widget. Forwards to a given function whenever it is clicked.
	class Button : public Widget
	{
	public:
		struct SetColorSchemeScope {
			explicit SetColorSchemeScope(variant v);
			~SetColorSchemeScope();
			variant backup;
		};

		Button(const std::string& label, boost::function<void ()> onclick);
		Button(WidgetPtr label, boost::function<void ()> onclick, BUTTON_STYLE button_style = BUTTON_STYLE_NORMAL, BUTTON_RESOLUTION buttonResolution = BUTTON_SIZE_NORMAL_RESOLUTION);
		Button(const variant& v, game_logic::FormulaCallable* e);
		void setColorScheme(const variant& v);
		virtual WidgetPtr getWidgetById(const std::string& id);
		virtual ConstWidgetPtr getWidgetById(const std::string& id) const;
		void setClickHandler(boost::function<void ()> click_fun) { onclick_ = click_fun; }

		void setHPadding(int hpad);
		void setVPadding(int vpad);

		virtual void setFocus(bool f=true);

		virtual void doExecute();

		std::vector<WidgetPtr> getChildren() const;
	protected:
		void set_label(WidgetPtr label);
		virtual void handleProcess();
		virtual variant handleWrite();
		BUTTON_RESOLUTION buttonResolution() const { return button_resolution_; }
		virtual WidgetSettingsDialog* settingsDialog(int x, int y, int w, int h);

		DECLARE_CALLABLE(button);
	private:
		virtual void visitValues(game_logic::FormulaCallableVisitor& visitor);

		void setup();

		void handleDraw() const;
		bool handleEvent(const SDL_Event& event, bool claimed);
		void click();
		int vpadding_;
		int hpadding_;

		BUTTON_RESOLUTION button_resolution_;
		BUTTON_STYLE button_style_;
		WidgetPtr label_;
		boost::function<void ()> onclick_;
		bool down_;
		game_logic::formula_ptr click_handler_;
		game_logic::FormulaCallablePtr handler_arg_;
	
		ConstFramedGuiElementPtr normal_button_image_set_,depressed_button_image_set_,focus_button_image_set_,current_button_image_set_;

		boost::scoped_ptr<KRE::Color> normal_color_, depressed_color_, focus_color_;
		boost::scoped_ptr<KRE::Color> text_normal_color_, text_depressed_color_, text_focus_color_;
	};

	typedef boost::intrusive_ptr<Button> ButtonPtr;

}
