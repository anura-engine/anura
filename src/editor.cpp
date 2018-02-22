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
#include <iostream>
#include <cmath>
#include <algorithm>

#include <boost/algorithm/string.hpp>
#if defined(_MSC_VER)
#include <boost/math/special_functions/round.hpp>
#define bmround	boost::math::round
#else
#define bmround	round
#endif

#include "Canvas.hpp"
#include "ColorScope.hpp"
#include "Effects.hpp"
#include "Font.hpp"
#include "ModelMatrixScope.hpp"
#include "WindowManager.hpp"

#include "asserts.hpp"
#include "border_widget.hpp"
#include "button.hpp"
#include "character_editor_dialog.hpp"
#include "checkbox.hpp"
#include "code_editor_dialog.hpp"
#include "collision_utils.hpp"
#include "custom_object_dialog.hpp"
#include "dialog.hpp"
#include "draw_scene.hpp"
#include "draw_tile.hpp"
#include "debug_console.hpp"
#include "editor.hpp"
#include "editor_dialogs.hpp"
#include "editor_formula_functions.hpp"
#include "editor_layers_dialog.hpp"
#include "editor_level_properties_dialog.hpp"
#include "editor_module_properties_dialog.hpp"
#include "editor_stats_dialog.hpp"
#include "entity.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "formula_callable_definition.hpp"
#include "frame.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "json_parser.hpp"
#include "label.hpp"
#include "level.hpp"
#include "level_object.hpp"
#include "level_runner.hpp"
#include "load_level.hpp"
#include "module.hpp"
#include "multiplayer.hpp"
#include "object_events.hpp"
#include "player_info.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "property_editor_dialog.hpp"
#include "screen_handling.hpp"
#include "segment_editor_dialog.hpp"
#include "stats.hpp"
#include "text_editor_widget.hpp"
#include "tile_map.hpp"
#include "tileset_editor_dialog.hpp"
#include "tooltip.hpp"
#include "variant.hpp"
#include "variant_utils.hpp"

extern int g_tile_scale;
extern int g_tile_size;
#define BaseTileSize g_tile_size

using std::placeholders::_1;
using std::placeholders::_2;

#ifndef _MSC_VER
#pragma GCC diagnostic ignored "-Wmissing-braces"
#endif
// XXX: fix this in the code?

extern std::string g_editor_object;

namespace 
{
	
	class BuiltinEditor : public editor
	{
	public:
		BuiltinEditor(const char* level_cfg) : editor(level_cfg)
		{}

		virtual void process() override;
		virtual bool handleEvent(const SDL_Event& event, bool swallowed) override;

		virtual void draw_gui() const override;

	private:
	};
	
	class CustomEditor : public editor
	{
	public:
		CustomEditor(const char* level_cfg) : editor(level_cfg)
		{}

		virtual void process() override {}

		virtual bool handleEvent(const SDL_Event& event, bool swallowed) override {
			return false;
		}

		virtual void draw_gui() const override {
		}

	private:
	};

	//keep a map of editors so that when we edit a level and then later
	//come back to it we'll save all the state we had previously
	std::map<std::string, EditorPtr> all_editors;

	//the last level we edited
	std::string& g_last_edited_level() 
	{
		static std::string str;
		return str;
	}

	bool g_draw_stats = false;

	void toggle_draw_stats() 
	{
		g_draw_stats = !g_draw_stats;
	}

	PREF_BOOL_PERSISTENT(editor_grid, true, "Turns the editor grid on/off");

	void toggle_draw_grid() 
	{
		g_editor_grid = !g_editor_grid;
	}
}

class editor_menu_dialog : public gui::Dialog
{
public:
	struct menu_item {
		std::string description;
		std::string hotkey;
		std::function<void()> action;
	};

	void showMenu(const std::vector<menu_item>& items) {
		using namespace gui;
		gui::Grid* grid = new gui::Grid(2);
		grid->setHpad(40);
		grid->setShowBackground(true);
		grid->allowSelection();
		grid->swallowClicks();
		grid->swallowAllEvents();
		grid->registerSelectionCallback(std::bind(&editor_menu_dialog::executeMenuItem, this, items, _1));
		for(const menu_item& item : items) {
			grid->addCol(WidgetPtr(new Label(item.description, KRE::Color::colorWhite()))).
			      addCol(WidgetPtr(new Label(item.hotkey, KRE::Color::colorWhite())));
		}

		int mousex, mousey;
		input::sdl_get_mouse_state(&mousex, &mousey);

		mousex -= x();
		mousey -= y();

		removeWidget(context_menu_);
		context_menu_.reset(grid);
		LOG_DEBUG(mousex << "," << mousey);
		addWidget(context_menu_, mousex, mousey);
	}

private:
	void executeMenuItem(const std::vector<menu_item>& items, int n) 
	{
		if(n >= 0 && static_cast<unsigned>(n) < items.size()) {
			items[n].action();
		}

		removeWidget(context_menu_);
		context_menu_.reset();
	}

	void show_file_menu() 
	{
		menu_item items[] = {
			"New...", "", std::bind(&editor_menu_dialog::new_level, this),
			"Open...", "ctrl+o", std::bind(&editor_menu_dialog::open_level, this),
			"Save", "ctrl+s", std::bind(&editor::save_level, &editor_),
			"Save As...", "", std::bind(&editor_menu_dialog::save_level_as, this),
// This doesn't really work. Use --create-module utility instead.
//			"Create New Module...", "", std::bind(&editor::create_new_module, &editor_),
			"Edit Module Properties...", "", std::bind(&editor::edit_module_properties, &editor_),
			"Create New Object...", "", std::bind(&editor::create_new_object, &editor_),
			"Exit", "<esc>", std::bind(&editor::quit, &editor_),
		};

		std::vector<menu_item> res;
		for(const menu_item& m : items) {
			res.push_back(m);
		}
		showMenu(res);
	}

	void show_edit_menu() {
		menu_item items[] = {
			"Level Properties", "", std::bind(&editor::edit_level_properties, &editor_),
			"Undo", "u", std::bind(&editor::undo_command, &editor_),
			"Redo", "r", std::bind(&editor::redo_command, &editor_),
			"Restart Level", "ctrl+r", std::bind(&editor::reset_playing_level, &editor_, true),
			"Restart Level (including player)", "ctrl+alt+r", std::bind(&editor::reset_playing_level, &editor_, false),
			"Pause Game", "ctrl+p", std::bind(&editor::toggle_pause, &editor_),
			"Code", "", std::bind(&editor::toggle_code, &editor_),
			"Shaders", "", std::bind(&editor::edit_shaders, &editor_),
			"Level Code", "", std::bind(&editor::edit_level_code, &editor_),
			"Add Subcomponent", "", std::bind(&editor::add_new_sub_component, &editor_),
		};

		menu_item duplicate_item = { "Duplicate Object(s)", "ctrl+1", std::bind(&editor::duplicate_selected_objects, &editor_) };

		std::vector<menu_item> res;
		for(const menu_item& m : items) {
			res.push_back(m);
		}

		if(editor_.get_level().editor_selection().empty() == false) {
			res.push_back(duplicate_item);
		}

		showMenu(res);
	}

	void show_view_menu() 
	{
		menu_item items[] = {
			"Zoom Out", "x", std::bind(&editor::zoomOut, &editor_),
			"Zoom In", "z", std::bind(&editor::zoomIn, &editor_),
			editor_.get_level().show_foreground() ? "Hide Foreground" : "Show Foreground", "f", std::bind(&Level::setShowForeground, &editor_.get_level(), !editor_.get_level().show_foreground()),
			editor_.get_level().show_background() ? "Hide Background" : "Show Background", "b", std::bind(&Level::setShowBackground, &editor_.get_level(), !editor_.get_level().show_background()),
			g_draw_stats ? "Hide Stats" : "Show Stats", "", toggle_draw_stats,
			g_editor_grid ? "Hide Grid" : "Show Grid", "", toggle_draw_grid,
			preferences::show_debug_hitboxes() ? "Hide Hit Boxes" : "Show Hit Boxes", "h", [](){ preferences::toogle_debug_hitboxes(); },
		};

		std::vector<menu_item> res;
		for(const menu_item& m : items) {
			res.push_back(m);
		}
		showMenu(res);
	}

	void show_stats_menu()
	{
		menu_item items[] = {
		        "Details...", "", std::bind(&editor::show_stats, &editor_),
		        "Refresh stats", "", std::bind(&editor::download_stats, &editor_),
		};
		std::vector<menu_item> res;
		for(const menu_item& m : items) {
			res.push_back(m);
		}
		showMenu(res);
	}

	void show_scripts_menu() 
	{
		std::vector<menu_item> res;
		for(const editor_script::info& script : editor_script::all_scripts()) {
			menu_item item = { script.name, "", std::bind(&editor::run_script, &editor_, script.name) };
			res.push_back(item);
		}
		
		showMenu(res);
	}

	void show_window_menu() 
	{
		std::vector<menu_item> res;
		for(std::map<std::string, EditorPtr>::const_iterator i = all_editors.begin(); i != all_editors.end(); ++i) {
			std::string name = i->first;
			if(name == g_last_edited_level()) {
				name += " *";
			}
			menu_item item = { name, "", std::bind(&editor_menu_dialog::open_level_in_editor, this, i->first) };
			res.push_back(item);
		}
		showMenu(res);
	}

	editor& editor_;
	gui::WidgetPtr context_menu_;
	gui::ButtonPtr code_button_;
	std::string code_button_text_;
public:
	explicit editor_menu_dialog(editor& e)
	  : gui::Dialog(0, 0, e.xres() ? e.xres() : 1200, EDITOR_MENUBAR_HEIGHT),
	  editor_(e)
	{
		setClearBgAmount(255);
		init();
	}

	void init() 
	{
		clear();

		using namespace gui;
		gui::Grid* grid = new gui::Grid(6);
		grid->addCol(WidgetPtr(
		  new Button(WidgetPtr(new Label("File", KRE::Color::colorWhite())),
		             std::bind(&editor_menu_dialog::show_file_menu, this))));
		grid->addCol(WidgetPtr(
		  new Button(WidgetPtr(new Label("Edit", KRE::Color::colorWhite())),
		             std::bind(&editor_menu_dialog::show_edit_menu, this))));
		grid->addCol(WidgetPtr(
		  new Button(WidgetPtr(new Label("View", KRE::Color::colorWhite())),
		             std::bind(&editor_menu_dialog::show_view_menu, this))));
		grid->addCol(WidgetPtr(
		  new Button(WidgetPtr(new Label("Window", KRE::Color::colorWhite())),
		             std::bind(&editor_menu_dialog::show_window_menu, this))));
		grid->addCol(WidgetPtr(
		  new Button(WidgetPtr(new Label("Statistics", KRE::Color::colorWhite())),
		             std::bind(&editor_menu_dialog::show_stats_menu, this))));

		grid->addCol(WidgetPtr(
		  new Button(WidgetPtr(new Label("Scripts", KRE::Color::colorWhite())),
		             std::bind(&editor_menu_dialog::show_scripts_menu, this))));
		addWidget(WidgetPtr(grid));

		code_button_text_ = "";
		set_code_button_text("Code ->");
	}

	void set_code_button_text(const std::string& text)
	{
		using namespace gui;

		if(code_button_text_ == text) {
			return;
		}

		code_button_text_ = text;

		if(code_button_) {
			removeWidget(code_button_);
		}

		if(text.empty()) {
			return;
		}

		code_button_ = ButtonPtr(new Button(text, std::bind(&editor::toggle_code, &editor_)));

		addWidget(code_button_, (editor_.xres() ? editor_.xres() : 1200) - 612, 4);
	}


	void new_level() {
		using namespace gui;
		auto wnd = KRE::WindowManager::getMainWindow();
		Dialog d(100, 100, wnd->width()-200, wnd->height()-200);
		d.setBackgroundFrame("empty_window");
		d.setDrawBackgroundFn(draw_last_scene);
		d.setCursor(20, 20);
		d.addWidget(WidgetPtr(new Label("New Level", KRE::Color::colorWhite(), 48)));
		TextEditorWidget* entry = new TextEditorWidget(200);
		entry->setOnEnterHandler(std::bind(&Dialog::close, &d));
		entry->setFocus(true);
		d.addWidget(WidgetPtr(new Label("Filename:", KRE::Color::colorWhite())))
		 .addWidget(WidgetPtr(entry));

		Checkbox* clone_level_check = new Checkbox("Clone current level", false, [](bool value) {});
		d.addWidget(WidgetPtr(clone_level_check));

		GridPtr ok_cancel_grid(new gui::Grid(2));
		ok_cancel_grid->setHpad(12);
		ok_cancel_grid->addCol(WidgetPtr(
		  new Button(WidgetPtr(new Label("Ok", KRE::Color::colorWhite())),
		             [&d]() { d.close(); })));

		ok_cancel_grid->addCol(WidgetPtr(
		  new Button(WidgetPtr(new Label("Cancel", KRE::Color::colorWhite())),
		             [&d]() { d.cancel(); })));

		ok_cancel_grid->finishRow();

		d.addWidget(ok_cancel_grid);

		d.showModal();
		
		std::string name = entry->text();
		if(name.empty() == false) {
			if(name.size() < 4 || std::equal(name.end()-4, name.end(), ".cfg") == false) {
				name += ".cfg";
			}

			variant empty_lvl;
			
			if(clone_level_check->checked()) {
				empty_lvl = Level::current().write();
			} else {
				empty_lvl = json::parse_from_file("data/level/empty.cfg");
			}

			std::string id = module::make_module_id(name);
			empty_lvl.add_attr(variant("id"), variant(module::get_id(id)));
			std::string nn = module::get_id(name);
			std::string modname = module::get_module_id(name);
			sys::write_file(module::get_module_path(modname, (preferences::editor_save_to_user_preferences() ? module::BASE_PATH_USER : module::BASE_PATH_GAME)) + "data/level/" + nn, empty_lvl.write_json());
			load_level_paths();
			editor_.close();
			g_last_edited_level() = id;
		}
	}

	void save_level_as() 
	{
		using namespace gui;
		auto wnd = KRE::WindowManager::getMainWindow();
		Dialog d(0, 0, wnd->width(), wnd->height());
		d.addWidget(WidgetPtr(new Label("Save As", KRE::Color::colorWhite(), 48)));
		TextEditorWidget* entry = new TextEditorWidget(200);
		entry->setOnEnterHandler(std::bind(&Dialog::close, &d));
		d.addWidget(WidgetPtr(new Label("Name:", KRE::Color::colorWhite())))
		 .addWidget(WidgetPtr(entry));
		d.showModal();
		
		if(!d.cancelled() && entry->text().empty() == false) {
			editor_.save_level_as(entry->text());
		}
	}

	void open_level() 
	{
		open_level_in_editor(show_choose_level_dialog("Open Level"));
	}

	void open_level_in_editor(const std::string& lvl) 
	{
		if(lvl.empty() == false && lvl != g_last_edited_level()) {
			removeWidget(context_menu_);
			context_menu_.reset();
			editor_.close();
			g_last_edited_level() = lvl;
		}
	}

};

namespace 
{
	const char* ModeStrings[] = {
		"Tiles", 
		"Objects", 
		"Properties",
	};

	const char* ToolStrings[] = {
		"Add tiles by drawing rectangles",
		"Select Tiles",
		"Select connected regions of tiles",
		"Add tiles by drawing pencil strokes",
		"Pick tiles or objects",
		"Add Objects",
		"Select Objects",
		"Edit Level Segments",
	};

	const char* ToolIcons[] = {
		"editor_draw_rect", 
		"editor_rect_select", 
		"editor_wand", 
		"editor_pencil", 
		"editor_eyedropper", 
		"editor_add_object", 
		"editor_select_object", 
		"editor_rect_select", 
	};
}

class editor_mode_dialog : public gui::Dialog
{
	editor& editor_;
	gui::WidgetPtr context_menu_;

	std::vector<gui::BorderWidget*> tool_borders_;

	void select_tool(int tool)
	{
		if(tool >= 0 && tool < editor::NUM_TOOLS) {
			editor_.change_tool(static_cast<editor::EDIT_TOOL>(tool));
		}
	}

	bool handleEvent(const SDL_Event& event, bool claimed) override
	{
		if(!claimed) {
			const bool ctrl_pressed = (SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) != 0;
			if(ctrl_pressed) {
				return false;
			}
		}

		return claimed || Dialog::handleEvent(event, claimed);
	}

public:
	explicit editor_mode_dialog(editor& e)
	  : gui::Dialog(KRE::WindowManager::getMainWindow()->width() - EDITOR_SIDEBAR_WIDTH, 0, EDITOR_SIDEBAR_WIDTH, 160), editor_(e)
	{
		setClearBgAmount(255);
		init();
	}

	void init()
	{
		using namespace gui;
		clear();

		tool_borders_.clear();

		GridPtr grid(new Grid(3));
		for(int n = 0; n < editor::NUM_TOOLS; ++n) {
			if(n == editor::TOOL_EDIT_SEGMENTS && editor_.get_level().segment_width() == 0 && editor_.get_level().segment_height() == 0) {
				continue;
			}
			ButtonPtr tool_button(
			  new Button(WidgetPtr(new GuiSectionWidget(ToolIcons[n], 26, 26)),
			             std::bind(&editor_mode_dialog::select_tool, this, n)));
			tool_button->setTooltip(ToolStrings[n]);
			tool_borders_.push_back(new BorderWidget(tool_button, KRE::Color(0,0,0,0)));
			grid->addCol(WidgetPtr(tool_borders_.back()));
		}

		grid->finishRow();
		addWidget(grid, 5, 5);

		refreshSelection();
	}

	void refreshSelection() {
		using namespace gui;
		for(int n = 0; n != tool_borders_.size(); ++n) {
			tool_borders_[n]->setColor(n == editor_.tool() ? KRE::Color::colorWhite() : KRE::Color(0,0,0,0));
		}
	}
};

namespace 
{
	const int RectEdgeSelectThreshold = 6;

	void execute_functions(const std::vector<std::function<void()> >& v) 
	{
		for(const std::function<void()>& f : v) {
			f();
		}
	}

	bool g_started_dragging_object = false;

	//the current state of the rectangle we're dragging
	rect g_rect_drawing;

	//the tiles that we've drawn in the current action.
	std::vector<point> g_current_draw_tiles;

	const EditorVariableInfo* g_variable_editing = nullptr;
	int g_variable_editing_index = -1;
	variant g_variable_editing_original_value;
	const EditorVariableInfo* variable_info_selected(ConstEntityPtr e, int xpos, int ypos, int zoom, int* index_selected=nullptr)
	{
		if(index_selected) {
			*index_selected = -1;
		}

		if(!e || !e->getEditorInfo()) {
			return nullptr;
		}

		for(const EditorVariableInfo& var : e->getEditorInfo()->getVarsAndProperties()) {
			const variant value = e->queryValue(var.getVariableName());
			switch(var.getType()) {
				case VARIABLE_TYPE::XPOSITION: {
					if(!value.is_int()) {
						break;
					}

					if(xpos >= value.as_int() - zoom*RectEdgeSelectThreshold && xpos <= value.as_int() + zoom*RectEdgeSelectThreshold) {
						return &var;
					}
					break;
				}
				case VARIABLE_TYPE::YPOSITION: {
					if(!value.is_int()) {
						break;
					}

					if(ypos >= value.as_int() - zoom*RectEdgeSelectThreshold && ypos <= value.as_int() + zoom*RectEdgeSelectThreshold) {
						return &var;
					}
					break;
				}
				case VARIABLE_TYPE::POINTS: {
					if(!value.is_list()) {
						break;
					}

					int index = 0;
					for(variant p : value.as_list()) {
						point pt(p);
						if(pointInRect(point(xpos, ypos), rect(pt.x-10, pt.y-10, 20, 20))) {
							if(index_selected) {
								*index_selected = index;
							}
							return &var;
						}

						++index;
					}
				}
				default:
					break;
			}
		}

		return nullptr;
	}

	int round_tile_size(int n)
	{
		if(n >= 0) {
			return n - n%TileSize;
		} else {
			n = -n + 32;
			return -(n - n%TileSize);
		}
	}

	bool resizing_left_level_edge = false,
		 resizing_right_level_edge = false,
		 resizing_top_level_edge = false,
		 resizing_bottom_level_edge = false;
	
	bool resizing_sub_component_bottom_edge = false,
	     resizing_sub_component_right_edge = false;
	bool dragging_sub_component = false;
	int resizing_sub_component_index = -1;

	int dragging_sub_component_usage_index = -1;

	rect modify_selected_rect(const editor& e, rect boundaries, int xpos, int ypos) 
	{
		const int x = round_tile_size(xpos);
		const int y = round_tile_size(ypos);

		if(resizing_left_level_edge) {
			boundaries = rect(x, boundaries.y(), boundaries.w() + (boundaries.x() - x), boundaries.h());
			if(e.get_level().segment_width() > 0) {
				while(boundaries.w()%e.get_level().segment_width() != 0) {
					boundaries = rect(boundaries.x()-1, boundaries.y(), boundaries.w()+1, boundaries.h());
				}
			}
		}

		if(resizing_right_level_edge) {
			boundaries = rect(boundaries.x(), boundaries.y(), x - boundaries.x(), boundaries.h());
			if(e.get_level().segment_width() > 0) {
				while(boundaries.w()%e.get_level().segment_width() != 0) {
					boundaries = rect(boundaries.x(), boundaries.y(), boundaries.w()+1, boundaries.h());
				}
			}
		}

		if(resizing_top_level_edge) {
			boundaries = rect(boundaries.x(), y, boundaries.w(), boundaries.h() + (boundaries.y() - y));
			if(e.get_level().segment_height() > 0) {
				while(boundaries.h()%e.get_level().segment_height() != 0) {
					boundaries = rect(boundaries.x(), boundaries.y()-1, boundaries.w(), boundaries.h()+1);
				}
			}
		}

		if(resizing_bottom_level_edge) {
			boundaries = rect(boundaries.x(), boundaries.y(), boundaries.w(), y - boundaries.y());
			if(e.get_level().segment_height() > 0) {
				while(boundaries.h()%e.get_level().segment_height() != 0) {
					boundaries = rect(boundaries.x(), boundaries.y(), boundaries.w(), boundaries.h()+1);
				}
			}
		}

		return boundaries;
	}


