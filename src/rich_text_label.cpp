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

rich_text_label::rich_text_label(const variant& v, game_logic::formula_callable* e)
	: scrollable_widget(v,e)
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
					widget_ptr label_widget_holder(widget_factory::create(label_info, e));
					label_ptr label_widget(static_cast<label*>(label_widget_holder.get()));

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
						label_widget->set_text(words);
					}

					line.erase(line.begin(), line.begin() + candidate.size());
					if(skip_leading_space && candidate.empty() == false && candidate[0] == ' ') {
						candidate.erase(candidate.begin());
					}
					label_widget->set_text(candidate);
					label_widget->set_loc(xpos, ypos);

					if(label_widget->height() > line_height) {
						line_height = label_widget->height();
					}

					xpos += label_widget->width();

					children_.back().push_back(label_widget);
				}
			}
		} else {
			//any widget other than a label
			widget_ptr w(widget_factory::create(item, e));

			if(xpos != 0 && xpos + w->width() > width()) {
				xpos = 0;
				ypos += line_height;
				line_height = 0;
				children_.resize(children_.size()+1);
			}

			if(w->height() > line_height) {
				line_height = w->height();
			}

			w->set_loc(xpos, ypos);

			xpos += w->width();

			children_.back().push_back(w);
		}
	}

	if(v["align"].as_string_default("left") == "right") {
		foreach(std::vector<widget_ptr>& v, children_) {
			if(!v.empty()) {
				const int delta = x() + width() - (v.back()->x() + v.back()->width());
				foreach(widget_ptr w, v) {
					w->set_loc(w->x() + delta, w->y());
				}
			}
		}
	}

	if(v["valign"].as_string_default("center") == "center") {
		foreach(std::vector<widget_ptr>& v, children_) {
			if(!v.empty()) {
				int height = 0;
				foreach(const widget_ptr& w, v) {
					if(w->height() > height) {
						height = w->height();
					}
				}

				foreach(const widget_ptr& w, v) {
					if(w->height() < height) {
						w->set_loc(w->x(), w->y() + (height - w->height())/2);
					}
				}
			}
		}
	}

	if(!v.has_key("height")) {
		//if height isn't given, auto set it.
		set_dim(width(), ypos + line_height);
	}
	
	//ypos + line_height);
	set_virtual_height(ypos + line_height);
	set_arrow_scroll_step(16);
	update_scrollbar();

	set_claim_mouse_events(v["claim_mouse_events"].as_bool(false));
}

std::vector<widget_ptr> rich_text_label::get_children() const
{
	std::vector<widget_ptr> result;
	foreach(const std::vector<widget_ptr>& row, children_) {
		result.insert(result.end(), row.begin(), row.end());
	}

	return result;
}

void rich_text_label::handle_process()
{
	scrollable_widget::handle_process();

	foreach(const std::vector<widget_ptr>& v, children_) {
		foreach(const widget_ptr& widget, v) {
			widget->process();
		}
	}
}

void rich_text_label::handle_draw() const
{
	scrollable_widget::handle_draw();

	const SDL_Rect rect = {x(),y(),width(),height()};
	const graphics::clip_scope clipping_scope(rect);

	glPushMatrix();
	glTranslatef(x() & ~1, (y() & ~1) - yscroll(), 0.0);

	{

	foreach(const std::vector<widget_ptr>& v, children_) {
		foreach(const widget_ptr& widget, v) {
			if(widget->y() > yscroll() + height() ||
			   widget->y() + widget->height() < yscroll()) {
				continue;
			}

			widget->draw();
		}
	}

	}

	glPopMatrix();
}

bool rich_text_label::handle_event(const SDL_Event& event, bool claimed)
{
	claimed = scrollable_widget::handle_event(event, claimed);

	SDL_Event ev = event;
	normalize_event(&ev);
	foreach(const std::vector<widget_ptr>& v, children_) {
		foreach(const widget_ptr& widget, v) {
			claimed = widget->process_event(ev, claimed);
		}
	}

	return claimed;
}

variant rich_text_label::get_value(const std::string& key) const
{
	return scrollable_widget::get_value(key);
}

void rich_text_label::set_value(const std::string& key, const variant& v)
{
	scrollable_widget::set_value(key, v);
}

}
