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
#ifndef RICH_TEXT_LABEL_HPP_INCLUDED
#define RICH_TEXT_LABEL_HPP_INCLUDED

#include <string>
#include <vector>

#include "formula_callable.hpp"
#include "scrollable_widget.hpp"
#include "widget.hpp"

namespace gui
{

class rich_text_label : public ScrollableWidget
{
public:
	rich_text_label(const variant& v, game_logic::FormulaCallable* e);

	std::vector<WidgetPtr> getChildren() const;
private:

	void handleProcess() override;
	void handleDraw() const override;
	bool handleEvent(const SDL_Event& event, bool claimed) override;

	variant getValue(const std::string& key) const;
	void setValue(const std::string& key, const variant& v);

	std::vector<std::vector<WidgetPtr> > children_;
};

}

#endif