	rect findSubComponentArea(const Level::SubComponent& sub, int xpos, int ypos, int zoom)
	{
		return rect((sub.source_area.x() + (sub.source_area.w()+TileSize*4)*sub.num_variations + 20 - xpos)/zoom, (sub.source_area.y() + 20 - ypos)/zoom, 16, 16);
	}

	//find if an edge of a rectangle is selected
	bool rect_left_edge_selected(const rect& r, int x, int y, int zoom) 
	{
		return y >= r.y() - RectEdgeSelectThreshold*zoom &&
			   y <= r.y2() + RectEdgeSelectThreshold*zoom &&
			   x >= r.x() - RectEdgeSelectThreshold*zoom &&
			   x <= r.x() + RectEdgeSelectThreshold*zoom;
	}

	bool rect_right_edge_selected(const rect& r, int x, int y, int zoom) 
	{
		return y >= r.y() - RectEdgeSelectThreshold*zoom &&
			   y <= r.y2() + RectEdgeSelectThreshold*zoom &&
			   x >= r.x2() - RectEdgeSelectThreshold*zoom &&
			   x <= r.x2() + RectEdgeSelectThreshold*zoom;
	}

	bool rect_top_edge_selected(const rect& r, int x, int y, int zoom) 
	{
		return x >= r.x() - RectEdgeSelectThreshold*zoom &&
			   x <= r.x2() + RectEdgeSelectThreshold*zoom &&
			   y >= r.y() - RectEdgeSelectThreshold*zoom &&
			   y <= r.y() + RectEdgeSelectThreshold*zoom;
	}

	bool rect_bottom_edge_selected(const rect& r, int x, int y, int zoom) 
	{
		return x >= r.x() - RectEdgeSelectThreshold*zoom &&
			   x <= r.x2() + RectEdgeSelectThreshold*zoom &&
			   y >= r.y2() - RectEdgeSelectThreshold*zoom &&
			   y <= r.y2() + RectEdgeSelectThreshold*zoom;
	}

	bool rect_any_edge_selected(const rect& r, int x, int y, int zoom)
	{
		return rect_left_edge_selected(r, x, y, zoom) ||
		       rect_right_edge_selected(r, x, y, zoom) ||
		       rect_top_edge_selected(r, x, y, zoom) ||
		       rect_bottom_edge_selected(r, x, y, zoom);
	}

	bool is_rect_selected(const rect& r, int x, int y, int zoom)
	{
		return x >= r.x() &&
			   x <= r.x2() &&
			   y >= r.y() &&
			   y <= r.y2();
	}

	std::vector<editor::tileset> tilesets;

	std::vector<editor::enemy_type> enemy_types;

	int selected_property = 0;
}

editor::manager::~manager() 
{
	enemy_types.clear();
}

editor::enemy_type::enemy_type(const std::string& type, const std::string& category, variant frame_info)
  : category(category), 
  frame_info_(frame_info)
{
	variant_builder new_node;
	new_node.add("type", type);
	new_node.add("custom", true);
	new_node.add("face_right", false);
	new_node.add("x", 1500);
	new_node.add("y", 0);

	node = new_node.build();
}

const EntityPtr& editor::enemy_type::preview_object() const
{
	if(!preview_object_) {
		preview_object_ = Entity::build(node);
	}

	return preview_object_;
}

const ffl::IntrusivePtr<const Frame>& editor::enemy_type::preview_frame() const
{
	if(!preview_frame_) {
		if(frame_info_.is_map() && !preview_object_) {
			preview_frame_.reset(new Frame(frame_info_));
		} else {
			LOG_WARN("COULD NOT READ FROM FRAME: " << frame_info_.write_json());
			preview_frame_.reset(new Frame(preview_object()->getCurrentFrame()));
		}
	}

	return preview_frame_;
}

void editor::tileset::init(variant node)
{
	for(variant tileset_node : node["tileset"].as_list()) {
		tilesets.push_back(editor::tileset(tileset_node));
	}
}

editor::tileset::tileset(variant node)
  : category(node["category"].as_string()), 
    type(node["type"].as_string()),
    zorder(parse_zorder(node["zorder"])),
	x_speed(node["x_speed"].as_int(100)),
	y_speed(node["y_speed"].as_int(100)),
	sloped(node["sloped"].as_bool()),
	node_info(node)
{
}

std::shared_ptr<TileMap> editor::tileset::preview() const
{
	if(!preview_ && node_info.has_key("preview")) {
		preview_.reset(new TileMap(node_info["preview"]));
	}

	return preview_;
}


EditorPtr editor::get_editor(const char* level_cfg)
{
	EditorPtr& e = all_editors[level_cfg];
	if(!e) {
		if(g_editor_object.empty()) {
			e.reset(new BuiltinEditor(level_cfg));
		} else {
			e.reset(new CustomEditor(level_cfg));
		}
	}
	e->done_ = false;
	return e;
}

// This returns the area for the ENTIRE code editor, including the area with buttons.
rect editor::get_code_editor_rect()
{
	return rect(KRE::WindowManager::getMainWindow()->width() - 620, 30, 620, KRE::WindowManager::getMainWindow()->height() - 60);
}

std::string editor::last_edited_level()
{
	return g_last_edited_level();
}

namespace 
{
	int g_codebar_width = 0;
}

PREF_BOOL(editor_history, false, "Allow editor history feature");

int editor::sidebar_width()
{
	return g_codebar_width == 0 ? 180 : g_codebar_width;
}

int editor::codebar_height()
{
	return 0; //g_codebar_height;
}

editor::editor(const char* level_cfg)
  : zoom_(1), 
    xpos_(0), 
	ypos_(0), 
	anchorx_(0), 
	anchory_(0),
    selected_entity_startx_(0), 
	selected_entity_starty_(0),
    filename_(level_cfg), 
	tool_(TOOL_ADD_RECT),
    done_(false), 
	face_right_(true), 
	upside_down_(false),
	cur_tileset_(0), 
	cur_object_(0),
    current_dialog_(nullptr), 
	drawing_rect_(false), 
	dragging_(false), 
	level_changed_(0),
	selected_segment_(-1),
	mouse_buttons_down_(0), 
	prev_mousex_(-1), 
	prev_mousey_(-1),
	xres_(0), 
	yres_(0), 
	middle_mouse_deltax_(0),
	middle_mouse_deltay_(0),
	mouselook_mode_(false)
{
	LOG_INFO("BEGIN EDITOR::EDITOR");
	const int begin = profile::get_tick_time();

	if(g_editor_history) {
		preferences::set_record_history(true);
	}

	static bool first_time = true;
	if(first_time) {
		variant editor_cfg = json::parse_from_file_or_die("data/editor.cfg");
		const int begin = profile::get_tick_time();
		TileMap::loadAll();
		const int mid = profile::get_tick_time();
		LOG_INFO("TileMap::loadAll(): " << (mid-begin) << "ms");
		tileset::init(editor_cfg);
		LOG_INFO("tileset::init(): " << (profile::get_tick_time() - mid) << "ms");
		first_time = false;
		if(editor_cfg.is_map()) {
			if(editor_cfg["resolution"].is_null() == false) {
				std::vector<int> v = editor_cfg["resolution"].as_list_int();
				xres_ = v[0];
				yres_ = v[1];
			}
		}
	}

	assert(!tilesets.empty());
	lvl_.reset(new Level(level_cfg));
	lvl_->set_editor();
	lvl_->finishLoading();
	lvl_->setAsCurrentLevel();

	levels_.push_back(lvl_);

	editor_menu_dialog_.reset(new editor_menu_dialog(*this));
	editor_mode_dialog_.reset(new editor_mode_dialog(*this));

	property_dialog_.reset(new editor_dialogs::PropertyEditorDialog(*this));

	if(preferences::external_code_editor().is_null() == false && !external_code_editor_) {
		external_code_editor_ = ExternalTextEditor::create(preferences::external_code_editor());
	}

	LOG_INFO("END EDITOR::EDITOR: " << (profile::get_tick_time() - begin) << "ms");
}

editor::~editor()
{
	if(g_editor_history) {
		preferences::set_record_history(false);
	}
}

void editor::group_selection()
{
	std::vector<std::function<void()> > undo, redo;

	for(LevelPtr lvl : levels_) {
		const int group = lvl->add_group();
		for(const EntityPtr& e : lvl_->editor_selection()) {
			EntityPtr c = lvl->get_entity_by_label(e->label());
			if(!c) {
				continue;
			}

			undo.push_back(std::bind(&Level::set_character_group, lvl.get(), c, c->group()));
			redo.push_back(std::bind(&Level::set_character_group, lvl.get(), c, group));
		}
	}

	executeCommand(
		std::bind(execute_functions, redo),
		std::bind(execute_functions, undo));
}

void editor::toggle_facing()
{
	face_right_ = !face_right_;
	if(character_dialog_) {
		character_dialog_->init();
	}

	begin_command_group();
	for(const EntityPtr& e : lvl_->editor_selection()) {
		for(LevelPtr lvl : levels_) {
			EntityPtr obj = lvl->get_entity_by_label(e->label());
			if(obj) {
				executeCommand(std::bind(&editor::toggle_object_facing, this, lvl, obj, false),
				               std::bind(&editor::toggle_object_facing, this, lvl, obj, false));
			}
		}
	}
	end_command_group();

	on_modify_level();
}

void editor::toggle_isUpsideDown()
{
	upside_down_ = !upside_down_;
	if(character_dialog_) {
		character_dialog_->init();
	}

	begin_command_group();
	for(const EntityPtr& e : lvl_->editor_selection()) {
		for(LevelPtr lvl : levels_) {
			EntityPtr obj = lvl->get_entity_by_label(e->label());
			if(obj) {
				executeCommand(std::bind(&editor::toggle_object_facing, this, lvl, obj, true),
				               std::bind(&editor::toggle_object_facing, this, lvl, obj, true));
			}
		}
	}
	end_command_group();

	on_modify_level();
}

//First, note the current angle of the mouse from the object. Then, when the rotation changes, calculate the difference.
float rotation_reference_degrees = 0; //ideally, this would be an array for each object being rotated. Right now, it uses the last object selected and calculates an absolute rotation from the group from it. However, it would be more useful if each object was rotated by the degrees rotated. (This should work the same way as rotation does in Blender3D.) In addition, it would be nice if we could calculate the midpoint of all objects selected and rotate around *that*, instead of just the last object selected.
void editor::set_rotate_reference()
{
	const float radians_to_degrees = 57.29577951308232087f;
	
	int mousex, mousey;
    input::sdl_get_mouse_state(&mousex, &mousey);
	mousex = xpos_ + mousex*zoom_;
	mousey = ypos_ + mousey*zoom_;
	
	if(character_dialog_) {
		character_dialog_->init();
	}

	for(const EntityPtr& e : lvl_->editor_selection()) {
		const int selx = e->x() + e->getCurrentFrame().width()/2; //This might not work correctly, I can't tell because the editor is so distorted.
		const int sely = e->y() + e->getCurrentFrame().height()/2;
		rotation_reference_degrees = atan2(mousey-sely, mousex-selx)*radians_to_degrees - e->getRotateZ().as_float();
	}
}

void editor::change_rotation()
{
	const float radians_to_degrees = 57.29577951308232087f;
	
	const bool ctrl_pressed = (SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) != 0;
	
	int mousex, mousey;
    input::sdl_get_mouse_state(&mousex, &mousey);
	mousex = xpos_ + mousex*zoom_;
	mousey = ypos_ + mousey*zoom_;
	
	if(character_dialog_) {
		character_dialog_->init();
	}
	
	float new_angle = 0;
	for(const EntityPtr& e : lvl_->editor_selection()) {
		const int selx = e->x() + e->getCurrentFrame().width()/2; //This might not work correctly, I can't tell because the editor is so distorted.
		const int sely = e->y() + e->getCurrentFrame().height()/2;
		new_angle = atan2(mousey-sely, mousex-selx)*radians_to_degrees - rotation_reference_degrees;
	}
	
	if(!ctrl_pressed) {
		const float snap_step = 360.0/16;
		new_angle = round(new_angle/snap_step)*snap_step;
	}
	
	new_angle = fmod(new_angle, 360); //360 = 0, but only 0 = don't serialize the value in the level file

	for(const EntityPtr& e : lvl_->editor_selection()) {
		if((int) (e->getRotateZ().as_float()*1000) == (int) (new_angle*1000)) { //Compare as integers so free rotation doesn't always result in a falsehood here; some loss of granularity.
			continue; //this doesn't prevent some sort of long rebuild from running if nothing passes
		}
		
		for(LevelPtr lvl : levels_) {
			EntityPtr obj = lvl->get_entity_by_label(e->label());
			if(obj) {
				executeCommand(std::bind(&editor::change_object_rotation, this, lvl, obj, new_angle),
				               std::bind(&editor::change_object_rotation, this, lvl, obj, e->getRotateZ().as_float())); //subsequent undo steps should not stack
			}
		}
	}

	on_modify_level();
}

//Note the difference between the object's scale and the mouse distance from the object's point of origin. Use this to recompute the object's scale as the mouse position changes.
float scale_reference_ratio = 0; 
void editor::set_scale_reference()
{
	int mousex, mousey;
    input::sdl_get_mouse_state(&mousex, &mousey);
	mousex = xpos_ + mousex*zoom_;
	mousey = ypos_ + mousey*zoom_;
	
	if(character_dialog_) {
		character_dialog_->init();
	}

	for(const EntityPtr& e : lvl_->editor_selection()) {
		const int selx = e->x() + e->getCurrentFrame().width()/2; //This might not work correctly, I can't tell because the editor is so distorted.
		const int sely = e->y() + e->getCurrentFrame().height()/2;
		scale_reference_ratio = e->getDrawScale().as_float() / sqrt(pow(mousey-sely, 2) + pow(mousex-selx, 2)); //Doesn't handle negative scales yet; for now flip and invert can be used to mimic it. This feature should work like in Blender 3D, "If the mouse pointer crosses from the original side of the pivot point to the opposite side, the scale will continue in the negative direction, making the object/data appear flipped (mirrored)." (http://wiki.blender.org/index.php/Doc:2.4/Manual/3D_interaction/Transformations/Basics/Scale)
	}
}

void editor::change_scale()
{
	const bool ctrl_pressed = (SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) != 0;
	
	int mousex, mousey;
	input::sdl_get_mouse_state(&mousex, &mousey);
	mousex = xpos_ + mousex*zoom_;
	mousey = ypos_ + mousey*zoom_;
	
	if(character_dialog_) {
		character_dialog_->init();
	}

	float new_scale = 0;
	for(const EntityPtr& e : lvl_->editor_selection()) {
		const int selx = e->x() + e->getCurrentFrame().width()/2; //This might not work correctly, I can't tell because the editor is so distorted.
		const int sely = e->y() + e->getCurrentFrame().height()/2;
		new_scale = scale_reference_ratio * sqrt(pow(mousey-sely, 2) + pow(mousex-selx, 2)); //Doesn't handle negative scales yet; for now flip and invert can be used to mimic it. This feature should work like in Blender 3D, "If the mouse pointer crosses from the original side of the pivot point to the opposite side, the scale will continue in the negative direction, making the object/data appear flipped (mirrored)." (http://wiki.blender.org/index.php/Doc:2.4/Manual/3D_interaction/Transformations/Basics/Scale)
	}
	
	if(!ctrl_pressed) {
		if(new_scale >= 1) {
			new_scale = round(new_scale);
		} else {
			new_scale = 1.0/round(1.0/new_scale);
		}
	}
	
	//Keep the object from disappearing completely, because it's not recoverable then.
	const float editor_min_scale = 0.1;
	if(new_scale < editor_min_scale) {
		new_scale = editor_min_scale;
	}
	
	for(const EntityPtr& e : lvl_->editor_selection()) {
		if((int) (e->getDrawScale().as_float()*1000) == (int) (new_scale*1000)) { //Compare as integers so free rotation doesn't always result in a falsehood here; some loss of granularity.
			continue; //this doesn't prevent some sort of long rebuild from running if nothing passes
		}
		
		for(LevelPtr lvl : levels_) {
			EntityPtr obj = lvl->get_entity_by_label(e->label());
			if(obj) {
				executeCommand(std::bind(&editor::change_object_scale, this, lvl, obj, new_scale),
				               std::bind(&editor::change_object_scale, this, lvl, obj, e->getDrawScale().as_float())); //subsequent undo steps should not stack
			}
		}
	}

	on_modify_level();
}

void editor::duplicate_selected_objects()
{
	std::vector<std::function<void()>> redo, undo;
	for(const EntityPtr& c : lvl_->editor_selection()) {
		EntityPtr duplicate_obj = c->clone();

		for(LevelPtr lvl : levels_) {
			EntityPtr obj = duplicate_obj->backup();
			if(!place_entity_in_level_with_large_displacement(*lvl, *obj)) {
				continue;
			}
		
			redo.push_back(std::bind(&editor::add_object_to_level, this, lvl, duplicate_obj));
			undo.push_back(std::bind(&editor::remove_object_from_level, this, lvl, duplicate_obj));
		}
	}

	executeCommand(
		std::bind(execute_functions, redo),
		std::bind(execute_functions, undo));

	on_modify_level();
}

void editor::process_ghost_objects()
{
	if(editing_level_being_played()) {
		return;
	}

	lvl_->swap_chars(ghost_objects_);

	const std::vector<EntityPtr> chars = lvl_->get_chars();
	for(const EntityPtr& p : chars) {
		p->process(*lvl_);
	}

	for(const EntityPtr& p : chars) {
		p->handleEvent(OBJECT_EVENT_DRAW);
	}

	lvl_->swap_chars(ghost_objects_);

	for(EntityPtr& p : ghost_objects_) {
		if(p && p->destroyed()) {
			lvl_->remove_character(p);
			p = EntityPtr();
		}
	}

	ghost_objects_.erase(std::remove(ghost_objects_.begin(), ghost_objects_.end(), EntityPtr()), ghost_objects_.end());
}

void editor::remove_ghost_objects()
{
	for(EntityPtr c : ghost_objects_) {
		lvl_->remove_character(c);
	}
}

namespace 
{
	int editor_resolution_manager_count = 0;

	int editor_x_resolution = 0, editor_y_resolution = 0;
}

bool EditorResolutionManager::isActive()
{
	return editor_resolution_manager_count != 0;
}

EditorResolutionManager::EditorResolutionManager(int xres, int yres) 
	: original_width_(KRE::WindowManager::getMainWindow()->width()),
	  original_height_(KRE::WindowManager::getMainWindow()->height()) 
{
	LOG_INFO("EDITOR RESOLUTION MANAGER: " << xres << ", " << yres);

	// XXX Some notes for fixing this.
	// If we are in fullscreen mode need to do things differently.
	// We keep the resolution the same and shrink the GameScreen down to accomodate
	// This is assuming the fullscreen window size is adequate for our needs.
	//
	// We should be checking the maximum monitor resolution, 1200 seems anachronistic in
	// these days where 4k monitors are available and 1920 being a very common width.
	if(!editor_x_resolution) {
		if(xres != 0 && yres != 0) {
			editor_x_resolution = xres;
			editor_y_resolution = yres;
		} else {
			if(original_width_ > 1200) {
				editor_x_resolution = KRE::WindowManager::getMainWindow()->width() + EDITOR_SIDEBAR_WIDTH  + editor_dialogs::LAYERS_DIALOG_WIDTH;
			} else {
				editor_x_resolution = 1200; //KRE::WindowManager::getMainWindow()->width() + EDITOR_SIDEBAR_WIDTH + editor_dialogs::LAYERS_DIALOG_WIDTH;
			}
			editor_y_resolution = KRE::WindowManager::getMainWindow()->height() + EDITOR_MENUBAR_HEIGHT;
		}
	}

	if(++editor_resolution_manager_count == 1) {
		LOG_INFO("EDITOR RESOLUTION: " << editor_x_resolution << "," << editor_y_resolution);
		KRE::WindowManager::getMainWindow()->setWindowSize(editor_x_resolution, editor_y_resolution); //, KRE::WindowSizeChangeFlags::NOTIFY_CANVAS_ONLY);
//		graphics::GameScreen::get().setLocation(0, EDITOR_MENUBAR_HEIGHT);
	}
}

EditorResolutionManager::~EditorResolutionManager()
{
	if(--editor_resolution_manager_count == 0) {
		KRE::WindowManager::getMainWindow()->setWindowSize(original_width_, original_height_);
		graphics::GameScreen::get().setLocation(0, 0);
	}
}

void editor::setup_for_editing()
{
	stats::flush();
	try {
		load_stats();
	} catch(...) {
		debug_console::addMessage("Error parsing stats");
		LOG_INFO("ERROR LOADING STATS");
	}

	lvl_->setAsCurrentLevel();

	for(LevelPtr lvl : levels_) {
		for(EntityPtr c : lvl->get_chars()) {
			if(entity_collides_with_level(*lvl, *c, MOVE_DIRECTION::NONE)) {
				const int x = c->x();
				const int y = c->y();
				if(place_entity_in_level_with_large_displacement(*lvl, *c)) {
					assert(c->allowLevelCollisions() || !entity_collides_with_level(*lvl, *c, MOVE_DIRECTION::NONE));
					if(lvl == lvl_) {
						debug_console::addMessage(formatter() << "Adjusted position of " << c->getDebugDescription() << " to fit: (" << x << "," << y << ") -> (" << c->x() << "," << c->y() << ")");
					}
				} else {
					debug_console::addMessage(formatter() << c->getDebugDescription() << " is in an illegal position and can't be auto-corrected");
				}
			}
		}
	}

	g_last_edited_level() = filename_;

	tileset_dialog_.reset(new editor_dialogs::TilesetEditorDialog(*this));
	layers_dialog_.reset(new editor_dialogs::EditorLayersDialog(*this));
	current_dialog_ = tileset_dialog_.get();

	//reset the tool status.
	change_tool(tool_);
}

