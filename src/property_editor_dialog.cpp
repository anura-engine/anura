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
#include <assert.h>
#include <sstream>

#include "WindowManager.hpp"

#include "button.hpp"
#include "checkbox.hpp"
#include "code_editor_widget.hpp"
#include "controls.hpp"
#include "custom_object.hpp"
#include "custom_object_functions.hpp"
#include "difficulty.hpp"
#include "editor_dialogs.hpp"
#include "formatter.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "input.hpp"
#include "label.hpp"
#include "load_level.hpp"
#include "object_events.hpp"
#include "property_editor_dialog.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"

namespace editor_dialogs
{
	using std::placeholders::_1;

	PropertyEditorDialog::PropertyEditorDialog(editor& e)
	  : gui::Dialog(KRE::WindowManager::getMainWindow()->width() - EDITOR_SIDEBAR_WIDTH, 160, EDITOR_SIDEBAR_WIDTH, 440), editor_(e)
	{
		setClearBgAmount(255);
		init();
	}

	void PropertyEditorDialog::init()
	{
		clear();
		if(entity_.empty()) {
			return;
		}

		using namespace gui;

		setPadding(5);

		const Frame& frame = getStaticEntity()->getCurrentFrame();
		ImageWidget* preview = new ImageWidget(frame.img());
		preview->setDim(frame.width(), frame.height());
		preview->setArea(frame.area());

		GridPtr preview_grid(new Grid(2));
		preview_grid->addCol(WidgetPtr(preview));

		//draw the object's difficulty settings.
		GridPtr difficulty_grid(new Grid(3));
		auto obj = boost::dynamic_pointer_cast<const CustomObject>(getEntity());
		ASSERT_LOG(obj, "ENTITY IS NOT AN OBJECT");
		std::string min_difficulty = difficulty::to_string(obj->getMinDifficulty());
		std::string max_difficulty = difficulty::to_string(obj->getMaxDifficulty());
	
		if(min_difficulty.empty()) {
			min_difficulty = formatter() << obj->getMinDifficulty();
		}

		if(max_difficulty.empty()) {
			max_difficulty = formatter() << obj->getMaxDifficulty();
		}
		min_difficulty = " " + min_difficulty;
		max_difficulty = " " + max_difficulty;

		ButtonPtr difficulty_button;
		difficulty_button.reset(new Button(WidgetPtr(new Label("-", KRE::Color::colorWhite())), std::bind(&PropertyEditorDialog::changeMinDifficulty, this, -1)));
		difficulty_button->setTooltip("Decrease minimum difficulty");
		difficulty_button->setDim(difficulty_button->width()-10, difficulty_button->height()-4);
		difficulty_grid->addCol(difficulty_button);
		difficulty_button.reset(new Button(WidgetPtr(new Label("+", KRE::Color::colorWhite())), std::bind(&PropertyEditorDialog::changeMinDifficulty, this, 1)));
		difficulty_button->setTooltip("Increase minimum difficulty");
		difficulty_button->setDim(difficulty_button->width()-10, difficulty_button->height()-4);
		difficulty_grid->addCol(difficulty_button);
		difficulty_grid->addCol(WidgetPtr(new Label(min_difficulty, KRE::Color::colorWhite())));

		difficulty_button.reset(new Button(WidgetPtr(new Label("-", KRE::Color::colorWhite())), std::bind(&PropertyEditorDialog::changeMaxDifficulty, this, -1)));
		difficulty_button->setTooltip("Decrease maximum difficulty");
		difficulty_button->setDim(difficulty_button->width()-10, difficulty_button->height()-4);
		difficulty_grid->addCol(difficulty_button);
		difficulty_button.reset(new Button(WidgetPtr(new Label("+", KRE::Color::colorWhite())), std::bind(&PropertyEditorDialog::changeMaxDifficulty, this, 1)));
		difficulty_button->setTooltip("Increase maximum difficulty");
		difficulty_button->setDim(difficulty_button->width()-10, difficulty_button->height()-4);
		difficulty_grid->addCol(difficulty_button);
		difficulty_grid->addCol(WidgetPtr(new Label(max_difficulty, KRE::Color::colorWhite())));


		preview_grid->addCol(difficulty_grid);
	
		addWidget(preview_grid, 10, 10);

		addWidget(WidgetPtr(new Label(obj->getDebugDescription(), KRE::Color::colorWhite())));

		if(getEntity()->label().empty() == false) {
			GridPtr labels_grid(new Grid(2));
			labels_grid->setHpad(5);
			TextEditorWidget* e = new TextEditorWidget(120);
			e->setText(getEntity()->label());
			e->setOnChangeHandler(std::bind(&PropertyEditorDialog::setLabel, this, e));
			labels_grid->addCol(WidgetPtr(new Label("Label: ")))
						.addCol(WidgetPtr(e));
			addWidget(labels_grid);
		}

		std::map<std::string, int> types_selected;
		for(EntityPtr e : entity_) {
			types_selected[e->queryValue("type").as_string()]++;
		}

		if(types_selected.size() > 1) {
			GridPtr types_grid(new Grid(3));
			types_grid->setHpad(5);
			for(std::map<std::string, int>::const_iterator i = types_selected.begin(); i != types_selected.end(); ++i) {
				std::string type = i->first;
				if(type.size() > 24) {
					type.resize(24);
					std::fill(type.end()-3, type.end(), '.');
				}
				types_grid->addCol(WidgetPtr(new Label(formatter() << i->second, 10)))
						   .addCol(WidgetPtr(new Label(type, 10)))
						   .addCol(WidgetPtr(new Button("Deselect", std::bind(&PropertyEditorDialog::deselectObjectType, this, i->first))));
			}

			addWidget(types_grid);
		}

		if(entity_.size() > 1) {
			addWidget(WidgetPtr(new Button("Group Objects", std::bind(&editor::group_selection, &editor_))));
		}

		game_logic::FormulaCallable* vars = getStaticEntity()->vars();
		if(getEntity()->getEditorInfo() && types_selected.size() == 1) {
			if(getEntity()->group() >= 0) {
				GridPtr group_grid(new Grid(1));

				group_grid->addCol(WidgetPtr(new Button("Remove from Group", std::bind(&PropertyEditorDialog::removeObjectFromGroup, this, getEntity()))));
				group_grid->addCol(WidgetPtr(new Button("Breakup Group", std::bind(&PropertyEditorDialog::removeGroup, this, getEntity()->group()))));

				addWidget(group_grid);
			}

			//output an editing area for each editable event.
			for(const std::string& handler : getEntity()->getEditorInfo()->getEditableEvents()) {
				LabelPtr lb = Label::create(handler + " event handler", KRE::Color::colorWhite());
				addWidget(lb);

				TextEditorWidget* e = new TextEditorWidget(220, 90);
				game_logic::ConstFormulaPtr f = getEntity()->getEventHandler(get_object_event_id(handler));
				if(f) {
					e->setText(f->str());
				}
				addWidget(WidgetPtr(e));

				e->setOnChangeHandler(std::bind(&PropertyEditorDialog::changeEventHandler, this, handler, lb.get(), e));

			}

			for(const EditorVariableInfo& info : getEntity()->getEditorInfo()->getVarsAndProperties()) {

				if(info.getType() == VARIABLE_TYPE::XPOSITION ||
				   info.getType() == VARIABLE_TYPE::YPOSITION) {
					//don't show x/y position as they are directly editable on
					//the map.
					continue;
				}

				variant val = info.isProperty() ? getStaticEntity()->queryValue(info.getVariableName()) : vars->queryValue(info.getVariableName());
				std::string current_val_str;
				if(info.getType() == VARIABLE_TYPE::POINTS) {
					if(!val.is_list()) {
						current_val_str = "null";
					} else {
						current_val_str = formatter() << val.num_elements() << " points";
					}
				} else {
					current_val_str = val.to_debug_string();
				}

				if(info.getType() != VARIABLE_TYPE::TEXT &&
				   info.getType() != VARIABLE_TYPE::INTEGER &&
				   info.getType() != VARIABLE_TYPE::ENUM &&
				   info.getType() != VARIABLE_TYPE::BOOLEAN) {
					std::ostringstream s;
					s << info.getVariableName() << ": " << current_val_str;
					LabelPtr lb = Label::create(s.str(), KRE::Color::colorWhite());
					addWidget(WidgetPtr(lb));
				}

				if(info.getType() == VARIABLE_TYPE::TEXT) {

					GridPtr text_grid(new Grid(2));

					LabelPtr lb = Label::create(info.getVariableName() + ":", KRE::Color::colorWhite());
					text_grid->addCol(lb);

					std::string current_value;
					variant current_value_var = getStaticEntity()->queryValue(info.getVariableName());
					if(current_value_var.is_string()) {
						current_value = current_value_var.as_string();
					}

					TextEditorWidget* e = new TextEditorWidget(200 - lb->width());
					e->setText(current_value);
					e->setOnChangeHandler(std::bind(&PropertyEditorDialog::changeTextProperty, this, info.getVariableName(), e));

					text_grid->addCol(WidgetPtr(e));

					addWidget(WidgetPtr(text_grid));

				} else if(info.getType() == VARIABLE_TYPE::ENUM) {
					GridPtr enum_grid(new Grid(2));
					LabelPtr lb = Label::create(info.getVariableName() + ":", KRE::Color::colorWhite());
					enum_grid->addCol(lb);

					std::string current_value;
					variant current_value_var = getStaticEntity()->queryValue(info.getVariableName());
					if(current_value_var.is_string()) {
						current_value = current_value_var.as_string();
					}
					else if (current_value_var.is_enum()) {
						current_value = current_value_var.as_enum();
					}

					if(std::count(info.getEnumValues().begin(), info.getEnumValues().end(), current_value) == 0) {
						current_value = info.getEnumValues().front();
					}

					enum_grid->addCol(WidgetPtr(new Button(
						 WidgetPtr(new Label(current_value, KRE::Color::colorWhite())),
						 std::bind(&PropertyEditorDialog::changeEnumProperty, this, info.getVariableName()))));
					addWidget(WidgetPtr(enum_grid));
				} else if(info.getType() == VARIABLE_TYPE::LEVEL) {
					std::string current_value;
					variant current_value_var = getStaticEntity()->queryValue(info.getVariableName());
					if(current_value_var.is_string()) {
						current_value = current_value_var.as_string();
					}

					addWidget(WidgetPtr(new Button(
							 WidgetPtr(new Label(current_value.empty() ? "(set level)" : current_value, KRE::Color::colorWhite())),
							 std::bind(&PropertyEditorDialog::changeLevelProperty, this, info.getVariableName()))));
				} else if(info.getType() == VARIABLE_TYPE::LABEL) {
					std::string current_value;
					variant current_value_var = getStaticEntity()->queryValue(info.getVariableName());
					if(current_value_var.is_string()) {
						current_value = current_value_var.as_string();
					}

					addWidget(WidgetPtr(new Button(
							 WidgetPtr(new Label(current_value.empty() ? "(set label)" : current_value, KRE::Color::colorWhite())),
							 std::bind(&PropertyEditorDialog::changeLevelProperty, this, info.getVariableName()))));
				
				} else if(info.getType() == VARIABLE_TYPE::BOOLEAN) {
					variant current_value = getStaticEntity()->queryValue(info.getVariableName());

					addWidget(WidgetPtr(new Checkbox(WidgetPtr(new Label(info.getVariableName(), KRE::Color::colorWhite())),
								   current_value.as_bool(),
								   std::bind(&PropertyEditorDialog::toggleProperty, this, info.getVariableName()))));
				} else if(info.getType() == VARIABLE_TYPE::POINTS) {
					variant current_value = getStaticEntity()->queryValue(info.getVariableName());
					const int npoints = current_value.is_list() ? current_value.num_elements() : 0;

					const bool already_adding = editor_.adding_points() == info.getVariableName();
					addWidget(WidgetPtr(new Button(already_adding ? "Done Adding" : "Add Points",
							 std::bind(&PropertyEditorDialog::changePointsProperty, this, info.getVariableName()))));

				} else {
					GridPtr text_grid(new Grid(2));

					LabelPtr lb = Label::create(info.getVariableName() + ":", KRE::Color::colorWhite());
					text_grid->addCol(lb);

					decimal value;
					std::string current_value = "0";
					variant current_value_var = getStaticEntity()->queryValue(info.getVariableName());
					if(current_value_var.is_int()) {
						current_value = formatter() << current_value_var.as_int();
						value = current_value_var.as_decimal();
					} else if(current_value_var.is_decimal()) {
						current_value = formatter() << current_value_var.as_decimal();
						value = current_value_var.as_decimal();
					}

					std::shared_ptr<numeric_widgets> widgets(new numeric_widgets);

					TextEditorWidget* e = new TextEditorWidget(80);
					e->setText(current_value);
					e->setOnChangeHandler(std::bind(&PropertyEditorDialog::changeNumericProperty, this, info.getVariableName(), widgets));

					text_grid->addCol(WidgetPtr(e));

					addWidget(WidgetPtr(text_grid));

					float pos = static_cast<float>(((value - info.numericMin())/(info.numericMax() - info.numericMin())).as_float());
					if(pos < 0.0f) {
						pos = 0.0f;
					}

					if(pos > 1.0f) {
						pos = 1.0f;
					}

					lb.reset(new Label(formatter() << info.numericMin().as_int(), 10));
					addWidget(lb, text_grid->x(), text_grid->y() + text_grid->height() + 6);
					lb.reset(new Label(formatter() << info.numericMax().as_int(), 10));
					addWidget(lb, text_grid->x() + 170, text_grid->y() + text_grid->height() + 6);

					gui::Slider* Slider = new gui::Slider(160, std::bind(&PropertyEditorDialog::changeNumericPropertySlider, this, info.getVariableName(), widgets, _1), pos);

					addWidget(WidgetPtr(Slider), text_grid->x(), text_grid->y() + text_grid->height() + 8);

					*widgets = numeric_widgets(e, Slider);
				}
			}
		}
	}

