// XXX needs fixing
#if 0
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
#include "dialog.hpp"
#include "framed_gui_element.hpp"
#include "grid_widget.hpp"
#include "gui_section.hpp"
#include "image_widget.hpp"
#include "input.hpp"
#include "json_parser.hpp"
#include "label.hpp"
#include "module.hpp"
#include "preferences.hpp"
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
	"widget_Slider",
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

gui::WidgetPtr create_widget_from_tool(WIDGET_TOOL tool, size_t x, size_t y)
{
	assert(tool >= TOOL_BUTTON && tool < NUM_WIDGET_TOOLS);
	gui::WidgetPtr p;
	switch(tool) {
	case TOOL_BUTTON:
		p.reset(new gui::button("button", std::bind(dummy_fn, -1, 0.0)));
		break;
	case TOOL_LABEL:
		p.reset(new gui::label("label text", 14, default_font_name));
		break;
	case TOOL_GRID: {
		gui::grid* gg = new gui::grid(1);
		gg->setDim(100,100);
		gg->setShowBackground(true);
		p.reset(gg);
		break;
	}
	case TOOL_DIALOG: {
		gui::dialog* d = new gui::dialog(x, y, 100, 100);
		d->setBackgroundFrame("empty_window");
		return d;
	}
	case TOOL_CHECKBOX:
		p.reset(new gui::Checkbox("Checkbox", false, std::bind(dummy_fn, -1, 0.0)));
		break;
	case TOOL_IMAGE:
		p.reset(new gui::ImageWidget("window-icon.png"));
		break;
	case TOOL_SCROLLBAR:
		p.reset(new gui::ScrollBarWidget(std::bind(dummy_fn, _1, 0.0)));
		break;
	case TOOL_SLIDE:
		p.reset(new gui::Slider(100, std::bind(dummy_fn, -1, _1)));
		break;
	case TOOL_TEXTBOX:
		p.reset(new gui::TextEditorWidget(100, 20));
		break;
	default:
		break;
	}
	if(p) {
		p->setLoc(x, y);
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

	void save(const std::string& fname);
private:
	widget_editor& editor_;
	gui::WidgetPtr placement_;
	WIDGET_TOOL selected_;
	std::set<gui::WidgetPtr, gui::WidgetSortZOrder> widget_list_;
	SDL_Color text_color_;
	size_t info_bar_height_;
	gui::WidgetPtr highlighted_widget_;
	size_t cycle_;
	gui::WidgetPtr selected_widget_;

	void handleDraw() const override;
	bool handleEvent(const SDL_Event& event, bool claimed) override;
	void handleProcess() override;

	widget_window();
	widget_window(const widget_window&);
};
typedef ffl::IntrusivePtr<widget_window> widget_window_ptr;

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

	void set_selected_widget(gui::WidgetPtr w)
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
	std::vector<gui::BorderWidget*> tool_borders_;
	widget_window_ptr ww_;
	gui::WidgetPtr selected_widget_;

	void init()
	{
		clear();
		setClearBgAmount(255);

		if(!ww_) {
			ww_.reset(new widget_window(rect(area_.x(), area_.y(), area_.w()-sidebar_width,area_.h()), *this));
		}
		addWidget(ww_, 0, 0);

		gui::ButtonPtr save_button = new gui::button(new gui::label("Save", graphics::color("antique_white").as_sdl_color(), 16, default_font_name),
			[&](){ww_->save(fname_);});
		if(fname_.empty()) {
			save_button->enable(false);
		}
		addWidget(save_button, area_.x2() - sidebar_width, area_.y() + 4);

		tool_borders_.clear();
		gui::grid_ptr tools_grid(new gui::grid(5));
		for(size_t n = WIDGET_TOOL_FIRST; n != NUM_WIDGET_TOOLS; ++n) {
			gui::ButtonPtr tool_button(
			  new gui::button(gui::WidgetPtr(new gui::GuiSectionWidget(ToolIcons[n], 26, 26)),
				  std::bind(&widget_editor::select_tool, this, static_cast<WIDGET_TOOL>(n))));
			tool_borders_.push_back(new gui::BorderWidget(tool_button, tool_ == n ? KRE::Color::colorWhite() : graphics::color_black()));
			tools_grid->addCol(gui::WidgetPtr(tool_borders_.back()));
		}
		tools_grid->finishRow();

		addWidget(tools_grid, area_.x2() - sidebar_width, save_button->y() + save_button->height() + 4);

		// need a Checkbox for relative/absolute mode

		if(selected_widget_) {
			addWidget(selected_widget_->getSettingsDialog(0, 0, sidebar_width,height()-tools_grid->height()-20), 
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
	info_bar_height_ = KRE::Font::charHeight(14, default_font_name);
	setLoc(area.x(), area.y());
	setDim(area.w(), area.h());
	if(editor_.is_tool_widget()) {
		placement_.reset(new gui::GuiSectionWidget(ToolIcons[editor_.tool()], 26, 26));
	}
}

widget_window::~widget_window()
{}

void widget_window::init()
{
}

void widget_window::save(const std::string& fname)
{
	variant_builder res;
	for(auto widget : widget_list_) {
		res.add("widgets", widget->write());
	}
	sys::write_file(fname, res.build().write_json());
}

void widget_window::handleDraw() const
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
		input::sdl_get_mouse_state(&mx, &my);
		placement_->setLoc(mx, my);
		placement_->draw();
	}

	std::stringstream str;
	int mx, my;
	input::sdl_get_mouse_state(&mx, &my);
	str << "X: " << std::setw(4) << mx << ", Y: " << std::setw(4) << my;
	graphics::blit_texture(font::render_text_uncached(str.str(), text_color_, 14, default_font_name), 0, height() - info_bar_height_);
	glPopMatrix();
}

void widget_window::handleProcess()
{
	++cycle_;

	for(auto w : widget_list_) {
		w->process();
	}
}

bool widget_window::handleEvent(const SDL_Event& event, bool claimed)
{
	if(claimed) {
		return true;
	}

	for(auto w : widget_list_) {
		w->processEvent(event, false);
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
			//Uint8 button_state = input::sdl_get_mouse_state(nullptr, nullptr);
			if(motion.x >= x() && motion.x < x()+width() && motion.y >= y() && motion.y < y()+height()-info_bar_height_ && editor_.tool() < NUM_WIDGET_TOOLS) {
				if((selected_ != editor_.tool() || placement_ == nullptr) && editor_.is_tool_widget()) {
					placement_.reset(new gui::GuiSectionWidget(ToolIcons[editor_.tool()], 26, 26));
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
	GuiSection::init(gui_node);
	FramedGuiElement::init(gui_node);

	if(!fname.empty() && sys::file_exists(fname)) {
		// load file
	}
	
	ffl::IntrusivePtr<widget_editor> editor(new widget_editor(rect(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height()), fname));
	editor->showModal();
}
#endif