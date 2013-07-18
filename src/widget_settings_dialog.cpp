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
#include "grid_widget.hpp"
#include "label.hpp"
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
		g->add_col(new label("ID:", text_size_, font_name_));
		text_editor_widget* id_edit = new text_editor_widget(150, 30);
		id_edit->set_on_user_change_handler([&](){widget_->set_id(id_edit->text());});
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
		frame_set->set_zorder(10);
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
		tooltip_delay: int
		tooltip_color: colorwheel
		tooltip_font: string/font_selector
		tooltip_text: string
		tooltip_size: int
		claim_mouse_events: bool
		draw_with_object_shader: bool
		x: int
		y: int
		width: int
		height: int
		zorder: int
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