bool BuiltinEditor::handleEvent(const SDL_Event& event, bool swallowed)
{
	const bool dialog_started_with_focus = (code_dialog_ && code_dialog_->hasFocus()) || (current_dialog_ && current_dialog_->hasFocus());
	if(code_dialog_ && code_dialog_->processEvent(point(), event, swallowed)) {
		return true;
	}

	if(swallowed) {
		return true;
	}

//	if(done_) {
//		return false;
//	}

	if(editor_menu_dialog_->processEvent(point(), event, false)) {
		return true;
	}

	if(editor_mode_dialog_->processEvent(point(), event, false)) {
		return true;
	}

	if(current_dialog_ && current_dialog_->processEvent(point(), event, false)) {
		return true;
	}

	if(layers_dialog_ && layers_dialog_->processEvent(point(), event, false)) {
		return true;
	}
	
	switch(event.type) {
	case SDL_QUIT:
		done_ = true;
		break;
	case SDL_KEYDOWN:
		if(event.key.keysym.sym == SDLK_ESCAPE) {
			if(confirm_quit()) {
				done_ = true;
				return true;
			}
		}

		handleKeyPress(event.key);
		break;
	case SDL_MOUSEBUTTONDOWN:
		//if the code dialog started with focus, we ignore mouse
		//presses so that the first click just unfocuses it.
		if(!dialog_started_with_focus) {
			mouse_buttons_down_ = mouse_buttons_down_|SDL_BUTTON(event.button.button);

			handleMouseButtonDown(event.button);
		}
		break;

	case SDL_MOUSEBUTTONUP:
		//if the code dialog started with focus, we ignore mouse
		//presses so that the first click just unfocuses it.
		//Also don't handle up events unless we handled the down event.
		if(!dialog_started_with_focus && (mouse_buttons_down_&SDL_BUTTON(event.button.button))) {
			mouse_buttons_down_ = mouse_buttons_down_&(~SDL_BUTTON(event.button.button));
			handleMouseButtonUp(event.button);
		}
		break;
	case SDL_MOUSEWHEEL: {
			int mousex, mousey;
			input::sdl_get_mouse_state(&mousex, &mousey);

			//const int xpos = xpos_ + mousex*zoom_;
			if(mousex < editor_x_resolution-sidebar_width()) {
				if(event.wheel.y < 0) {
					zoomIn();
				} else {
					zoomOut();
				}
			}
		break;
		}
	case SDL_WINDOWEVENT:
		if(event.window.event == SDL_WINDOWEVENT_RESIZED) {
			video_resize(event);
			LevelRunner::getCurrent()->video_resize_event(event);
			editor_x_resolution = event.window.data1;
			editor_y_resolution = event.window.data2;
			reset_dialog_positions();
		}

		return false;
	case SDL_MOUSEMOTION:
		// handle_tracking_to_mouse(); //Can't call here; freezes the display while the rotate button (g) is held. Currently called in level_runner.cpp.
		break;
	default:
		break;
	}

	return false;
}

void BuiltinEditor::process()
{
	if(code_dialog_) {
		code_dialog_->process();
	}

	if(external_code_editor_) {
		external_code_editor_->process();
	}

	if(layers_dialog_) {
		layers_dialog_->process();
	}

	if(external_code_editor_ && external_code_editor_->replaceInGameEditor() && editor_menu_dialog_) {
		std::string type;
		if(lvl_->editor_selection().empty() == false) {
			type = lvl_->editor_selection().back()->queryValue("type").as_string();
		}
		if(type.empty() == false) {
			editor_menu_dialog_->set_code_button_text("edit " + type);
		} else {
			editor_menu_dialog_->set_code_button_text("");
		}
	}

	if(editor_mode_dialog_) {
		editor_mode_dialog_->refreshSelection();
	}

	g_codebar_width = code_dialog_ ? code_dialog_->width() : 0;

	if(code_dialog_ && code_dialog_->hasKeyboardFocus()) {
		return;
	}

	process_ghost_objects();

	int mousex, mousey;
	const unsigned int buttons = input::sdl_get_mouse_state(&mousex, &mousey) & mouse_buttons_down_;

	if(buttons == 0) {
		drawing_rect_ = false;
	}

	const int last_mousex = prev_mousex_;
	const int last_mousey = prev_mousey_;

	//make middle-clicking drag the screen around.
	if(prev_mousex_ != -1 && prev_mousey_ != -1 && (buttons&SDL_BUTTON_MIDDLE)) {
		const int diff_x = mousex - prev_mousex_;
		const int diff_y = mousey - prev_mousey_;
		middle_mouse_deltax_ = -diff_x*zoom_;
		middle_mouse_deltay_ = -diff_y*zoom_;
	}

	prev_mousex_ = mousex;
	prev_mousey_ = mousey;

	const bool object_mode = (tool() == TOOL_ADD_OBJECT || tool() == TOOL_SELECT_OBJECT);
	if(property_dialog_ && g_variable_editing) {
		const int diffx = (xpos_ + mousex*zoom_) - anchorx_;
		const int diffy = (ypos_ + mousey*zoom_) - anchory_;
		int diff = 0;
		switch(g_variable_editing->getType()) {
		case VARIABLE_TYPE::XPOSITION:
			diff = diffx;
			break;
		case VARIABLE_TYPE::YPOSITION:
			diff = diffy;
			break;
		default:
			break;
		}

		if(property_dialog_ && property_dialog_->getEntity()) {
			variant new_value;
			const bool ctrl_pressed = (SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) != 0;

			if(g_variable_editing->getType() == VARIABLE_TYPE::POINTS) {
				std::vector<variant> items = g_variable_editing_original_value.as_list();
				ASSERT_LOG(g_variable_editing_index >= 0 && static_cast<unsigned>(g_variable_editing_index) < items.size(), 
					"Variable editing points invalid: " << g_variable_editing_index << " / " << items.size());
				point orig_point(items[g_variable_editing_index]);
				point new_point(orig_point.x + diffx, orig_point.y + diffy);
				if(!ctrl_pressed) {
					new_point.x = new_point.x - new_point.x%(TileSize/2);
					new_point.y = new_point.y - new_point.y%(TileSize/2);
				}
				items[g_variable_editing_index] = new_point.write();
				new_value = variant(&items);

			} else {
				int new_value_int = g_variable_editing_original_value.as_int() + diff;
				if(!ctrl_pressed) {
					new_value_int = new_value_int - new_value_int%(TileSize/2);
				}

				new_value = variant(new_value_int);
			}

			if(!new_value.is_null()) {
				std::vector<std::function<void()> > undo, redo;
				generate_mutate_commands(property_dialog_->getEntity(), g_variable_editing->getVariableName(), new_value, undo, redo);
				executeCommand(
				  std::bind(execute_functions, redo),
				  std::bind(execute_functions, undo));

				//We don't want this to actually be undoable, since the whole
				//drag operation will be undoable when we're done, so remove
				//from the undo stack.
				undo_.pop_back();

				on_modify_level();
			}
		}
	} else if(object_mode && !buttons) {

		//remove ghost objects and re-add them. This guarantees ghost
		//objects always remain at the end of the level ordering.
		remove_ghost_objects();
		EntityPtr c = lvl_->get_next_character_at_point(xpos_ + mousex*zoom_, ypos_ + mousey*zoom_, xpos_, ypos_, nullptr);
		for(const EntityPtr& ghost : ghost_objects_) {
			lvl_->add_character(ghost);
		}

		lvl_->set_editor_highlight(c);
		//See if we should add ghost objects. Human objects don't get
		//ghost (it doesn't make much sense for them to do so)
		if(ghost_objects_.empty() && c && !c->isHuman() && !editing_level_being_played()) {
			//we have an object but no ghost for it, make the
			//object's ghost and deploy it.
			EntityPtr clone = c->clone();
			if(clone && !entity_collides_with_level(*lvl_, *clone, MOVE_DIRECTION::NONE)) {
				ghost_objects_.push_back(clone);
				lvl_->add_character(clone);

				//fire the event to tell the ghost it's been added.
				lvl_->swap_chars(ghost_objects_);
				clone->handleEvent(OBJECT_EVENT_START_LEVEL);
				lvl_->swap_chars(ghost_objects_);
			}
		} else if(ghost_objects_.empty() == false && !c) {
			//ghost objects are present but we are no longer moused-over
			//an object, so remove the ghosts.
			remove_ghost_objects();
			ghost_objects_.clear();
		}
	} else if(object_mode && lvl_->editor_highlight()) {
		for(LevelPtr lvl : levels_) {
			lvl->set_editor_dragging_objects();
		}
		
		//we're handling objects, and a button is down, and we have an
		//object under the mouse. This means we are dragging something.

		// check if cursor is not in the sidebar!
		if (mousex < editor_mode_dialog_->x()) {
			handle_object_dragging(mousex, mousey);
		}
	} else if(drawing_rect_) {
		handleDrawingRect(mousex, mousey);
	}

	if(!object_mode) {
		//not in object mode, the picker still highlights objects,
		//though it won't create ghosts, so remove all ghosts.
		if(tool() == TOOL_PICKER) {
			EntityPtr c = lvl_->get_next_character_at_point(xpos_ + mousex*zoom_, ypos_ + mousey*zoom_, xpos_, ypos_);
			lvl_->set_editor_highlight(c);
		} else {
			lvl_->set_editor_highlight(EntityPtr());
		}

		remove_ghost_objects();
		ghost_objects_.clear();
	}

	//if we're drawing with a pencil see if we add a new tile
	if(tool() == TOOL_PENCIL && dragging_ && buttons) {
		const int xpos = xpos_ + mousex*zoom_;
		const int ypos = ypos_ + mousey*zoom_;
		const int last_xpos = xpos_ + last_mousex*zoom_;
		const int last_ypos = ypos_ + last_mousey*zoom_;

		pencil_motion(last_xpos, last_ypos, xpos, ypos, buttons&SDL_BUTTON(SDL_BUTTON_LEFT));
	}

	for(LevelPtr lvl : levels_) {
		try {
			const assert_recover_scope safe_scope;
			lvl->complete_rebuild_tiles_in_background();
		} catch(validation_failure_exception& e) {
			if(!drawing_rect_) {
				undo_command();
			}
			debug_console::addMessage(formatter() << "Failed to add tiles: " << e.msg);
		}
	}
}

void editor::pencil_motion(int prev_x, int prev_y, int x, int y, bool left_button)
{
	if(abs(prev_y - y) > 2 || abs(prev_x - x) > 2) {
		const int mid_x = (prev_x + x)/2;
		const int mid_y = (prev_y + y)/2;

		pencil_motion(prev_x, prev_y, mid_x, mid_y, left_button);
		pencil_motion(mid_x, mid_y, x, y, left_button);
	}

	point p(x, y);
	point tile_pos(round_tile_size(x), round_tile_size(y));
	if(std::find(g_current_draw_tiles.begin(), g_current_draw_tiles.end(), tile_pos) == g_current_draw_tiles.end()) {
		g_current_draw_tiles.push_back(tile_pos);

		if(left_button) {
			add_tile_rect(p.x, p.y, p.x, p.y);
		} else {
			remove_tile_rect(p.x, p.y, p.x, p.y);
		}
	}
}


void editor::setPos(int x, int y)
{
	xpos_ = x;
	ypos_ = y;
}

void editor::set_playing_level(LevelPtr lvl)
{
	levels_.resize(1);
	levels_.push_back(lvl);
	lvl_ = lvl;
}

void editor::toggle_active_level()
{
	std::vector<LevelPtr>::iterator i = std::find(levels_.begin(), levels_.end(), lvl_);
	if(i != levels_.end()) {
		++i;
		if(i == levels_.end()) {
			i = levels_.begin();
		}

		lvl_ = *i;
	}
	lvl_->setAsCurrentLevel();
}

bool editor::editing_level_being_played() const
{
	return levels_.size() == 2 && std::find(levels_.begin(), levels_.end(), lvl_) != levels_.begin();
}

void editor::reset_dialog_positions()
{
	auto wnd = KRE::WindowManager::getMainWindow();
	if(editor_mode_dialog_) {
		editor_mode_dialog_->setLoc(wnd->width() - editor_mode_dialog_->width(), editor_mode_dialog_->y());
	}

#define SET_DIALOG_POS(d) if(d) { \
	d->setLoc(wnd->width() - d->width(), d->y()); \
	\
	d->setDim(d->width(), \
	 std::max<int>(10, wnd->height() - d->y())); \
}
	SET_DIALOG_POS(character_dialog_);
	SET_DIALOG_POS(property_dialog_);
	SET_DIALOG_POS(tileset_dialog_);
#undef SET_DIALOG_POS

	if(layers_dialog_ && editor_mode_dialog_) {
		layers_dialog_->setLoc(editor_mode_dialog_->x() - layers_dialog_->width(), EDITOR_MENUBAR_HEIGHT);
		layers_dialog_->setDim(layers_dialog_->width(), wnd->height() - EDITOR_MENUBAR_HEIGHT);
	}

	if(editor_menu_dialog_ && editor_mode_dialog_) {
		editor_menu_dialog_->setDim(wnd->width() - editor_mode_dialog_->width(), editor_menu_dialog_->height());
	}
}

namespace 
{
	bool sort_entity_zsub_orders(const EntityPtr& a, const EntityPtr& b) 
	{
		return a->zSubOrder() < b->zSubOrder();
	}
}

void editor::execute_shift_object(EntityPtr e, int dx, int dy)
{
	begin_command_group();
	for(LevelPtr lvl : levels_) {
		EntityPtr obj = lvl->get_entity_by_label(e->label());
		if(obj) {
			executeCommand(std::bind(&editor::move_object, this, lvl, obj, obj->x()+dx,obj->y()+dy),
						   std::bind(&editor::move_object,this, lvl, obj,obj->x(),obj->y()));
		}
	}
	end_command_group();

	on_modify_level();
}

void editor::handleKeyPress(const SDL_KeyboardEvent& key)
{
	if(key.keysym.sym == SDLK_e && (key.keysym.mod&KMOD_ALT) && levels_.size() > 1) {
		done_ = true;
		return;
	}

	if(key.keysym.sym == SDLK_s && (key.keysym.mod&KMOD_ALT)) {
		const std::string fname = KRE::WindowManager::getMainWindow()->saveFrameBuffer("screenshot.png");
		if(!fname.empty()) {
			LOG_INFO("Saved screenshot(in editor) to: " << fname);
		}
	}

	if(key.keysym.sym == SDLK_1 && key.keysym.mod&KMOD_CTRL) {
		duplicate_selected_objects();
	}

	if(key.keysym.sym == SDLK_u) {
		undo_command();
	}

	if(key.keysym.sym == SDLK_r &&
	   !(key.keysym.mod&KMOD_CTRL)) {
		redo_command();
	}

	if(key.keysym.sym == SDLK_z) {
		zoomIn();
	}

	if(key.keysym.sym == SDLK_h) {
		preferences::toogle_debug_hitboxes();
	}

	if(key.keysym.sym == SDLK_KP_8) {
		begin_command_group();
		for(const EntityPtr& e : lvl_->editor_selection()){
			execute_shift_object(e, 0, -2);
		}
		end_command_group();
	}

	if(key.keysym.sym == SDLK_KP_5) {
		begin_command_group();
		for(const EntityPtr& e : lvl_->editor_selection()){
			execute_shift_object(e, 0, 2);
		}
		end_command_group();
	}
	
	if(key.keysym.sym == SDLK_KP_4) {
		begin_command_group();
		for(const EntityPtr& e : lvl_->editor_selection()){
			execute_shift_object(e, -2, 0);
		}
		end_command_group();
	}
	
	if(key.keysym.sym == SDLK_KP_6) {
		begin_command_group();
		for(const EntityPtr& e : lvl_->editor_selection()){
			execute_shift_object(e, 2, 0);
		}
		end_command_group();
	}
	
	if(key.keysym.sym == SDLK_EQUALS || key.keysym.sym == SDLK_MINUS ) {
		if(lvl_->editor_selection().size() > 1){
			
			//store them in a new container
			std::vector <EntityPtr> v2;
			for(const EntityPtr& e : lvl_->editor_selection()){
				v2.push_back(e.get());
			}
			//sort this container in ascending zsub_order
			std::sort(v2.begin(),v2.end(), sort_entity_zsub_orders);
					
			//if it was +, then move the backmost object in front of the frontmost object.
			//if it was -, do vice versa (frontmost object goes behind backmost object)
			if(key.keysym.sym == SDLK_EQUALS){
				begin_command_group();
				for(LevelPtr lvl : levels_) {
					EntityPtr obj = lvl->get_entity_by_label(v2.front()->label());
					if(obj) {
						executeCommand(std::bind(&Entity::setZSubOrder, obj, v2.back()->zSubOrder()+1),
										std::bind(&Entity::setZSubOrder, obj, v2.front()->zSubOrder() ));
					}
				}
				end_command_group();

				on_modify_level();
			} else if(key.keysym.sym == SDLK_MINUS) {
				begin_command_group();
				for(LevelPtr lvl : levels_) {
					EntityPtr obj = lvl->get_entity_by_label(v2.back()->label());
					if(obj) {
						executeCommand(std::bind(&Entity::setZSubOrder, obj, v2.front()->zSubOrder()-1),
									   std::bind(&Entity::setZSubOrder, obj, v2.back()->zSubOrder() ));
				
					}
				}
				end_command_group();

				on_modify_level();
			}
		}
	}
	
	
	if(key.keysym.sym == SDLK_x) {
		zoomOut();
	}

	if(key.keysym.sym == SDLK_f) {
		lvl_->setShowForeground(!lvl_->show_foreground());
	}

	if(key.keysym.sym == SDLK_b) {
		lvl_->setShowBackground(!lvl_->show_background());
	}

	if(editing_objects() && (key.keysym.sym == SDLK_DELETE || key.keysym.sym == SDLK_BACKSPACE) && lvl_->editor_selection().empty() == false) {
		//deleting objects. We clear the selection as well as
		//deleting. To undo, the previous selection will be cleared,
		//and then the deleted objects re-selected.
		std::vector<std::function<void()> > redo, undo;
		undo.push_back(std::bind(&Level::editor_clear_selection, lvl_.get()));

		//if we undo, return the objects to the property dialog
		undo.push_back(std::bind(&editor_dialogs::PropertyEditorDialog::setEntityGroup, property_dialog_.get(), lvl_->editor_selection()));
		redo.push_back(std::bind(&Level::editor_clear_selection, lvl_.get()));
		//we want to clear the objects in the property dialog
		redo.push_back(std::bind(&editor_dialogs::PropertyEditorDialog::setEntityGroup, property_dialog_.get(), std::vector<EntityPtr>()));
		for(const EntityPtr& e : lvl_->editor_selection()) {
			generate_remove_commands(e, undo, redo);
			undo.push_back(std::bind(&Level::editor_select_object, lvl_.get(), e));
		}
		executeCommand(
		  std::bind(execute_functions, redo),
		  std::bind(execute_functions, undo));
		on_modify_level();
	}

	if(!tile_selection_.empty() && (key.keysym.sym == SDLK_DELETE || key.keysym.sym == SDLK_BACKSPACE)) {
		int min_x = std::numeric_limits<int>::max(), min_y = std::numeric_limits<int>::max(), max_x = std::numeric_limits<int>::min(), max_y = std::numeric_limits<int>::min();
		std::vector<std::function<void()> > redo, undo;

		for(LevelPtr lvl : levels_) {
			for(const point& p : tile_selection_.tiles) {
				const int x = p.x*TileSize;
				const int y = p.y*TileSize;

				min_x = std::min(x, min_x);
				max_x = std::max(x, max_x);
				min_y = std::min(y, min_y);
				max_y = std::max(y, max_y);

				redo.push_back([=](){ lvl->clear_tile_rect(x, y, x, y); });
				std::map<int, std::vector<std::string> > old_tiles;
				lvl->getAllTilesRect(x, y, x, y, old_tiles);
				for(auto i = old_tiles.begin(); i != old_tiles.end(); ++i) {
					undo.push_back([=](){ lvl->addTileRectVector(i->first, x, y, x, y, i->second); });
				}
			}

			if(!tile_selection_.tiles.empty()) {
				undo.push_back(std::bind(&Level::start_rebuild_tiles_in_background, lvl.get(), std::vector<int>()));
				redo.push_back(std::bind(&Level::start_rebuild_tiles_in_background, lvl.get(), std::vector<int>()));
			}
		}

		executeCommand(
		  std::bind(execute_functions, redo),
		  std::bind(execute_functions, undo));
		on_modify_level();
	}

	if(key.keysym.sym == SDLK_o && (key.keysym.mod&KMOD_CTRL)) {
		editor_menu_dialog_->open_level();
	}

	if(key.keysym.sym == SDLK_s && (key.keysym.mod&KMOD_CTRL)) {
		save_level();
	}

	if(key.keysym.sym == SDLK_f) {
		toggle_facing();
	}

	if(key.keysym.sym == SDLK_i) {
		toggle_isUpsideDown();
	}

	if(key.keysym.sym == SDLK_r &&
	   (key.keysym.mod&KMOD_CTRL) && levels_.size() == 2 &&
	   lvl_ == levels_.back()) {

		EntityPtr player;
		if(lvl_->player()) {
			player.reset(&lvl_->player()->getEntity());
		}

		levels_.front()->transfer_state_to(*levels_.back());

		if(player) {
			if(place_entity_in_level(*lvl_, *player)) {
				lvl_->add_player(player);
			}
		}

		controls::new_level(lvl_->cycle(), lvl_->players().empty() ? 1 : static_cast<int>(lvl_->players().size()), multiplayer::slot());

	}

	if(key.keysym.sym == SDLK_c) {
		for(const EntityPtr& obj : lvl_->get_chars()) {
			if(entity_collides_with_level(*lvl_, *obj, MOVE_DIRECTION::NONE)) {
				xpos_ = obj->x() - KRE::WindowManager::getMainWindow()->width()/2;
				ypos_ = obj->y() - KRE::WindowManager::getMainWindow()->height()/2;
				break;
			}
		}
	}

	if(key.keysym.sym == SDLK_n) {
		add_new_sub_component();
	}
}

