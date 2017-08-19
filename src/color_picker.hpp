/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "Color.hpp"

#include "button.hpp"
#include "formula_callable_definition.hpp"
#include "grid_widget.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"
#include "widget.hpp"

namespace gui
{
	class ColorPicker : public Widget
	{
	public:
		ColorPicker(const rect& area);
		explicit ColorPicker(const rect& area, std::function<void (const KRE::Color&)> change_fun);
		explicit ColorPicker(const variant& v, game_logic::FormulaCallable* e);
		virtual ~ColorPicker();
		void setChangeHandler(std::function<void (const KRE::Color&)> change_fun) { onchange_ = change_fun; }

		void setPrimaryColor(KRE::Color color);
		void setSecondaryColor(KRE::Color color);

		KRE::Color getPrimaryColor() const { return primary_; }
		KRE::Color getSecondaryColor() const { return secondary_; }
		KRE::Color getSelectedColor() const { return main_color_selected_ ? primary_ : secondary_; }
		KRE::Color getUnselectedColor() const { return main_color_selected_ ? secondary_ : primary_; }
		bool getPaletteColor(int n, KRE::Color* color);
		void setPaletteColor(int n, const KRE::Color& color);

		WidgetPtr clone() const override;
	protected:
		void init();
	private:
		DECLARE_CALLABLE(ColorPicker);

		void colorUpdated();

		KRE::Color primary_;
		KRE::Color secondary_;
		std::vector<KRE::Color> palette_;

		int main_color_selected_;
		unsigned selected_palette_color_;
		uint8_t hue_;
		uint8_t saturation_;
		uint8_t value_;
		uint8_t alpha_;
		uint8_t red_;
		uint8_t green_;
		uint8_t blue_;

		GridPtr g_;
		std::vector<SliderPtr> s_;
		std::vector<TextEditorWidgetPtr> t_;
		ButtonPtr copy_to_palette_;
		void copyToPaletteFn();

		void sliderChange(int n, float p);
		void textChange(int n);
		void textTabPressed(int n);

		void setSlidersFromColor(const KRE::Color& c);
		void setTextFromColor(const KRE::Color& c, int n=-1);

		void setHSVFromColor(const KRE::Color&);

		void handleDraw() const override;
		bool handleEvent(const SDL_Event& event, bool claimed) override;

		int color_box_length_;
		int wheel_radius_;
		int palette_offset_y_;

		void processMouseInWheel(int x, int y);
		bool dragging_;

		void change();
		std::function<void (const KRE::Color&)> onchange_;
		game_logic::FormulaPtr change_handler_;
		game_logic::FormulaCallablePtr handler_arg_;
	};

	typedef ffl::IntrusivePtr<ColorPicker> ColorPickerPtr;
	typedef ffl::IntrusivePtr<const ColorPicker> ConstColorPickerPtr;
}
