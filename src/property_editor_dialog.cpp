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
#ifndef NO_EDITOR
#include <assert.h>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <sstream>

#include "button.hpp"
#include "checkbox.hpp"
#include "code_editor_widget.hpp"
#include "controls.hpp"
#include "custom_object_functions.hpp"
#include "difficulty.hpp"
#include "editor_dialogs.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "input.hpp"
#include "label.hpp"
#include "load_level.hpp"
#include "object_events.hpp"
#include "property_editor_dialog.hpp"
#include "raster.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"

namespace editor_dialogs
{

property_editor_dialog::property_editor_dialog(editor& e)
  : gui::dialog(graphics::screen_width() - EDITOR_SIDEBAR_WIDTH, 160, EDITOR_SIDEBAR_WIDTH, 440), editor_(e)
{
	set_clear_bg_amount(255);
	init();
}

void property_editor_dialog::init()
{
	clear();
	if(entity_.empty()) {
		return;
	}

	using namespace gui;

	set_padding(5);

	const frame& frame = get_static_entity()->current_frame();
	image_widget* preview = new image_widget(frame.img());
	preview->set_dim(frame.width(), frame.height());
	preview->set_area(frame.area());

	grid_ptr preview_grid(new grid(2));
	preview_grid->add_col(widget_ptr(preview));

	//draw the object's difficulty settings.
	grid_ptr difficulty_grid(new grid(3));
	const custom_object* obj = dynamic_cast<const custom_object*>(get_entity().get());
	ASSERT_LOG(obj, "ENTITY IS NOT AN OBJECT");
	std::string min_difficulty = difficulty::to_string(obj->min_difficulty());
	std::string max_difficulty = difficulty::to_string(obj->max_difficulty());
	
	if(min_difficulty.empty()) {
		min_difficulty = formatter() << obj->min_difficulty();
	}

	if(max_difficulty.empty()) {
		max_difficulty = formatter() << obj->max_difficulty();
	}
	min_difficulty = " " + min_difficulty;
	max_difficulty = " " + max_difficulty;

	button_ptr difficulty_button;
	difficulty_button.reset(new button(widget_ptr(new label("-", graphics::color_white())), boost::bind(&property_editor_dialog::change_min_difficulty, this, -1)));
	difficulty_button->set_tooltip("Decrease minimum difficulty");
	difficulty_button->set_dim(difficulty_button->width()-10, difficulty_button->height()-4);
	difficulty_grid->add_col(difficulty_button);
	difficulty_button.reset(new button(widget_ptr(new label("+", graphics::color_white())), boost::bind(&property_editor_dialog::change_min_difficulty, this, 1)));
	difficulty_button->set_tooltip("Increase minimum difficulty");
	difficulty_button->set_dim(difficulty_button->width()-10, difficulty_button->height()-4);
	difficulty_grid->add_col(difficulty_button);
	difficulty_grid->add_col(widget_ptr(new label(min_difficulty, graphics::color_white())));

	difficulty_button.reset(new button(widget_ptr(new label("-", graphics::color_white())), boost::bind(&property_editor_dialog::change_max_difficulty, this, -1)));
	difficulty_button->set_tooltip("Decrease maximum difficulty");
	difficulty_button->set_dim(difficulty_button->width()-10, difficulty_button->height()-4);
	difficulty_grid->add_col(difficulty_button);
	difficulty_button.reset(new button(widget_ptr(new label("+", graphics::color_white())), boost::bind(&property_editor_dialog::change_max_difficulty, this, 1)));
	difficulty_button->set_tooltip("Increase maximum difficulty");
	difficulty_button->set_dim(difficulty_button->width()-10, difficulty_button->height()-4);
	difficulty_grid->add_col(difficulty_button);
	difficulty_grid->add_col(widget_ptr(new label(max_difficulty, graphics::color_white())));


	preview_grid->add_col(difficulty_grid);
	
	add_widget(preview_grid, 10, 10);

	add_widget(widget_ptr(new label(obj->debug_description(), graphics::color_white())));

	if(get_entity()->label().empty() == false) {
		grid_ptr labels_grid(new grid(2));
		labels_grid->set_hpad(5);
		text_editor_widget* e = new text_editor_widget(120);
		e->set_text(get_entity()->label());
		e->set_on_change_handler(boost::bind(&property_editor_dialog::set_label, this, e));
		labels_grid->add_col(widget_ptr(new label("Label: ")))
		            .add_col(widget_ptr(e));
		add_widget(labels_grid);
	}

	std::map<std::string, int> types_selected;
	foreach(entity_ptr e, entity_) {
		types_selected[e->query_value("type").as_string()]++;
	}

	if(types_selected.size() > 1) {
		grid_ptr types_grid(new grid(3));
		types_grid->set_hpad(5);
		for(std::map<std::string, int>::const_iterator i = types_selected.begin(); i != types_selected.end(); ++i) {
			std::string type = i->first;
			if(type.size() > 24) {
				type.resize(24);
				std::fill(type.end()-3, type.end(), '.');
			}
			types_grid->add_col(widget_ptr(new label(formatter() << i->second, 10)))
			           .add_col(widget_ptr(new label(type, 10)))
					   .add_col(widget_ptr(new button("Deselect", boost::bind(&property_editor_dialog::deselect_object_type, this, i->first))));
		}

		add_widget(types_grid);
	}

	if(entity_.size() > 1) {
		add_widget(widget_ptr(new button("Group Objects", boost::bind(&editor::group_selection, &editor_))));
	}

	game_logic::formula_callable* vars = get_static_entity()->vars();
	if(get_entity()->editor_info() && types_selected.size() == 1) {
		if(get_entity()->group() >= 0) {
			grid_ptr group_grid(new grid(1));

			group_grid->add_col(widget_ptr(new button("Remove from Group", boost::bind(&property_editor_dialog::remove_object_from_group, this, get_entity()))));
			group_grid->add_col(widget_ptr(new button("Breakup Group", boost::bind(&property_editor_dialog::remove_group, this, get_entity()->group()))));

			add_widget(group_grid);
		}

		//output an editing area for each editable event.
		foreach(const std::string& handler, get_entity()->editor_info()->editable_events()) {
			label_ptr lb = label::create(handler + " event handler", graphics::color_white());
			add_widget(lb);

			text_editor_widget* e = new text_editor_widget(220, 90);
			game_logic::const_formula_ptr f = get_entity()->get_event_handler(get_object_event_id(handler));
			if(f) {
				e->set_text(f->str());
			}
			add_widget(widget_ptr(e));

			e->set_on_change_handler(boost::bind(&property_editor_dialog::change_event_handler, this, handler, lb.get(), e));

		}

		foreach(const editor_variable_info& info, get_entity()->editor_info()->vars_and_properties()) {

			if(info.type() == editor_variable_info::XPOSITION ||
			   info.type() == editor_variable_info::YPOSITION) {
				//don't show x/y position as they are directly editable on
				//the map.
				continue;
			}

			variant val = info.is_property() ? get_static_entity()->query_value(info.variable_name()) : vars->query_value(info.variable_name());
			std::string current_val_str;
			if(info.type() == editor_variable_info::TYPE_POINTS) {
				if(!val.is_list()) {
					current_val_str = "null";
				} else {
					current_val_str = formatter() << val.num_elements() << " points";
				}
			} else {
				current_val_str = val.to_debug_string();
			}

			if(info.type() != editor_variable_info::TYPE_TEXT &&
			   info.type() != editor_variable_info::TYPE_INTEGER &&
			   info.type() != editor_variable_info::TYPE_ENUM &&
			   info.type() != editor_variable_info::TYPE_BOOLEAN) {
				std::ostringstream s;
				s << info.variable_name() << ": " << current_val_str;
				label_ptr lb = label::create(s.str(), graphics::color_white());
				add_widget(widget_ptr(lb));
			}

			if(info.type() == editor_variable_info::TYPE_TEXT) {

				grid_ptr text_grid(new grid(2));

				label_ptr lb = label::create(info.variable_name() + ":", graphics::color_white());
				text_grid->add_col(lb);

				std::string current_value;
				variant current_value_var = get_static_entity()->query_value(info.variable_name());
				if(current_value_var.is_string()) {
					current_value = current_value_var.as_string();
				}

				text_editor_widget* e = new text_editor_widget(200 - lb->width());
				e->set_text(current_value);
				e->set_on_change_handler(boost::bind(&property_editor_dialog::change_text_property, this, info.variable_name(), e));

				text_grid->add_col(widget_ptr(e));

				add_widget(widget_ptr(text_grid));

			} else if(info.type() == editor_variable_info::TYPE_ENUM) {
				grid_ptr enum_grid(new grid(2));
				label_ptr lb = label::create(info.variable_name() + ":", graphics::color_white());
				enum_grid->add_col(lb);

				std::string current_value;
				variant current_value_var = get_static_entity()->query_value(info.variable_name());
				if(current_value_var.is_string()) {
					current_value = current_value_var.as_string();
				}

				if(std::count(info.enum_values().begin(), info.enum_values().end(), current_value) == 0) {
					current_value = info.enum_values().front();
				}

				enum_grid->add_col(widget_ptr(new button(
				     widget_ptr(new label(current_value, graphics::color_white())),
					 boost::bind(&property_editor_dialog::change_enum_property, this, info.variable_name()))));
				add_widget(widget_ptr(enum_grid));
			} else if(info.type() == editor_variable_info::TYPE_LEVEL) {
				std::string current_value;
				variant current_value_var = get_static_entity()->query_value(info.variable_name());
				if(current_value_var.is_string()) {
					current_value = current_value_var.as_string();
				}

				add_widget(widget_ptr(new button(
				         widget_ptr(new label(current_value.empty() ? "(set level)" : current_value, graphics::color_white())),
				         boost::bind(&property_editor_dialog::change_level_property, this, info.variable_name()))));
			} else if(info.type() == editor_variable_info::TYPE_LABEL) {
				std::string current_value;
				variant current_value_var = get_static_entity()->query_value(info.variable_name());
				if(current_value_var.is_string()) {
					current_value = current_value_var.as_string();
				}

				add_widget(widget_ptr(new button(
				         widget_ptr(new label(current_value.empty() ? "(set label)" : current_value, graphics::color_white())),
				         boost::bind(&property_editor_dialog::change_label_property, this, info.variable_name()))));
				
			} else if(info.type() == editor_variable_info::TYPE_BOOLEAN) {
				variant current_value = get_static_entity()->query_value(info.variable_name());

				add_widget(widget_ptr(new checkbox(widget_ptr(new label(info.variable_name(), graphics::color_white())),
				               current_value.as_bool(),
							   boost::bind(&property_editor_dialog::toggle_property, this, info.variable_name()))));
			} else if(info.type() == editor_variable_info::TYPE_POINTS) {
				variant current_value = get_static_entity()->query_value(info.variable_name());
				const int npoints = current_value.is_list() ? current_value.num_elements() : 0;

				const bool already_adding = editor_.adding_points() == info.variable_name();
				add_widget(widget_ptr(new button(already_adding ? "Done Adding" : "Add Points",
						 boost::bind(&property_editor_dialog::change_points_property, this, info.variable_name()))));

			} else {
				grid_ptr text_grid(new grid(2));

				label_ptr lb = label::create(info.variable_name() + ":", graphics::color_white());
				text_grid->add_col(lb);

				decimal value;
				std::string current_value = "0";
				variant current_value_var = get_static_entity()->query_value(info.variable_name());
				if(current_value_var.is_int()) {
					current_value = formatter() << current_value_var.as_int();
					value = current_value_var.as_decimal();
				} else if(current_value_var.is_decimal()) {
					current_value = formatter() << current_value_var.as_decimal();
					value = current_value_var.as_decimal();
				}

				boost::shared_ptr<numeric_widgets> widgets(new numeric_widgets);

				text_editor_widget* e = new text_editor_widget(80);
				e->set_text(current_value);
				e->set_on_change_handler(boost::bind(&property_editor_dialog::change_numeric_property, this, info.variable_name(), widgets));

				text_grid->add_col(widget_ptr(e));

				add_widget(widget_ptr(text_grid));

				float pos = ((value - info.numeric_min())/(info.numeric_max() - info.numeric_min())).as_float();
				if(pos < 0.0) {
					pos = 0.0;
				}

				if(pos > 1.0) {
					pos = 1.0;
				}

				lb.reset(new gui::label(formatter() << info.numeric_min().as_int(), 10));
				add_widget(lb, text_grid->x(), text_grid->y() + text_grid->height() + 6);
				lb.reset(new gui::label(formatter() << info.numeric_max().as_int(), 10));
				add_widget(lb, text_grid->x() + 170, text_grid->y() + text_grid->height() + 6);

				gui::slider* slider = new gui::slider(160, boost::bind(&property_editor_dialog::change_numeric_property_slider, this, info.variable_name(), widgets, _1), pos);

				add_widget(widget_ptr(slider), text_grid->x(), text_grid->y() + text_grid->height() + 8);

				*widgets = numeric_widgets(e, slider);
			}
		}
	}
}

void property_editor_dialog::set_entity(entity_ptr e)
{
	entity_.clear();
	if(e) {
		entity_.push_back(e);
	}
	init();
}

void property_editor_dialog::set_entity_group(const std::vector<entity_ptr>& e)
{
	entity_ = e;
	init();
}

void property_editor_dialog::remove_object_from_group(entity_ptr entity_obj)
{
	foreach(level_ptr lvl, editor_.get_level_list()) {
		entity_ptr e = lvl->get_entity_by_label(entity_obj->label());
		if(!e) {
			continue;
		}
		lvl->set_character_group(e, -1);
	}
	init();
}

void property_editor_dialog::remove_group(int ngroup)
{
	foreach(level_ptr lvl, editor_.get_level_list()) {
		foreach(entity_ptr e, lvl->get_chars()) {
			if(e->group() == ngroup) {
				lvl->set_character_group(e, -1);
			}
		}
	}
	init();
}

void property_editor_dialog::change_min_difficulty(int amount)
{
	foreach(level_ptr lvl, editor_.get_level_list()) {
		foreach(entity_ptr entity_obj, entity_) {
			entity_ptr e = lvl->get_entity_by_label(entity_obj->label());
			if(!e) {
				continue;
			}
			custom_object* obj = dynamic_cast<custom_object*>(e.get());
			ASSERT_LOG(obj, "ENTITY IS NOT AN OBJECT");

			const int new_difficulty = std::max<int>(-1, obj->min_difficulty() + amount);
			obj->set_difficulty(new_difficulty, obj->max_difficulty());
		}
	}

	init();
}

void property_editor_dialog::change_max_difficulty(int amount)
{
	foreach(level_ptr lvl, editor_.get_level_list()) {
		foreach(entity_ptr entity_obj, entity_) {
			entity_ptr e = lvl->get_entity_by_label(entity_obj->label());
			if(!e) {
				continue;
			}
			custom_object* obj = dynamic_cast<custom_object*>(e.get());
			ASSERT_LOG(obj, "ENTITY IS NOT AN OBJECT");

			const int new_difficulty = std::max<int>(-1, obj->max_difficulty() + amount);
			obj->set_difficulty(obj->min_difficulty(), new_difficulty);
		}
	}

	init();
}

void property_editor_dialog::toggle_property(const std::string& id)
{
	mutate_value(id, variant::from_bool(!get_static_entity()->query_value(id).as_bool()));
	init();
}

void property_editor_dialog::change_property(const std::string& id, int change)
{
	mutate_value(id, get_static_entity()->query_value(id) + variant(change));
	init();
}

void property_editor_dialog::change_level_property(const std::string& id)
{
	std::string lvl = show_choose_level_dialog("Set " + id);
	if(lvl.empty() == false) {
		mutate_value(id, variant(lvl));
		init();
	}
}

void property_editor_dialog::set_label(gui::text_editor_widget* editor)
{
	if(editor->text().empty()) {
		return;
	}

	foreach(level_ptr lvl, editor_.get_level_list()) {
		foreach(entity_ptr entity_obj, entity_) {
			entity_ptr e = lvl->get_entity_by_label(entity_obj->label());
			if(e) {
				lvl->remove_character(e);
				e->set_label(editor->text());
				lvl->add_character(e);
			}
		}
	}
}

void property_editor_dialog::change_text_property(const std::string& id, const gui::text_editor_widget_ptr w)
{
	mutate_value(id, variant(w->text()));
}

void property_editor_dialog::change_numeric_property(const std::string& id, boost::shared_ptr<std::pair<gui::text_editor_widget_ptr, gui::slider_ptr> >  w)
{
	const editor_variable_info* var_info = get_static_entity()->editor_info() ? get_static_entity()->editor_info()->get_var_or_property_info(id) : NULL;
	if(!var_info) {
		return;
	}

	variant v(0);
	if(var_info->numeric_decimal()) {
		v = variant(decimal::from_string(w->first->text()));
	} else {
		v = variant(atoi(w->first->text().c_str()));
	}

	float pos = ((v.as_decimal() - var_info->numeric_min())/(var_info->numeric_max() - var_info->numeric_min())).as_float();
	if(pos < 0.0) {
		pos = 0.0;
	}

	if(pos > 1.0) {
		pos = 1.0;
	}
	w->second->set_position(pos);
	
	mutate_value(id, v);
}

void property_editor_dialog::change_numeric_property_slider(const std::string& id, boost::shared_ptr<std::pair<gui::text_editor_widget_ptr, gui::slider_ptr> >  w, double value)
{
	const editor_variable_info* var_info = get_static_entity()->editor_info() ? get_static_entity()->editor_info()->get_var_or_property_info(id) : NULL;
	if(!var_info) {
		return;
	}

	const float v = var_info->numeric_min().as_float() + value*(var_info->numeric_max() - var_info->numeric_min()).as_float();

	variant new_value;
	if(var_info->numeric_decimal()) {
		new_value = variant(decimal(v));
	} else {
		new_value = variant(static_cast<int>(v));
	}

	w->first->set_text(new_value.write_json());
	
	mutate_value(id, new_value);
}

void property_editor_dialog::change_enum_property(const std::string& id)
{
	const editor_variable_info* var_info = get_static_entity()->editor_info() ? get_static_entity()->editor_info()->get_var_or_property_info(id) : NULL;
	if(!var_info) {
		return;
	}

	using namespace gui;

	gui::grid* grid = new gui::grid(1);
	grid->set_zorder(100);
	grid->set_show_background(true);
	grid->allow_selection();

	grid->register_selection_callback(boost::bind(&property_editor_dialog::set_enum_property, this, id, var_info->enum_values(), _1));
	foreach(const std::string& s, var_info->enum_values()) {
		grid->add_col(gui::widget_ptr(new gui::label(s, graphics::color_white())));
	}

	int mousex, mousey;
	input::sdl_get_mouse_state(&mousex, &mousey);
	mousex -= x();
	mousey -= y();

	if(grid->height() > graphics::screen_height() - mousey) {
		mousey = graphics::screen_height() - grid->height();
	}

	if(grid->width() > graphics::screen_width() - mousex) {
		mousex = graphics::screen_width() - grid->width();
	}

	remove_widget(context_menu_);
	context_menu_.reset(grid);
	add_widget(context_menu_, mousex, mousey);
}

namespace {
bool hidden_label(const std::string& label) {
	return label.empty() || label[0] == '_';
}
}

void property_editor_dialog::change_label_property(const std::string& id)
{
	const controls::control_backup_scope ctrl_scope;

	const editor_variable_info* var_info = get_static_entity()->editor_info() ? get_static_entity()->editor_info()->get_var_or_property_info(id) : NULL;
	if(!var_info) {
		return;
	}
	
	bool loaded_level = false;
	std::vector<std::string> labels;
	if(var_info->info().empty() == false && var_info->info() != editor_.get_level().id()) {
		variant level_id = get_static_entity()->query_value(var_info->info());
		if(level_id.is_string() && level_id.as_string().empty() == false && level_id.as_string() != editor_.get_level().id()) {
			level lvl(level_id.as_string());
			lvl.finish_loading();
			lvl.get_all_labels(labels);
			loaded_level = true;
		}
	}

	if(!loaded_level) {
		editor_.get_level().get_all_labels(labels);
	}

	labels.erase(std::remove_if(labels.begin(), labels.end(), hidden_label), labels.end());

	if(labels.empty() == false) {

		gui::grid* grid = new gui::grid(1);
		grid->set_zorder(100);
		grid->set_show_background(true);
		grid->allow_selection();
		grid->register_selection_callback(boost::bind(&property_editor_dialog::set_enum_property, this, id, labels, _1));
		foreach(const std::string& lb, labels) {
			grid->add_col(gui::widget_ptr(new gui::label(lb, graphics::color_white())));
		}

		int mousex, mousey;
		input::sdl_get_mouse_state(&mousex, &mousey);
		mousex -= x();
		mousey -= y();

		if(grid->height() > graphics::screen_height() - mousey) {
			mousey = graphics::screen_height() - grid->height();
		}

		if(grid->width() > graphics::screen_width() - mousex) {
			mousex = graphics::screen_width() - grid->width();
		}

		remove_widget(context_menu_);
		context_menu_.reset(grid);
		add_widget(context_menu_, mousex, mousey);

	}
}

void property_editor_dialog::set_enum_property(const std::string& id, const std::vector<std::string>& labels, int index)
{
	remove_widget(context_menu_);
	context_menu_.reset();
	if(index < 0 || index >= labels.size()) {
		init();
		return;
	}

	mutate_value(id, variant(labels[index]));
	init();
}

void property_editor_dialog::change_points_property(const std::string& id)
{
	//Toggle whether we're adding points or not.
	if(editor_.adding_points() == id) {
		editor_.start_adding_points("");
	} else {
		editor_.start_adding_points(id);
	}
}

void property_editor_dialog::mutate_value(const std::string& key, variant value)
{
	foreach(level_ptr lvl, editor_.get_level_list()) {
		foreach(entity_ptr entity_obj, entity_) {
			entity_ptr e = lvl->get_entity_by_label(entity_obj->label());
			if(e) {
				editor_.mutate_object_value(lvl, e, key, value);
			}
		}
	}
}

void property_editor_dialog::deselect_object_type(std::string type)
{
	level& lvl = editor_.get_level();
	variant type_var(type);
	lvl.editor_clear_selection();
	foreach(entity_ptr& e, entity_) {
		if(e->query_value("type") != type_var) {
			lvl.editor_select_object(e);
		}
	}

	entity_ = lvl.editor_selection();

	init();
}

entity_ptr property_editor_dialog::get_static_entity() const
{
	entity_ptr result = editor_.get_level_list().front()->get_entity_by_label(get_entity()->label());
	if(result) {
		return result;
	} else {
		return get_entity();
	}
}

void property_editor_dialog::change_event_handler(const std::string& id, gui::label_ptr lb, gui::text_editor_widget_ptr text_editor)
{
	assert_recover_scope_.reset(new assert_recover_scope);
	static boost::intrusive_ptr<custom_object_callable> custom_object_definition(new custom_object_callable);

	std::cerr << "TRYING TO CHANGE EVENT HANDLER...\n";
	const std::string text = text_editor->text();
	try {
		game_logic::formula_ptr f(new game_logic::formula(variant(text), &get_custom_object_functions_symbol_table(), custom_object_definition));
		
		foreach(level_ptr lvl, editor_.get_level_list()) {
			foreach(entity_ptr entity_obj, entity_) {
				entity_ptr e = lvl->get_entity_by_label(entity_obj->label());
				if(e) {
					std::cerr << "SET EVENT HANDLER\n";
					e->set_event_handler(get_object_event_id(id), f);
				} else {
					std::cerr << "NO EVENT HANDLER FOUND\n";
				}
			}
		}

		std::cerr << "CHANGED EVENT HANDLER OKAY\n";

		lb->set_text(id + " event handler");

	} catch(validation_failure_exception& e) {
		lb->set_text(id + " event handler (Error)");
	}
}

}
#endif // !NO_EDITOR