	void PropertyEditorDialog::setEntity(EntityPtr e)
	{
		entity_.clear();
		if(e) {
			entity_.push_back(e);
		}
		init();
	}

	void PropertyEditorDialog::setEntityGroup(const std::vector<EntityPtr>& e)
	{
		entity_ = e;
		init();
	}

	void PropertyEditorDialog::removeObjectFromGroup(EntityPtr entity_obj)
	{
		for(LevelPtr lvl : editor_.get_level_list()) {
			EntityPtr e = lvl->get_entity_by_label(entity_obj->label());
			if(!e) {
				continue;
			}
			lvl->set_character_group(e, -1);
		}
		init();
	}

	void PropertyEditorDialog::removeGroup(int ngroup)
	{
		for(LevelPtr lvl : editor_.get_level_list()) {
			for(EntityPtr e : lvl->get_chars()) {
				if(e->group() == ngroup) {
					lvl->set_character_group(e, -1);
				}
			}
		}
		init();
	}

	void PropertyEditorDialog::changeMinDifficulty(int amount)
	{
		for(LevelPtr lvl : editor_.get_level_list()) {
			for(EntityPtr entity_obj : entity_) {
				EntityPtr e = lvl->get_entity_by_label(entity_obj->label());
				if(!e) {
					continue;
				}

				auto obj = boost::dynamic_pointer_cast<CustomObject>(e);
				ASSERT_LOG(obj, "ENTITY IS NOT AN OBJECT");

				const int new_difficulty = std::max<int>(-1, obj->getMinDifficulty() + amount);
				obj->setDifficultyRange(new_difficulty, obj->getMaxDifficulty());
			}
		}

		init();
	}

