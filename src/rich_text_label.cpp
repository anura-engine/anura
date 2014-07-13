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
#include <algorithm>

#include "foreach.hpp"
#include "label.hpp"
#include "raster.hpp"
#include "rich_text_label.hpp"
#include "string_utils.hpp"
#include "variant_utils.hpp"
#include "widget_factory.hpp"

namespace gui
{

namespace {
void flatten_recursively(const std::vector<variant>& v, std::vector<variant>* result)
{
	foreach(const variant& item, v) {
		if(item.is_list()) {
			flatten_recursively(item.as_list(), result);
		} else {
			result->push_back(item);
		}
	}
}

}

rich_text_label::rich_text_label(const variant& v, game_logic::FormulaCallable* e)
	: ScrollableWidget(v,e)
{
	children_.resize(1);
	children_.front().clear();

	int xpos = 0, ypos = 0;
	int line_height = 0;
	std::vector<variant> items;
	flatten_recursively(v["children"].as_list(), &items);
	foreach(const variant& item, items) {
		const std::string widget_type = item["type"].as_string();
		if(widget_type == "label") {
			const std::vector<std::string> lines = util::split(item["text"].as_string(), '\n', 0);

			variant label_info = deep_copy_variant(item);

			for(int n = 0; n != lines.size(); ++n) {
				if(n != 0) {
					xpos = 0;
					ypos += line_height;
					line_height = 0;
					children_.resize(children_.size()+1);
				}

				std::string candidate;
				std::string line = lines[n];
				while(!line.empty()) {
					std::string::iterator space_itor = std::find(line.begin()+1, line.end(), ' ');

					std::string words(line.begin(), space_itor);
					label_info.add_attr_mutation(variant("text"), variant(words));
					WidgetPtr label_widget_holder(widget_factory::create(label_info, e));
					LabelPtr label_widget(static_cast<label*>(label_widget_holder.get()));

					bool skip_leading_space = false;

					if(xpos != 0 && xpos + label_widget->width() > width()) {
						xpos = 0;
						ypos += line_height;
						line_height = 0;
						skip_leading_space = true;
						children_.resize(children_.size()+1);
					}


					candidate = words;

					while(xpos + label_widget->width() < width() && space_itor != line.end()) {
						candidate = words;
						
						space_itor = std::find(space_itor+1, line.end(), ' ');

						words = std::string(line.begin(), space_itor);
						label_widget->setText(words);
					}

					line.erase(line.begin(), line.begin() + candidate.size());
					if(skip_leading_space && candidate.empty() == false && candidate[0] == ' ') {
						candidate.erase(candidate.begin());
					}
					label_widget->setText(candidate);
					label_widget->setLoc(xpos, ypos);

					if(label_widget->height() > line_height) {
						line_height = label_widget->height();
					}

					xpos += label_widget->width();

					children_.back().push_back(label_widget);
				}
			}
		} else {
			//any widget other than a label
			WidgetPtr w(widget_factory::create(item, e));

			if(xpos != 0 && xpos + w->width() > width()) {
				xpos = 0;
				ypos += line_height;
				line_height = 0;
				children_.resize(children_.size()+1);
			}

			if(w->height() > line_height) {
				line_height = w->height();
			}

			w->setLoc(xpos, ypos);

			xpos += w->width();

			children_.back().push_back(w);
		}
	}

	if(v["align"].as_string_default("left") == "right") {
		foreach(std::vector<WidgetPtr>& v, children_) {
			if(!v.empty()) {
				const int delta = x() + width() - (v.back()->x() + v.back()->width());
				foreach(WidgetPtr w, v) {
					w->setLoc(w->x() + delta, w->y());
				}
			}
		}
	}

	if(v["valign"].as_string_default("center") == "center") {
		foreach(std::vector<WidgetPtr>& v, children_) {
			if(!v.empty()) {
				int height = 0;
				foreach(const WidgetPtr& w, v) {
					if(w->height() > height) {
						height = w->height();
					}
				}

				foreach(const WidgetPtr& w, v) {
					if(w->height() < height) {
						w->setLoc(w->x(), w->y() + (height - w->height())/2);
					}
				}
			}
		}
	}

	if(!v.has_key("height")) {
		//if height isn't given, auto set it.
		setDim(width(), ypos + line_height);
	}
	
	//ypos + line_height);
	set_virtual_height(ypos + line_height);
	set_arrow_scroll_step(16);
	update_scrollbar();

	setClaimMouseEvents(v["claim_mouse_events"].as_bool(false));
}

std::vector<WidgetPtr> rich_text_label::getChildren() const
{
	std::vector<WidgetPtr> result;
	foreach(const std::vector<WidgetPtr>& row, children_) {
		result.insert(result.end(), row.begin(), row.end());
	}

	return result;
}

void rich_text_label::handleProcess()
{
	ScrollableWidget::handleProcess();

	foreach(const std::vector<WidgetPtr>& v, children_) {
		foreach(const WidgetPtr& widget, v) {
			widget->process();
		}
	}
}

void rich_text_label::handleDraw() const
{
	ScrollableWidget::handleDraw();

	const SDL_Rect rect = {x(),y(),width(),height()};
	const graphics::clip_scope clipping_scope(rect);

	glPushMatrix();
	glTranslatef(x() & ~1, (y() & ~1) - getYscroll(), 0.0);

	{

	foreach(const std::vector<WidgetPtr>& v, children_) {
		foreach(const WidgetPtr& widget, v) {
			if(widget->y() > getYscroll() + height() ||
			   widget->y() + widget->height() < getYscroll()) {
				continue;
			}

			widget->draw();
		}
	}

	}

	glPopMatrix();
}

bool rich_text_label::handleEvent(const SDL_Event& event, bool claimed)
{
	claimed = ScrollableWidget::handleEvent(event, claimed);

	SDL_Event ev = event;
	normalizeEvent(&ev);
	foreach(const std::vector<WidgetPtr>& v, children_) {
		foreach(const WidgetPtr& widget, v) {
			claimed = widget->processEvent(ev, claimed);
		}
	}

	return claimed;
}

variant rich_text_label::getValue(const std::string& key) const
{
	return ScrollableWidget::getValue(key);
}

void rich_text_label::setValue(const std::string& key, const variant& v)
{
	ScrollableWidget::setValue(key, v);
}

}
