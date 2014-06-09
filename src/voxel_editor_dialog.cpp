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
#ifndef NO_EDITOR
#if defined(USE_ISOMAP)
#include <boost/bind.hpp>
#include <boost/function.hpp>

#include <iostream>

#include "base64.hpp"
#include "border_widget.hpp"
#include "button.hpp"
#include "compress.hpp"
#include "editor.hpp"
#include "foreach.hpp"
#include "grid_widget.hpp"
#include "image_widget.hpp"
#include "input.hpp"
#include "isochunk.hpp"
#include "label.hpp"
#include "preview_tileset_widget.hpp"
#include "raster.hpp"
#include "text_editor_widget.hpp"
#include "variant_utils.hpp"
#include "voxel_editor_dialog.hpp"

namespace editor_dialogs
{

	namespace 
	{
		std::set<voxel_editor_dialog*>& all_tileset_editor_dialogs() {
			static std::set<voxel_editor_dialog*> all;
			return all;
		}
	}

	void voxel_editor_dialog::global_tile_update()
	{
		for(auto d : all_tileset_editor_dialogs()) {
			d->init();
		}
	}

	voxel_editor_dialog::voxel_editor_dialog(editor& e)
	  : dialog(graphics::screen_width() - EDITOR_SIDEBAR_WIDTH, 160, EDITOR_SIDEBAR_WIDTH, graphics::screen_height()-160), 
	  editor_(e), 
	  first_index_(-1),
	  map_width_(16),
	  map_depth_(16),
	  map_height_(4),
	  textured_mode_(true)
	{
		all_tileset_editor_dialogs().insert(this);

		set_clear_bg_amount(255);
	
		if(voxel::chunk::getTexturedEditorTiles().empty() == false) {
			category_ = voxel::chunk::getTexturedEditorTiles().front().group;
		}

		if(level::current().iso_world()) {
			textured_mode_ = false;
		}

		// The color picker contains state, resetting it all the time loses that state.
		rect area(0, 0, EDITOR_SIDEBAR_WIDTH, 220);
		ColorPicker_ = new gui::ColorPicker(area);
		ColorPicker_->setPrimaryColor(graphics::color("lawn_green"));

		init();
	}

	voxel_editor_dialog::~voxel_editor_dialog()
	{
		all_tileset_editor_dialogs().erase(this);
	}

