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
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/
#include <boost/bind.hpp>

#include "asserts.hpp"
#include "checkbox.hpp"
#include "color_picker.hpp"
#include "dropdown_widget.hpp"
#include "font.hpp"
#include "formatter.hpp"
#include "grid_widget.hpp"
#include "label.hpp"
#include "preferences.hpp"
#include "widget_settings_dialog.hpp"

namespace gui
{
	widget_settings_dialog::widget_settings_dialog(int x, int y, int w, int h, widget_ptr ptr) 
		: dialog(x,y,w,h), widget_(ptr), text_size_(14)
	{
		ASSERT_LOG(ptr != NULL, "widget_settings_dialog::widget_settings_dialog: widget_ == NULL");
		init();
	}

	widget_settings_dialog::~widget_settings_dialog()
	{
	}

	void widget_settings_dialog::set_font(const std::string& fn) 
	{ 
		font_name_ = fn; 
		init();
	}
	void widget_settings_dialog::set_text_size(int ts) 
	{ 
		text_size_ = ts; 
		init();
	}

	void widget_settings_dialog::init()
	{
		set_clear_bg_amount(255);
	
		grid_ptr g = grid_ptr(new grid(2));
		g->set_max_height(height()-50);
		g->add_col(new label("ID:", text_size_, font_name_));
		text_editor_widget_ptr id_edit = new text_editor_widget(150, 30);
		id_edit->set_text(widget_->id());
		id_edit->set_on_user_change_handler([=](){widget_->set_id(id_edit->text());});
		g->add_col(id_edit);

		g->add_col(new label("", text_size(), font()))
			.add_col(new checkbox(widget_ptr(new label("Enabled", text_size_, font_name_)), 
			!widget_->disabled(), 
			[&](bool checked){widget_->enable(!checked);}, 
			BUTTON_SIZE_NORMAL_RESOLUTION));

		g->add_col(new label("Disabled Opacity:", text_size(), font()))
			.add_col(new slider(120, [&](double f){this->widget_->set_disabled_opacity(int(f*255.0));}, 
			this->widget_->disabled_opacity()/255.0, 1));

		g->add_col(new label("", text_size(), font()))
			.add_col(new checkbox(widget_ptr(new label("Visible", text_size_, font_name_)), 
			!widget_->visible(), 
			[&](bool checked){}, 
			BUTTON_SIZE_NORMAL_RESOLUTION));

		g->add_col(new label("Alpha:", text_size(), font()))
			.add_col(new slider(120, [&](double f){widget_->set_alpha(int(f*255.0));}, 
			this->widget_->get_alpha()/255.0, 1));

		std::vector<std::string> sections = framed_gui_element::get_elements();
		sections.insert(sections.begin(), "<<none>>");
		dropdown_widget_ptr frame_set(new dropdown_widget(sections, 150, 28, dropdown_widget::DROPDOWN_LIST));
		frame_set->set_font_size(14);
		frame_set->set_dropdown_height(height());
		
		auto it = std::find(sections.begin(), sections.end(), widget_->frame_set_name());
		frame_set->set_selection(it == sections.end() ? 0 : it-sections.begin());
		frame_set->set_on_select_handler([&](int n, const std::string& s){
			if(s != "<<none>>") {
				widget_->set_frame_set(s);
			} else {
				widget_->set_frame_set("");
			}
		});
		frame_set->set_zorder(20);
		g->add_col(new label("Frame Set:", text_size(), font()));
		g->add_col(frame_set);

		g->add_col(new label("", text_size(), font()))
			.add_col(new checkbox(widget_ptr(new label("Double frame size", text_size_, font_name_)), 
			widget_->get_frame_resolution() != 0, 
			[&](bool checked){widget_->set_frame_resolution(checked ? 1 : 0);}, 
			BUTTON_SIZE_NORMAL_RESOLUTION));

		g->add_col(new label("pad width:", text_size(), font()))
			.add_col(new slider(120, [&](double f){widget_->set_padding(int(f*100.0), widget_->get_pad_height());}, 
			widget_->get_pad_width()/100.0, 1));
		g->add_col(new label("pad height:", text_size(), font()))
			.add_col(new slider(120, [&](double f){widget_->set_padding(widget_->get_pad_width(), int(f*100.0));}, 
			widget_->get_pad_height()/100.0, 1));

		text_editor_widget_ptr tooltip_edit = new text_editor_widget(150, 30);
		tooltip_edit->set_text(widget_->tooltip_text());
		tooltip_edit->set_on_user_change_handler([=](){widget_->set_tooltip_text(tooltip_edit->text());});
		g->add_col(new label("Tooltip:", text_size_, font_name_))
			.add_col(tooltip_edit);
		g->add_col(new label("Tooltip Height:", text_size(), font()))
			.add_col(new slider(120, [&](double f){widget_->set_tooltip_fontsize(int(f*72.0+6.0));}, 
			(widget_->tooltip_fontsize()-6.0)/72.0, 1));
		
		std::vector<std::string> fonts = font::get_available_fonts();
		fonts.insert(fonts.begin(), "");
		dropdown_widget_ptr font_list(new dropdown_widget(fonts, 150, 28, dropdown_widget::DROPDOWN_LIST));
		font_list->set_font_size(14);
		font_list->set_dropdown_height(height());
		auto fit = std::find(fonts.begin(), fonts.end(), widget_->tooltip_font());
		font_list->set_selection(fit == fonts.end() ? 0 : fit-fonts.begin());
		font_list->set_on_select_handler([&](int n, const std::string& s){widget_->set_tooltip_font(s);});
		font_list->set_zorder(19);
		g->add_col(new label("Tooltip Font:", text_size(), font()))
			.add_col(font_list);
		g->add_col(new label("Tooltip Color:", text_size(), font()))
			.add_col(new button(new label("Choose...", text_size_, font_name_), [&](){
				int mx, my;
				SDL_GetMouseState(&mx, &my);
				mx = mx + 200 > preferences::actual_screen_width() ? preferences::actual_screen_width()-200 : mx;
				my = my + 600 > preferences::actual_screen_height() ? preferences::actual_screen_height()-600 : my;
				//mx -= x();
				my -= y();
				color_picker* cp = new color_picker(rect(0, 0, 200, 600), [&](const graphics::color& color){widget_->set_tooltip_color(color);});
				cp->set_primary_color(graphics::color(widget_->tooltip_color()));

				grid_ptr gg = new grid(1);
				gg->allow_selection();
				gg->swallow_clicks();
				gg->set_show_background(true);
				gg->allow_draw_highlight(false);
				gg->register_selection_callback([=](int n){std::cerr << "n = " << n << std::endl; if(n != 0){remove_widget(gg); init();}});
				gg->set_zorder(100);
				gg->add_col(cp);
				add_widget(gg, x()-mx-200, my);
		}));

		g->add_col(new label("Tooltip Delay:", text_size(), font()))
			.add_col(new slider(120, [&](double f){widget_->set_tooltip_delay(int(f*5000.0));}, 
			widget_->get_tooltip_delay()/5000.0, 1));

		g->add_col(new label("", text_size(), font()))
			.add_col(new checkbox(widget_ptr(new label("Claim Mouse Events", text_size_, font_name_)), 
			claim_mouse_events(), 
			[&](bool checked){widget_->set_claim_mouse_events(checked);}, 
			BUTTON_SIZE_NORMAL_RESOLUTION));

		g->add_col(new label("", text_size(), font()))
			.add_col(new checkbox(widget_ptr(new label("Draw with Object shader", text_size_, font_name_)), 
			draw_with_object_shader(), 
			[&](bool checked){widget_->set_draw_with_object_shader(checked);}, 
			BUTTON_SIZE_NORMAL_RESOLUTION));

		g->add_col(new label("Width:", text_size(), font()))
			.add_col(new slider(120, [&](double f){widget_->set_dim(int(f*width()), widget_->height());}, 
			widget_->width()/double(width()), 1));
		g->add_col(new label("Height:", text_size(), font()))
			.add_col(new slider(120, [&](double f){widget_->set_dim(widget_->width(), int(f*height()));}, 
			widget_->height()/double(height()), 1));

		g->add_col(new label("X:", text_size(), font()))
			.add_col(new slider(120, [&](double f){widget_->set_loc(int(f*width()), widget_->y());}, 
			widget_->x()/double(width()), 1));
		g->add_col(new label("Y:", text_size(), font()))
			.add_col(new slider(120, [&](double f){widget_->set_loc(widget_->x(), int(f*height()));}, 
			widget_->y()/double(height()), 1));

		grid* zg = new grid(3);
		text_editor_widget_ptr z_edit = new text_editor_widget(60, 30);
		z_edit->set_text(formatter() << widget_->zorder());
		z_edit->set_on_user_change_handler([=](){widget_->set_zorder(atoi(z_edit->text().c_str()));});
		zg->add_col(new button(new label("+", text_size(), font()), [=](){widget_->set_zorder(widget_->zorder()+1); z_edit->set_text(formatter() << widget_->zorder());}))
			.add_col(z_edit)
			.add_col(new button(new label("-", text_size(), font()), [=](){widget_->set_zorder(widget_->zorder()-1); z_edit->set_text(formatter() << widget_->zorder());}));
		g->add_col(new label("Z-order:", text_size(), font()))
			.add_col(zg);

		/*
		*** disabled_opacity : int
		*** id: string 
		*** enabled: bool
		*** visible: bool
		*** alpha: int
		*** frame_set: string
		*** frame_size: int
		*** frame_pad_width: int
		*** frame_pad_height: int
		*** tooltip_delay: int
		*** tooltip_color: colorwheel
		*** tooltip_font: string/font_selector
		*** tooltip_text: string
		*** tooltip_size: int
		*** claim_mouse_events: bool
		*** draw_with_object_shader: bool
		*** x: int
		*** y: int
		*** width: int
		*** height: int
		*** zorder: int
		align_h: left|right|center
		align_v: left|right|center
		on_process: function
		children: widget_list
		*/
		add_widget(g);
	}

	void widget_settings_dialog::id_changed(text_editor_widget_ptr text)
	{
		ASSERT_LOG(text != NULL, "widget_settings_dialog::id_changed: text == NULL");
		ASSERT_LOG(widget_ != NULL, "widget_settings_dialog::id_changed: widget_ == NULL");
		widget_->set_id(text->text());
	}
}