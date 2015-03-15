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
	int height = vh- 20;
	if(vh > 480) {
		height -= 100;
	}
	Dialog d(200, (vh > 480) ? 60 : 10, vw - 400, height);
	d.setBackgroundFrame("empty_window");
	d.setDrawBackgroundFn(draw_last_scene);


	for(int n = 0; n < NUM_CONTROLS; ++n) {
		const CONTROL_ITEM item = static_cast<CONTROL_ITEM>(n);
		KeyButtons[item] = KeyButtonPtr(new KeyButton(get_keycode(item), BUTTON_SIZE_DOUBLE_RESOLUTION));
		KeyButtons[item]->setDim(70, 60);
	}

	WidgetPtr t1(new GraphicalFontLabel(_("Directions"), "door_label", 2));
	WidgetPtr b1(KeyButtons[CONTROL_UP]);
	WidgetPtr b2(KeyButtons[CONTROL_DOWN]);
	WidgetPtr b3(KeyButtons[CONTROL_LEFT]);
	WidgetPtr b4(KeyButtons[CONTROL_RIGHT]);
	WidgetPtr t2(new GraphicalFontLabel(_("Jump"), "door_label", 2));
	WidgetPtr b5(KeyButtons[CONTROL_JUMP]);
	WidgetPtr t3(new GraphicalFontLabel(_("Tongue"), "door_label", 2));
	WidgetPtr b6(KeyButtons[CONTROL_TONGUE]);
	WidgetPtr t4(new GraphicalFontLabel(_("Attack"), "door_label", 2));
	WidgetPtr b7(KeyButtons[CONTROL_ATTACK]);
	WidgetPtr b8(new Button(WidgetPtr(new GraphicalFontLabel(_("Back"), "door_label", 2)), std::bind(end_dialog, &d), BUTTON_STYLE_DEFAULT, BUTTON_SIZE_DOUBLE_RESOLUTION));
	b8->setDim(230, 60);

	int start_y = static_cast<int>((d.height() - 4.0*b1->height() - 2.0*t1->height() - 7.0*d.padding())/2.0);
	d.addWidget(t1, static_cast<int>(d.width()/2.0 - b1->width()*1.5 - d.padding()), start_y);
	d.addWidget(b1, static_cast<int>(d.width()/2.0 - b1->width()/2.0), start_y + t1->height() + d.padding());
	d.addWidget(b3, static_cast<int>(d.width()/2.0 - b1->width()*1.5 - d.padding()), static_cast<int>(start_y + t1->height() + b1->height() + 2.0*d.padding()), Dialog::MOVE_DIRECTION::RIGHT);
	d.addWidget(b2, Dialog::MOVE_DIRECTION::RIGHT);
	d.addWidget(b4);

	start_y += t1->height() + 5*d.padding() + 2*b1->height();
	d.addWidget(t2, static_cast<int>(d.width()/2 - b1->width()*1.5 - d.padding()), start_y);
	d.addWidget(b5);
	d.addWidget(t3, d.width()/2 - b1->width()/2, start_y);
	d.addWidget(b6);
	d.addWidget(t4, d.width()/2 + b1->width()/2 + d.padding(), start_y);
	d.addWidget(b7);
	d.addWidget(b8, d.width()/2 - b8->width()/2, start_y + t2->height() + b5->height() + 3*d.padding());

	d.showModal();
}
