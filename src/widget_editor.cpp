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

#include <iomanip>
#include <set>
#include <sstream>

#include "border_widget.hpp"
#include "button.hpp"
#include "checkbox.hpp"
#include "color_utils.hpp"
#include "dialog.hpp"
#include "font.hpp"
#include "framed_gui_element.hpp"
#include "graphics.hpp"
#include "grid_widget.hpp"
#include "gui_section.hpp"
#include "image_widget.hpp"
#include "json_parser.hpp"
#include "label.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "unit_test.hpp"
#include "scrollbar_widget.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"
#include "widget_factory.hpp"

const int sidebar_width = 300;
const std::string default_font_name = "Montaga-Regular";

const char* ToolIcons[] = {
	"editor_select_object",
	"editor_rect_select",
	"widget_button",
	"widget_label",
	"widget_grid",
	"widget_dialog",
	"widget_checkbox",
	"widget_image",
	"widget_scrollbar",
	"widget_slider",
	"widget_textbox",
};

enum WIDGET_TOOL {
	WIDGET_TOOL_FIRST,
	TOOL_SELECT = WIDGET_TOOL_FIRST,
	TOOL_RECT_SELECT,

	TOOL_BUTTON,
	TOOL_LABEL,
	TOOL_GRID,
	TOOL_DIALOG,
	TOOL_CHECKBOX,
	TOOL_IMAGE,
	TOOL_SCROLLBAR,
	TOOL_SLIDE,
	TOOL_TEXTBOX,
	NUM_WIDGET_TOOLS,
};

void dummy_fn(int n, double d) {
}

gui::widget_ptr create_widget_from_tool(WIDGET_TOOL tool, size_t x, size_t y)
{
	assert(tool >= TOOL_BUTTON && tool < NUM_WIDGET_TOOLS);
	gui::widget_ptr p;
	switch(tool) {
	case TOOL_BUTTON:
		p.reset(new gui::button("button", boost::bind(dummy_fn, -1, 0.0)));
		break;
	case TOOL_LABEL:
		p.reset(new gui::label("label text", 14, default_font_name));
		break;
	case TOOL_GRID:
		p.reset(new gui::grid(1));
		break;
	case TOOL_DIALOG: {
		gui::dialog* d = new gui::dialog(x, y, 100, 100);
		d->set_background_frame("empty_window");
		return d;
	}
	case TOOL_CHECKBOX:
		p.reset(new gui::checkbox("checkbox", false, boost::bind(dummy_fn, -1, 0.0)));
		break;
	case TOOL_IMAGE:
		p.reset(new gui::image_widget("window-icon.png"));
		break;
	case TOOL_SCROLLBAR:
		p.reset(new gui::scrollbar_widget(boost::bind(dummy_fn, _1, 0.0)));
		break;
	case TOOL_SLIDE:
		p.reset(new gui::slider(100, boost::bind(dummy_fn, -1, _1)));
		break;
	case TOOL_TEXTBOX:
		p.reset(new gui::text_editor_widget(100, 20));
		break;
	default:
		break;
	}
	if(p) {
		p->set_loc(x, y);
	}
	return p;
}

class widget_editor;

class widget_window : public gui::widget
{
public:
	widget_window(const rect& area, widget_editor& editor);
	virtual ~widget_window();
	void init();
private:
	widget_editor& editor_;
	gui::widget_ptr placement_;
	WIDGET_TOOL selected_;
	std::set<gui::widget_ptr, gui::widget_sort_zorder> widget_list_;
	SDL_Color text_color_;
	size_t info_bar_height_;
	gui::widget_ptr highlighted_widget_;
	size_t cycle_;
	gui::widget_ptr selected_widget_;

	void handle_draw() const;
	bool handle_event(const SDL_Event& event, bool claimed);
	void handle_process();

	widget_window();
	widget_window(const widget_window&);
};
typedef boost::intrusive_ptr<widget_window> widget_window_ptr;

class widget_editor : public gui::dialog
{
public:
	explicit widget_editor(const rect& r, const std::string& fname)
		: gui::dialog(r.x(), r.y(), r.w(), r.h()), area_(r), fname_(fname),
		tool_(TOOL_SELECT)
	{
		init();
	}

	WIDGET_TOOL tool() const { return tool_; }
	bool is_tool_widget() const { return tool_ >= TOOL_BUTTON; }

	void set_selected_widget(gui::widget_ptr w)
	{
		selected_widget_ = w;
		init();
	}

	virtual ~widget_editor()
	{}
private:
	rect area_;
	std::string fname_;
	WIDGET_TOOL tool_;
	std::vector<gui::border_widget*> tool_borders_;
	widget_window_ptr ww_;
	gui::widget_ptr selected_widget_;

	void init()
	{
		clear();
		set_clear_bg_amount(255);

		if(!ww_) {
			ww_.reset(new widget_window(rect(area_.x(), area_.y(), area_.w()-sidebar_width,area_.h()), *this));
		}
		add_widget(ww_, 0, 0);

		tool_borders_.clear();
		gui::grid_ptr tools_grid(new gui::grid(5));
		for(size_t n = WIDGET_TOOL_FIRST; n != NUM_WIDGET_TOOLS; ++n) {
			gui::button_ptr tool_button(
			  new gui::button(gui::widget_ptr(new gui::gui_section_widget(ToolIcons[n], 26, 26)),
				  boost::bind(&widget_editor::select_tool, this, static_cast<WIDGET_TOOL>(n))));
			tool_borders_.push_back(new gui::border_widget(tool_button, tool_ == n ? graphics::color_white() : graphics::color_black()));
			tools_grid->add_col(gui::widget_ptr(tool_borders_.back()));
		}
		tools_grid->finish_row();

		add_widget(tools_grid, area_.x2() - sidebar_width, area_.y() + 4);

		// need a checkbox for relative/absolute mode

		if(selected_widget_) {
			add_widget(selected_widget_->get_settings_dialog(0, 0, sidebar_width,height()-tools_grid->height()-20), 
				area_.x2()-sidebar_width, 
				tools_grid->height()+20);
		}
	}

