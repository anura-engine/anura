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
#pragma once
#ifndef NO_EDITOR
#if defined(USE_ISOMAP)

#include <boost/shared_ptr.hpp>

#include "color_picker.hpp"
#include "dialog.hpp"
#include "isochunk.hpp"
#include "widget.hpp"

class editor;

namespace editor_dialogs
{
	class voxel_editor_dialog : public gui::dialog
	{
	public:
		static void global_tile_update();
		explicit voxel_editor_dialog(editor& e);
		virtual ~voxel_editor_dialog();
	
		void init();
		void select_category(const std::string& category);
		void set_tileset(int index);

		bool textured_mode() const { return textured_mode_; }
		graphics::color selected_color() const;
	protected:
		void increment_width(int n);
		void decrement_width(int n);
		void increment_depth(int n);
		void decrement_depth(int n);
		void increment_height(int n);
		void decrement_height(int n);
	private:
		voxel_editor_dialog(const voxel_editor_dialog&);

		void close_context_menu(int index);
		void show_category_menu();

		void random_isomap();
		void flat_plane_isomap();

		bool handle_event(const SDL_Event& event, bool claimed);
		editor& editor_;

		gui::widget_ptr context_menu_;
		std::string category_;

		bool textured_mode_;
		gui::widget_ptr mode_swap_button_;
		void swap_mode();

		gui::color_picker_ptr color_picker_;		

		//index of the first item in the current category
		int first_index_;

		size_t map_width_;
		size_t map_depth_;
		size_t map_height_;
	};
}

#endif
#endif // !NO_EDITOR
