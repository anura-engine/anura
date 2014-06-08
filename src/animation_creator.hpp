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

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <vector>
#include <map>

#include "animation_preview_widget.hpp"
#include "checkbox.hpp"
#include "button.hpp"
#include "dialog.hpp"
#include "dropdown_widget.hpp"
#include "grid_widget.hpp"
#include "label.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"
#include "variant.hpp"

namespace gui {

class animation_creator_dialog : public dialog
{
public:
	animation_creator_dialog(int x, int y, int w, int h, const variant& anims=variant());
	virtual ~animation_creator_dialog() 
	{}
	variant get_animations() { return variant(&anims_); }
	void process();
protected:
	void init();
	
	void set_destination();
	void select_animation(int index);
	void set_option();
	void anim_del();
	void anim_new();
	void anim_save(dialog* d);
	void finish();
	bool show_attribute(variant v);

	void check_anim_changed();
	void reset_current_object();

	virtual void handleDraw() const;
	virtual bool handleEvent(const SDL_Event& event, bool claimed);
private:
	std::vector<variant> anims_;
	variant current_;				// Holds the currently selected variant.
	int selected_frame_;

	std::string copy_path_;
	std::string image_file_name_;	// file name.
	std::string image_file_;		// full path.
	std::string rel_path_;			// Path relative to modules images path.

	bool changed_;					// current animation modified?
	bool simple_options_;			// simplified list of options.

	std::vector<std::string> common_animation_list();
	void on_id_change(dropdown_WidgetPtr editor, const std::string& s);
	void on_id_set(dropdown_WidgetPtr editor, int selection, const std::string& s);
	void set_image_file();
	void change_text(const std::string& s, TextEditorWidgetPtr editor, slider_ptr slider);
	void execute_change_text(const std::string& s, TextEditorWidgetPtr editor, slider_ptr slider);
	void change_slide(const std::string& s, TextEditorWidgetPtr editor, double d);
	void end_slide(const std::string& s, slider_ptr slide, TextEditorWidgetPtr editor, double d);

	void set_animation_rect(rect r);
	void move_solid_rect(int dx, int dy);
	void set_integer_attr(const char* attr, int value);

	typedef std::pair<std::string, int> slider_offset_pair;
	std::map<std::string, int> slider_offset_;
	bool dragging_slider_;

	animation_preview_WidgetPtr animation_preview_;
};

}

#endif // NO_EDITOR