	void PropertyEditorDialog::changeMaxDifficulty(int amount)
	{
		for(LevelPtr lvl : editor_.get_level_list()) {
			for(EntityPtr entity_obj : entity_) {
				EntityPtr e = lvl->get_entity_by_label(entity_obj->label());
				if(!e) {
					continue;
				}
				auto obj = boost::dynamic_pointer_cast<CustomObject>(e);
				ASSERT_LOG(obj, "ENTITY IS NOT AN OBJECT");

				const int new_difficulty = std::max<int>(-1, obj->getMaxDifficulty() + amount);
				obj->setDifficultyRange(obj->getMinDifficulty(), new_difficulty);
			}
		}

		init();
	}

	void PropertyEditorDialog::toggleProperty(const std::string& id)
	{
		mutateValue(id, variant::from_bool(!getStaticEntity()->queryValue(id).as_bool()));
		init();
	}

	void PropertyEditorDialog::changeProperty(const std::string& id, int change)
	{
		mutateValue(id, getStaticEntity()->queryValue(id) + variant(change));
		init();
	}

	void PropertyEditorDialog::changeLevelProperty(const std::string& id)
	{
		std::string lvl = show_choose_level_dialog("Set " + id);
		if(lvl.empty() == false) {
			mutateValue(id, variant(lvl));
			init();
		}
	}

