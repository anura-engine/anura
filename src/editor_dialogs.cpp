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

#ifndef NO_EDITOR

#include "kre/WindowManager.hpp"

#include "dialog.hpp"
#include "editor_dialogs.hpp"
#include "grid_widget.hpp"
#include "label.hpp"
#include "load_level.hpp"
#include "widget.hpp"

namespace 
{
	void do_select_level(gui::Dialog* d, const std::vector<std::string>& levels, int index, std::string* result) 
	{
		if(index >= 0 && static_cast<unsigned>(index) < levels.size()) {
			d->close();
			*result = levels[index];
		}
	}
}

std::string show_choose_level_dialog(const std::string& prompt)
{
	using namespace gui;
	using std::placeholders::_1;

	Dialog d(0, 0, KRE::WindowManager::getMainWindow()->width(), KRE::WindowManager::getMainWindow()->height());
	d.addWidget(WidgetPtr(new Label(prompt, KRE::Color::colorWhite(), 48)));

	std::string result;
	std::vector<std::string> levels = get_known_levels();
	Grid* grid = new Grid(1);
	grid->setMaxHeight(KRE::WindowManager::getMainWindow()->height() - 80);
	grid->setShowBackground(true);
	grid->allowSelection();

	grid->registerSelectionCallback(std::bind(&do_select_level, &d, levels, _1, &result));
	for(const std::string& lvl : levels) {
		grid->addCol(WidgetPtr(new Label(lvl, KRE::Color::colorWhite())));
	}

	d.addWidget(WidgetPtr(grid));
	d.showModal();
	return result;
}

#endif // NO_EDITOR
