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

#include "asserts.hpp"
#include "widget_factory.hpp"
#include "widget.hpp"

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
#include "dropdown_widget.hpp"
#include "file_chooser_dialog.hpp"
#include "graphical_font_label.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "gui_section.hpp"
#include "key_button.hpp"
#include "label.hpp"
#include "layout_widget.hpp"
#include "play_vpx.hpp"
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

namespace widget_factory 
{
	using namespace gui;

	WidgetPtr create(const variant& v, game_logic::FormulaCallable* e)
	{
		if(v.is_callable()) {
			WidgetPtr w = v.try_convert<Widget>();
			ASSERT_LOG(w != nullptr, "Error converting widget from callable.");
			return w;
		}
		ASSERT_LOG(v.is_map(), "TYPE ERROR: widget must be specified by a map, found: " << v.to_debug_string());
		std::string wtype = v["type"].as_string();
		if(wtype == "animation_widget") {
			return WidgetPtr(new AnimationWidget(v,e));
	#ifndef NO_EDITOR
		} else if(wtype == "animation_preview") {
			return WidgetPtr(new AnimationPreviewWidget(v,e));
	#endif
		} else if(wtype == "border_widget") {
			return WidgetPtr(new BorderWidget(v,e));
		} else if(wtype == "button") {
			return WidgetPtr(new Button(v,e));
		} else if(wtype == "checkbox") {
			return WidgetPtr(new Checkbox(v,e));
	} else if(wtype == "combobox" || wtype == "listbox") {
		return widget_ptr(new gui::dropdown_widget(v,e));
		} else if(wtype == "dialog") {
			return WidgetPtr(new Dialog(v,e));
	#ifndef NO_EDITOR
		} else if(wtype == "drag_widget") {
			return WidgetPtr(new DragWidget(v,e));
	#endif
		} else if(wtype == "graphical_font_label") {
			return WidgetPtr(new GraphicalFontLabel(v,e));
		} else if(wtype == "grid") {
			return WidgetPtr(new Grid(v,e));
		} else if(wtype == "image") {
			return WidgetPtr(new ImageWidget(v,e));
		} else if(wtype == "section") {
			return WidgetPtr(new GuiSectionWidget(v,e));
		} else if(wtype == "key_button") {
			return WidgetPtr(new KeyButton(v,e));
		} else if(wtype == "label") {
			return WidgetPtr(new Label(v,e));
		} else if(wtype == "poly_line_widget") {
			return WidgetPtr(new PolyLineWidget(v,e));
		} else if(wtype == "rich_text_label") {
			return WidgetPtr(new RichTextLabel(v,e));
		} else if(wtype == "tileset_preview") {
			return WidgetPtr(new PreviewTilesetWidget(v,e));
		} else if(wtype == "scrollbar") {
			return WidgetPtr(new ScrollBarWidget(v,e));
		} else if(wtype == "slider") {
			return WidgetPtr(new Slider(v,e));
		} else if(wtype == "text_editor") {
			return WidgetPtr(new TextEditorWidget(v,e));
		} else if(wtype == "progress") {
			return WidgetPtr(new ProgressBar(v, e));
		} else if(wtype == "selector") {
			return WidgetPtr(new SelectorWidget(v, e));
		} else if(wtype == "object") {
			return WidgetPtr(new CustomObjectWidget(v, e));
		} else if(wtype == "bar") {
			return WidgetPtr(new BarWidget(v, e));
		} else if(wtype == "color_picker") {
			return WidgetPtr(new ColorPicker(v, e));
		} else if(wtype == "layout") {
			return WidgetPtr(new LayoutWidget(v, e));
		} else if(wtype == "file_chooser") {
			return WidgetPtr(new FileChooserDialog(v, e));
		} else if(wtype == "view3d") {
			return WidgetPtr(new View3DWidget(v, e));
		} else if(wtype == "poly_map") {
			return WidgetPtr(new geometry::PolyMap(v, e));
	#if defined(USE_LIBVPX)
		} else if(wtype == "movie") {
			return WidgetPtr(new movie::vpx(v, e));
	#endif
		//} else if(wtype == "scrollable") {
		//} else if(wtype == "widget") {
		} else if(wtype == "tree") {
			return WidgetPtr(new TreeViewWidget(v, e));
		} else {
			ASSERT_LOG(false, "Unable to create a widget of type " << wtype);
			return WidgetPtr();
		}
	}
}