	void voxel_editor_dialog::init()
	{
		clear();
		using namespace gui;
		setPadding(20);

		ASSERT_LOG(editor_.get_voxel_tileset() >= 0 
			&& size_t(editor_.get_voxel_tileset()) < voxel::chunk::getTexturedEditorTiles().size(),
			"Index of isometric tileset out of bounds must be between 0 and " 
			<< voxel::chunk::getTexturedEditorTiles().size() << ", found " << editor_.get_voxel_tileset());

		std::stringstream str;

		grid_ptr grid(new gui::grid(6));
		grid->set_hpad(5);

		grid->add_col(new label("W: "));
		grid->add_col(new button("-10", boost::bind(&voxel_editor_dialog::decrement_width, this, 10)));
		grid->add_col(new button("-", boost::bind(&voxel_editor_dialog::decrement_width, this, 1)));
		str << map_width_;
		grid->add_col(new label(str.str()));
		grid->add_col(new button("+", boost::bind(&voxel_editor_dialog::increment_width, this, 1)));
		grid->add_col(new button("+10", boost::bind(&voxel_editor_dialog::increment_width, this, 10)));

		grid->add_col(new label("D: "));
		grid->add_col(new button("-10", boost::bind(&voxel_editor_dialog::decrement_depth, this, 10)));
		grid->add_col(new button("-", boost::bind(&voxel_editor_dialog::decrement_depth, this, 1)));
		str.str("");
		str << map_depth_;
		grid->add_col(new label(str.str()));
		grid->add_col(new button("+", boost::bind(&voxel_editor_dialog::increment_depth, this, 1)));
		grid->add_col(new button("+10", boost::bind(&voxel_editor_dialog::increment_depth, this, 10)));

		grid->add_col(new label("H: "));
		grid->add_col(new button("-10", boost::bind(&voxel_editor_dialog::decrement_height, this, 10)));
		grid->add_col(new button("-", boost::bind(&voxel_editor_dialog::decrement_height, this, 1)));
		str.str("");
		str << map_height_;
		grid->add_col(new label(str.str()));
		grid->add_col(new button("+", boost::bind(&voxel_editor_dialog::increment_height, this, 1)));
		grid->add_col(new button("+10", boost::bind(&voxel_editor_dialog::increment_height, this, 10)));

		add_widget(grid, 10, 10);
		int next_height = grid->x() + grid->height() + 5;

		button* random_landscape = new button(WidgetPtr(new label("Random", graphics::color_white())), boost::bind(&voxel_editor_dialog::random_isomap, this));
		button* flat_landscape = new button(WidgetPtr(new label("Flat", graphics::color_white())), boost::bind(&voxel_editor_dialog::flat_plane_isomap, this));
		mode_swap_button_.reset(new button(WidgetPtr(new label(textured_mode_ ? "Textured" : "Colored", graphics::color_white())), boost::bind(&voxel_editor_dialog::swap_mode, this)));
		grid.reset(new gui::grid(2));
		grid->set_hpad(10);
		grid->add_col(random_landscape);
		grid->add_col(flat_landscape);
		grid->add_col(mode_swap_button_).finish_row();
		add_widget(grid, 10, next_height);

		if(textured_mode_) {
			button* category_button = new button(WidgetPtr(new label("Category: " + category_, graphics::color_white())), boost::bind(&voxel_editor_dialog::show_category_menu, this));
			add_widget(category_button, 10, grid->y() + grid->height() + 5);

			grid.reset(new gui::grid(3));
			int index = 0, first_index = -1;
			first_index_ = -1;
	
			foreach(const voxel::TexturedTileEditorInfo& t, voxel::chunk::getTexturedEditorTiles()) {
				if(t.group == category_) {
					if(first_index_ == -1) {
						first_index_ = index;
					}
					ImageWidget* preview = new ImageWidget(t.tex, 54, 54);
					preview->setArea(t.area);
					ButtonPtr tileset_button(new button(WidgetPtr(preview), boost::bind(&voxel_editor_dialog::set_tileset, this, index)));
					tileset_button->setTooltip(t.name + "(" + t.id.as_string() + ")", 14);
					tileset_button->setDim(58, 58);
					grid->add_col(gui::WidgetPtr(new gui::border_widget(tileset_button, index == editor_.get_voxel_tileset() ? graphics::color(255,255,255,255) : graphics::color(0,0,0,0))));
				}
				++index;
			}

			grid->finish_row();
			add_widget(grid);
		} else {
			/*button* category_button = new button(WidgetPtr(new label("Category: " + category_, graphics::color_white())), boost::bind(&voxel_editor_dialog::show_category_menu, this));
			add_widget(category_button, 10, grid->y() + grid->height() + 5);

			grid.reset(new gui::grid(3));
			int index = 0, first_index = -1;
			first_index_ = -1;
	
			foreach(const voxel::ColoredTileEditorInfo& t, voxel::chunk::getColoredEditorTiles()) {
				if(t.group == category_) {
					if(first_index_ == -1) {
						first_index_ = index;
					}
					ImageWidget* preview = new ImageWidget(t.tex, 54, 54);
					preview->setArea(t.area);
					ButtonPtr tileset_button(new button(WidgetPtr(preview), boost::bind(&voxel_editor_dialog::set_tileset, this, index)));
					tileset_button->setTooltip(t.name + "(" + t.id.as_string() + ")", 14);
					tileset_button->setDim(58, 58);
					grid->add_col(gui::WidgetPtr(new gui::border_widget(tileset_button, index == editor_.get_voxel_tileset() ? graphics::color(255,255,255,255) : graphics::color(0,0,0,0))));
				}
				++index;
			}

			grid->finish_row();
			add_widget(grid);*/
			add_widget(ColorPicker_);
		}
	}