void editor::handle_scrolling()
{
	xpos_ += middle_mouse_deltax_;
	ypos_ += middle_mouse_deltay_;

	middle_mouse_deltax_ = middle_mouse_deltay_ = 0;

	if(code_dialog_ && code_dialog_->hasKeyboardFocus()) {
		return;
	}
	const int ScrollSpeed = 24*zoom_;
	const int FastScrollSpeed = 384*zoom_;
	const Uint8* keystate = SDL_GetKeyboardState(nullptr);

	if(keystate[SDL_SCANCODE_LEFT]) {
		xpos_ -= ScrollSpeed;
		if(keystate[SDL_SCANCODE_KP_0]) {
			xpos_ -= FastScrollSpeed;
		}
	}
	if(keystate[SDL_SCANCODE_RIGHT]) {
		xpos_ += ScrollSpeed;
		if(keystate[SDL_SCANCODE_KP_0]) {
			xpos_ += FastScrollSpeed;
		}
	}
	if(keystate[SDL_SCANCODE_UP]) {
		ypos_ -= ScrollSpeed;
		if(keystate[SDL_SCANCODE_KP_0]) {
			ypos_ -= FastScrollSpeed;
		}
	}
	if(keystate[SDL_SCANCODE_DOWN]) {
		ypos_ += ScrollSpeed;
		if(keystate[SDL_SCANCODE_KP_0]) {
			ypos_ += FastScrollSpeed;
		}
	}
}

void editor::handle_tracking_to_mouse()
{
	if(code_dialog_ && code_dialog_->hasKeyboardFocus()) {
		return;
	}
	const Uint8* keystate = SDL_GetKeyboardState(nullptr);
	
	//The keys here, g and m, were chosen at random because all the sensible ones were used for other stuff.
	static bool rotateReferenceSet = false;
	if(keystate[SDL_GetScancodeFromKey(SDLK_g)]) { //typed g, not literal g key
		if(!rotateReferenceSet) {
			set_rotate_reference();
			rotateReferenceSet = true;
			begin_command_group();
		} else {
			change_rotation();
		}
	} else {
		if(rotateReferenceSet) {
			rotateReferenceSet = false;
			end_command_group();
		}
	}
	
	static bool scaleReferenceSet = false;
	if(keystate[SDL_GetScancodeFromKey(SDLK_m)]) {
		if(!scaleReferenceSet) {
			set_scale_reference();
			scaleReferenceSet = true;
			begin_command_group();
		} else {
			change_scale();
		}
	} else {
		if(scaleReferenceSet) {
			scaleReferenceSet = false;
			end_command_group();
		}
	}
}

void editor::reset_playing_level(bool keep_player)
{
	if(levels_.size() == 2 && lvl_ == levels_.back()) {
		EntityPtr player;
		if(keep_player && lvl_->player()) {
			player.reset(&lvl_->player()->getEntity());
		}

		levels_.front()->transfer_state_to(*levels_.back());

		if(player) {
			if(place_entity_in_level(*lvl_, *player)) {
				lvl_->add_player(player);
			}
		}

		controls::new_level(lvl_->cycle(), lvl_->players().empty() ? 1 : static_cast<int>(lvl_->players().size()), multiplayer::slot());

	}
}

void editor::toggle_pause() const
{
	if(LevelRunner::getCurrent()) {
		LevelRunner::getCurrent()->toggle_pause();
	}
}

void editor::handle_object_dragging(int mousex, int mousey)
{
	if(std::count(lvl_->editor_selection().begin(), lvl_->editor_selection().end(), lvl_->editor_highlight()) == 0) {
		return;
	}

	const bool ctrl_pressed = (SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) != 0;
	const int dx = xpos_ + mousex*zoom_ - anchorx_;
	const int dy = ypos_ + mousey*zoom_ - anchory_;
	const int xpos = selected_entity_startx_ + dx;
	const int ypos = selected_entity_starty_ + dy;

	const int new_x = xpos - (ctrl_pressed ? 0 : (xpos%TileSize));
	const int new_y = ypos - (ctrl_pressed ? 0 : (ypos%TileSize));

	const int delta_x = new_x - lvl_->editor_highlight()->x();
	const int delta_y = new_y - lvl_->editor_highlight()->y();

	//don't move the object from its starting position until the
	//delta in movement is large enough.
	const bool in_starting_position =
	  lvl_->editor_highlight()->x() == selected_entity_startx_ &&
	  lvl_->editor_highlight()->y() == selected_entity_starty_;
	const bool too_small_to_move = in_starting_position &&
	         abs(dx) < 5 && abs(dy) < 5;

	if(!too_small_to_move && (new_x != lvl_->editor_highlight()->x() || new_y != lvl_->editor_highlight()->y())) {
		std::vector<std::function<void()> > redo, undo;

		for(const EntityPtr& e : lvl_->editor_selection()) {
			for(LevelPtr lvl : levels_) {
				EntityPtr obj = lvl->get_entity_by_label(e->label());
				if(obj) {
					redo.push_back(std::bind(&editor::move_object, this, lvl, obj, e->x() + delta_x, e->y() + delta_y));
					undo.push_back(std::bind(&editor::move_object, this, lvl, obj, obj->x(), obj->y()));
				}
			}

		}

		//all dragging that is done should be treated as one operation
		//from an undo/redo perspective. So, we see if we're already dragging
		//and have performed existing drag operations, and if so we
		//roll the previous undo command into this.
		std::function<void()> undo_fn = std::bind(execute_functions, undo);

		if(g_started_dragging_object && undo_.empty() == false && undo_.back().type == COMMAND_TYPE_DRAG_OBJECT) {
			undo_fn = undo_.back().undo_command;
			undo_command();
		}

		executeCommand(std::bind(execute_functions, redo), undo_fn, COMMAND_TYPE_DRAG_OBJECT);

		g_started_dragging_object = true;

		remove_ghost_objects();
		ghost_objects_.clear();

		on_modify_level();
	}
}

void editor::handleDrawingRect(int mousex, int mousey)
{
	const unsigned int buttons = input::sdl_get_mouse_state(&mousex, &mousey);

	const int xpos = xpos_ + mousex*zoom_;
	const int ypos = ypos_ + mousey*zoom_;

	int x1 = xpos;
	int x2 = anchorx_;
	int y1 = ypos;
	int y2 = anchory_;
	if(x1 > x2) {
		std::swap(x1, x2);
	}

	if(y1 > y2) {
		std::swap(y1, y2);
	}

	x1 = round_tile_size(x1);
	x2 = round_tile_size(x2 + TileSize);
	y1 = round_tile_size(y1);
	y2 = round_tile_size(y2 + TileSize);

	const rect new_rect = rect(x1, y1, x2 - x1, y2 - y1);
	if(g_rect_drawing == new_rect) {
		return;
	}

	if(tool() == TOOL_ADD_RECT) {
		lvl_->freeze_rebuild_tiles_in_background();
		if(tmp_undo_.get()) {
			tmp_undo_->undo_command();
		}

		if(buttons&SDL_BUTTON(SDL_BUTTON_LEFT)) {
			add_tile_rect(anchorx_, anchory_, xpos, ypos);
		} else {
			remove_tile_rect(anchorx_, anchory_, xpos, ypos);
		}

		tmp_undo_.reset(new executable_command(undo_.back()));
		undo_.pop_back();
		lvl_->unfreeze_rebuild_tiles_in_background();
	}
	g_rect_drawing = new_rect;
}

void editor::handleMouseButtonDown(const SDL_MouseButtonEvent& event)
{
	const bool ctrl_pressed = (SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) != 0;
	const bool shift_pressed = (SDL_GetModState()&(KMOD_LSHIFT|KMOD_RSHIFT)) != 0;
	const bool alt_pressed = (SDL_GetModState()&KMOD_ALT) != 0;
	int mousex, mousey;
	const unsigned int buttons = input::sdl_get_mouse_state(&mousex, &mousey);

	anchorx_ = xpos_ + mousex*zoom_;
	anchory_ = ypos_ + mousey*zoom_;
	if(event.button == SDL_BUTTON_MIDDLE && !alt_pressed) {
		return;
	}

	int nsub_index = 0;
	for(const Level::SubComponent& sub : lvl_->getSubComponents()) {
		rect addArea(findSubComponentArea(sub, xpos_, ypos_, zoom_));
		bool addAreaMouseover = pointInRect(point(mousex,mousey), addArea);

		//add another variation of this sub component.
		if(addAreaMouseover) {

			std::vector<std::function<void()> > redo, undo;

			redo.push_back(std::bind(&editor::add_sub_component_variations, this, nsub_index, 1));
			undo.push_back(std::bind(&editor::add_sub_component_variations, this, nsub_index, -1));

			rect src(sub.source_area.x() + (sub.num_variations-1)*(TileSize*4 + sub.source_area.w()), sub.source_area.y(), sub.source_area.w(), sub.source_area.h());
			rect dst(sub.source_area.x() + (sub.num_variations)*(TileSize*4 + sub.source_area.w()), sub.source_area.y(), sub.source_area.w(), sub.source_area.h());

			copy_rectangle(src, dst, redo, undo, true);

			executeCommand(
			  std::bind(execute_functions, redo),
			  std::bind(execute_functions, undo));
			return;
		}

		dragging_sub_component = rect_top_edge_selected(sub.source_area, anchorx_, anchory_, zoom_);
		resizing_sub_component_right_edge = rect_right_edge_selected(sub.source_area, anchorx_, anchory_, zoom_);
		resizing_sub_component_bottom_edge = rect_bottom_edge_selected(sub.source_area, anchorx_, anchory_, zoom_);

		if(dragging_sub_component || resizing_sub_component_right_edge || resizing_sub_component_bottom_edge) {
			resizing_sub_component_index = nsub_index;
			return;
		}

		++nsub_index;
	}

	dragging_sub_component_usage_index = -1;

	nsub_index = 0;
	for(const Level::SubComponentUsage& sub : lvl_->getSubComponentUsages()) {
		rect area = sub.dest_area;
		const bool mouse_over = rect_any_edge_selected(area, anchorx_, anchory_, zoom_);
		if(mouse_over) {
			dragging_sub_component_usage_index = nsub_index;
			return;
		}

		++nsub_index;
	}

	resizing_left_level_edge = rect_left_edge_selected(lvl_->boundaries(), anchorx_, anchory_, zoom_);
	resizing_right_level_edge = rect_right_edge_selected(lvl_->boundaries(), anchorx_, anchory_, zoom_);
	resizing_top_level_edge = rect_top_edge_selected(lvl_->boundaries(), anchorx_, anchory_, zoom_);
	resizing_bottom_level_edge = rect_bottom_edge_selected(lvl_->boundaries(), anchorx_, anchory_, zoom_);

	if(resizing_left_level_edge || resizing_right_level_edge || resizing_top_level_edge || resizing_bottom_level_edge) {
		return;
	}

	dragging_ = drawing_rect_ = false;

	if(adding_points_.empty() == false) {
		if(event.button == SDL_BUTTON_LEFT && property_dialog_ && property_dialog_->getEntity()) {
			const int xpos = anchorx_;
			const int ypos = anchory_;
			LOG_INFO("ADD POINT: " << xpos << ", " << ypos);

			EntityPtr c = property_dialog_->getEntity();

			variant current_value = c->queryValue(adding_points_);
			std::vector<variant> new_value;
			if(current_value.is_list()) {
				new_value = current_value.as_list();
			}

			std::vector<variant> point;
			point.push_back(variant(xpos));
			point.push_back(variant(ypos));
			new_value.push_back(variant(&point));

			std::vector<std::function<void()> > redo, undo;
			generate_mutate_commands(c, adding_points_, variant(&new_value), undo, redo);

			executeCommand(
			  std::bind(execute_functions, redo),
			  std::bind(execute_functions, undo));

			start_adding_points(adding_points_);

			on_modify_level();

		} else {
			start_adding_points("");
		}
	} else if(tool() == TOOL_EDIT_SEGMENTS) {
		if(pointInRect(point(anchorx_, anchory_), lvl_->boundaries())) {
			const int xpos = anchorx_ - lvl_->boundaries().x();
			const int ypos = anchory_ - lvl_->boundaries().y();
			const int segment = lvl_->segment_width() ? xpos/lvl_->segment_width() : ypos/lvl_->segment_height();

			if(selected_segment_ == -1) {
				selected_segment_ = segment;
				segment_dialog_->setSegment(segment);
			} else if(buttons&SDL_BUTTON(SDL_BUTTON_RIGHT)) {
				if(segment != selected_segment_ && selected_segment_ >= 0) {
					variant next = lvl_->get_var(formatter() << "segments_after_" << selected_segment_);
					std::vector<variant> v;
					if(next.is_list()) {
						for(int n = 0; n != next.num_elements(); ++n) {
							v.push_back(next[n]);
						}
					}

					std::vector<variant>::iterator i = std::find(v.begin(), v.end(), variant(segment));
					if(i != v.end()) {
						v.erase(i);
					} else {
						v.push_back(variant(segment));
					}

					lvl_->set_var(formatter() << "segments_after_" << selected_segment_, variant(&v));
				}
			}
		} else {
			selected_segment_ = -1;
			segment_dialog_->setSegment(selected_segment_);
		}
	} else if(tool() == TOOL_PICKER) {
		if(lvl_->editor_highlight()) {
			change_tool(TOOL_ADD_OBJECT);

			variant node = lvl_->editor_highlight()->write();
			const std::string type = node["type"].as_string();
			for(unsigned n = 0; n != all_characters().size(); ++n) {
				const enemy_type& c = all_characters()[n];
				std::string enemy_type_str = c.node["type"].as_string();
				auto colon_itor = std::find(enemy_type_str.begin(), enemy_type_str.end(), ':');
				if(colon_itor != enemy_type_str.end()) {
					enemy_type_str = std::string(colon_itor+1, enemy_type_str.end());
				}
				if(enemy_type_str == type) {
					character_dialog_->select_category(c.category);
					character_dialog_->set_character(n);
					return;
				}
			}
			return;
		} else {
			//pick the top most tile at this point.
			std::map<int, std::vector<std::string> > tiles;
			lvl_->getAllTilesRect(anchorx_, anchory_, anchorx_, anchory_, tiles);
			std::string tile;
			for(std::map<int, std::vector<std::string> >::reverse_iterator i = tiles.rbegin(); i != tiles.rend(); ++i) {
				if(i->second.empty() == false) {
					tile = i->second.back();
					LOG_INFO("picking tile: '" << tile << "'");
					break;
				}
			}

			if(!tile.empty()) {
				for(unsigned n = 0; n != all_tilesets().size(); ++n) {
					if(all_tilesets()[n].type == tile) {
						tileset_dialog_->selectCategory(all_tilesets()[n].category);
						tileset_dialog_->setTileset(n);
						LOG_INFO("pick tile " << n);
						//if we're in adding objects mode then switch to adding tiles mode.
						if(tool_ == TOOL_ADD_OBJECT) {
							change_tool(TOOL_ADD_RECT);
						}
						return;
					}
				}
			}
		}
	} else if(editing_tiles() && !tile_selection_.empty() &&
	   std::binary_search(tile_selection_.tiles.begin(), tile_selection_.tiles.end(), point(round_tile_size(anchorx_)/TileSize, round_tile_size(anchory_)/TileSize))) {
		//we are beginning to drag our selection
		dragging_ = true;
	} else if(tool() == TOOL_ADD_RECT || tool() == TOOL_SELECT_RECT) {
		tmp_undo_.reset();
		drawing_rect_ = true;
		g_rect_drawing = rect();
	} else if(tool() == TOOL_MAGIC_WAND) {
		drawing_rect_ = false;
	} else if(tool() == TOOL_PENCIL) {
		drawing_rect_ = false;
		dragging_ = true;
		point p(anchorx_, anchory_);
		if(buttons&SDL_BUTTON(SDL_BUTTON_LEFT)) {
			add_tile_rect(p.x, p.y, p.x, p.y);
		} else {
			remove_tile_rect(p.x, p.y, p.x, p.y);
		}
		g_current_draw_tiles.clear();
		point tile_pos(round_tile_size(p.x), round_tile_size(p.y));
		g_current_draw_tiles.push_back(tile_pos);
	} else if(property_dialog_ && variable_info_selected(property_dialog_->getEntity(), anchorx_, anchory_, zoom_)) {
		g_variable_editing = variable_info_selected(property_dialog_->getEntity(), anchorx_, anchory_, zoom_, &g_variable_editing_index);
		g_variable_editing_original_value = property_dialog_->getEntity()->queryValue(g_variable_editing->getVariableName());

		if(g_variable_editing->getType() == VARIABLE_TYPE::POINTS && event.button == SDL_BUTTON_RIGHT) {
			std::vector<variant> points = g_variable_editing_original_value.as_list();
			ASSERT_LOG(g_variable_editing_index >= 0 && static_cast<unsigned>(g_variable_editing_index) < points.size(), 
				"INVALID VALUE WHEN EDITING POINTS: " << g_variable_editing_index << " / " << points.size());

			points.erase(points.begin() + g_variable_editing_index);

			variant new_value(&points);

			std::vector<std::function<void()> > undo, redo;
			generate_mutate_commands(property_dialog_->getEntity(), g_variable_editing->getVariableName(), new_value, undo, redo);
			executeCommand(
			  std::bind(execute_functions, redo),
			  std::bind(execute_functions, undo));

			g_variable_editing = nullptr;
			g_variable_editing_original_value = variant();
			g_variable_editing_index = -1;

			on_modify_level();
		}

		//If we select a variable to edit, return here so we don't select
		//another object instead, swallowing the event.
		return;
		
	} else if(tool() == TOOL_SELECT_OBJECT && !lvl_->editor_highlight()) {
		//dragging a rectangle to select objects
		drawing_rect_ = true;
	} else if(property_dialog_) {
		property_dialog_->setEntity(lvl_->editor_highlight());

		set_code_file();
	}

	if(lvl_->editor_highlight() && event.button == SDL_BUTTON_RIGHT) {
		//pass. This is either the start of a right click drag, or will show
		//a context menu on mouse up.
	} else if(lvl_->editor_highlight()) {
		EntityPtr obj_selecting = lvl_->editor_highlight();
		if(std::count(lvl_->editor_selection().begin(),
		              lvl_->editor_selection().end(), lvl_->editor_highlight()) == 0) {
			//set the object as selected in the editor.
			if(!shift_pressed) {
				lvl_->editor_clear_selection();
			}

			obj_selecting = lvl_->editor_highlight();
			const bool ctrl_pressed = (SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) != 0;
			while(!ctrl_pressed && obj_selecting->wasSpawnedBy().empty() == false && lvl_->get_entity_by_label(obj_selecting->wasSpawnedBy())) {
				obj_selecting = lvl_->get_entity_by_label(obj_selecting->wasSpawnedBy());
			}

			lvl_->editor_select_object(obj_selecting);

			property_dialog_->setEntityGroup(lvl_->editor_selection());

			if(!lvl_->editor_selection().empty() && tool() == TOOL_ADD_OBJECT) {
				//we are in add objects mode and we clicked on an object,
				//so change to select mode.
				change_tool(TOOL_SELECT_OBJECT);
			}

			current_dialog_ = property_dialog_.get();
		} else if(shift_pressed) {
			lvl_->editor_deselect_object(lvl_->editor_highlight());
		}

		//start dragging the object
		selected_entity_startx_ = obj_selecting->x();
		selected_entity_starty_ = obj_selecting->y();

		g_started_dragging_object = false;

	} else {
		//clear any selection in the editor
		lvl_->editor_clear_selection();
	}

	if(tool() == TOOL_ADD_OBJECT && event.button == SDL_BUTTON_LEFT && !lvl_->editor_highlight()) {
		int x = round_tile_size(xpos_ + mousex*zoom_) + TileSize/(2*zoom_);
		int y = round_tile_size(ypos_ + mousey*zoom_) + TileSize/(2*zoom_);

		if(ctrl_pressed) {
			x = xpos_ + mousex*zoom_;
			y = ypos_ + mousey*zoom_;
		}

		x -= all_characters()[cur_object_].preview_object()->getCurrentFrame().width()/2;
		y -= all_characters()[cur_object_].preview_object()->getCurrentFrame().height()/2;

		variant_builder node;
		node.merge_object(all_characters()[cur_object_].node);
		node.set("x", x);
		node.set("y",  y);
		node.set("face_right", face_right_);
		node.set("upside_down", upside_down_);

		if(CustomObjectType::get(all_characters()[cur_object_].node["type"].as_string())->isHuman()) {
			node.set("is_human", true);
		}

		EntityPtr c(Entity::build(node.build()));

		//any vars that require formula initialization are calculated here.
		std::map<std::string, variant> vars, props;
		for(auto& info : c->getEditorInfo()->getVars()) {
			if(info.getFormula()) {
				vars[info.getVariableName()] = info.getFormula()->execute(*c);
			}
		}

		for(auto& info : c->getEditorInfo()->getProperties()) {
			if(info.getFormula()) {
				props[info.getVariableName()] = info.getFormula()->execute(*c);
			}
		}
		
		//if we have parallax, offset the object so it's placed at the same position it's graphically visible at
		c->setX( c->x() +  + ((1000 - (c->parallaxScaleMillisX()))* xpos_ )/1000);
		c->setY( c->y() +  + ((1000 - (c->parallaxScaleMillisY()))* ypos_ )/1000);
		

		//we only want to actually set the vars once we've calculated all of
		//them, to avoid any ordering issues etc. So set them all here.
		for(const auto& i : vars) {
			game_logic::FormulaCallable* obj_vars = c->queryValue("vars").mutable_callable();
			obj_vars->mutateValue(i.first, i.second);
		}

		for(const auto& i : props) {
			c->mutateValue(i.first, i.second);
		}

		if(!place_entity_in_level(*lvl_, *c)) {
			//could not place entity. Not really an error; the user just
			//clicked in an illegal position to place an object.

		} else if(c->isHuman() && lvl_->player()) {
			if(!shift_pressed) {
				begin_command_group();
				for(LevelPtr lvl : levels_) {
					EntityPtr obj(c->backup());
					executeCommand(
					  std::bind(&editor::add_object_to_level, this, lvl, obj),
					  std::bind(&editor::add_object_to_level, this, lvl, &lvl->player()->getEntity()));
				}
				end_command_group();

				on_modify_level();
			} else {
				begin_command_group();
				for(LevelPtr lvl : levels_) {
					EntityPtr obj(c->backup());
					executeCommand(
					  std::bind(&editor::add_multi_object_to_level, this, lvl, obj),
					  std::bind(&editor::add_object_to_level, this, lvl, &lvl->player()->getEntity()));
				}
				end_command_group();

				on_modify_level();
			}

		} else {
			begin_command_group();
			for(LevelPtr lvl : levels_) {
				EntityPtr obj(c->backup());
				executeCommand(
				  std::bind(&editor::add_object_to_level, this, lvl, obj),
				  std::bind(&editor::remove_object_from_level, this, lvl, obj));
				LOG_INFO("ADD OBJECT: " << obj->x() << "," << obj->y());
			}
			end_command_group();
			on_modify_level();
		}
	}
}