	void PropertyEditorDialog::setLabel(gui::TextEditorWidget* editor)
	{
		if(editor->text().empty()) {
			return;
		}

		for(LevelPtr lvl : editor_.get_level_list()) {
			for(EntityPtr entity_obj : entity_) {
				EntityPtr e = lvl->get_entity_by_label(entity_obj->label());
				if(e) {
					lvl->remove_character(e);
					e->setLabel(editor->text());
					lvl->add_character(e);
				}
			}
		}
	}

	void PropertyEditorDialog::changeTextProperty(const std::string& id, const gui::TextEditorWidgetPtr w)
	{
		mutateValue(id, variant(w->text()));
	}

	void PropertyEditorDialog::changeNumericProperty(const std::string& id, std::shared_ptr<std::pair<gui::TextEditorWidgetPtr, gui::SliderPtr> >  w)
	{
		const EditorVariableInfo* var_info = getStaticEntity()->getEditorInfo() ? getStaticEntity()->getEditorInfo()->getVarOrPropertyInfo(id) : nullptr;
		if(!var_info) {
			return;
		}

		variant v(0);
		if(var_info->numericDecimal()) {
			v = variant(decimal::from_string(w->first->text()));
		} else {
			v = variant(atoi(w->first->text().c_str()));
		}

		float pos = static_cast<float>(((v.as_decimal() - var_info->numericMin())/(var_info->numericMax() - var_info->numericMin())).as_float());
		if(pos < 0.0f) {
			pos = 0.0f;
		}

		if(pos > 1.0f) {
			pos = 1.0f;
		}
		w->second->setPosition(pos);
	
		mutateValue(id, v);
	}

