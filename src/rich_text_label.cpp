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

#include <algorithm>

#include "ClipScope.hpp"

#include "label.hpp"
#include "rich_text_label.hpp"
#include "string_utils.hpp"
#include "variant_utils.hpp"
#include "widget_factory.hpp"

namespace gui
{
	namespace 
	{
		void flatten_recursively(const std::vector<variant>& v, std::vector<variant>* result)
		{
			for(const variant& item : v) {
				if(item.is_list()) {
					flatten_recursively(item.as_list(), result);
				} else {
					result->push_back(item);
				}
			}
		}
	}

	RichTextLabel::RichTextLabel(const variant& v, game_logic::FormulaCallable* e)
		: ScrollableWidget(v,e)
	{
		children_.resize(1);
		children_.front().clear();

		int xpos = 0, ypos = 0;
		int line_height = 0;
		std::vector<variant> items;
		flatten_recursively(v["children"].as_list(), &items);
		for(const variant& item : items) {
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
						LabelPtr label_widget(static_cast<Label*>(label_widget_holder.get()));

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
			for(auto& v : children_) {
				if(!v.empty()) {
					const int delta = x() + width() - (v.back()->x() + v.back()->width());
					for(WidgetPtr w : v) {
						w->setLoc(w->x() + delta, w->y());
					}
				}
			}
		}

		if(v["valign"].as_string_default("center") == "center") {
			for(std::vector<WidgetPtr>& v : children_) {
				if(!v.empty()) {
					int height = 0;
					for(const WidgetPtr& w : v) {
						if(w->height() > height) {
							height = w->height();
						}
					}

					for(const WidgetPtr& w : v) {
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
		setVirtualHeight(ypos + line_height);
		setArrowScrollStep(16);
		updateScrollbar();

		setClaimMouseEvents(v["claim_mouse_events"].as_bool(false));
	}

	std::vector<WidgetPtr> RichTextLabel::getChildren() const
	{
		std::vector<WidgetPtr> result;
		for(const auto& row : children_) {
			result.insert(result.end(), row.begin(), row.end());
		}

		return result;
	}

	void RichTextLabel::handleProcess()
	{
		ScrollableWidget::handleProcess();

		for(const std::vector<WidgetPtr>& v : children_) {
			for(const WidgetPtr& widget : v) {
				widget->process();
			}
		}
	}

	void RichTextLabel::handleDraw() const
	{
		ScrollableWidget::handleDraw();

		using namespace KRE;
		ClipScope::Manager clip_scope(rect(x(),y(),width(),height()));

		for(const std::vector<WidgetPtr>& v : children_) {
			for(const WidgetPtr& widget : v) {
				if(widget->y() > getYscroll() + height() ||
				   widget->y() + widget->height() < getYscroll()) {
					continue;
				}

				widget->draw(x() & ~1, (y() & ~1) - getYscroll());
			}
		}
	}

	bool RichTextLabel::handleEvent(const SDL_Event& event, bool claimed)
	{
		claimed = ScrollableWidget::handleEvent(event, claimed);

		SDL_Event ev = event;
		//normalizeEvent(&ev);
		for(const std::vector<WidgetPtr>& v : children_) {
			for(const WidgetPtr& widget : v) {
				claimed = widget->processEvent(getPos(), ev, claimed);
			}
		}

		return claimed;
	}

	BEGIN_DEFINE_CALLABLE(RichTextLabel, ScrollableWidget)
		DEFINE_FIELD(dummy, "null")
			return variant();
	END_DEFINE_CALLABLE(RichTextLabel)
}
