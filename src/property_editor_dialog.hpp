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

#pragma once

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
	class PropertyEditorDialog : public gui::Dialog
	{
	public:
		explicit PropertyEditorDialog(editor& e);
		void init();

		EntityPtr getEntity() const { return entity_.empty() ? EntityPtr() : entity_.front(); }
		const std::vector<EntityPtr>& getEntityList() const { return entity_; }
		void setEntity(EntityPtr e);
		void setEntityGroup(const std::vector<EntityPtr>& entities);
		void removeObjectFromGroup(EntityPtr e);
		void removeGroup(int ngroup);
	private:
		void setLabel(gui::TextEditorWidget* e);
		EntityPtr getStaticEntity() const;
		void changeMinDifficulty(int amount);
		void changeMaxDifficulty(int amount);
		void toggleProperty(const std::string& id);
		void changeProperty(const std::string& id, int change);
		void changeLevelProperty(const std::string& id);

		void changeLabelProperty(const std::string& id);
		void changeTextProperty(const std::string& id, const gui::TextEditorWidgetPtr w);

		typedef std::pair<gui::TextEditorWidgetPtr, gui::SliderPtr> numeric_widgets;
		void changeNumericProperty(const std::string& id, std::shared_ptr<numeric_widgets> w);
		void changeNumericPropertySlider(const std::string& id, std::shared_ptr<numeric_widgets> w, float value);
		void changeEnumProperty(const std::string& id);
		void setEnumProperty(const std::string& id, const std::vector<std::string>& options, int index, bool real_enum);

		void changePointsProperty(const std::string& id);

		void mutateValue(const std::string& key, variant value);

		void deselectObjectType(std::string type);

		void changeEventHandler(const std::string& id, gui::LabelPtr lb, gui::TextEditorWidgetPtr text_editor);

		editor& editor_;
		std::vector<EntityPtr> entity_;
		gui::WidgetPtr context_menu_;

		std::unique_ptr<assert_recover_scope> assert_recover_scope_;
	};

	typedef ffl::IntrusivePtr<PropertyEditorDialog> PropertyEditorDialogPtr;
}

#endif // !NO_EDITOR