	void PropertyEditorDialog::changeNumericPropertySlider(const std::string& id, std::shared_ptr<std::pair<gui::TextEditorWidgetPtr, gui::SliderPtr> >  w, float value)
	{
		const EditorVariableInfo* var_info = getStaticEntity()->getEditorInfo() ? getStaticEntity()->getEditorInfo()->getVarOrPropertyInfo(id) : nullptr;
		if(!var_info) {
			return;
		}

		const float v = static_cast<float>(var_info->numericMin().as_float() + value*(var_info->numericMax() - var_info->numericMin()).as_float());

		variant new_value;
		if(var_info->numericDecimal()) {
			new_value = variant(decimal(v));
		} else {
			new_value = variant(static_cast<int>(v));
		}

		w->first->setText(new_value.write_json());
	
		mutateValue(id, new_value);
	}

	void PropertyEditorDialog::changeEnumProperty(const std::string& id)
	{
		const EditorVariableInfo* var_info = getStaticEntity()->getEditorInfo() ? getStaticEntity()->getEditorInfo()->getVarOrPropertyInfo(id) : nullptr;
		if(!var_info) {
			return;
		}

		using namespace gui;

		Grid* grid = new Grid(1);
		grid->setZOrder(100);
		grid->setShowBackground(true);
		grid->allowSelection();

		grid->registerSelectionCallback(std::bind(&PropertyEditorDialog::setEnumProperty, this, id, var_info->getEnumValues(), _1, var_info->realEnum()));
		for(const std::string& s : var_info->getEnumValues()) {
			grid->addCol(gui::WidgetPtr(new Label(s, KRE::Color::colorWhite())));
		}

		int mousex, mousey;
		input::sdl_get_mouse_state(&mousex, &mousey);

		if(grid->height() > KRE::WindowManager::getMainWindow()->height() - mousey) {
			mousey = KRE::WindowManager::getMainWindow()->height() - grid->height();
		}

		if(grid->width() > KRE::WindowManager::getMainWindow()->width() - mousex) {
			mousex = KRE::WindowManager::getMainWindow()->width() - grid->width();
		}

		mousex -= x();
		mousey -= y();

		removeWidget(context_menu_);
		context_menu_.reset(grid);
		addWidget(context_menu_, mousex, mousey);
	}

	namespace 
	{
		bool hidden_label(const std::string& label) 
		{
			return label.empty() || label[0] == '_';
		}
	}