	void voxel_editor_dialog::swap_mode()
	{
		textured_mode_ = !textured_mode_;
		init();
	}

	void voxel_editor_dialog::select_category(const std::string& category)
	{
		category_ = category;
		init();

		if(first_index_ != -1) {
			set_tileset(first_index_);
		}
	}

	void voxel_editor_dialog::close_context_menu(int index)
	{
		remove_widget(context_menu_);
		context_menu_.reset();
	}

	void voxel_editor_dialog::increment_width(int n)
	{
		map_width_ += n;
		if(map_width_ > 1024) { 
			map_width_ = 1024;
		}
		init();
	}

	void voxel_editor_dialog::decrement_width(int n)
	{
		if(n >= map_width_ || map_width_ - n < 2) { 
			map_width_ = 1;
		} else {
			map_width_ -= n;
		}
		init();
	}

	void voxel_editor_dialog::increment_depth(int n)
	{
		map_depth_ += n;
		if(map_depth_ > 1024) { 
			map_depth_ = 1024;
		}
		init();
	}

	void voxel_editor_dialog::decrement_depth(int n)
	{
		if(n >= map_depth_ || map_depth_ - n < 2) { 
			map_depth_ = 1;
		} else {
			map_depth_ -= n;
		}
		init();
	}

	void voxel_editor_dialog::increment_height(int n)
	{
		map_height_ += n;
		if(map_height_ > 1024) { 
			map_height_ = 1024;
		}
		init();
	}

	void voxel_editor_dialog::decrement_height(int n)
	{
		if(n >= map_height_ || map_height_ - n < 2) { 
			map_height_ = 1;
		} else {
			map_height_ -= n;
		}
		init();
	}

	void voxel_editor_dialog::show_category_menu()
	{
		using namespace gui;
		gui::grid* grid = new gui::grid(2);
		grid->setZOrder(100);
		grid->swallow_clicks();
		grid->set_show_background(true);
		grid->set_hpad(10);
		grid->allow_selection();
		grid->register_selection_callback(boost::bind(&voxel_editor_dialog::close_context_menu, this, _1));

		std::set<std::string> categories;
		foreach(const voxel::TexturedTileEditorInfo& t, voxel::chunk::getTexturedEditorTiles()) {
			if(categories.count(t.group)) {
				continue;
			}

			categories.insert(t.group);

			ImageWidget* preview = new ImageWidget(t.tex, 54, 54);
			preview->setArea(t.area);
			grid->add_col(WidgetPtr(preview))
				 .add_col(WidgetPtr(new label(t.group, graphics::color_white())));
			grid->register_row_selection_callback(boost::bind(&voxel_editor_dialog::select_category, this, t.group));
		}

		int mousex, mousey;
		input::sdl_get_mouse_state(&mousex, &mousey);
		if(mousex + grid->width() > graphics::screen_width()) {
			mousex = graphics::screen_width() - grid->width();
		}

		if(mousey + grid->height() > graphics::screen_height()) {
			mousey = graphics::screen_height() - grid->height();
		}

		mousex -= x();
		mousey -= y();

		remove_widget(context_menu_);
		context_menu_.reset(grid);
		add_widget(context_menu_, mousex, mousey);
	}

	void voxel_editor_dialog::set_tileset(int index)
	{
		if(editor_.get_voxel_tileset() != index) {
			editor_.set_voxel_tileset(index);
			init();
		}
	}

