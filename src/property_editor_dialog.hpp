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
#ifndef PROPERTY_EDITOR_DIALOG_HPP_INCLUDED
#define PROPERTY_EDITOR_DIALOG_HPP_INCLUDED
#ifndef NO_EDITOR

#include <string>

#include "asserts.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "entity.hpp"
#include "label.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"

namespace editor_dialogs
{

class property_editor_dialog : public gui::dialog
{
public:
	explicit property_editor_dialog(editor& e);
	void init();

	EntityPtr getEntity() const { return entity_.empty() ? EntityPtr() : entity_.front(); }
	const std::vector<EntityPtr>& get_entity_list() const { return entity_; }
	void setEntity(EntityPtr e);
	void set_entity_group(const std::vector<EntityPtr>& entities);
	void remove_object_from_group(EntityPtr e);
	void remove_group(int ngroup);
private:
	void setLabel(gui::TextEditorWidget* e);
	EntityPtr get_static_entity() const;
	void change_min_difficulty(int amount);
	void change_max_difficulty(int amount);
	void toggle_property(const std::string& id);
	void change_property(const std::string& id, int change);
	void change_level_property(const std::string& id);

	void change_label_property(const std::string& id);
	void change_text_property(const std::string& id, const gui::TextEditorWidgetPtr w);

	typedef std::pair<gui::TextEditorWidgetPtr, gui::SliderPtr> numeric_widgets;
	void change_numeric_property(const std::string& id, std::shared_ptr<numeric_widgets> w);
	void change_numeric_property_Slider(const std::string& id, std::shared_ptr<numeric_widgets> w, double value);
	void change_enum_property(const std::string& id);
	void set_enum_property(const std::string& id, const std::vector<std::string>& options, int index);

	void change_points_property(const std::string& id);

	void mutate_value(const std::string& key, variant value);

	void deselect_object_type(std::string type);

	void change_event_handler(const std::string& id, gui::LabelPtr lb, gui::TextEditorWidgetPtr text_editor);

	editor& editor_;
	std::vector<EntityPtr> entity_;
	gui::WidgetPtr context_menu_;

	std::unique_ptr<assert_recover_scope> assert_recover_scope_;
};

typedef boost::intrusive_ptr<property_editor_dialog> property_editor_DialogPtr;

}

#endif
#endif // !NO_EDITOR