void editor::handleMouseButtonUp(const SDL_MouseButtonEvent& event)
{
	int mousex, mousey;
	input::sdl_get_mouse_state(&mousex, &mousey);
			
	const int xpos = xpos_ + mousex*zoom_;
	const int ypos = ypos_ + mousey*zoom_;

	if(g_variable_editing) {
		if(property_dialog_ && property_dialog_->getEntity()) {
			EntityPtr e = property_dialog_->getEntity();
			const std::string& var = g_variable_editing->getVariableName();

			begin_command_group();
			for(LevelPtr lvl : levels_) {
				EntityPtr obj = lvl->get_entity_by_label(e->label());
				if(obj) {
					executeCommand(
					  std::bind(&editor::mutate_object_value, this, lvl, obj.get(), var, e->queryValue(var)),
					  std::bind(&editor::mutate_object_value, this, lvl, obj.get(), var, g_variable_editing_original_value));
				}
			}
			end_command_group();
			property_dialog_->init();
			on_modify_level();
		}
		g_variable_editing = nullptr;
		return;
	}

	if(dragging_sub_component_usage_index != -1) {
		const int dx = (xpos - anchorx_)/TileSize;
		const int dy = (ypos - anchory_)/TileSize;

		std::vector<Level::SubComponentUsage> usages = lvl_->getSubComponentUsages();
		std::vector<Level::SubComponentUsage> new_usages = usages;

		Level::SubComponentUsage& usage = new_usages[dragging_sub_component_usage_index];

		if(dx == 0 && dy == 0) {
			//no movement, is a click
			if(event.button == SDL_BUTTON_RIGHT) {
				//delete this usage with a right-click.
				new_usages.erase(new_usages.begin() + dragging_sub_component_usage_index);
			} else {
				usage.ninstance = (usage.ninstance+1)%usage.getSubComponent(*lvl_).num_variations;
			}

		} else {

			rect area = usage.dest_area;

			area = rect(area.x() + dx*TileSize, area.y() + dy*TileSize, area.w(), area.h());

			usage.dest_area = area;

		}

		executeCommand(
			std::bind(&editor::set_sub_component_usage, this, new_usages),
			std::bind(&editor::set_sub_component_usage, this, usages)
		);

		on_modify_level();

		dragging_sub_component_usage_index = -1;
		return;
	}

	if(dragging_sub_component) {
		if(resizing_sub_component_index >= 0 && resizing_sub_component_index < lvl_->getSubComponents().size()) {
			rect source_area = lvl_->getSubComponents()[resizing_sub_component_index].source_area;

			int deltax = xpos - anchorx_;
			int deltay = ypos - anchory_;

			rect dest_area(source_area.x() + (deltax/TileSize)*TileSize, source_area.y() + (deltay/TileSize)*TileSize, source_area.w(), source_area.h());

			if(!rects_intersect(source_area, dest_area)) {
				std::vector<Level::SubComponentUsage> usages = lvl_->getSubComponentUsages();

				executeCommand(
					std::bind(&editor::add_sub_component_usage, this, resizing_sub_component_index, dest_area),
					std::bind(&editor::set_sub_component_usage, this, usages)
				);

				on_modify_level();
			}
		}

		dragging_sub_component = resizing_sub_component_right_edge = resizing_sub_component_bottom_edge = false;
		return;
	} else if(resizing_sub_component_right_edge || resizing_sub_component_bottom_edge) {
		if(resizing_sub_component_index >= 0 && resizing_sub_component_index < lvl_->getSubComponents().size()) {

			rect source_area = lvl_->getSubComponents()[resizing_sub_component_index].source_area;
			rect orig_area = source_area;

			if(resizing_sub_component_right_edge) {
				int deltax = xpos - anchorx_;
				int w = (std::max<int>(TileSize, source_area.w() + deltax)/TileSize)*TileSize;

				source_area = rect(source_area.x(), source_area.y(), w, source_area.h());
			}

			int deltah = 0;

			if(resizing_sub_component_bottom_edge) {
				int deltay = ypos - anchory_;
				int h = (std::max<int>(TileSize, source_area.h() + deltay)/TileSize)*TileSize;

				deltah = h - source_area.h();

				source_area = rect(source_area.x(), source_area.y(), source_area.w(), h);
			}

			std::vector<std::function<void()>> redo, undo;

			if(deltah != 0) {
				//shuffle all areas below us downwards
				std::vector<int> indexes;
				std::vector<Level::SubComponent> subs;
				for(int n = resizing_sub_component_index+1; n < lvl_->getSubComponents().size(); ++n) {
					subs.push_back(lvl_->getSubComponents()[n]);
					indexes.push_back(n);
				}

				if(deltah > 0) {
					std::reverse(subs.begin(), subs.end());
					std::reverse(indexes.begin(), indexes.end());
				}

				int n = 0;
				for(const Level::SubComponent& sub : subs) {
					rect sub_orig_area = sub.source_area;
					rect sub_new_area(sub_orig_area.x(), sub_orig_area.y()+deltah, sub_orig_area.w(), sub_orig_area.h());

					redo.push_back(std::bind(&editor::set_sub_component_area, this, indexes[n], sub_new_area));
					undo.push_back(std::bind(&editor::set_sub_component_area, this, indexes[n], sub_orig_area));

					clear_rectangle(sub_orig_area, redo, undo);
					copy_rectangle(sub_orig_area, sub_new_area, redo, undo, true);

					++n;
				}
			}

			redo.push_back(std::bind(&editor::set_sub_component_area, this, resizing_sub_component_index, source_area));
			undo.push_back(std::bind(&editor::set_sub_component_area, this, resizing_sub_component_index, orig_area));

			executeCommand(
			  std::bind(execute_functions, redo),
			  std::bind(execute_functions, undo));

			on_modify_level();
		}

		dragging_sub_component = resizing_sub_component_right_edge = resizing_sub_component_bottom_edge = false;
		return;
	}

	if(resizing_left_level_edge || resizing_right_level_edge ||resizing_top_level_edge || resizing_bottom_level_edge) {
		rect boundaries = modify_selected_rect(*this, lvl_->boundaries(), xpos, ypos);

		resizing_left_level_edge = resizing_right_level_edge = resizing_top_level_edge = resizing_bottom_level_edge = false;

		if(boundaries != lvl_->boundaries()) {
			const int deltay = boundaries.y2() - lvl_->boundaries().y2();

			begin_command_group();

			std::vector<std::function<void()>> redo, undo;
			for(LevelPtr lvl : levels_) {
				redo.push_back(std::bind(&Level::set_boundaries, lvl.get(), boundaries));
				undo.push_back(std::bind(&Level::set_boundaries, lvl.get(), lvl->boundaries()));

			}

			int nsub = 0;
			std::vector<Level::SubComponent> subs = lvl_->getSubComponents();
			std::vector<int> indexes;
			for(int n = 0; n != subs.size(); ++n) {
				indexes.push_back(n);
			}

			if(deltay > 0) {
				std::reverse(subs.begin(), subs.end());
				std::reverse(indexes.begin(), indexes.end());
			}
			for(const Level::SubComponent& sub : subs) {
				rect area = sub.source_area;
				rect new_area(area.x(), area.y() + deltay, area.w(), area.h());

				clear_rectangle(area, redo, undo);
				copy_rectangle(area, new_area, redo, undo, true);

				redo.push_back(std::bind(&editor::set_sub_component_area, this, indexes[nsub], new_area));
				undo.push_back(std::bind(&editor::set_sub_component_area, this, indexes[nsub], area));
				++nsub;
			}

			executeCommand(
			  std::bind(execute_functions, redo),
			  std::bind(execute_functions, undo));

			end_command_group();
			on_modify_level();
		}
		return;
	}


	if(editing_tiles()) {
		if(dragging_) {
			const int selectx = xpos_ + mousex*zoom_;
			const int selecty = ypos_ + mousey*zoom_;

			//dragging selection
			int diffx = (selectx - anchorx_)/TileSize;
			int diffy = (selecty - anchory_)/TileSize;

			LOG_INFO("MAKE DIFF: " << diffx << "," << diffy);
			std::vector<std::function<void()> > redo, undo;

			for(LevelPtr lvl : levels_) {
				for(const point& p : tile_selection_.tiles) {
					const int x = (p.x+diffx)*TileSize;
					const int y = (p.y+diffy)*TileSize;
					undo.push_back([=](){ lvl->clear_tile_rect(x, y, x, y); });
				}

				int min_x = std::numeric_limits<int>::max();
				int min_y = std::numeric_limits<int>::max();
				int max_x = std::numeric_limits<int>::min(); 
				int max_y = std::numeric_limits<int>::min();

				//backup both the contents of the old and new regions, so we can restore them both
				for(const point& p : tile_selection_.tiles) {
					int x = p.x*TileSize;
					int y = p.y*TileSize;

					min_x = std::min(x, min_x);
					max_x = std::max(x, max_x);
					min_y = std::min(y, min_y);
					max_y = std::max(y, max_y);

					std::map<int, std::vector<std::string> > old_tiles;
					lvl->getAllTilesRect(x, y, x, y, old_tiles);
					for (auto i = old_tiles.begin(); i != old_tiles.end(); ++i) {
						int zorder = i->first;
						std::vector<std::string> tiles = i->second;
						undo.push_back([=](){ lvl->addTileRectVector(zorder, x, y, x, y, tiles); });
						redo.push_back([=](){ lvl->addTileRectVector(zorder, x, y, x, y, std::vector<std::string>(1,"")); });
					}

					old_tiles.clear();
	
					x += diffx*TileSize;
					y += diffy*TileSize;

					min_x = std::min(x, min_x);
					max_x = std::max(x, max_x);
					min_y = std::min(y, min_y);
					max_y = std::max(y, max_y);

					lvl->getAllTilesRect(x, y, x, y, old_tiles);
					for (auto i = old_tiles.begin(); i != old_tiles.end(); ++i) {
						int zorder = i->first;
						std::vector<std::string> tiles = i->second;
						undo.push_back([=](){ lvl->addTileRectVector(zorder, x, y, x, y, tiles); });
						redo.push_back([=](){ lvl->addTileRectVector(zorder, x, y, x, y, std::vector<std::string>(1,"")); });
					}
				}

			
				for(const point& p : tile_selection_.tiles) {
					const int x = p.x*TileSize;
					const int y = p.y*TileSize;

					min_x = std::min(x + diffx*TileSize, min_x);
					max_x = std::max(x + diffx*TileSize, max_x);
					min_y = std::min(y + diffy*TileSize, min_y);
					max_y = std::max(y + diffy*TileSize, max_y);
	
					std::map<int, std::vector<std::string> > old_tiles;
					lvl->getAllTilesRect(x, y, x, y, old_tiles);
					for (auto i = old_tiles.begin(); i != old_tiles.end(); ++i) {
						int zorder = i->first;
						std::vector<std::string> tiles = i->second;
						redo.push_back([=](){ lvl->addTileRectVector(zorder, x + diffx*TileSize, y + diffy*TileSize, x + diffx*TileSize, y + diffy*TileSize, tiles); });
					}
				}

				if(!tile_selection_.tiles.empty()) {
					undo.push_back(std::bind(&Level::start_rebuild_tiles_in_background, lvl.get(), std::vector<int>()));
					redo.push_back(std::bind(&Level::start_rebuild_tiles_in_background, lvl.get(), std::vector<int>()));
				}
			}

			tile_selection new_selection = tile_selection_;
			for(point& p : new_selection.tiles) {
				p.x += diffx;
				p.y += diffy;
			}
			
			redo.push_back(std::bind(&editor::setSelection, this, new_selection));
			undo.push_back(std::bind(&editor::setSelection, this, tile_selection_));

			executeCommand(
			  std::bind(execute_functions, redo),
			  std::bind(execute_functions, undo));
			
		} else if(!drawing_rect_) {
			//wasn't drawing a rect.
			if(event.button == SDL_BUTTON_LEFT && tool() == TOOL_MAGIC_WAND) {
				select_magic_wand(anchorx_, anchory_);
			}
		} else if(event.button == SDL_BUTTON_LEFT) {

			if(tool() == TOOL_ADD_RECT) {

				lvl_->freeze_rebuild_tiles_in_background();
				if(tmp_undo_.get()) {
					//if we have a temporary change that was made while dragging
					//to preview the change, undo that now.
					tmp_undo_->undo_command();
					tmp_undo_.reset();
				}

				add_tile_rect(anchorx_, anchory_, xpos, ypos);
				lvl_->unfreeze_rebuild_tiles_in_background();
			} else if(tool() == TOOL_SELECT_RECT) {
				select_tile_rect(anchorx_, anchory_, xpos, ypos);
			}
			  
		} else if(event.button == SDL_BUTTON_RIGHT) {
			lvl_->freeze_rebuild_tiles_in_background();
			if(tmp_undo_.get()) {
				//if we have a temporary change that was made while dragging
				//to preview the change, undo that now.
				tmp_undo_->undo_command();
				tmp_undo_.reset();
			}
			remove_tile_rect(anchorx_, anchory_, xpos, ypos);
			lvl_->unfreeze_rebuild_tiles_in_background();
		}
	} else {
		//some kind of object editing
		if(event.button == SDL_BUTTON_RIGHT) {
			LOG_DEBUG("RIGHT: " << anchorx_ << ", " << xpos << " -- " << anchory_ << ", " << ypos);
			if(abs(anchorx_ - xpos) < 16 && abs(anchory_ - ypos) < 16) {
				std::vector<EntityPtr> chars = lvl_->get_characters_at_point(anchorx_, anchory_, xpos_, ypos_);
				std::vector<editor_menu_dialog::menu_item> items;
				for(EntityPtr e : chars) {
					editor_menu_dialog::menu_item item;
					item.description = e->getDebugDescription();
					item.action = [=]() {
						lvl_->editor_clear_selection();
						lvl_->editor_select_object(e);
						property_dialog_->setEntityGroup(lvl_->editor_selection());
						if(this->tool() == TOOL_ADD_OBJECT) {
							this->change_tool(TOOL_SELECT_OBJECT);
						}

						this->current_dialog_ = property_dialog_.get();
					};

					items.push_back(item);
				}

				editor_menu_dialog_->showMenu(items);
				return;
			}
			
			std::vector<std::function<void()> > undo, redo;
			const rect rect_selected(rect::from_coordinates(anchorx_, anchory_, xpos, ypos));
			std::vector<EntityPtr> chars = lvl_->get_characters_in_rect(rect_selected, xpos_, ypos_);

			//Delete all the objects in the rect.
			for(const EntityPtr& c : chars) {
				if(c->wasSpawnedBy().empty() == false) {
					continue;
				}
				LOG_INFO("REMOVING RECT CHAR: " << c->getDebugDescription());
				for(LevelPtr lvl : levels_) {
					EntityPtr obj = lvl->get_entity_by_label(c->label());
					generate_remove_commands(obj, undo, redo);
				}
			}

			if(property_dialog_ && property_dialog_.get() == current_dialog_ && property_dialog_->getEntity() && property_dialog_->getEntity()->getEditorInfo()) {
				//As well as removing objects, we will remove any vertices
				//that we see.
				for(auto& var : property_dialog_->getEntity()->getEditorInfo()->getVarsAndProperties()) {
					const std::string& name = var.getVariableName();
					const VARIABLE_TYPE type = var.getType();
					if(type != VARIABLE_TYPE::POINTS) {
						continue;
					}

					variant value = property_dialog_->getEntity()->queryValue(name);
					if(!value.is_list()) {
						continue;
					}

					std::vector<point> points;
					for(const variant& v : value.as_list()) {
						points.push_back(point(v));
					}

					bool modified = false;
					for(std::vector<point>::iterator i = points.begin(); i != points.end(); ) {
						if(pointInRect(*i, rect_selected)) {
							modified = true;
							i = points.erase(i);
						} else {
							++i;
						}
					}

					if(modified) {
						std::vector<variant> points_var;
						for(const point& p : points) {
							points_var.push_back(p.write());
						}

						generate_mutate_commands(property_dialog_->getEntity(), name, variant(&points_var), undo, redo);
					}
				}
			}

			executeCommand(
			  std::bind(execute_functions, redo),
			  std::bind(execute_functions, undo));
			on_modify_level();
		} else if(tool() == TOOL_SELECT_OBJECT && drawing_rect_) {
			std::vector<EntityPtr> chars = lvl_->get_characters_in_rect(rect::from_coordinates(anchorx_, anchory_, xpos, ypos), xpos_, ypos_);
			if(chars.empty()) {
				//no chars is just a no-op.
				drawing_rect_ = dragging_ = false;
				return;
			}

			const bool ctrl_pressed = (SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) != 0;
			for(const EntityPtr& c : chars) {
				if(c->wasSpawnedBy().empty() || ctrl_pressed) {
					lvl_->editor_select_object(c);
				}
			}

			property_dialog_->setEntityGroup(lvl_->editor_selection());

			if(lvl_->editor_selection().size() == 1) {
				current_dialog_ = property_dialog_.get();
				property_dialog_->setEntity(lvl_->editor_selection().front());

				set_code_file();
			} else {
				current_dialog_ = property_dialog_.get();
			}
		}
	}

	drawing_rect_ = dragging_ = false;
}


void editor::load_stats()
{
}

void editor::show_stats()
{
	editor_dialogs::EditorStatsDialog stats_dialog(*this);
	stats_dialog.showModal();
}

void editor::download_stats()
{
	const bool result = stats::download(lvl_->id());
	if(result) {
		debug_console::addMessage("Got latest stats from the server");
		try {
			load_stats();
		} catch(...) {
			debug_console::addMessage("Error parsing stats");
			LOG_ERROR("ERROR LOADING STATS");
		}
	} else {
		debug_console::addMessage("Download of stats failed");
	}
}

int editor::get_tile_zorder(const std::string& tile_id) const
{
	for(const editor::tileset& tile : tilesets) {
		if(tile.type == tile_id) {
			return tile.zorder;
		}
	}

	return 0;
}

void editor::add_tile_rect(int zorder, const std::string& tile_id, int x1, int y1, int x2, int y2)
{
	if(x2 < x1) {
		std::swap(x1, x2);
	}

	if(y2 < y1) {
		std::swap(y1, y2);
	}

	std::vector<std::function<void()> > undo, redo;

	for(LevelPtr lvl : levels_) {
		std::vector<std::string> old_rect;
		lvl->get_tile_rect(zorder, x1, y1, x2, y2, old_rect);

		if(std::count(old_rect.begin(), old_rect.end(), tile_id) == old_rect.size()) {
			//not modifying anything, so skip.
			continue;
		}

		redo.push_back([=](){ lvl->add_tile_rect(zorder, x1, y1, x2, y2, tile_id); });
		undo.push_back([=](){ lvl->addTileRectVector(zorder, x1, y1, x2, y2, old_rect); });

		std::vector<int> layers;
		layers.push_back(zorder);
		undo.push_back(std::bind(&Level::start_rebuild_tiles_in_background, lvl.get(), layers));
		redo.push_back(std::bind(&Level::start_rebuild_tiles_in_background, lvl.get(), layers));
	}

	executeCommand(
	  std::bind(execute_functions, redo),
	  std::bind(execute_functions, undo));
	on_modify_level();

	if(layers_dialog_) {
		layers_dialog_->init();
	}
}

void editor::add_tile_rect(int x1, int y1, int x2, int y2)
{
	x1 += ((100 - tilesets[cur_tileset_].x_speed)*xpos_)/100;
	x2 += ((100 - tilesets[cur_tileset_].x_speed)*xpos_)/100;
	y1 += ((100 - tilesets[cur_tileset_].y_speed)*ypos_)/100;
	y2 += ((100 - tilesets[cur_tileset_].y_speed)*ypos_)/100;

	add_tile_rect(tilesets[cur_tileset_].zorder, tilesets[cur_tileset_].type, x1, y1, x2, y2);
	for(LevelPtr lvl : levels_) {
		lvl->set_tile_layer_speed(tilesets[cur_tileset_].zorder,
		                          tilesets[cur_tileset_].x_speed,
								  tilesets[cur_tileset_].y_speed);
	}
}