	void PropertyEditorDialog::changeLabelProperty(const std::string& id)
	{
		const controls::control_backup_scope ctrl_scope;

		const EditorVariableInfo* var_info = getStaticEntity()->getEditorInfo() ? getStaticEntity()->getEditorInfo()->getVarOrPropertyInfo(id) : nullptr;
		if(!var_info) {
			return;
		}
	
		bool loaded_level = false;
		std::vector<std::string> labels;
		if(var_info->getInfo().empty() == false && var_info->getInfo() != editor_.get_level().id()) {
			variant level_id = getStaticEntity()->queryValue(var_info->getInfo());
			if(level_id.is_string() && level_id.as_string().empty() == false && level_id.as_string() != editor_.get_level().id()) {
				Level lvl(level_id.as_string());
				lvl.finishLoading();
				lvl.getAllLabels(labels);
				loaded_level = true;
			}
		}

		if(!loaded_level) {
			editor_.get_level().getAllLabels(labels);
		}

		labels.erase(std::remove_if(labels.begin(), labels.end(), hidden_label), labels.end());

		if(labels.empty() == false) {

			gui::Grid* grid = new gui::Grid(1);
			grid->setZOrder(100);
			grid->setShowBackground(true);
			grid->allowSelection();
			grid->registerSelectionCallback(std::bind(&PropertyEditorDialog::setEnumProperty, this, id, labels, _1, var_info->realEnum()));
			for(const std::string& lb : labels) {
				grid->addCol(gui::WidgetPtr(new gui::Label(lb, KRE::Color::colorWhite())));
			}

			int mousex, mousey;
			input::sdl_get_mouse_state(&mousex, &mousey);
			mousex -= x();
			mousey -= y();

			if(grid->height() > KRE::WindowManager::getMainWindow()->height() - mousey) {
				mousey = KRE::WindowManager::getMainWindow()->height() - grid->height();
			}

			if(grid->width() > KRE::WindowManager::getMainWindow()->width() - mousex) {
				mousex = KRE::WindowManager::getMainWindow()->width() - grid->width();
			}

			removeWidget(context_menu_);
			context_menu_.reset(grid);
			addWidget(context_menu_, mousex, mousey);

		}
	}

	void PropertyEditorDialog::setEnumProperty(const std::string& id, const std::vector<std::string>& labels, int index, bool real_enum)
	{
		removeWidget(context_menu_);
		context_menu_.reset();
		if(index < 0 || static_cast<unsigned>(index) >= labels.size()) {
			init();
			return;
		}

		mutateValue(id, real_enum ? variant::create_enum(labels[index]) : variant(labels[index]));
		init();
	}

	void PropertyEditorDialog::changePointsProperty(const std::string& id)
	{
		//Toggle whether we're adding points or not.
		if(editor_.adding_points() == id) {
			editor_.start_adding_points("");
		} else {
			editor_.start_adding_points(id);
		}
	}

	void PropertyEditorDialog::mutateValue(const std::string& key, variant value)
	{
		for(LevelPtr lvl : editor_.get_level_list()) {
			for(EntityPtr entity_obj : entity_) {
				EntityPtr e = lvl->get_entity_by_label(entity_obj->label());
				if(e) {
					editor_.mutate_object_value(lvl, e, key, value);
				}
			}
		}
	}

	void PropertyEditorDialog::deselectObjectType(std::string type)
	{
		Level& lvl = editor_.get_level();
		variant type_var(type);
		lvl.editor_clear_selection();
		for(EntityPtr& e : entity_) {
			if(e->queryValue("type") != type_var) {
				lvl.editor_select_object(e);
			}
		}

		entity_ = lvl.editor_selection();

		init();
	}

	EntityPtr PropertyEditorDialog::getStaticEntity() const
	{
		EntityPtr result = editor_.get_level_list().front()->get_entity_by_label(getEntity()->label());
		if(result) {
			return result;
		} else {
			return getEntity();
		}
	}

	void PropertyEditorDialog::changeEventHandler(const std::string& id, gui::LabelPtr lb, gui::TextEditorWidgetPtr text_editor)
	{
		assert_recover_scope_.reset(new assert_recover_scope);
		static ffl::IntrusivePtr<CustomObjectCallable> custom_object_definition(new CustomObjectCallable);

		LOG_INFO("TRYING TO CHANGE EVENT HANDLER...");
		const std::string text = text_editor->text();
		try {
			game_logic::FormulaPtr f(new game_logic::Formula(variant(text), &get_custom_object_functions_symbol_table(), custom_object_definition));
		
			for(LevelPtr lvl : editor_.get_level_list()) {
				for(EntityPtr entity_obj : entity_) {
					EntityPtr e = lvl->get_entity_by_label(entity_obj->label());
					if(e) {
						LOG_INFO("SET EVENT HANDLER");
						e->setEventHandler(get_object_event_id(id), f);
					} else {
						LOG_INFO("NO EVENT HANDLER FOUND");
					}
				}
			}

			LOG_INFO("CHANGED EVENT HANDLER OKAY");

			lb->setText(id + " event handler");

		} catch(validation_failure_exception&) {
			lb->setText(id + " event handler (Error)");
		}
	}
}

#endif // !NO_EDITOR
