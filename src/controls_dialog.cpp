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

#include "button.hpp"
#include "controls.hpp"
#include "controls_dialog.hpp"
#include "dialog.hpp"
#include "draw_scene.hpp"
#include "graphical_font_label.hpp"
#include "i18n.hpp"
#include "key_button.hpp"
#include "screen_handling.hpp"

namespace
{
	gui::KeyButtonPtr KeyButtons[controls::NUM_CONTROLS];

	void end_dialog(gui::Dialog* d)
	{
		using namespace controls;
		for(int n = 0; n < NUM_CONTROLS; ++n) {
			const CONTROL_ITEM item = static_cast<CONTROL_ITEM>(n);
			set_keycode(item, KeyButtons[item]->get_key());
		}
		d->close();
	}
}

void show_controls_dialog()
{
	using namespace gui;
	using namespace controls;
	const int vw = graphics::GameScreen::get().getVirtualWidth();
	const int vh = graphics::GameScreen::get().getVirtualHeight();
	
	//HACK: 10 and 4 are the default button padding. Padding should be taken from buttons in KeyButtons list
	int butt_padx = 10;
	int butt_pady = 4;

	int butt_width = 70;
	int butt_height = 60;

	int butt_width_wp = butt_width + butt_padx;
	int butt_height_wp = butt_height + butt_pady;
	
	int sep_y = 50;

	int height = vh- 20;
	/*if(vh > 480) {
		height -= 100;
	}*/
	Dialog d(200, (vh > 480) ? 60 : 10, vw - 400, height);
	d.setBackgroundFrame("empty_window");
	d.setDrawBackgroundFn(draw_last_scene);


	for(int n = 0; n < NUM_CONTROLS; ++n) {
		const CONTROL_ITEM item = static_cast<CONTROL_ITEM>(n);
		KeyButtons[item] = KeyButtonPtr(new KeyButton(get_keycode(item), BUTTON_SIZE_DOUBLE_RESOLUTION));
		KeyButtons[item]->setDim(butt_width, butt_height);
	}

	WidgetPtr t_dirs(new GraphicalFontLabel(_("Directions"), "door_label", 2));
	WidgetPtr b_up(KeyButtons[CONTROL_UP]);
	WidgetPtr b_down(KeyButtons[CONTROL_DOWN]);
	WidgetPtr b_left(KeyButtons[CONTROL_LEFT]);
	WidgetPtr b_right(KeyButtons[CONTROL_RIGHT]);

	WidgetPtr t_jump(new GraphicalFontLabel(_("Jump"), "door_label", 2));
	WidgetPtr b_jump(KeyButtons[CONTROL_JUMP]);
	WidgetPtr t_tongue(new GraphicalFontLabel(_("Tongue"), "door_label", 2));
	WidgetPtr b_tongue(KeyButtons[CONTROL_TONGUE]);
	WidgetPtr t_item(new GraphicalFontLabel(_("Item"), "door_label", 2));
	WidgetPtr b_item(KeyButtons[CONTROL_ATTACK]);
	
	//WidgetPtr b_sprint(KeyButtons[CONTROL_SPRINT]);
	//WidgetPtr t_sprint(new GraphicalFontLabel(_("Sprint"), "door_label", 2));

	WidgetPtr back_button(new Button(WidgetPtr(new GraphicalFontLabel(_("Back"), "door_label", 2)), std::bind(end_dialog, &d), BUTTON_STYLE_DEFAULT, BUTTON_SIZE_DOUBLE_RESOLUTION));
	back_button->setDim(230, 60);

	
	int top_label_height = d.padding();
	int top_label_botm_edge = top_label_height+t_dirs->height();

	int button_grid_width = 3*butt_width_wp;
	int left_edge = d.width()/2 - button_grid_width/2;

	int reference_y = static_cast<int>(d.padding() + butt_height_wp);

	d.addWidget(t_dirs, static_cast<int>(left_edge), static_cast<int>(reference_y));
	reference_y += t_dirs->height();

	d.addWidget(b_up, static_cast<int>(left_edge+butt_width_wp), static_cast<int>(reference_y));
	d.addWidget(b_left, static_cast<int>(left_edge), static_cast<int>(reference_y + butt_height_wp), Dialog::MOVE_DIRECTION::RIGHT);
	d.addWidget(b_down, Dialog::MOVE_DIRECTION::RIGHT);
	d.addWidget(b_right);
	reference_y += butt_height_wp*2 + sep_y;

	d.addWidget(t_jump, left_edge, reference_y);
	d.addWidget(t_tongue, static_cast<int>(left_edge+butt_width_wp), reference_y);
	d.addWidget(t_item, static_cast<int>(left_edge+butt_width_wp*2), reference_y);
	reference_y += t_jump->height();

	d.addWidget(b_jump, left_edge, reference_y, Dialog::MOVE_DIRECTION::RIGHT);
	d.addWidget(b_tongue, Dialog::MOVE_DIRECTION::RIGHT);
	d.addWidget(b_item);

	reference_y += butt_height_wp + sep_y;

	d.addWidget(back_button, d.width()/2 - back_button->width()/2, reference_y);

	d.showModal();
}
