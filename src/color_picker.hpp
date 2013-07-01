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

#include "color_utils.hpp"
#include "color_chart.hpp"
#include "formula_callable_definition.hpp"
#include "grid_widget.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"
#include "widget.hpp"

namespace gui
{
	class color_picker : public widget
	{
	public:
		color_picker(const rect& area, boost::function<void (const graphics::color&)>* onchange = 0);
		color_picker(const variant& v, game_logic::formula_callable* e);
		virtual ~color_picker();
		void set_change_handler(boost::function<void (const graphics::color&)> change_fun) { onchange_ = change_fun; }

		graphics::color get_primary_color() { return primary_; }
		graphics::color get_secondary_color() { return secondary_; }
		bool get_palette_color(int n, graphics::color* color);
		void set_palette_color(int n, const graphics::color& color);
	protected:
		void init();
	private:
		DECLARE_CALLABLE(color_picker);

		graphics::color primary_;
		graphics::color secondary_;
		std::vector<graphics::color> palette_;

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
		std::vector<text_editor_widget_ptr> t_;

		void slider_change(int n, double p);
		void text_change(int n);
		void text_tab_pressed(int n);

		void set_sliders_from_color(const graphics::color& c);
		void set_text_from_color(const graphics::color& c, int n=-1);

		void set_hsv_from_color(const graphics::color&);
		void set_primary_from_hsv();

		void handle_process();
		void handle_draw() const;
		bool handle_event(const SDL_Event& event, bool claimed);

		int color_box_length_;
		int wheel_radius_;

		void process_mouse_in_wheel(int x, int y);
		bool dragging_;

		void change();
		boost::function<void (const graphics::color&)> onchange_;
		game_logic::formula_ptr change_handler_;
		game_logic::formula_callable_ptr handler_arg_;
	};
}