void editor::remove_tile_rect(int x1, int y1, int x2, int y2)
{
	x1 += ((100 - tilesets[cur_tileset_].x_speed)*xpos_)/100;
	x2 += ((100 - tilesets[cur_tileset_].x_speed)*xpos_)/100;
	y1 += ((100 - tilesets[cur_tileset_].y_speed)*ypos_)/100;
	y2 += ((100 - tilesets[cur_tileset_].y_speed)*ypos_)/100;

	if(x2 < x1) {
		std::swap(x1, x2);
	}

	if(y2 < y1) {
		std::swap(y1, y2);
	}

	std::vector<std::function<void()>> redo, undo;
	for(LevelPtr lvl : levels_) {

		std::map<int, std::vector<std::string> > old_tiles;
		lvl->getAllTilesRect(x1, y1, x2, y2, old_tiles);
		std::vector<int> layers;
		for (auto i = old_tiles.begin(); i != old_tiles.end(); ++i) {
			if(std::count(layers.begin(), layers.end(), i->first) == 0) {
				layers.push_back(i->first);
			}

			int key = i->first;
			std::vector<std::string> value = i->second;
			undo.push_back([=](){ lvl->addTileRectVector(key, x1, y1, x2, y2, value); });
		}

		redo.push_back([=](){ lvl->clear_tile_rect(x1, y1, x2, y2); });

		undo.push_back(std::bind(&Level::start_rebuild_tiles_in_background, lvl.get(), layers));
		redo.push_back(std::bind(&Level::start_rebuild_tiles_in_background, lvl.get(), layers));
	}

	executeCommand(std::bind(execute_functions, redo), std::bind(execute_functions, undo));
	on_modify_level();
}

void editor::select_tile_rect(int x1, int y1, int x2, int y2)
{
	tile_selection new_selection;

	const bool shift_pressed = (SDL_GetModState()&KMOD_SHIFT) != 0;
	if(shift_pressed) {
		//adding to the selection
		new_selection = tile_selection_;
	}

	if(x2 < x1) {
		std::swap(x1, x2);
	}

	if(y2 < y1) {
		std::swap(y1, y2);
	}

	if(x2 - x1 > TileSize/4 || y2 - y1 > TileSize/4) {
		x2 += TileSize;
		y2 += TileSize;

		x1 = round_tile_size(x1)/TileSize;
		y1 = round_tile_size(y1)/TileSize;
		x2 = round_tile_size(x2)/TileSize;
		y2 = round_tile_size(y2)/TileSize;

		for(int x = x1; x != x2; ++x) {
			for(int y = y1; y != y2; ++y) {
				const point p(x, y);
				new_selection.tiles.push_back(p);
			}
		}

		std::sort(new_selection.tiles.begin(), new_selection.tiles.end());

		const bool alt_pressed = (SDL_GetModState()&(KMOD_LALT|KMOD_RALT)) != 0;
		if(alt_pressed) {
			//diff from selection
			tile_selection diff;
			for(const point& p : tile_selection_.tiles) {
				if(std::binary_search(new_selection.tiles.begin(), new_selection.tiles.end(), p) == false) {
					diff.tiles.push_back(p);
				}
			}

			new_selection.tiles.swap(diff.tiles);
		}
	}

	executeCommand(
	  std::bind(&editor::setSelection, this, new_selection),	
	  std::bind(&editor::setSelection, this, tile_selection_));
}

void editor::select_magic_wand(int xpos, int ypos)
{
	tile_selection new_selection;

	const bool ctrl_pressed = (SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) != 0;
	if(ctrl_pressed) {
		//adding to the selection
		new_selection = tile_selection_;
	}

	std::vector<point> tiles = lvl_->get_solid_contiguous_region(xpos, ypos);
	new_selection.tiles.insert(new_selection.tiles.end(), tiles.begin(), tiles.end());
	executeCommand(
	  std::bind(&editor::setSelection, this, new_selection),	
	  std::bind(&editor::setSelection, this, tile_selection_));
}

void editor::setSelection(const tile_selection& s)
{
	tile_selection_ = s;
}

void editor::move_object(LevelPtr lvl, EntityPtr e, int new_x, int new_y)
{
	CurrentLevelScope scope(lvl.get());
	lvl->relocate_object(e, new_x, new_y);
}

void editor::toggle_object_facing(LevelPtr lvl, EntityPtr e, bool upside_down)
{
	CurrentLevelScope scope(lvl.get());
	if(upside_down) {
		e->setUpsideDown(!e->isUpsideDown());
	} else {
		e->setFacingRight(!e->isFacingRight());
	}
}

void editor::change_object_rotation(LevelPtr lvl, EntityPtr e, float rotation)
{
	e->setRotateZ(rotation);
}

void editor::change_object_scale(LevelPtr lvl, EntityPtr e, float scale)
{
	e->setDrawScale(scale);
}

const std::vector<editor::tileset>& editor::all_tilesets() const
{
	return tilesets;
}

std::vector<editor::enemy_type>& editor::all_characters() const
{
	if(enemy_types.empty()) {
		typedef std::pair<std::string, CustomObjectType::EditorSummary> type_cat;
		for(const type_cat& item : CustomObjectType::getEditorCategories()) {
			enemy_types.push_back(enemy_type(item.first, item.second.category, item.second.first_frame));
			enemy_types.back().help = item.second.help;
		}
	}

	return enemy_types;
}

void editor::set_tileset(int index)
{
	cur_tileset_ = index;
	if(cur_tileset_ < 0) {
		cur_tileset_ = static_cast<int>(tilesets.size()) - 1;
	} else if(static_cast<unsigned>(cur_tileset_) >= tilesets.size()) {
		cur_tileset_ = 0;
	}

	for(LevelPtr lvl : levels_) {
		lvl->set_tile_layer_speed(tilesets[cur_tileset_].zorder,
		                          tilesets[cur_tileset_].x_speed,
								  tilesets[cur_tileset_].y_speed);
	}
}

void editor::setObject(int index)
{
	int max = static_cast<int>(all_characters().size());

	if(index < 0) {
		index = max - 1;
	} else if(index >= max) {
		index = 0;
	}

	cur_object_ = index;
}

editor::EDIT_TOOL editor::tool() const
{
	const bool alt_pressed = (SDL_GetModState()&KMOD_ALT) != 0;
	if(alt_pressed) {
		switch(tool_) {
		case TOOL_ADD_OBJECT:
		case TOOL_ADD_RECT:
		case TOOL_SELECT_RECT:
		case TOOL_MAGIC_WAND:
		case TOOL_PENCIL:
		case TOOL_PICKER:
			return TOOL_PICKER;
		default:
			break;
		}
	}

	return tool_;
}

void editor::change_tool(EDIT_TOOL tool)
{
	tool_ = tool;
	selected_segment_ = -1;

	LOG_INFO("CHANGE TOOL: " << (int)tool);

	switch(tool_) {
	case TOOL_ADD_RECT:
	case TOOL_SELECT_RECT:
	case TOOL_MAGIC_WAND:
	case TOOL_PENCIL:
	case TOOL_PICKER: {
		if(!tileset_dialog_) {
			tileset_dialog_.reset(new editor_dialogs::TilesetEditorDialog(*this));
		}
		current_dialog_ = tileset_dialog_.get();
		lvl_->editor_clear_selection();
		break;
	}
	case TOOL_ADD_OBJECT: {
		if(!character_dialog_) {
			character_dialog_.reset(new editor_dialogs::CharacterEditorDialog(*this));
		}
		current_dialog_ = character_dialog_.get();
		character_dialog_->set_character(cur_object_);
		break;
	}
	case TOOL_SELECT_OBJECT: {
		current_dialog_ = property_dialog_.get();
		break;
	}
	case TOOL_EDIT_SEGMENTS: {

		if(!segment_dialog_) {
			segment_dialog_.reset(new editor_dialogs::SegmentEditorDialog(*this));
		}
	
		current_dialog_ = segment_dialog_.get();
		segment_dialog_->setSegment(selected_segment_);
		break;
	}
	default: {
		break;
	}
	}

	if(editor_mode_dialog_) {
		editor_mode_dialog_->init();
	}

	reset_dialog_positions();
}

void editor::save_level_as(const std::string& fname)
{
	const std::string id = module::make_module_id(fname);
	all_editors.erase(filename_);
	all_editors[id] = this;

	std::string path = module::get_id(fname);
	std::string modname = module::get_module_id(fname);
	sys::write_file(module::get_module_path(modname, preferences::editor_save_to_user_preferences() ? module::BASE_PATH_USER : module::BASE_PATH_GAME) + path, "");
	load_level_paths();
	filename_ = id;
	save_level();
	g_last_edited_level() = id;
}

void editor::quit()
{
	if(confirm_quit()) {
		done_ = true;
	}
}

namespace 
{
	void quit_editor_result(gui::Dialog* d, int* result_ptr, int result) {
		d->close();
		*result_ptr = result;
	}
}

bool editor::confirm_quit(bool allow_cancel)
{
	if(mouselook_mode()) {
		mouselook_mode_ = false;
		SDL_SetRelativeMouseMode(SDL_FALSE);
	}

	if(!level_changed_) {
		return true;
	}

	const int center_x = KRE::WindowManager::getMainWindow()->width()/2;
	const int center_y = KRE::WindowManager::getMainWindow()->height()/2;
	using namespace gui;
	Dialog d(center_x - 140, center_y - 100, center_x + 140, center_y + 100);

	d.addWidget(WidgetPtr(new Label("Do you want to save the level?", KRE::Color::colorWhite())), Dialog::MOVE_DIRECTION::DOWN);

	Grid* grid = new Grid(allow_cancel ? 3 : 2);

	int result = 0;
	grid->addCol(WidgetPtr(
	  new Button(WidgetPtr(new Label("Yes", KRE::Color::colorWhite())),
	             std::bind(quit_editor_result, &d, &result, 0))));
	grid->addCol(WidgetPtr(
	  new Button(WidgetPtr(new Label("No", KRE::Color::colorWhite())),
	             std::bind(quit_editor_result, &d, &result, 1))));
	if(allow_cancel) {
		grid->addCol(WidgetPtr(
		  new Button(WidgetPtr(new Label("Cancel", KRE::Color::colorWhite())),
		             std::bind(quit_editor_result, &d, &result, 2))));
	}
	d.addWidget(WidgetPtr(grid));
	d.showModal();

	if(result == 2) {
		return false;
	}

	if(result == 0 && !d.cancelled()) {
		save_level();
	}

	return true;
}

void editor::autosave_level()
{
	controls::control_backup_scope ctrl_backup;

	toggle_active_level();

	remove_ghost_objects();
	ghost_objects_.clear();

	std::string data;
	variant lvl_node = lvl_->write();
	std::map<variant,variant> attr = lvl_node.as_map();
	attr.erase(variant("cycle"));  //levels saved in the editor should never
	                               //have a cycle attached to them so that
								   //all levels start at cycle 0.
	lvl_node = variant(&attr);
	const std::string target_path = std::string(preferences::user_data_path()) + "/autosave.cfg";
	if(sys::file_exists(target_path)) {
		const std::string backup_path = target_path + ".1";
		if(sys::file_exists(backup_path)) {
			sys::remove_file(backup_path);
		}

		sys::move_file(target_path, backup_path);
	}

	sys::write_file(target_path, lvl_node.write_json(true));

	toggle_active_level();
}

void editor::save_level()
{
	controls::control_backup_scope ctrl_backup;

	toggle_active_level();

	lvl_->setId(filename_);

	level_changed_ = 0;

	remove_ghost_objects();
	ghost_objects_.clear();

	std::string data;
	variant lvl_node = lvl_->write();
	std::map<variant,variant> attr = lvl_node.as_map();
	attr.erase(variant("cycle"));  //levels saved in the editor should never
	                               //have a cycle attached to them so that
								   //all levels start at cycle 0.
	lvl_node = variant(&attr);
	LOG_INFO("GET LEVEL FILENAME: " << filename_);
	std::string path = get_level_path(filename_);
	if(preferences::editor_save_to_user_preferences()) {
		path = module::get_module_path(module::get_module_name(), module::BASE_PATH_USER) + "/data/level/" + filename_;
	}

	LOG_INFO("WRITE_LEVEL: " << path);
	sys::write_file(path, lvl_node.write_json(true));

	//see if we should write the next/previous levels also
	//based on them having changed.
	if(lvl_->previous_level().empty() == false) {
		try {
			LevelPtr prev(new Level(lvl_->previous_level()));
			prev->finishLoading();
			if(prev->next_level() != lvl_->id()) {
				prev->set_next_level(lvl_->id());
				if(preferences::editor_save_to_user_preferences()) {
					sys::write_file(module::get_module_path(module::get_module_name(), module::BASE_PATH_USER) + "/data/level/" + prev->id(), prev->write().write_json(true));
				} else {
					sys::write_file(module::map_file(prev->id()), prev->write().write_json(true));
				}
			}
		} catch(...) {
		}
	}

	if(lvl_->next_level().empty() == false) {
		try {
			LevelPtr next(new Level(lvl_->next_level()));
			next->finishLoading();
			if(next->previous_level() != lvl_->id()) {
				next->set_previous_level(lvl_->id());
				if(preferences::editor_save_to_user_preferences()) {
					sys::write_file(module::get_module_path("", module::BASE_PATH_USER) + "/data/level/" + next->id(), next->write().write_json(true));
				} else {
					sys::write_file(module::map_file(next->id()), next->write().write_json(true));
				}
			}
		} catch(...) {
		}
	}

	toggle_active_level();
}

void editor::zoomIn()
{
	if(zoom_ > 1) {
		zoom_ /= 2;
	}
}

void editor::zoomOut()
{
	if(zoom_ < 8) {
		zoom_ *= 2;
	}
}

