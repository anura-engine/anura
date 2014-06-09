/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include <vector>
#include <boost/function.hpp>

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
		explicit ColorPicker(const rect& area, boost::function<void (const KRE::Color&)> change_fun);
		explicit ColorPicker(const variant& v, game_logic::FormulaCallable* e);
		virtual ~ColorPicker();
		void setChangeHandler(boost::function<void (const KRE::Color&)> change_fun) { onchange_ = change_fun; }

		void setPrimaryColor(KRE::Color color);
		void setSecondaryColor(KRE::Color color);

		KRE::Color getPrimaryColor() const { return primary_; }
		KRE::Color getSecondaryColor() const { return secondary_; }
		KRE::Color getSelectedColor() const { return main_color_selected_ ? primary_ : secondary_; }
		KRE::Color getUnselectedColor() const { return main_color_selected_ ? secondary_ : primary_; }
		bool getPaletteColor(int n, KRE::Color* color);
		void setPaletteColor(int n, const KRE::Color& color);
	protected:
		void init();
	private:
		DECLARE_CALLABLE(ColorPicker);

		void colorUpdated();

		KRE::Color primary_;
		KRE::Color secondary_;
		std::vector<KRE::Color> palette_;

		int main_color_selected_;
		int selected_palette_color_;
		uint8_t hue_;
		uint8_t saturation_;
		uint8_t value_;
		uint8_t alpha_;
		uint8_t red_;
		uint8_t green_;
		uint8_t blue_;

		grid_ptr g_;
		std::vector<slider_ptr> s_;
		std::vector<TextEditorWidgetPtr> t_;
		ButtonPtr copy_to_palette_;
		void copyToPaletteFn();

		void sliderChange(int n, double p);
		void textChange(int n);
		void textTabPressed(int n);

		void setSlidersFromColor(const KRE::Color& c);
		void setTextFromColor(const KRE::Color& c, int n=-1);

		void setHSVFromColor(const KRE::Color&);

		void handleProcess();
		void handleDraw() const;
		bool handleEvent(const SDL_Event& event, bool claimed);

		int color_box_length_;
		int wheel_radius_;
		int palette_offset_y_;

		void processMouseInWheel(int x, int y);
		bool dragging_;

		void change();
		boost::function<void (const KRE::Color&)> onchange_;
		game_logic::formula_ptr change_handler_;
		game_logic::FormulaCallablePtr handler_arg_;
	};

	typedef boost::intrusive_ptr<ColorPicker> ColorPickerPtr;
	typedef boost::intrusive_ptr<const ColorPicker> ConstColorPickerPtr;
}