	void select_tool(WIDGET_TOOL tool)
	{
		tool_ = tool;
		init();
	}

	widget_editor();
	widget_editor(const widget_editor&);
};

widget_window::widget_window(const rect& area, widget_editor& editor)
	: editor_(editor), selected_(editor.tool()),
	text_color_(graphics::color("antique_white").as_sdl_color())
{
	info_bar_height_ = font::char_height(14, default_font_name);
	set_loc(area.x(), area.y());
	set_dim(area.w(), area.h());
	if(editor_.is_tool_widget()) {
		placement_.reset(new gui::gui_section_widget(ToolIcons[editor_.tool()], 26, 26));
	}
}

widget_window::~widget_window()
{}

void widget_window::init()
{
}

void widget_window::handle_draw() const
{
	glPushMatrix();
	glTranslatef(GLfloat(x() & ~1), GLfloat(y() & ~1), 0.0);

	for(auto widget : widget_list_) {
		widget->draw();
	}

	if(highlighted_widget_) {
		graphics::draw_rect(rect(highlighted_widget_->x()-2, 
			highlighted_widget_->y()-2, 
			highlighted_widget_->width()+4, 
			highlighted_widget_->height()+4), 
			graphics::color(255,255,255,92));
	}
	if(selected_widget_) {
		graphics::draw_hollow_rect(rect(selected_widget_->x()-2, 
			selected_widget_->y()-2, 
			selected_widget_->width()+4, 
			selected_widget_->height()+4), 
			graphics::color(255,255,255,255));
	}

	if(placement_) {
		int mx, my;
		SDL_GetMouseState(&mx, &my);
		placement_->set_loc(mx, my);
		placement_->draw();
	}

	std::stringstream str;
	int mx, my;
	SDL_GetMouseState(&mx, &my);
	str << "X: " << std::setw(4) << mx << ", Y: " << std::setw(4) << my;
	graphics::blit_texture(font::render_text_uncached(str.str(), text_color_, 14, default_font_name), 0, height() - info_bar_height_);
	glPopMatrix();
}

void widget_window::handle_process()
{
	++cycle_;

	for(auto w : widget_list_) {
		w->process();
	}
}

bool widget_window::handle_event(const SDL_Event& event, bool claimed)
{
	if(claimed) {
		return true;
	}

	for(auto w : widget_list_) {
		w->process_event(event, false);
	}

	switch(event.type) {
		case SDL_KEYUP: {
			const SDL_KeyboardEvent& key = event.key;
			if(highlighted_widget_ && key.keysym.sym == SDLK_DELETE) {
				if(highlighted_widget_.get() == selected_widget_.get()) {
					selected_widget_.reset();
				}
				auto it = widget_list_.find(highlighted_widget_);
				if(it != widget_list_.end()) {
					widget_list_.erase(it);
				}
				highlighted_widget_.reset();
			}
			break;
		}

		case SDL_MOUSEMOTION: {
			const SDL_MouseMotionEvent& motion = event.motion;
			//Uint8 button_state = SDL_GetMouseState(NULL, NULL);
			if(motion.x >= x() && motion.x < x()+width() && motion.y >= y() && motion.y < y()+height()-info_bar_height_ && editor_.tool() < NUM_WIDGET_TOOLS) {
				if((selected_ != editor_.tool() || placement_ == NULL) && editor_.is_tool_widget()) {
					placement_.reset(new gui::gui_section_widget(ToolIcons[editor_.tool()], 26, 26));
					selected_ = editor_.tool();
				} else if(editor_.tool() == TOOL_SELECT) {
					bool highlight_one = false;
					for(auto widget : widget_list_) {
						if(motion.x >= widget->x() && motion.x < widget->x()+widget->width() && motion.y >= widget->y() && motion.y < widget->y()+widget->height()) {
							highlighted_widget_ = widget;
							highlight_one = true;
						}
					}
					if(!highlight_one) {
						highlighted_widget_.reset();
					}
				}
				return true;
			} else {
				placement_.reset();
			}
			break;
		}
		case SDL_MOUSEBUTTONDOWN: {
			const SDL_MouseButtonEvent& button = event.button;
			if(button.x >= x() && button.x < x()+width() && button.y >= y() && button.y < y()+height()-info_bar_height_) {
				// mouse in our area
				if(editor_.is_tool_widget()) {
					// placing widget
					widget_list_.insert(create_widget_from_tool(editor_.tool(), button.x-x(), button.y-y()));
				} else if(editor_.tool() == TOOL_SELECT) {
					// select or rect select
					if(highlighted_widget_) {
						selected_widget_ = highlighted_widget_;
						highlighted_widget_.reset();
						editor_.set_selected_widget(selected_widget_);
					}
				}
				return true;
			}
			break;
		}
	}
	return false;
}


UTILITY(widget_editor)
{
	std::deque<std::string> arguments(args.begin(), args.end());

	ASSERT_LOG(arguments.size() <= 1, "Unexpected arguments");

	std::string fname;
	if(arguments.empty() == false) {
		fname = module::map_file(arguments.front());
	}

	variant gui_node = json::parse_from_file("data/gui.cfg");
	gui_section::init(gui_node);
	framed_gui_element::init(gui_node);

	if(!fname.empty() && !sys::file_exists(fname)) {
		// create new file
	}
	
	boost::intrusive_ptr<widget_editor> editor(new widget_editor(rect(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height()), fname));
	editor->show_modal();
}