void BuiltinEditor::draw_gui() const
{
	auto canvas = KRE::Canvas::getInstance();
	auto mm = std::unique_ptr<KRE::ModelManager2D>(new KRE::ModelManager2D(-xpos_, -ypos_, 0, 1.0f/zoom_));

	const bool ctrl_pressed = (SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) != 0;
	int mousex, mousey;
	input::sdl_get_mouse_state(&mousex, &mousey);
	const int selectx = xpos_ + mousex*zoom_;
	const int selecty = ypos_ + mousey*zoom_;

	{
	std::string next_level = "To " + lvl_->next_level();
	std::string previous_level = "To " + lvl_->previous_level();
	if(lvl_->next_level().empty()) {
		next_level = "(no next level)";
	}
	if(lvl_->previous_level().empty()) {
		previous_level = "(no previous level)";
	}
	auto t = KRE::Font::getInstance()->renderText(previous_level, KRE::Color::colorBlack(), 24);
	auto t2 = KRE::Font::getInstance()->renderText(previous_level, KRE::Color::colorWhite(), 24);
	int x = lvl_->boundaries().x() - t->width();
	int y = ypos_ + canvas->height()/2;

	//outline the text with white to make it readable against any background
	canvas->blitTexture(t2, 0, rect(x-2,y,0,0));
	canvas->blitTexture(t2, 0, rect(x+2,y,0,0));
	canvas->blitTexture(t2, 0, rect(x,y-2,0,0));
	canvas->blitTexture(t2, 0, rect(x,y+2,0,0));

	canvas->blitTexture(t, 0, rect(x,y,0,0));
	

	t = KRE::Font::getInstance()->renderText(next_level, KRE::Color::colorBlack(), 24);
	x = lvl_->boundaries().x2();
	canvas->blitTexture(t, 0, rect(x,y,0,0));
	}

	if(tool() == TOOL_ADD_OBJECT && !lvl_->editor_highlight()) {
		Entity& e = *all_characters()[cur_object_].preview_object();
		int x = round_tile_size(xpos_ + mousex*zoom_) + TileSize/(2*zoom_);
		int y = round_tile_size(ypos_ + mousey*zoom_) + TileSize/(2*zoom_);

		if(ctrl_pressed) {
			x = xpos_ + mousex*zoom_;
			y = ypos_ + mousey*zoom_;
		}

		x -= e.getCurrentFrame().width()/2;
		y -= e.getCurrentFrame().height()/2;

		e.setPos(x, y);
		if(place_entity_in_level(*lvl_, e)) {
			graphics::GameScreen::Manager screen_manager(KRE::WindowManager::getMainWindow());
			KRE::ColorScope cm(KRE::Color(1.0f, 1.0f, 1.0f, 0.5f));
			all_characters()[cur_object_].preview_frame()->draw(nullptr, e.x(), e.y(), face_right_, upside_down_);
		}
	}

	if(drawing_rect_) {
		const int x1 = anchorx_;
		const int x2 = xpos_ + mousex*zoom_;

		const int y1 = anchory_;
		const int y2 = ypos_ + mousey*zoom_;

		canvas->drawHollowRect(rect::from_coordinates(x1, y1, x2, y2), KRE::Color::colorWhite());
	}
	
	if(property_dialog_ && property_dialog_.get() == current_dialog_ &&
	   property_dialog_->getEntity() &&
	   property_dialog_->getEntity()->getEditorInfo() &&
	   std::count(lvl_->get_chars().begin(), lvl_->get_chars().end(),
	              property_dialog_->getEntity())) {

		//number of variables seen of each type, used to
		//cycle through colors for each variable type.
		std::map<VARIABLE_TYPE, int> nseen_variables;

		int selected_index = -1;
		const auto* selected_var = variable_info_selected(property_dialog_->getEntity(), xpos_ + mousex*zoom_, ypos_ + mousey*zoom_, zoom_, &selected_index);
		for(auto& var : property_dialog_->getEntity()->getEditorInfo()->getVarsAndProperties()) {
			const std::string& name = var.getVariableName();
			const VARIABLE_TYPE type = var.getType();
			const int color_index = nseen_variables[type]++;
			variant value = property_dialog_->getEntity()->queryValue(name);
			KRE::Color color;
			switch(color_index) {
			case 0: color = KRE::Color(255, 0, 0, 255); break;
			case 1: color = KRE::Color(0, 255, 0, 255); break;
			case 2: color = KRE::Color(0, 0, 255, 255); break;
			case 3: color = KRE::Color(255, 255, 0, 255); break;
			default:color = KRE::Color(255, 0, 255, 255); break;
			}

			KRE::Color line_color = (&var == selected_var) ? KRE::Color(255, 255, 0, 255) : color;

			std::vector<glm::vec2> varray;
			switch(type) {
				case VARIABLE_TYPE::XPOSITION:
					if(value.is_int()) {
						varray.emplace_back(value.as_int(), ypos_);
						varray.emplace_back(value.as_int(), ypos_ + canvas->height()*zoom_);
					}
					break;
				case VARIABLE_TYPE::YPOSITION:
					if(value.is_int()) {
						varray.emplace_back(xpos_, value.as_int());
						varray.emplace_back(xpos_ + canvas->width()*zoom_, value.as_int());
					}
					break;
				case VARIABLE_TYPE::POINTS:
					if(value.is_list()) {
						std::vector<variant> items = value.as_list();

						int index = 0;
						for(const variant& item : items) {
							point p(item);
							KRE::Color col = color;
							if(&var == selected_var && index == selected_index) {
								col = KRE::Color(255, 255, 0, 255);
							}

							canvas->drawSolidRect(rect(p.x, p.y-10, 1, 20), col);
							canvas->drawSolidRect(rect(p.x-10, p.y, 20, 1), col);
							canvas->blitTexture(KRE::Font::getInstance()->renderText(formatter() << (index+1), col, 12), 0, rect(p.x+4, p.y-14));
							++index;
						}
					}
					break;
				default:
					break;
			}

			if(!varray.empty()) {
				canvas->drawLines(varray, 1.0f, line_color);
			}
		}
	}

	if(g_draw_stats) {
//		stats::draw_stats(stats_);
	}

	if(dragging_ && g_current_draw_tiles.empty() == false) {
		std::vector<glm::vec2> varray;

		for(point p : g_current_draw_tiles) {
			const int x = 1 + p.x - xpos_;
			const int y = 1 + p.y - ypos_;
			const int dim = TileSize - 2;
			 
			varray.emplace_back(x,     y    ); varray.emplace_back(x+dim, y    );
			varray.emplace_back(x+dim, y    ); varray.emplace_back(x+dim, y+dim);
			varray.emplace_back(x+dim, y+dim); varray.emplace_back(x,     y+dim);
			varray.emplace_back(x,     y+dim); varray.emplace_back(x,     y    );
		}
		canvas->drawLines(varray, 1.0f, KRE::Color(255, 255, 255, 128));
	}

	// Clear the current applied model
	mm.reset();

	//draw the difficulties of segments.
	if(lvl_->segment_width() > 0 || lvl_->segment_height() > 0) {
		const int seg_width = lvl_->segment_width() ? lvl_->segment_width() : lvl_->boundaries().w();
		const int seg_height = lvl_->segment_height() ? lvl_->segment_height() : lvl_->boundaries().h();
		rect boundaries = modify_selected_rect(*this, lvl_->boundaries(), selectx, selecty);
		int seg = 0;
		for(int ypos = boundaries.y(); ypos < boundaries.y2(); ypos += seg_height) {
			const int y1 = ypos/zoom_;
			for(int xpos = boundaries.x(); xpos < boundaries.x2(); xpos += seg_width) {
				const int difficulty = lvl_->get_var(formatter() << "segment_difficulty_start_" << seg).as_int();
//				if(difficulty) {
					canvas->blitTexture(KRE::Font::getInstance()->renderText(formatter() << "Difficulty: " << difficulty, KRE::Color::colorWhite(), 14), 0, rect((xpos - xpos_)/zoom_, y1 - 20 - ypos_/zoom_, 0, 0));
//				}		
				++seg;
			}
		}
	}

	//draw grid
	if(g_editor_grid){
		std::vector<glm::vec2> varray;
		std::vector<glm::u8vec4> carray;
		const int w = canvas->width();
		const int h = canvas->height();
		for(int x = -TileSize - (xpos_%TileSize)/zoom_; x < w; x += (BaseTileSize*g_tile_scale)/zoom_) {
			varray.emplace_back(x, 0);
			varray.emplace_back(x, h);

			const int xco = xpos_ + x*zoom_;

			if(std::abs(xco) <= zoom_) {
				carray.emplace_back(255, 128, 128, 255);
				carray.emplace_back(255, 128, 128, 255);
			} else {
				carray.emplace_back(255, 255, 255, 96);
				carray.emplace_back(255, 255, 255, 96);
			}
		}
		for(int y = -TileSize - (ypos_%TileSize)/zoom_; y < h; y += (BaseTileSize*g_tile_scale)/zoom_) {
			varray.emplace_back(0, y);
			varray.emplace_back(w, y);

			const int yco = ypos_ + y*zoom_;

			if(std::abs(yco) <= zoom_) {
				carray.emplace_back(255, 128, 128, 255);
				carray.emplace_back(255, 128, 128, 255);
			} else {
				carray.emplace_back(255, 255, 255, 96);
				carray.emplace_back(255, 255, 255, 96);
			}
		}
		//canvas->drawLines(varray, 1.0f, KRE::Color(255,255,255,64));
		canvas->drawLines(varray, 1.0f, carray);
	}

	// draw level boundaries in clear white
	{
		std::vector<glm::vec2> varray;
		std::vector<glm::u8vec4> carray;

		rect boundaries = modify_selected_rect(*this, lvl_->boundaries(), selectx, selecty);
		const int x1 = boundaries.x()/zoom_;
		const int x2 = boundaries.x2()/zoom_;
		const int y1 = boundaries.y()/zoom_;
		const int y2 = boundaries.y2()/zoom_;
		
		glm::u8vec4 selected_color = KRE::Color::colorYellow().as_u8vec4();
		glm::u8vec4 normal_color = KRE::Color::colorWhite().as_u8vec4();

		if(resizing_top_level_edge || rect_top_edge_selected(lvl_->boundaries(), selectx, selecty, zoom_)) {
			carray.emplace_back(selected_color);
			carray.emplace_back(selected_color);
		} else {
			carray.emplace_back(normal_color);
			carray.emplace_back(normal_color);
		}
		
		varray.emplace_back(x1 - xpos_/zoom_, y1 - ypos_/zoom_);
		varray.emplace_back(x2 - xpos_/zoom_, y1 - ypos_/zoom_);

		if(resizing_left_level_edge || rect_left_edge_selected(lvl_->boundaries(), selectx, selecty, zoom_)) {
			carray.emplace_back(selected_color);
			carray.emplace_back(selected_color);
		} else {
			carray.emplace_back(normal_color);
			carray.emplace_back(normal_color);
		}

		varray.emplace_back(x1 - xpos_/zoom_, y1 - ypos_/zoom_);
		varray.emplace_back(x1 - xpos_/zoom_, y2 - ypos_/zoom_);

		if(resizing_right_level_edge || rect_right_edge_selected(lvl_->boundaries(), selectx, selecty, zoom_)) {
			carray.emplace_back(selected_color);
			carray.emplace_back(selected_color);
		} else {
			carray.emplace_back(normal_color);
			carray.emplace_back(normal_color);
		}
		
		varray.emplace_back(x2 - xpos_/zoom_, y1 - ypos_/zoom_);
		varray.emplace_back(x2 - xpos_/zoom_, y2 - ypos_/zoom_);

		if(resizing_bottom_level_edge || rect_bottom_edge_selected(lvl_->boundaries(), selectx, selecty, zoom_)) {
			carray.emplace_back(selected_color);
			carray.emplace_back(selected_color);
		} else {
			carray.emplace_back(normal_color);
			carray.emplace_back(normal_color);
		}
		
		varray.emplace_back(x1 - xpos_/zoom_, y2 - ypos_/zoom_);
		varray.emplace_back(x2 - xpos_/zoom_, y2 - ypos_/zoom_);

		if(lvl_->segment_width() > 0) {
			for(int xpos = boundaries.x() + lvl_->segment_width(); xpos < boundaries.x2(); xpos += lvl_->segment_width()) {
				varray.emplace_back((xpos - xpos_)/zoom_, y1 - ypos_/zoom_);
				varray.emplace_back((xpos - xpos_)/zoom_, y2 - ypos_/zoom_);

				carray.emplace_back(normal_color);
				carray.emplace_back(normal_color);
			}
		}

		if(lvl_->segment_height() > 0) {
			for(int ypos = boundaries.y() + lvl_->segment_height(); ypos < boundaries.y2(); ypos += lvl_->segment_height()) {
				varray.emplace_back(x1 - xpos_/zoom_, (ypos - ypos_)/zoom_);
				varray.emplace_back(x2 - xpos_/zoom_, (ypos - ypos_)/zoom_);

				carray.emplace_back(normal_color);
				carray.emplace_back(normal_color);
			}
		}
		
		canvas->drawLines(varray, 1.0f, carray);
	}

	//draw level sub-components
	int nsub = 0;
	for(const Level::SubComponent& sub : lvl_->getSubComponents()) {

		std::vector<glm::vec2> varray;
		std::vector<glm::u8vec4> carray;

		{

			rect source_area = sub.source_area;
			rect area(source_area.x() + -(source_area.w()+TileSize*4), source_area.y(), source_area.w(), source_area.h());

			std::vector<rect> areas;
			areas.push_back(area);

			if(LevelRunner::getCurrent()->is_paused()) {
				for(const Level::SubComponentUsage& usage : lvl_->getSubComponentUsages()) {
					if(usage.ncomponent == nsub) {
						rect usage_area(usage.dest_area.x(), usage.dest_area.y(), usage.dest_area.w(), usage.dest_area.h());
						areas.push_back(usage_area);
					}
				}
			}

			const int x1 = area.x()/zoom_;
			const int x2 = area.x2()/zoom_;
			const int y1 = area.y()/zoom_;
			const int y2 = area.y2()/zoom_;

			varray.emplace_back(x1 - xpos_/zoom_, y1 - ypos_/zoom_);
			varray.emplace_back(x2 - xpos_/zoom_, y1 - ypos_/zoom_);

			varray.emplace_back(x2 - xpos_/zoom_, y1 - ypos_/zoom_);
			varray.emplace_back(x2 - xpos_/zoom_, y2 - ypos_/zoom_);

			varray.emplace_back(x2 - xpos_/zoom_, y2 - ypos_/zoom_);
			varray.emplace_back(x1 - xpos_/zoom_, y2 - ypos_/zoom_);

			varray.emplace_back(x1 - xpos_/zoom_, y2 - ypos_/zoom_);
			varray.emplace_back(x1 - xpos_/zoom_, y1 - ypos_/zoom_);

			for(int n = 0; n != 8; ++n) {
				carray.emplace_back(KRE::Color::colorBlue().as_u8vec4());
			}

			KRE::Color solid_color(255, 255, 255, 255);
			KRE::Color semi_color(127, 127, 127, 255);

			for(int ypos = 0; ypos < area.h(); ypos += TileSize) {
				for(int xpos = 0; xpos < area.w(); xpos += TileSize) {
					int nsolid = 0;
					for(int i = 0; i < sub.num_variations; ++i) {
						rect var_area(source_area.x() + (source_area.w()+TileSize*4)*i, source_area.y(), source_area.w(), source_area.h());
						if(lvl_->solid(var_area.x() + xpos + TileSize/2, var_area.y() + ypos + TileSize/2)) {
							++nsolid;
						}
					}

					if(nsolid > 0) {

						for(const rect& area : areas) {
							const int next_xpos = xpos + TileSize;
							const int next_ypos = ypos + TileSize;

							const int px_x = ((area.x() + xpos) - xpos_)/zoom_;
							const int px_y = ((area.y() + ypos) - ypos_)/zoom_;
							const int px_x2 = ((area.x() + next_xpos) - xpos_)/zoom_;
							const int px_y2 = ((area.y() + next_ypos) - ypos_)/zoom_;

							rect tile_area(px_x, px_y, px_x2 - px_x, px_y2 - px_y);
							canvas->drawSolidRect(tile_area, nsolid == sub.num_variations ? solid_color : semi_color);
						}
					}
				}
			}
		}

		for(int i = 0; i < sub.num_variations; ++i) {

			rect source_area = sub.source_area;
			bool dragging_right = resizing_sub_component_right_edge && resizing_sub_component_index == nsub;
			bool dragging_bottom = resizing_sub_component_bottom_edge && resizing_sub_component_index == nsub;

			if(dragging_right) {
				int deltax = xpos_ + mousex*zoom_ - anchorx_;

				int w = (std::max<int>(TileSize, source_area.w() + deltax)/TileSize)*TileSize;

				source_area = rect(source_area.x(), source_area.y(), w, source_area.h());
			}

			if(dragging_bottom) {
				int deltay = ypos_ + mousey*zoom_ - anchory_;

				int h = (std::max<int>(TileSize, source_area.h() + deltay)/TileSize)*TileSize;

				source_area = rect(source_area.x(), source_area.y(), source_area.w(), h);
			}

			rect area(source_area.x() + (source_area.w()+TileSize*4)*i, source_area.y(), source_area.w(), source_area.h());

			const int x1 = area.x()/zoom_;
			const int x2 = area.x2()/zoom_;
			const int y1 = area.y()/zoom_;
			const int y2 = area.y2()/zoom_;

			glm::u8vec4 selected_color = KRE::Color::colorYellow().as_u8vec4();
			glm::u8vec4 normal_color = KRE::Color::colorRed().as_u8vec4();

			if(rect_top_edge_selected(area, selectx, selecty, zoom_)) {
				normal_color = selected_color;
			}

			varray.emplace_back(x1 - xpos_/zoom_, y1 - ypos_/zoom_);
			varray.emplace_back(x2 - xpos_/zoom_, y1 - ypos_/zoom_);

			carray.emplace_back(normal_color);
			carray.emplace_back(normal_color);

			varray.emplace_back(x2 - xpos_/zoom_, y1 - ypos_/zoom_);
			varray.emplace_back(x2 - xpos_/zoom_, y2 - ypos_/zoom_);

			if(i == 0 && (rect_right_edge_selected(area, selectx, selecty, zoom_) || dragging_right)) {
				carray.emplace_back(selected_color);
				carray.emplace_back(selected_color);
			} else {
				carray.emplace_back(normal_color);
				carray.emplace_back(normal_color);
			}

			varray.emplace_back(x2 - xpos_/zoom_, y2 - ypos_/zoom_);
			varray.emplace_back(x1 - xpos_/zoom_, y2 - ypos_/zoom_);

			if(i == 0 && (rect_bottom_edge_selected(area, selectx, selecty, zoom_) || dragging_bottom)) {
				carray.emplace_back(selected_color);
				carray.emplace_back(selected_color);
			} else {
				carray.emplace_back(normal_color);
				carray.emplace_back(normal_color);
			}

			varray.emplace_back(x1 - xpos_/zoom_, y2 - ypos_/zoom_);
			varray.emplace_back(x1 - xpos_/zoom_, y1 - ypos_/zoom_);

			carray.emplace_back(normal_color);
			carray.emplace_back(normal_color);

			if(dragging_sub_component && resizing_sub_component_index == nsub) {
				//dragging a new sub-component usage, draw the rectangle
				int deltax = ((xpos_ + mousex*zoom_ - anchorx_)/TileSize)*TileSize;
				int deltay = ((ypos_ + mousey*zoom_ - anchory_)/TileSize)*TileSize;

				rect dest_area(source_area.x() + deltax, source_area.y() + deltay, source_area.w(), source_area.h());

				const int x1 = dest_area.x()/zoom_;
				const int x2 = dest_area.x2()/zoom_;
				const int y1 = dest_area.y()/zoom_;
				const int y2 = dest_area.y2()/zoom_;

				varray.emplace_back(x1 - xpos_/zoom_, y1 - ypos_/zoom_);
				varray.emplace_back(x2 - xpos_/zoom_, y1 - ypos_/zoom_);

				varray.emplace_back(x2 - xpos_/zoom_, y1 - ypos_/zoom_);
				varray.emplace_back(x2 - xpos_/zoom_, y2 - ypos_/zoom_);

				varray.emplace_back(x2 - xpos_/zoom_, y2 - ypos_/zoom_);
				varray.emplace_back(x1 - xpos_/zoom_, y2 - ypos_/zoom_);

				varray.emplace_back(x1 - xpos_/zoom_, y2 - ypos_/zoom_);
				varray.emplace_back(x1 - xpos_/zoom_, y1 - ypos_/zoom_);

				for(int n = 0; n != 8; ++n) {
					carray.emplace_back(selected_color);
				}
			}
		}

		canvas->drawLines(varray, 1.0f, carray);

		rect addArea(findSubComponentArea(sub, xpos_, ypos_, zoom_));
		bool addAreaMouseover = pointInRect(point(mousex,mousey), addArea);
		canvas->drawSolidRect(addArea, KRE::Color(addAreaMouseover ? 255 : 0, 255, 0, 255));

		rect vertCross(addArea.x() + int(addArea.w()*0.4), addArea.y() + int(addArea.h()*0.2), int(addArea.w()*0.2), int(addArea.h()*0.6));
		canvas->drawSolidRect(vertCross, KRE::Color(255,255,255,255));

		rect horzCross(addArea.x() + int(addArea.w()*0.2), addArea.y() + int(addArea.h()*0.4), int(addArea.w()*0.6), int(addArea.h()*0.2));
		canvas->drawSolidRect(horzCross, KRE::Color(255,255,255,255));
		
		++nsub;
	}

	int nsub_index = 0;
	for(const Level::SubComponentUsage& sub : lvl_->getSubComponentUsages()) {
		rect area = sub.dest_area;

		const bool dragging = (dragging_sub_component_usage_index == nsub_index);
		const bool mouse_over = rect_any_edge_selected(area, selectx, selecty, zoom_) || dragging;

		if(dragging) {
			const int dx = (selectx - anchorx_)/TileSize;
			const int dy = (selecty - anchory_)/TileSize;

			area = rect(area.x() + dx*TileSize, area.y() + dy*TileSize, area.w(), area.h());
		}

		const int x1 = area.x()/zoom_;
		const int x2 = area.x2()/zoom_;
		const int y1 = area.y()/zoom_;
		const int y2 = area.y2()/zoom_;

		glm::u8vec4 selected_color = KRE::Color::colorYellow().as_u8vec4();
		glm::u8vec4 normal_color = KRE::Color::colorBlue().as_u8vec4();

		std::vector<glm::vec2> varray;
		std::vector<glm::u8vec4> carray;

		varray.emplace_back(x1 - xpos_/zoom_, y1 - ypos_/zoom_);
		varray.emplace_back(x2 - xpos_/zoom_, y1 - ypos_/zoom_);

		varray.emplace_back(x2 - xpos_/zoom_, y1 - ypos_/zoom_);
		varray.emplace_back(x2 - xpos_/zoom_, y2 - ypos_/zoom_);

		varray.emplace_back(x2 - xpos_/zoom_, y2 - ypos_/zoom_);
		varray.emplace_back(x1 - xpos_/zoom_, y2 - ypos_/zoom_);

		varray.emplace_back(x1 - xpos_/zoom_, y2 - ypos_/zoom_);
		varray.emplace_back(x1 - xpos_/zoom_, y1 - ypos_/zoom_);

		for(int i = 0; i != 8; ++i) {
			carray.emplace_back(mouse_over ? selected_color : normal_color);
		}

		if(pointInRect(point(selectx, selecty), area)) {
			ASSERT_LOG(sub.ncomponent < lvl_->getSubComponents().size(), "Illegal component: " << sub.ncomponent);
			rect src_area = sub.getSourceArea(*lvl_);

			const int src_x1 = src_area.x()/zoom_;
			const int src_x2 = src_area.x2()/zoom_;
			const int src_y1 = src_area.y()/zoom_;
			const int src_y2 = src_area.y2()/zoom_;

			varray.emplace_back(x1 - xpos_/zoom_, y1 - ypos_/zoom_);
			varray.emplace_back(src_x1 - xpos_/zoom_, src_y1 - ypos_/zoom_);

			varray.emplace_back(x1 - xpos_/zoom_, y2 - ypos_/zoom_);
			varray.emplace_back(src_x1 - xpos_/zoom_, src_y2 - ypos_/zoom_);

			varray.emplace_back(x2 - xpos_/zoom_, y2 - ypos_/zoom_);
			varray.emplace_back(src_x2 - xpos_/zoom_, src_y2 - ypos_/zoom_);

			varray.emplace_back(x2 - xpos_/zoom_, y1 - ypos_/zoom_);
			varray.emplace_back(src_x2 - xpos_/zoom_, src_y1 - ypos_/zoom_);

			for(int i = 0; i != 4; ++i) {
				carray.emplace_back(normal_color);
				carray.emplace_back(KRE::Color::colorRed().as_u8vec4());
			}
		}

		canvas->drawLines(varray, 2.0f, carray);
		++nsub_index;
	}

	draw_selection(0, 0);
	
	if(dragging_) {
		int diffx = (selectx - anchorx_)/TileSize;
		int diffy = (selecty - anchory_)/TileSize;

		if(diffx != 0 || diffy != 0) {
			LOG_INFO("DRAW DIFF: " << diffx << "," << diffy);
			draw_selection(diffx*TileSize, diffy*TileSize);
		}
	}

	if(tool() == TOOL_EDIT_SEGMENTS && selected_segment_ >= 0) {
		rect area = rect(lvl_->boundaries().x() + selected_segment_*lvl_->segment_width(), lvl_->boundaries().y() + selected_segment_*lvl_->segment_height(),
		lvl_->segment_width() ? lvl_->segment_width() : lvl_->boundaries().w(),
		lvl_->segment_height() ? lvl_->segment_height() : lvl_->boundaries().h());
		area = rect((area.x() - xpos_)/zoom_, (area.y() - ypos_)/zoom_,
		            area.w()/zoom_, area.h()/zoom_);
		canvas->drawSolidRect(area, KRE::Color(255, 255, 0, 64));

		variant next = lvl_->get_var(formatter() << "segments_after_" << selected_segment_);
		if(next.is_list()) {
			for(int n = 0; n != next.num_elements(); ++n) {
				const int segment = next[n].as_int();
				rect area = rect(lvl_->boundaries().x() + segment*lvl_->segment_width(), lvl_->boundaries().y() + segment*lvl_->segment_height(),
				lvl_->segment_width() ? lvl_->segment_width() : lvl_->boundaries().w(),
				lvl_->segment_height() ? lvl_->segment_height() : lvl_->boundaries().h());
				area = rect((area.x() - xpos_)/zoom_, (area.y() - ypos_)/zoom_,
				            area.w()/zoom_, area.h()/zoom_);
				canvas->drawSolidRect(area, KRE::Color(255, 0, 0, 64));
			}
		}
	}

	auto xtex = KRE::Font::getInstance()->renderText(formatter() << (xpos_ + mousex*zoom_) << ",", KRE::Color::colorWhite(), 14);
	auto ytex = KRE::Font::getInstance()->renderText(formatter() << (ypos_ + mousey*zoom_), KRE::Color::colorWhite(), 14);
	
	canvas->blitTexture(xtex, 0, rect(10, 80));
	canvas->blitTexture(ytex, 0, rect(10 + xtex->width(), 80));
	
	if(!code_dialog_ && current_dialog_) {
		current_dialog_->draw();
	}

	if(!code_dialog_ && layers_dialog_) {
		layers_dialog_->draw();
	}

	editor_menu_dialog_->draw();

	if(!code_dialog_) {
		editor_mode_dialog_->draw();
	}

	if(code_dialog_) {
		code_dialog_->draw();
	}

	gui::draw_tooltip();
}

void editor::draw_selection(int xoffset, int yoffset) const
{
	if(tile_selection_.empty()) {
		return;
	}

	const int ticks = (profile::get_tick_time()/40)%16;
	uint32_t stipple_bits = 0xFF;
	stipple_bits <<= ticks;
	const uint16_t stipple_mask = (stipple_bits&0xFFFF) | ((stipple_bits&0xFFFF0000) >> 16);

	// XXX may need to review the efficiency of this code.
	variant_builder effect;
	effect.add("type", "stipple");
	effect.add("pattern", stipple_mask);
	KRE::EffectPtr stipple_effect = KRE::Effect::create(effect.build());

	std::vector<glm::vec2> varray;
	std::vector<glm::u8vec4> carray;
	for(const point& p : tile_selection_.tiles) {
		const int size = TileSize/zoom_;
		const int xpos = xoffset/zoom_ + p.x*size - xpos_/zoom_;
		const int ypos = yoffset/zoom_ + p.y*size - ypos_/zoom_;

		if(std::binary_search(tile_selection_.tiles.begin(), tile_selection_.tiles.end(), point(p.x, p.y - 1)) == false) {
			varray.emplace_back(xpos, ypos);
			varray.emplace_back(xpos + size, ypos);
			carray.emplace_back(255, 0, 0, 255);
			carray.emplace_back(255, 255, 0, 255);
		}

		if(std::binary_search(tile_selection_.tiles.begin(), tile_selection_.tiles.end(), point(p.x, p.y + 1)) == false) {
			varray.emplace_back(xpos + size, ypos + size);
			varray.emplace_back(xpos, ypos + size);
			carray.emplace_back(255, 0, 0, 255);
			carray.emplace_back(255, 255, 0, 255);
		}

		if(std::binary_search(tile_selection_.tiles.begin(), tile_selection_.tiles.end(), point(p.x - 1, p.y)) == false) {
			varray.emplace_back(xpos, ypos + size);
			varray.emplace_back(xpos, ypos);
			carray.emplace_back(255, 0, 0, 255);
			carray.emplace_back(255, 255, 0, 255);
		}

		if(std::binary_search(tile_selection_.tiles.begin(), tile_selection_.tiles.end(), point(p.x + 1, p.y)) == false) {
			varray.emplace_back(xpos + size, ypos);
			varray.emplace_back(xpos + size, ypos + size);
			carray.emplace_back(255, 0, 0, 255);
			carray.emplace_back(255, 255, 0, 255);
		}
	}
	KRE::EffectsManager em(stipple_effect);
	KRE::Canvas::getInstance()->drawLines(varray, 0, carray);
}

void editor::run_script(const std::string& id)
{
	editor_script::execute(id, *this);
}

void editor::executeCommand(std::function<void()> command, std::function<void()> undo, EXECUTABLE_COMMAND_TYPE type)
{
	level_changed_++;

	command();

	executable_command cmd;
	cmd.redo_command = command;
	cmd.undo_command = undo;
	cmd.type = type;
	undo_.push_back(cmd);
	redo_.clear();

	autosave_level();
}

