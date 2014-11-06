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
#include "widget.hpp"
#include "asserts.hpp"
#include "formula_callable.hpp"
#include "widget_factory.hpp"
#include "variant.hpp"

#include "animation_preview_widget.hpp"
#include "animation_widget.hpp"
#include "bar_widget.hpp"
#include "border_widget.hpp"
#include "button.hpp"
#include "code_editor_widget.hpp"
#include "color_picker.hpp"
#include "checkbox.hpp"
#include "custom_object_widget.hpp"
#include "dialog.hpp"
#include "drag_widget.hpp"
#include "file_chooser_dialog.hpp"
#include "graphical_font_label.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "gui_section.hpp"
#include "key_button.hpp"
#include "label.hpp"
#include "layout_widget.hpp"
#include "play_vpx.hpp"
#include "psystem2.hpp"
#include "poly_line_widget.hpp"
#include "poly_map.hpp"
#include "preview_tileset_widget.hpp"
#include "progress_bar.hpp"
#include "rich_text_label.hpp"
#include "scrollable_widget.hpp"
#include "scrollbar_widget.hpp"
#include "selector_widget.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"
#include "tree_view_widget.hpp"
#include "view3d_widget.hpp"

namespace widget_factory {

using gui::widget_ptr;

widget_ptr create(const variant& v, game_logic::formula_callable* e)
{
	if(v.is_callable()) {
		widget_ptr w = v.try_convert<gui::widget>();
		ASSERT_LOG(w != NULL, "Error converting widget from callable.");
		return w;
	}
	ASSERT_LOG(v.is_map(), "TYPE ERROR: widget must be specified by a map, found: " << v.to_debug_string());
	std::string wtype = v["type"].as_string();
	if(wtype == "animation_widget") {
		return widget_ptr(new gui::animation_widget(v,e));
#ifndef NO_EDITOR
	} else if(wtype == "animation_preview") {
		return widget_ptr(new gui::animation_preview_widget(v,e));
#endif
	} else if(wtype == "border_widget") {
		return widget_ptr(new gui::border_widget(v,e));
	} else if(wtype == "button") {
		return widget_ptr(new gui::button(v,e));
	} else if(wtype == "checkbox") {
		return widget_ptr(new gui::checkbox(v,e));
	} else if(wtype == "dialog") {
		return widget_ptr(new gui::dialog(v,e));
#ifndef NO_EDITOR
	} else if(wtype == "drag_widget") {
		return widget_ptr(new gui::drag_widget(v,e));
#endif
	} else if(wtype == "graphical_font_label") {
		return widget_ptr(new gui::graphical_font_label(v,e));
	} else if(wtype == "grid") {
		return widget_ptr(new gui::grid(v,e));
	} else if(wtype == "image") {
		return widget_ptr(new gui::image_widget(v,e));
	} else if(wtype == "section") {
		return widget_ptr(new gui::gui_section_widget(v,e));
	} else if(wtype == "key_button") {
		return widget_ptr(new gui::key_button(v,e));
	} else if(wtype == "label") {
		return widget_ptr(new gui::label(v,e));
	} else if(wtype == "poly_line_widget") {
		return widget_ptr(new gui::poly_line_widget(v,e));
	} else if(wtype == "rich_text_label") {
		return widget_ptr(new gui::rich_text_label(v,e));
	} else if(wtype == "tileset_preview") {
		return widget_ptr(new gui::preview_tileset_widget(v,e));
	} else if(wtype == "scrollbar") {
		return widget_ptr(new gui::scrollbar_widget(v,e));
	} else if(wtype == "slider") {
		return widget_ptr(new gui::slider(v,e));
	} else if(wtype == "text_editor") {
		return widget_ptr(new gui::text_editor_widget(v,e));
	} else if(wtype == "progress") {
		return widget_ptr(new gui::progress_bar(v, e));
	} else if(wtype == "selector") {
		return widget_ptr(new gui::selector_widget(v, e));
	} else if(wtype == "object") {
		return widget_ptr(new gui::custom_object_widget(v, e));
	} else if(wtype == "bar") {
		return widget_ptr(new gui::bar_widget(v, e));
	} else if(wtype == "color_picker") {
		return widget_ptr(new gui::color_picker(v, e));
	} else if(wtype == "layout") {
		return widget_ptr(new gui::layout_widget(v, e));
	} else if(wtype == "file_chooser") {
		return widget_ptr(new gui::file_chooser_dialog(v, e));
#if defined(USE_ISOMAP)
	} else if(wtype == "view3d") {
		return widget_ptr(new gui::view3d_widget(v, e));
#endif
	} else if(wtype == "poly_map") {
		return widget_ptr(new geometry::poly_map(v, e));
	} else if(wtype == "particle_system") {
		return widget_ptr(new graphics::particles::particle_system_widget(v, e));
#if defined(USE_LIBVPX)
	} else if(wtype == "movie") {
		return widget_ptr(new movie::vpx(v, e));
#endif
	//} else if(wtype == "scrollable") {
	//} else if(wtype == "widget") {
	} else if(wtype == "tree") {
		return widget_ptr(new gui::tree_view_widget(v, e));
	} else {
		ASSERT_LOG(true, "Unable to create a widget of type " << wtype);
		return widget_ptr();
	}
}

}