	bool voxel_editor_dialog::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(!claimed) {
			if(context_menu_) {
				gui::WidgetPtr ptr = context_menu_;
				SDL_Event ev = event;
				normalizeEvent(&ev);
				return ptr->processEvent(ev, claimed);
			}

			switch(event.type) {
			case SDL_KEYDOWN:
				if(event.key.keysym.sym == SDLK_COMMA) {
					editor_.set_voxel_tileset(editor_.get_voxel_tileset()-1);
					while(voxel::chunk::getTexturedEditorTiles()[editor_.get_voxel_tileset()].group != category_) {
						editor_.set_voxel_tileset(editor_.get_voxel_tileset()-1);
					}
					set_tileset(editor_.get_voxel_tileset());
					claimed = true;
				} else if(event.key.keysym.sym == SDLK_PERIOD) {
					editor_.set_voxel_tileset(editor_.get_voxel_tileset()+1);
					while(voxel::chunk::getTexturedEditorTiles()[editor_.get_voxel_tileset()].group != category_) {
						editor_.set_voxel_tileset(editor_.get_voxel_tileset()+1);
					}
					set_tileset(editor_.get_voxel_tileset());
					claimed = true;
				}
				break;
			}
		}

		return dialog::handleEvent(event, claimed);
	}

	void voxel_editor_dialog::random_isomap()
	{
		variant tile_to_add;

		int tileset = editor_.get_voxel_tileset();
		if(textured_mode_) {
			if(tileset < 0 || size_t(tileset) >= voxel::chunk::getTexturedEditorTiles().size()) {
				return;
			}
			tile_to_add = voxel::chunk::getTexturedEditorTiles()[editor_.get_voxel_tileset()].id;
		} else {
			tile_to_add = ColorPicker_->getSelectedColor().write();
		}

		variant_builder res;
		variant_builder iso;
		iso.add("width", map_width_);
		iso.add("height", map_height_);
		iso.add("depth", map_depth_);
		iso.add("seed", int(time(NULL)));
		iso.add("type", tile_to_add);

		if(textured_mode_) {
			res.add("type", "textured");
			res.add("shader", "lighted_texture_shader");
		} else {
			res.add("type", "colored");
			res.add("shader", "lighted_color_shader");
		}
		res.add("random", iso.build());

		//level::current().isomap() = voxel::chunk_factory::create(res.build());
	}

	void voxel_editor_dialog::flat_plane_isomap()
	{
		variant tile_to_add;

		int tileset = editor_.get_voxel_tileset();
		if(textured_mode_) {
			if(tileset < 0 || size_t(tileset) >= voxel::chunk::getTexturedEditorTiles().size()) {
				return;
			}
			tile_to_add = voxel::chunk::getTexturedEditorTiles()[editor_.get_voxel_tileset()].id;
		} else {
			tile_to_add = ColorPicker_->getSelectedColor().write();
		}

		variant_builder res;
		const int xmax = map_width_;
		const int zmax = map_depth_;
		const int ymax = map_height_;

		std::map<variant,variant> voxels;
		for(int x = 0; x != xmax; ++x) {
			for(int z = 0; z != zmax; ++z) {
				for(int y = 0; y != ymax; ++y) {
					std::vector<variant> loc;
					loc.push_back(variant(x));
					loc.push_back(variant(y));
					loc.push_back(variant(z));
					voxels[variant(&loc)] = tile_to_add;
				}
			}
		}

		if(textured_mode_) {
			res.add("type", "textured");
			res.add("shader", "lighted_texture_shader");
		} else {
			res.add("type", "colored");
			res.add("shader", "lighted_color_shader");
		}

		std::string s = variant(&voxels).write_json();
		std::vector<char> enc = base64::b64encode(zip::compress(std::vector<char>(s.begin(), s.end())));
		res.add("voxels", std::string(enc.begin(), enc.end()));
	
		//level::current().isomap() = voxel::chunk_factory::create(res.build());
	}

	graphics::color voxel_editor_dialog::selected_color() const
	{
		if(ColorPicker_) {
			return ColorPicker_->getSelectedColor();
		}
		return graphics::color(255,255,255,255);
	}

}

#endif
#endif // !NO_EDITOR
