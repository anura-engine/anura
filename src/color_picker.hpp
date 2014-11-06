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

#include <vector>
#include <boost/function.hpp>

#include "button.hpp"
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
		color_picker(const rect& area);
		explicit color_picker(const rect& area, boost::function<void (const graphics::color&)> change_fun);
		explicit color_picker(const variant& v, game_logic::formula_callable* e);
		virtual ~color_picker();
		void set_change_handler(boost::function<void (const graphics::color&)> change_fun) { onchange_ = change_fun; }

		void set_primary_color(graphics::color color);
		void set_secondary_color(graphics::color color);

		graphics::color get_primary_color() const { return primary_; }
		graphics::color get_secondary_color() const { return secondary_; }
		graphics::color get_selected_color() const { return main_color_selected_ ? primary_ : secondary_; }
		graphics::color get_unselected_color() const { return main_color_selected_ ? secondary_ : primary_; }
		bool get_palette_color(int n, graphics::color* color);
		void set_palette_color(int n, const graphics::color& color);
	protected:
		void init();
	private:
		DECLARE_CALLABLE(color_picker);

		void color_updated();

		graphics::color primary_;
		graphics::color secondary_;
		std::vector<graphics::color> palette_;

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
		std::vector<text_editor_widget_ptr> t_;
		button_ptr copy_to_palette_;
		void copy_to_palette_fn();

		void slider_change(int n, double p);
		void text_change(int n);
		void text_tab_pressed(int n);

		void set_sliders_from_color(const graphics::color& c);
		void set_text_from_color(const graphics::color& c, int n=-1);

		void set_hsv_from_color(const graphics::color&);

		void handle_process();
		void handle_draw() const;
		bool handle_event(const SDL_Event& event, bool claimed);

		int color_box_length_;
		int wheel_radius_;
		int palette_offset_y_;

		void process_mouse_in_wheel(int x, int y);
		bool dragging_;

		void change();
		boost::function<void (const graphics::color&)> onchange_;
		game_logic::formula_ptr change_handler_;
		game_logic::formula_callable_ptr handler_arg_;
	};

	typedef boost::intrusive_ptr<color_picker> color_picker_ptr;
	typedef boost::intrusive_ptr<const color_picker> const_color_picker_ptr;
}
