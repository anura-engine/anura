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

#include "custom_object.hpp"
#include "custom_object_type.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "label.hpp"
#include "module.hpp"
#include "variant.hpp"
#include "scrollable_widget.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"

namespace editor_dialogs 
{
	class CustomObjectDialog : public gui::Dialog
	{
	public:
		explicit CustomObjectDialog(editor& e, int x, int y, int w, int h);
		void init();
		variant getObject() const { return object_template_; }
		void showModal() override;
	protected:
		void changeTextAttribute(const gui::TextEditorWidgetPtr editor, const std::string& s);
		void changeIntAttributeText(const gui::TextEditorWidgetPtr editor, const std::string& s, gui::SliderPtr slide);
		void changeIntAttributeSlider(const gui::TextEditorWidgetPtr editor, const std::string& s, float d);
		void sliderDragEnd(const gui::TextEditorWidgetPtr editor, const std::string& s, gui::SliderPtr slide, float d);
		void changeTemplate(int selection, const std::string& s);
		void changePrototype();
		void removePrototype(const std::string& s);
		void executeChangePrototype(const std::vector<std::string>& choices, size_t index);
		void onCreate();
		void idChangeFocus(bool);

		void onSetPath();
	
		void onEditAnimations();
		void onEditItems(const std::string& name, const std::string& attr, bool allow_functions);

		std::vector<gui::WidgetPtr> getWidgetForAttribute(const std::string& attr);
	private:
		module::module_file_pair template_file_;
		variant object_template_;
		CustomObjectTypePtr object_;
		int selected_template_;
		std::string current_object_save_path_;

		gui::WidgetPtr context_menu_;

		std::string error_text_;

		std::string image_file_, image_file_name_, rel_path_;

		//std::map<std::string, double> slider_magnitude_;
		std::map<std::string, int> slider_offset_;
		std::vector<std::string> prototypes_;	// cached prototypes.
		bool dragging_slider_;
	};
}
