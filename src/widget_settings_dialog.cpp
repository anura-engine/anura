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

#include "asserts.hpp"
#include "checkbox.hpp"
#include "color_picker.hpp"
#include "dropdown_widget.hpp"
#include "font.hpp"
#include "formatter.hpp"
#include "grid_widget.hpp"
#include "input.hpp"
#include "label.hpp"
#include "preferences.hpp"
#include "widget_settings_dialog.hpp"

namespace gui
{
	WidgetSettingsDialog::WidgetSettingsDialog(int x, int y, int w, int h, WidgetPtr ptr) 
		: dialog(x,y,w,h), widget_(ptr), text_size_(14)
	{
		ASSERT_LOG(ptr != NULL, "WidgetSettingsDialog::WidgetSettingsDialog: widget_ == NULL");
		init();
	}

	WidgetSettingsDialog::~WidgetSettingsDialog()
	{
	}

	void WidgetSettingsDialog::setFont(const std::string& fn) 
	{ 
		font_name_ = fn; 
		init();
	}
	void WidgetSettingsDialog::setTextSize(int ts) 
	{ 
		text_size_ = ts; 
		init();
	}

	void WidgetSettingsDialog::init()
	{
		setClearBgAmount(255);
	
		grid_ptr g = grid_ptr(new grid(2));
		g->setMaxHeight(height()-50);
		g->addCol(new label("ID:", text_size_, font_name_));
		TextEditorWidgetPtr id_edit = new TextEditorWidget(150, 30);
		id_edit->setText(widget_->id());
		id_edit->setOnUserChangeHandler([=](){widget_->setId(id_edit->text());});
		g->addCol(id_edit);

		g->addCol(new label("", getTextSize(), font()))
			.addCol(new Checkbox(WidgetPtr(new label("Enabled", text_size_, font_name_)), 
			!widget_->disabled(), 
			[&](bool checked){widget_->enable(!checked);}, 
			BUTTON_SIZE_NORMAL_RESOLUTION));

		g->addCol(new label("Disabled Opacity:", getTextSize(), font()))
			.addCol(new Slider(120, [&](double f){this->widget_->setDisabledOpacity(int(f*255.0));}, 
			this->widget_->disabledOpacity()/255.0, 1));

		g->addCol(new label("", getTextSize(), font()))
			.addCol(new Checkbox(WidgetPtr(new label("Visible", text_size_, font_name_)), 
			!widget_->visible(), 
			[&](bool checked){}, 
			BUTTON_SIZE_NORMAL_RESOLUTION));

		g->addCol(new label("Alpha:", getTextSize(), font()))
			.addCol(new Slider(120, [&](double f){widget_->setAlpha(int(f*255.0));}, 
			this->widget_->getAlpha()/255.0, 1));

		std::vector<std::string> sections = FramedGuiElement::getElements();
		sections.insert(sections.begin(), "<<none>>");
		DropdownWidgetPtr frame_set(new DropdownWidget(sections, 150, 28, dropdown_widget::DROPDOWN_LIST));
		frame_set->setFontSize(14);
		frame_set->setDropdownHeight(height());
		
		auto it = std::find(sections.begin(), sections.end(), widget_->frameSetName());
		frame_set->setSelection(it == sections.end() ? 0 : it-sections.begin());
		frame_set->setOnSelectHandler([&](int n, const std::string& s){
			if(s != "<<none>>") {
				widget_->setFrameSet(s);
			} else {
				widget_->setFrameSet("");
			}
		});
		frame_set->setZOrder(20);
		g->addCol(new label("Frame Set:", getTextSize(), font()));
		g->addCol(frame_set);

		g->addCol(new label("", getTextSize(), font()))
			.addCol(new Checkbox(WidgetPtr(new label("Double frame size", text_size_, font_name_)), 
			widget_->getFrameResolution() != 0, 
			[&](bool checked){widget_->setFrameResolution(checked ? 1 : 0);}, 
			BUTTON_SIZE_NORMAL_RESOLUTION));

		g->addCol(new label("pad width:", getTextSize(), font()))
			.addCol(new Slider(120, [&](double f){widget_->setPadding(int(f*100.0), widget_->getPadHeight());}, 
			widget_->getPadWidth()/100.0, 1));
		g->addCol(new label("pad height:", getTextSize(), font()))
			.addCol(new Slider(120, [&](double f){widget_->setPadding(widget_->getPadWidth(), int(f*100.0));}, 
			widget_->getPadHeight()/100.0, 1));

		TextEditorWidgetPtr tooltip_edit = new TextEditorWidget(150, 30);
		tooltip_edit->setText(widget_->tooltipText());
		tooltip_edit->setOnUserChangeHandler([=](){widget_->setTooltipText(tooltip_edit->text());});
		g->addCol(new label("Tooltip:", text_size_, font_name_))
			.addCol(tooltip_edit);
		g->addCol(new label("Tooltip Height:", getTextSize(), font()))
			.addCol(new Slider(120, [&](double f){widget_->setTooltipFontSize(int(f*72.0+6.0));}, 
			(widget_->tooltipFontSize()-6.0)/72.0, 1));
		
		std::vector<std::string> fonts = font::getAvailableFonts();
		fonts.insert(fonts.begin(), "");
		DropdownWidgetPtr font_list(new DropdownWidget(fonts, 150, 28, dropdown_widget::DROPDOWN_LIST));
		font_list->setFontSize(14);
		font_list->setDropdownHeight(height());
		auto fit = std::find(fonts.begin(), fonts.end(), widget_->tooltipFont());
		font_list->setSelection(fit == fonts.end() ? 0 : fit-fonts.begin());
		font_list->setOnSelectHandler([&](int n, const std::string& s){widget_->setTooltipFont(s);});
		font_list->setZOrder(19);
		g->addCol(new label("Tooltip Font:", getTextSize(), font()))
			.addCol(font_list);
		g->addCol(new label("Tooltip Color:", getTextSize(), font()))
			.addCol(new button(new label("Choose...", text_size_, font_name_), [&](){
				int mx, my;
				input::sdl_get_mouse_state(&mx, &my);
				mx = mx + 200 > preferences::actual_screen_width() ? preferences::actual_screen_width()-200 : mx;
				my = my + 600 > preferences::actual_screen_height() ? preferences::actual_screen_height()-600 : my;
				my -= y();
				ColorPicker* cp = new ColorPicker(rect(0, 0, 200, 600), [&](const graphics::color& color){widget_->setTooltipColor(color);});
				cp->setPrimaryColor(graphics::color(widget_->tooltipColor()));

				grid_ptr gg = new grid(1);
				gg->allowSelection();
				gg->swallowClicks();
				gg->setShowBackground(true);
				gg->allowDrawHighlight(false);
				gg->registerSelectionCallback([=](int n){std::cerr << "n = " << n << std::endl; if(n != 0){removeWidget(gg); init();}});
				gg->setZOrder(100);
				gg->addCol(cp);
				addWidget(gg, x()-mx-100, my);
		}));

		g->addCol(new label("Tooltip Delay:", getTextSize(), font()))
			.addCol(new Slider(120, [&](double f){widget_->setTooltipDelay(int(f*5000.0));}, 
			widget_->getTooltipDelay()/5000.0, 1));

		g->addCol(new label("", getTextSize(), font()))
			.addCol(new Checkbox(WidgetPtr(new label("Claim Mouse Events", text_size_, font_name_)), 
			claimMouseEvents(), 
			[&](bool checked){widget_->setClaimMouseEvents(checked);}, 
			BUTTON_SIZE_NORMAL_RESOLUTION));

		g->addCol(new label("", getTextSize(), font()))
			.addCol(new Checkbox(WidgetPtr(new label("Draw with Object shader", text_size_, font_name_)), 
			drawWithObjectShader(), 
			[&](bool checked){widget_->setDrawWithObjectShader(checked);}, 
			BUTTON_SIZE_NORMAL_RESOLUTION));

		g->addCol(new label("Width:", getTextSize(), font()))
			.addCol(new Slider(120, [&](double f){widget_->setDim(int(f*width()), widget_->height());}, 
			widget_->width()/double(width()), 1));
		g->addCol(new label("Height:", getTextSize(), font()))
			.addCol(new Slider(120, [&](double f){widget_->setDim(widget_->width(), int(f*height()));}, 
			widget_->height()/double(height()), 1));

		g->addCol(new label("X:", getTextSize(), font()))
			.addCol(new Slider(120, [&](double f){widget_->setLoc(int(f*width()), widget_->y());}, 
			widget_->x()/double(width()), 1));
		g->addCol(new label("Y:", getTextSize(), font()))
			.addCol(new Slider(120, [&](double f){widget_->setLoc(widget_->x(), int(f*height()));}, 
			widget_->y()/double(height()), 1));

		grid* zg = new grid(3);
		TextEditorWidgetPtr z_edit = new TextEditorWidget(60, 30);
		z_edit->setText(formatter() << widget_->zorder());
		z_edit->setOnUserChangeHandler([=](){widget_->setZOrder(atoi(z_edit->text().c_str()));});
		zg->addCol(new button(new label("+", getTextSize(), font()), [=](){widget_->setZOrder(widget_->zorder()+1); z_edit->setText(formatter() << widget_->zorder());}))
			.addCol(z_edit)
			.addCol(new button(new label("-", getTextSize(), font()), [=](){widget_->setZOrder(widget_->zorder()-1); z_edit->setText(formatter() << widget_->zorder());}));
		g->addCol(new label("Z-order:", getTextSize(), font()))
			.addCol(zg);

		grid* ahg = new grid(3);
		ahg->addCol(new button(new label("Left", getTextSize(), font()), [&](){widget_->set_halign(HALIGN_LEFT);}))
			.addCol(new button(new label("Center", getTextSize(), font()), [&](){widget_->set_halign(HALIGN_CENTER);}))
			.addCol(new button(new label("Right", getTextSize(), font()), [&](){widget_->set_halign(HALIGN_RIGHT);}));
		g->addCol(new label("Horiz Align:", getTextSize(), font()))
			.addCol(ahg);

		grid* avg = new grid(3);
		avg->addCol(new button(new label("Top", getTextSize(), font()), [&](){widget_->set_valign(VALIGN_TOP);}))
			.addCol(new button(new label("Center", getTextSize(), font()), [&](){widget_->set_valign(VALIGN_CENTER);}))
			.addCol(new button(new label("Bottom", getTextSize(), font()), [&](){widget_->set_valign(VALIGN_BOTTOM);}));
		g->addCol(new label("Vert Align:", getTextSize(), font()))
			.addCol(avg);
		/*
		on_process: function
		children: widget_list
		*/
		addWidget(g);
	}

	void WidgetSettingsDialog::idChanged(TextEditorWidgetPtr text)
	{
		ASSERT_LOG(text != NULL, "WidgetSettingsDialog::idChanged: text == NULL");
		ASSERT_LOG(widget_ != NULL, "WidgetSettingsDialog::idChanged: widget_ == NULL");
		widget_->setId(text->text());
	}
}
