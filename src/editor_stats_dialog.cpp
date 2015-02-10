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

#include <algorithm>
#include <iostream>

#include "WindowManager.hpp"

#include "background.hpp"
#include "button.hpp"
#include "editor.hpp"
#include "editor_stats_dialog.hpp"
#include "formatter.hpp"
#include "grid_widget.hpp"
#include "label.hpp"
#include "stats.hpp"

namespace editor_dialogs
{
	EditorStatsDialog::EditorStatsDialog(editor& e)
	  : Dialog(0, 0, KRE::WindowManager::getMainWindow()->width(), KRE::WindowManager::getMainWindow()->height()), 
	  editor_(e)
	{
		setClearBgAmount(255);
		init();
	}

	void EditorStatsDialog::init()
	{
		using namespace gui;
		addWidget(WidgetPtr(new Label("Statistics (whole level)", KRE::Color::colorWhite(), 36)));
	/*
		std::vector<stats::record_ptr> stats = editor_.stats();
		add_stats(stats);

		if(!editor_.selection().empty()) {
			for(stats::record_ptr& s : stats) {
				const point loc = s->location();
				bool in_selection = false;
				const int TileSize = 32;
				for(const point& tile : editor_.selection().tiles) {
					if(loc.x >= tile.x*TileSize && loc.y >= tile.y*TileSize && loc.x < (tile.x+1)*TileSize && loc.y < (tile.y+1)*TileSize) {
						in_selection = true;
						break;
					}
				}

				if(!in_selection) {
					s = stats::record_ptr();
				}
			}

			addWidget(WidgetPtr(new label("Statistics (selection)", KRE::Color::colorWhite(), 36)));
			add_stats(stats);
		}
		*/
	}
}

#endif // !NO_EDITOR