void editor::on_modify_level()
{
	for(const Level::SubComponentUsage& usage : lvl_->getSubComponentUsagesOrdered()) {
		const rect& dst = usage.dest_area;
		const rect& src = usage.getSourceArea(*lvl_);

		std::vector<std::function<void()>> redo, undo;
		copy_rectangle(src, dst, redo, undo);
		for(auto& fn : redo) {
			fn();
		}
	}
}

void editor::begin_command_group()
{
	undo_commands_groups_.push(static_cast<int>(undo_.size()));

	lvl_->editor_freeze_tile_updates(true);
}

void editor::end_command_group()
{
	lvl_->editor_freeze_tile_updates(false);

	ASSERT_NE(undo_commands_groups_.empty(), true);

	const int index = undo_commands_groups_.top();
	undo_commands_groups_.pop();

	if(static_cast<unsigned>(index) >= undo_.size()) {
		return;
	}

	//group all of the commands since beginning into one command
	std::vector<std::function<void()> > undo, redo;
	for(int n = index; n != undo_.size(); ++n) {
		undo.push_back(undo_[n].undo_command);
		redo.push_back(undo_[n].redo_command);
	}

	//reverse the undos, since we want them executed in reverse order.
	std::reverse(undo.begin(), undo.end());

	//make it so undoing and redoing will freeze tile updates during the
	//group command, and then do a full refresh of tiles once we're done.
	undo.insert(undo.begin(), std::bind(&Level::editor_freeze_tile_updates, lvl_.get(), true));
	undo.push_back(std::bind(&Level::editor_freeze_tile_updates, lvl_.get(), false));
	redo.insert(redo.begin(), std::bind(&Level::editor_freeze_tile_updates, lvl_.get(), true));
	redo.push_back(std::bind(&Level::editor_freeze_tile_updates, lvl_.get(), false));

	executable_command cmd;
	cmd.redo_command = std::bind(execute_functions, redo);
	cmd.undo_command = std::bind(execute_functions, undo);

	//replace all the individual commands with the one group command.
	undo_.erase(undo_.begin() + index, undo_.end());
	undo_.push_back(cmd);
}

void editor::undo_command()
{
	if(undo_.empty()) {
		return;
	}

	--level_changed_;

	undo_.back().undo_command();
	redo_.push_back(undo_.back());
	undo_.pop_back();

	if(layers_dialog_) {
		layers_dialog_->init();
	}

	on_modify_level();
}

void editor::redo_command()
{
	if(redo_.empty()) {
		return;
	}

	++level_changed_;

	redo_.back().redo_command();
	undo_.push_back(redo_.back());
	redo_.pop_back();

	if(layers_dialog_) {
		layers_dialog_->init();
	}

	on_modify_level();
}

void show_object_editor_dialog(const std::string& obj_type);

void launch_object_editor(const std::vector<std::string>& args);

void editor::edit_level_properties()
{
	editor_dialogs::EditorLevelPropertiesDialog prop_dialog(*this);
	prop_dialog.showModal();
}

void editor::create_new_module()
{
	editor_dialogs::EditorModulePropertiesDialog prop_dialog(*this);
	prop_dialog.showModal();
	if(prop_dialog.cancelled() == false) {
		prop_dialog.onExit();
		close();
		g_last_edited_level() = prop_dialog.onExit();
	}
}

void editor::edit_module_properties()
{
	editor_dialogs::EditorModulePropertiesDialog prop_dialog(*this, module::get_module_name());
	prop_dialog.showModal();
	if(prop_dialog.cancelled() == false) {
		prop_dialog.onExit();
		KRE::WindowManager::getMainWindow()->setWindowTitle(module::get_module_pretty_name());
	}
}

void editor::create_new_object()
{
	auto wnd = KRE::WindowManager::getMainWindow();
	editor_dialogs::CustomObjectDialog object_dialog(*this, 
		static_cast<int>(wnd->width() * 0.05f), 
		static_cast<int>(wnd->height() * 0.05f), 
		static_cast<int>(wnd->width() * 0.9f), 
		static_cast<int>(wnd->height() * 0.9f));
	object_dialog.setBackgroundFrame("empty_window");
	object_dialog.setDrawBackgroundFn(draw_last_scene);
	object_dialog.showModal();
	if(object_dialog.cancelled() == false) {
		CustomObjectType::ReloadFilePaths();
		lvl_->editor_clear_selection();
		change_tool(TOOL_ADD_OBJECT);
		const std::string type = object_dialog.getObject()["id"].as_string();
		ConstCustomObjectTypePtr obj = CustomObjectType::get(type);

		if(obj->getEditorInfo()) {
			all_characters().push_back(editor::enemy_type(type, obj->getEditorInfo()->getCategory(), variant()));
			current_dialog_ = character_dialog_.get();

			for(int n = 0; n != all_characters().size(); ++n) {
				const enemy_type& c = all_characters()[n];
				if(c.node["type"].as_string() == type) {
					character_dialog_->select_category(c.category);
					character_dialog_->set_character(n);
				}
			}
		}
	}
}

void editor::edit_shaders()
{
	const std::string path = module::map_file("data/shaders.cfg");
	if(sys::file_exists(path) == false) {
		sys::write_file(path, "{\n\t\"shaders\": {\n\t},\n\t\"programs\": [\n\t],\n}");
	}
	if(external_code_editor_ && external_code_editor_->replaceInGameEditor()) {

		LOG_INFO("Loading file in external editor: " << path);
		external_code_editor_->loadFile(path);
	}

	if(code_dialog_) {
		code_dialog_.reset();
	} else {
		code_dialog_.reset(new CodeEditorDialog(get_code_editor_rect()));
		code_dialog_->load_file(path);
	}
}

void editor::edit_level_code()
{
	const std::string& path = get_level_path(lvl_->id());
	if(external_code_editor_ && external_code_editor_->replaceInGameEditor()) {
		external_code_editor_->loadFile(path);
	}
	
	code_dialog_.reset(new CodeEditorDialog(get_code_editor_rect()));
	code_dialog_->load_file(path);
}

void editor::add_multi_object_to_level(LevelPtr lvl, EntityPtr e)
{
	CurrentLevelScope scope(lvl.get());
	lvl->add_multi_player(e);
	e->handleEvent("editor_added");
}

void editor::add_object_to_level(LevelPtr lvl, EntityPtr e)
{
	CurrentLevelScope scope(lvl.get());
	lvl->add_character(e);
	e->handleEvent("editor_added");
}

void editor::remove_object_from_level(LevelPtr lvl, EntityPtr e)
{
	CurrentLevelScope scope(lvl.get());
	e->handleEvent("editor_removed");
	lvl->remove_character(e);
	lvl->set_active_chars();
}

void editor::mutate_object_value(LevelPtr lvl, EntityPtr e, const std::string& value, variant new_value)
{
	CurrentLevelScope scope(lvl.get());
	e->handleEvent("editor_changing_variable");
	e->mutateValue(value, new_value);
	e->handleEvent("editor_changed_variable");
}

void editor::generate_mutate_commands(EntityPtr c, const std::string& attr, variant new_value, std::vector<std::function<void()> >& undo, std::vector<std::function<void()> >& redo)
{
	if(!c || c->wasSpawnedBy().empty() == false) {
		return;
	}

	for(LevelPtr lvl : levels_) {
		EntityPtr obj = lvl->get_entity_by_label(c->label());
		if(!obj) {
			continue;
		}
		variant current_value = obj->queryValue(attr);

		redo.push_back(std::bind(&editor::mutate_object_value, this, lvl, obj, attr, new_value));
		undo.push_back(std::bind(&editor::mutate_object_value, this, lvl, obj, attr, current_value));
	}
}

void editor::generate_remove_commands(EntityPtr c, std::vector<std::function<void()> >& undo, std::vector<std::function<void()> >& redo)
{
	if(!c || c->wasSpawnedBy().empty() == false) {
		return;
	}
	
	for(LevelPtr lvl : levels_) {
		EntityPtr obj = lvl->get_entity_by_label(c->label());
		if(!obj) {
			continue;
		}

		redo.push_back(std::bind(&editor::remove_object_from_level, this, lvl, obj));
		undo.push_back(std::bind(&editor::add_object_to_level, this, lvl, obj));
		if(obj->label().empty() == false) {
			for(EntityPtr child : lvl->get_chars()) {
				if(child->wasSpawnedBy() == obj->label()) {
					LOG_INFO("REMOVING CHILD OBJECT: " << child->getDebugDescription() << " " << child->label());
					redo.push_back(std::bind(&editor::remove_object_from_level, this, lvl, child));
					undo.push_back(std::bind(&editor::add_object_to_level, this, lvl, child));
				}
			}
		}
	}
}

bool editor::hasKeyboardFocus() const
{
	if(code_dialog_ && code_dialog_->hasKeyboardFocus()) {
		return true;
	}

	if(current_dialog_ && current_dialog_->hasFocus()) {
		return true;
	}

	return false;
}

void editor::toggle_code()
{
	if(external_code_editor_ && external_code_editor_->replaceInGameEditor()) {

		std::string type;
		if(lvl_->editor_selection().empty() == false) {
			type = lvl_->editor_selection().back()->queryValue("type").as_string();
		}

		if(type.empty()) {
			LOG_INFO("no object selected to open code for");
		} else {
			//if this is a nested type, convert it to their parent type.
			std::string::iterator dot_itor = std::find(type.begin(), type.end(), '.');
			type.erase(dot_itor, type.end());

			const std::string* path = CustomObjectType::getObjectPath(type + ".cfg");
			ASSERT_LOG(path, "Could not find path for object " << type);
			LOG_INFO("Loading file in external editor: " << *path);
			external_code_editor_->loadFile(*path);
		}

		return;
	}

	if(code_dialog_) {
		code_dialog_.reset();
	} else {
		code_dialog_.reset(new CodeEditorDialog(get_code_editor_rect()));
		set_code_file();
	}
}

void editor::set_code_file()
{
	if(tool_ == TOOL_ADD_RECT || tool_ == TOOL_SELECT_RECT || tool_ == TOOL_MAGIC_WAND || tool_ == TOOL_PENCIL) {
		LOG_INFO("SET TILESET..");
		if(cur_tileset_ >= 0 && static_cast<unsigned>(cur_tileset_) < tilesets.size()) {
			const std::vector<std::string>& files = TileMap::getFiles(tilesets[cur_tileset_].type);
			LOG_INFO("TILESET: " << files.size() << " FOR " << tilesets[cur_tileset_].type);
			for(const std::string& file : files) {
				std::map<std::string, std::string> fnames;
				module::get_unique_filenames_under_dir("data/tiles", &fnames);
				const std::map<std::string, std::string>::const_iterator itor =
				  module::find(fnames, file);
				if(itor != fnames.end() && code_dialog_) {
					LOG_INFO("TILESET FNAME: " << itor->second);
					code_dialog_->load_file(itor->second);
				}
			}
		}

		return;
	}
	
	std::string type;
	if(lvl_->editor_selection().empty() == false) {
		type = lvl_->editor_selection().back()->queryValue("type").as_string();
	} else if(lvl_->player()) {
		type = lvl_->player()->getEntity().queryValue("type").as_string();
	}

	if(type.empty()) {
		return;
	}

	if(std::count(type.begin(), type.end(), '.')) {
		//it's a subtype, so find the parent.
		type = std::string(type.begin(), std::find(type.begin(), type.end(), '.'));
	}

	const std::string* path = CustomObjectType::getObjectPath(type + ".cfg");

	EntityPtr obj_instance;
	if(code_dialog_ && lvl_->editor_selection().empty() == false && tool() == TOOL_SELECT_OBJECT && levels_.size() == 2 && lvl_ == levels_.back()) {
		// See if we can find an instance of the object in the canonical
		// version of the level. If we can we allow object instance editing,
		// otherwise we'll just allow editing of the type.
		EntityPtr selected = lvl_->editor_selection().back();

		obj_instance = levels_.front()->get_entity_by_label(selected->label());
	}

	if(code_dialog_) {
		if(obj_instance) {
			variant v = obj_instance->write();
			const std::string pseudo_fname = "@instance:" + obj_instance->label();
			json::set_file_contents(pseudo_fname, v.write_json());
	
			std::function<void()> fn(std::bind(&editor::object_instance_modified_in_editor, this, obj_instance->label()));
			code_dialog_->load_file(pseudo_fname, true, &fn);
		}
		
		if(path) {
			code_dialog_->load_file(*path);
		}
	}
}

void editor::start_adding_points(const std::string& field_name)
{
	adding_points_ = field_name;

	if(property_dialog_) {
		property_dialog_->init();
	}
}

void editor::object_instance_modified_in_editor(const std::string& label)
{
	std::vector<std::function<void()> > undo, redo;
	const std::string pseudo_fname = "@instance:" + label;

	EntityPtr existing_obj = lvl_->get_entity_by_label(label);
	if(!existing_obj) {
		return;
	}

	generate_remove_commands(existing_obj, undo, redo);
	for(LevelPtr lvl : levels_) {
		EntityPtr new_obj(Entity::build(json::parse_from_file(pseudo_fname)));
		redo.push_back(std::bind(&editor::add_object_to_level, this, lvl, new_obj));
		undo.push_back(std::bind(&editor::remove_object_from_level, this, lvl, new_obj));
	}

	executeCommand(
	  std::bind(execute_functions, redo),
	  std::bind(execute_functions, undo));
	on_modify_level();
}

void editor::add_new_sub_component()
{
	std::vector<std::function<void()> > redo, undo;

	int w = TileSize*16;
	int h = TileSize*16;

	const bool has_usage = (selection().empty() == false);

	if(has_usage) {
		int min_x = selection().tiles.front().x*TileSize;
		int max_x = selection().tiles.front().x*TileSize;
		int min_y = selection().tiles.front().y*TileSize;
		int max_y = selection().tiles.front().y*TileSize;
		for(const point& p : selection().tiles) {
			min_x = std::min<int>(p.x*TileSize, min_x);
			min_y = std::min<int>(p.y*TileSize, min_y);
			max_x = std::max<int>(p.x*TileSize, max_x);
			max_y = std::max<int>(p.y*TileSize, max_y);
		}

		max_x += TileSize;
		max_y += TileSize;

		rect area(min_x, min_y, max_x - min_x, max_y - min_y);
		w = area.w();
		h = area.h();

		std::vector<Level::SubComponentUsage> usage = lvl_->getSubComponentUsages();

		redo.push_back(std::bind(&editor::add_sub_component_usage, this, lvl_->getSubComponents().size(), area));
		undo.push_back(std::bind(&editor::set_sub_component_usage, this, usage));
	}

	redo.insert(redo.begin(), std::bind(&editor::add_sub_component, this, w, h));
	undo.insert(undo.begin(), std::bind(&editor::remove_sub_component, this));

	begin_command_group();

	executeCommand(
	  std::bind(execute_functions, redo),
	  std::bind(execute_functions, undo));

	undo.clear();
	redo.clear();


	if(has_usage) {
		const auto& usage = lvl_->getSubComponentUsages().back();
		const auto& sub = lvl_->getSubComponents()[usage.ncomponent];
		copy_rectangle(usage.dest_area, sub.source_area, redo, undo);
	}

	executeCommand(
	  std::bind(execute_functions, redo),
	  std::bind(execute_functions, undo));

	end_command_group();

	on_modify_level();
}

void editor::add_sub_component(int w, int h)
{
	for(LevelPtr lvl : levels_) {
		lvl->addSubComponent(w, h);
	}
}

void editor::remove_sub_component()
{
	for(LevelPtr lvl : levels_) {
		lvl->removeSubComponent();
	}
}

void editor::add_sub_component_variations(int nsub, int delta)
{
	for(LevelPtr lvl : levels_) {
		lvl->addSubComponentVariations(nsub, delta);
	}
}

void editor::set_sub_component_area(int nsub, rect area)
{
	for(LevelPtr lvl : levels_) {
		lvl->setSubComponentArea(nsub, area);
	}
}

void editor::add_sub_component_usage(int nsub, rect area)
{
	for(LevelPtr lvl : levels_) {
		lvl->addSubComponentUsage(nsub, area);
	}
}

void editor::set_sub_component_usage(std::vector<Level::SubComponentUsage> u)
{
	for(LevelPtr lvl : levels_) {
		lvl->setSubComponentUsages(u);
	}
}

void editor::clear_rectangle(const rect& area, std::vector<std::function<void()>>& redo, std::vector<std::function<void()>>& undo)
{
	const rect tile_area(area.x(), area.y(), area.w()-TileSize, area.h()-TileSize);
	for(LevelPtr lvl : levels_) {
		std::map<int, std::vector<std::string> > old_tiles;
		lvl->getAllTilesRect(tile_area.x(), tile_area.y(), tile_area.x2(), tile_area.y2(), old_tiles);

		redo.push_back([=]() { lvl->clear_tile_rect(area.x(), area.y(), area.x() + area.w(), area.y() + area.h()); });
		for (auto i = old_tiles.begin(); i != old_tiles.end(); ++i) {
			undo.push_back([=]() { lvl->addTileRectVector(i->first, area.x(), area.y(), area.x()+area.w(), area.y()+area.h(), i->second); });
		}

		std::vector<EntityPtr> chars = lvl->get_chars();
		for(auto c : chars) {
			if(c->x() >= area.x() && c->x() <= area.x2() && c->y() >= area.y() && c->y() <= area.y2()) {
				redo.push_back(std::bind(&Level::remove_character, lvl.get(), c));
				undo.push_back(std::bind(&Level::add_character, lvl.get(), c));
			}
		}

		std::vector<Level::SubComponentUsage> usages, old_usages = lvl_->getSubComponentUsages();
		
		for(const Level::SubComponentUsage& usage : old_usages) {
			if(!rects_intersect(usage.dest_area, area)) {
				usages.push_back(usage);
			}
		}

		if(usages.size() != old_usages.size()) {
			redo.push_back(std::bind(&Level::setSubComponentUsages, lvl.get(), usages));
			undo.push_back(std::bind(&Level::setSubComponentUsages, lvl.get(), old_usages));
		}
	}
}


void editor::copy_rectangle(const rect& src, const rect& dst, std::vector<std::function<void()>>& redo, std::vector<std::function<void()>>& undo, bool copy_usages)
{
	const rect tile_src(src.x(), src.y(), src.w()-TileSize, src.h()-TileSize);
	const rect tile_dst(dst.x(), dst.y(), dst.w()-TileSize, dst.h()-TileSize);
	for(LevelPtr lvl : levels_) {
		std::map<int, std::vector<std::string> > src_tiles, dst_tiles;
		lvl->getAllTilesRect(tile_src.x(), tile_src.y(), tile_src.x2(), tile_src.y2(), src_tiles);
		lvl->getAllTilesRect(tile_dst.x(), tile_dst.y(), tile_dst.x2(), tile_dst.y2(), dst_tiles);

		redo.push_back([=]() { lvl->clear_tile_rect(tile_dst.x(), tile_dst.y(), tile_dst.x2(), tile_dst.y2()); });
		undo.push_back([=]() { lvl->clear_tile_rect(tile_dst.x(), tile_dst.y(), tile_dst.x2(), tile_dst.y2()); });

		for(auto& p : src_tiles) {
			redo.push_back([=]() { lvl->addTileRectVector(p.first, tile_dst.x(), tile_dst.y(), tile_dst.x2(), tile_dst.y2(), p.second); });
		}

		for(auto& p : dst_tiles) {
			undo.push_back([=]() { lvl->addTileRectVector(p.first, tile_dst.x(), tile_dst.y(), tile_dst.x2(), tile_dst.y2(), p.second); });
		}

		std::vector<EntityPtr> chars = lvl->get_chars();
		for(auto c : chars) {
			if(c->x() >= dst.x() && c->x() <= dst.x2() && c->y() >= dst.y() && c->y() <= dst.y2()) {
				redo.push_back(std::bind(&Level::remove_character, lvl.get(), c));
				undo.push_back(std::bind(&Level::add_character, lvl.get(), c));
			}
		}

		chars = levels_.front()->get_chars();

		for(auto c : chars) {
			if(c->x() >= src.x() && c->x() <= src.x2() && c->y() >= src.y() && c->y() <= src.y2()) {
				auto clone = c->clone();
				clone->shiftPosition(dst.x() - src.x(), dst.y() - src.y());
				redo.push_back(std::bind(&Level::add_character, lvl.get(), clone));
				undo.push_back(std::bind(&Level::remove_character, lvl.get(), clone));
			}
		}

		undo.push_back(std::bind(&Level::start_rebuild_tiles_in_background, lvl.get(), std::vector<int>()));
		redo.push_back(std::bind(&Level::start_rebuild_tiles_in_background, lvl.get(), std::vector<int>()));

		if(copy_usages) {
			std::vector<Level::SubComponentUsage> usages;
			
			for(const Level::SubComponentUsage& usage : lvl_->getSubComponentUsages()) {
				if(rects_intersect(usage.dest_area, src)) {
					usages.push_back(usage);
				}
			}

			if(usages.empty() == false) {
				const int dx = dst.x() - src.x();
				const int dy = dst.y() - src.y();

				std::vector<Level::SubComponentUsage> old_usages = lvl_->getSubComponentUsages();
				std::vector<Level::SubComponentUsage> new_usages = old_usages;
				for(auto& usage : usages) {
					usage.dest_area = rect(usage.dest_area.x() + dx, usage.dest_area.y() + dy, usage.dest_area.w(), usage.dest_area.h());
					new_usages.push_back(usage);
				}

				redo.push_back(std::bind(&Level::setSubComponentUsages, lvl.get(), new_usages));
				undo.push_back(std::bind(&Level::setSubComponentUsages, lvl.get(), old_usages));
			}
		}
	}
}

BEGIN_DEFINE_CALLABLE_NOBASE(editor)
DEFINE_FIELD(test, "int")
	return variant(5);
END_DEFINE_CALLABLE(editor)

#endif // !NO_EDITOR
