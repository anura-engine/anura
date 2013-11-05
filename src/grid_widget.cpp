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
#include <functional>
#include <iostream>
#include <boost/bind.hpp>

#include "controls.hpp"
#include "foreach.hpp"
#include "formula_callable_visitor.hpp"
#include "grid_widget.hpp"
#include "input.hpp"
#include "joystick.hpp"
#include "label.hpp"
#include "raster.hpp"
#include "widget_factory.hpp"

namespace gui {

grid::grid(int ncols)
  : ncols_(ncols), col_widths_(ncols, 0),
    col_aligns_(ncols, grid::ALIGN_LEFT), row_height_(0),
	selected_row_(-1), allow_selection_(false), must_select_(false),
    swallow_clicks_(false), hpad_(0), show_background_(false),
	max_height_(-1), allow_highlight_(true), set_h_(0), set_w_(0),
	default_selection_(-1), draw_selection_highlight_(false)
{
	set_environment();
	set_dim(0,0);
}

grid::grid(const variant& v, game_logic::formula_callable* e)
	: scrollable_widget(v, e), row_height_(v["row_height"].as_int(0)), selected_row_(-1), 
	allow_selection_(false), must_select_(false),
    swallow_clicks_(false), hpad_(0), show_background_(false),
	max_height_(-1), allow_highlight_(true), set_h_(0), set_w_(0),
	default_selection_(v["default_select"].as_int(-1)), 
	draw_selection_highlight_(v["draw_selection_highlighted"].as_bool(false))
{
	ASSERT_LOG(get_environment() != 0, "You must specify a callable environment");
	if(v.has_key("on_select")) {
		const variant on_select_value = v["on_select"];
		if(on_select_value.is_function()) {
			ASSERT_LOG(on_select_value.min_function_arguments() <= 1 && on_select_value.max_function_arguments() >= 1, "on_select grid function should take 1 argument: " << v.debug_location());
			static const variant fml("fn(selection)");
			ffl_on_select_.reset(new game_logic::formula(fml));

			game_logic::map_formula_callable* callable = new game_logic::map_formula_callable;
			callable->add("fn", on_select_value);

			select_arg_.reset(callable);
		} else {
			ffl_on_select_ = get_environment()->create_formula(on_select_value);
		}
		on_select_ = boost::bind(&grid::select_delegate, this, _1);
	}
	if(v.has_key("on_mouseover")) {
		allow_selection_ = true;
		on_mouseover_ = boost::bind(&grid::mouseover_delegate, this, _1);
		const variant on_mouseover_value = v["on_mouseover"];
		if(on_mouseover_value.is_function()) {
			ASSERT_LOG(on_mouseover_value.min_function_arguments() <= 1 && on_mouseover_value.max_function_arguments() >= 1, "on_mouseover grid function should take 1 argument: " << v.debug_location());
			static const variant fml("fn(selection)");
			ffl_on_mouseover_.reset(new game_logic::formula(fml));

			game_logic::map_formula_callable* callable = new game_logic::map_formula_callable;
			callable->add("fn", on_mouseover_value);

			mouseover_arg_.reset(callable);
		} else {
			ffl_on_mouseover_ = get_environment()->create_formula(v["on_mouseover"]);
		}
	}

	ncols_ = v["columns"].as_int(1);
	if(v.has_key("column_widths")) {
		if(v["column_widths"].is_list()) {
			ASSERT_LOG(v["column_widths"].num_elements() == ncols_, "List of column widths must have " << ncols_ << " elements");
			std::vector<int> li = v["column_widths"].as_list_int();
			col_widths_.assign(li.begin(), li.end());
		} else if(v["column_widths"].is_int()) {
			col_widths_.assign(ncols_, v["column_widths"].as_int());
		} else {
			ASSERT_LOG(false, "grid: column_widths must be an int or list of ints");
		}
	} else {
		col_widths_.assign(ncols_, 0);
	}
	col_aligns_.resize(ncols_);
	if(v.has_key("column_alignments")) {
		if(v["column_alignments"].is_list()) {
			// XXX this could be a list of strings as well.
			int col = 0;
			foreach(const variant& c, v["column_alignments"].as_list()) {
				if(c.is_int()) {
					set_align(col, static_cast<COLUMN_ALIGN>(c.as_int()));
				} else if(c.is_string()) {
					const std::string& s = c.as_string();
					if(s == "center" || s == "centre") {
						set_align(col, ALIGN_CENTER);
					} else if(s == "right") {
						set_align(col, ALIGN_RIGHT);
					} else if(s == "left") {
						set_align(col, ALIGN_LEFT);
					} else {
						ASSERT_LOG(false, "grid: column_alignments must be \"left\", \"right\" or \"center\"");
					}
				} else {
					ASSERT_LOG(false, "grid: column alignment members must be an integer or a string.");
				}
				col++;
			}
		} else if(v["column_alignments"].is_int()) {
			col_aligns_.assign(ncols_, static_cast<COLUMN_ALIGN>(v["column_alignments"].as_int()));
		} else if(v["column_alignments"].is_string()) {
			const std::string& s = v["column_alignments"].as_string();
			if(s == "center" || s == "centre") {
				col_aligns_.assign(ncols_, ALIGN_CENTER);
			} else if(s == "right") {
				col_aligns_.assign(ncols_, ALIGN_RIGHT);
			} else if(s == "left") {
				col_aligns_.assign(ncols_, ALIGN_LEFT);
			} else {
				ASSERT_LOG(false, "grid: column_alignments must be \"left\", \"right\" or \"center\"");
			}
		} else {
			ASSERT_LOG(false, "grid: column_alignments must be an int or list of ints");
		}
	} else {
		col_aligns_.assign(ncols_, ALIGN_LEFT);
	}

	allow_selection_ = v["allow_selection"].as_bool(allow_selection_);
	if(v.has_key("must_select")) {
		must_select_ = v["must_select_"].as_bool();
		if(v.has_key("must_select_row")) {
			selected_row_ = v["must_select_row"].as_int();
		}
	}
	if(v.has_key("swallow_clicks")) {
		swallow_clicks_ = v["swallow_clicks"].as_bool();
	}
	if(v.has_key("max_height")) {
		max_height_ = v["max_height"].as_int();
	}
	if(v.has_key("allow_draw_highlight")) {
		allow_highlight_ = v["allow_draw_highlight"].as_bool();
	}
	if(v.has_key("header_rows")) {
		if(v["header_rows"].is_int()) {
			set_header_row(v["header_rows"].as_int());
		} else if(v["header_rows"].is_list()) {
			header_rows_.assign(v["header_rows"].as_list_int().begin(), v["header_rows"].as_list_int().end());
		} else {
			ASSERT_LOG(false, "grid: header_rows must be an int or list of ints");
		}
	}
	if(v.has_key("horizontal_padding")) {
		set_hpad(v["horizontal_padding"].as_int());
	}
	if(v.has_key("show_background")) {
		show_background_ = v["show_background"].as_bool();
	}

	if(v.has_key("children")) {
		// children is a list of lists or a list of single widgets, the outmost list being rows, 
		// the inner list being the columns. 
		reset_contents(v["children"]);
	}

	set_h_ = height();
	set_w_ = width();

	if(v["scroll_to_bottom"].as_bool(false) && virtual_height() > height()) {
		set_yscroll(virtual_height() - height());
	}

	if(on_select_ && default_selection_ >= 0) {
		on_select_(default_selection_);
	}

	if(!ffl_on_select_ && !ffl_on_mouseover_) {
		set_claim_mouse_events(v["claim_mouse_events"].as_bool(false));
	}
}

void grid::set_dim(int w, int h)
{
	widget::set_dim(w,h);
	set_h_ = h;
	set_w_ = w;
}

void grid::handle_process()
{
    foreach(widget_ptr w, cells_) {
		if(w != NULL) {
			w->process();
		}
	}
	widget::handle_process();
}

void grid::add_row(const std::vector<widget_ptr>& widgets)
{
	assert(widgets.size() == ncols_);
	int index = 0;
	foreach(const widget_ptr& widget, widgets) {
		cells_.push_back(widget);

		if(widget && widget->width()+hpad_ > col_widths_[index]) {
			col_widths_[index] = widget->width()+hpad_;
		}

		if(widget && widget->height() > row_height_) {
			row_height_ = widget->height();
		}

		++index;
	}

	recalculate_dimensions();
}

grid& grid::add_col(const std::string& str) {
	return add_col(widget_ptr(new label(str, graphics::color_white())));
}

grid& grid::add_col(const widget_ptr& widget) {
	new_row_.push_back(widget);
	if(new_row_.size() == ncols_) {
		add_row(new_row_);
		new_row_.clear();
	}
	return *this;
}

grid& grid::finish_row()
{
	while(!new_row_.empty()) {
		add_col();
	}

	return *this;
}

grid& grid::set_col_width(int col, int width)
{
	assert(col >= 0 && col < ncols_);
	col_widths_[col] = width;
	recalculate_dimensions();
	return *this;
}

grid& grid::set_align(int col, grid::COLUMN_ALIGN align)
{
	assert(col >= 0 && col < ncols_);
	col_aligns_[col] = align;
	recalculate_dimensions();
	return *this;
}

grid& grid::set_hpad(int pad)
{
	hpad_ = pad;
	return *this;
}

void grid::reset_contents(const variant& v)
{
	cells_.clear();
	if(v.is_null()) {
		return;
	}
	bool check_end = false;
	foreach(const variant& row, v.as_list()) {
		if(row.is_list()) {
			foreach(const variant& col, row.as_list()) {
				add_col(widget_factory::create(col,get_environment()));
			}
			finish_row();
		} else {
			add_col(widget_factory::create(row,get_environment()));
				//.finish_row();
			check_end = true;
		}
	}
	if(check_end && v.num_elements() % ncols_) {
		finish_row();
	}
}

void grid::register_mouseover_callback(grid::callback_type ptr)
{
	on_mouseover_ = ptr;
}

void grid::register_selection_callback(grid::callback_type ptr)
{
	on_select_ = ptr;
}

void grid::register_row_selection_callback(boost::function<void()> ptr)
{
	row_callbacks_.push_back(ptr);
}

int grid::row_at(int xpos, int ypos) const
{
	if(row_height_ == 0) {
		return -1;
	} else if(xpos > x() && xpos < x() + width() &&
	   ypos > y() && ypos < y() + height()) {
		return (ypos + yscroll() - y()) / row_height_;
	} else {
		return -1;
	}
}

void grid::recalculate_dimensions()
{
	visible_cells_.clear();

	int w = 0;
	foreach(int width, col_widths_) {
		w += width;
	}

	int desired_height = row_height_*nrows();
	set_virtual_height(desired_height);
	set_scroll_step(1);
	set_arrow_scroll_step(row_height_);

	if(max_height_ > 0 && desired_height > max_height_) {
		desired_height = max_height_;
	//	while(desired_height%row_height_) {
	//		--desired_height;
	//	}
	}

	if(set_h_ != 0 || set_w_ != 0) {
		widget::set_dim(set_w_ ? set_w_ : w, set_h_ ? set_h_ : desired_height);
	} else {
		widget::set_dim(w, desired_height);
	}

	int y = 0;
	for(int n = 0; n != nrows(); ++n) {
		int x = 0;
		for(int m = 0; m != ncols_; ++m) {
			int align = 0;
			widget_ptr widget = cells_[n*ncols_ + m];
			if(widget) {
				switch(col_aligns_[m]) {
				case ALIGN_LEFT:
					align = 0;
					break;
				case ALIGN_CENTER:
					align = (col_widths_[m] - widget->width())/2;
					break;
				case ALIGN_RIGHT:
					align = col_widths_[m] - widget->width();
					break;
				}

				widget->set_loc(x+align,y+row_height_/2 - widget->height()/2 - yscroll());
				if(widget->y() + widget->height() > 0 && widget->y() < height()) {
					visible_cells_.push_back(widget);
					widget->set_clip_area(rect(0, 0, width(), height()));
				}
				std::sort(visible_cells_.begin(), visible_cells_.end(), widget_sort_zorder());
			}

			x += col_widths_[m];
		}

		y += row_height_;
	}

	update_scrollbar();
}

void grid::visit_values(game_logic::formula_callable_visitor& visitor)
{
	foreach(widget_ptr& cell, cells_) {
		visitor.visit(&cell);
	}
}

void grid::on_set_yscroll(int old_value, int value)
{
	recalculate_dimensions();
}

void grid::handle_draw() const
{
	GLfloat current_color[4];
#if defined(USE_SHADERS)
	memcpy(current_color, gles2::get_color(), sizeof(current_color));
#else
	glGetFloatv(GL_CURRENT_COLOR, current_color);
#endif

	{
	const int xpos = x() & ~1;
	const int ypos = y() & ~1;

	const SDL_Rect grid_rect = {xpos, ypos, width(), height()};
	const graphics::clip_scope clip_scope(grid_rect);

	glPushMatrix();
	glTranslatef(GLfloat(x() & ~1), GLfloat(y() & ~1), 0.0);

	if(show_background_) {
		const SDL_Color bg = {50,50,50};
		SDL_Rect rect = {0,0,width(),height()};
		graphics::draw_rect(rect,bg);
	}

	if(draw_selection_highlight_ && default_selection_ >= 0 && default_selection_ < nrows()) {
		if(std::find(header_rows_.begin(), header_rows_.end(), default_selection_) == header_rows_.end()) {
			SDL_Rect rect = {0,row_height_*default_selection_ - yscroll(),width(),row_height_};
			const SDL_Color col = {0x00,0x00,0xff,0x00};
			graphics::draw_rect(rect,col,128);
		}
	}

	if(allow_highlight_ && selected_row_ >= 0 && selected_row_ < nrows()) {
		if(std::find(header_rows_.begin(), header_rows_.end(), selected_row_) == header_rows_.end()) {
			SDL_Rect rect = {0,row_height_*selected_row_ - yscroll(),width(),row_height_};
			const SDL_Color col = {0xff,0x00,0x00,0x00};
			graphics::draw_rect(rect,col,128);
		}
	}
	glColor4f(current_color[0], current_color[1], current_color[2], current_color[3]);
	foreach(const widget_ptr& widget, visible_cells_) {
		if(widget) {
			widget->draw();
		}
	}
	glPopMatrix();
	} //end of scope so clip_scope goes away.

	scrollable_widget::handle_draw();
}

bool grid::handle_event(const SDL_Event& event, bool claimed)
{
	claimed = scrollable_widget::handle_event(event, claimed);

	SDL_Event ev = event;
	normalize_event(&ev);
	std::vector<widget_ptr> cells = visible_cells_;
	reverse_foreach(widget_ptr widget, cells) {
		if(widget) {
			claimed = widget->process_event(ev, claimed);
		}
	}

	if(!claimed && event.type == SDL_MOUSEWHEEL) {
		int mx, my;
		input::sdl_get_mouse_state(&mx, &my);
		point p(mx, my);
		rect r(x(), y(), width(), height());
		if(point_in_rect(p, r)) {
			if(event.wheel.y > 0) {
				set_yscroll(yscroll() - 3*row_height_ < 0 ? 0 : yscroll() - 3*row_height_);
				if(allow_selection_) {
					selected_row_ -= 3;
					if(selected_row_ < 0) {
						selected_row_ = 0;
					}
				}
			} else {
				int y3 = yscroll() + 3*row_height_;
				set_yscroll(virtual_height() - y3 < height() 
					? virtual_height() - height()
					: y3);
				if(allow_selection_) {
					selected_row_ += 3;
					if(selected_row_ >= nrows()) {
						selected_row_ = nrows() - 1;
					}
				}
			}
			claimed = claim_mouse_events();
		}
	}

	if(!claimed && allow_selection_) {
		if(event.type == SDL_MOUSEMOTION) {
			const SDL_MouseMotionEvent& e = event.motion;
			int new_row = row_at(e.x,e.y);
			if(new_row != selected_row_) {
				selected_row_ = new_row;
				if(on_mouseover_) {
					on_mouseover_(new_row);
				}
			}
		} else if(event.type == SDL_MOUSEBUTTONDOWN) {
			point p(event.button.x, event.button.y);
			rect r(x(), y(), width(), height());
			const SDL_MouseButtonEvent& e = event.button;
			if(e.state == SDL_PRESSED) {
				const int row_index = row_at(e.x, e.y);
				std::cerr << "SELECT ROW: " << row_index << "\n";
				if(row_index >= 0 && row_index < int(row_callbacks_.size()) &&
					row_callbacks_[row_index]) {
					std::cerr << "ROW CB: " << row_index << "\n";
					row_callbacks_[row_index]();
				}

				default_selection_ = row_index;
				if(on_select_) {
					on_select_(row_index);
				}
			}
			if(swallow_clicks_) {
				std::cerr << "SWALLOW CLICK\n";
				claimed = true;
			}
		}
	}

	if(!claimed && must_select_) {
		if(event.type == SDL_KEYDOWN) {
			if(event.key.keysym.sym == SDLK_UP) {
				set_yscroll(yscroll() - row_height_ < 0 ? 0 : yscroll() - row_height_);
				if(selected_row_-- == 0) {
					selected_row_ = nrows()-1;
					set_yscroll(std::min(virtual_height(),row_height_*nrows()) - height());
				}
				claimed = true;
			} else if(event.key.keysym.sym == SDLK_DOWN) {
				int y1 = yscroll() + row_height_;
				set_yscroll(std::min(virtual_height(),row_height_*nrows()) - y1 < height() 
					? std::min(virtual_height(),row_height_*nrows()) - height()
					: y1);
				if(++selected_row_ == nrows()) {
					set_yscroll(0);
					selected_row_ = 0;
				}
				claimed = true;
			} else if(ev.key.keysym.sym == controls::get_keycode(controls::CONTROL_ATTACK) 
				|| ev.key.keysym.sym == controls::get_keycode(controls::CONTROL_JUMP)) {
				if(on_select_) {
					on_select_(selected_row_);
				}
				claimed = true;
			}
		}
	}

	return claimed;
}

bool grid::has_focus() const
{
	foreach(const widget_ptr& w, cells_) {
		if(w && w->has_focus()) {
			return true;
		}
	}

	return false;
}

void grid::select_delegate(int selection)
{
	if(select_arg_) {
		using namespace game_logic;
		map_formula_callable_ptr callable = map_formula_callable_ptr(new map_formula_callable(select_arg_.get()));
		callable->add("selection", variant(selection));
		variant value = ffl_on_select_->execute(*callable);
		get_environment()->execute_command(value);
	} else if(get_environment()) {
		game_logic::map_formula_callable* callable = new game_logic::map_formula_callable(get_environment());
		callable->add("selection", variant(selection));
		variant v(callable);
		variant value = ffl_on_select_->execute(*callable);
		get_environment()->execute_command(value);
	} else {
		std::cerr << "grid::select_delegate() called without environment!" << std::endl;
	}
}

void grid::mouseover_delegate(int selection)
{
	if(mouseover_arg_) {
		using namespace game_logic;
		map_formula_callable_ptr callable = map_formula_callable_ptr(new map_formula_callable(mouseover_arg_.get()));
		callable->add("selection", variant(selection));
		variant value = ffl_on_mouseover_->execute(*callable);
		get_environment()->execute_command(value);
	} else if(get_environment()) {
		game_logic::map_formula_callable* callable = new game_logic::map_formula_callable(get_environment());
		callable->add("selection", variant(selection));
		variant v(callable);
		variant value = ffl_on_mouseover_->execute(*callable);
		get_environment()->execute_command(value);
	} else {
		std::cerr << "grid::mouseover_delegate() called without environment!" << std::endl;
	}
}

const_widget_ptr grid::get_widget_by_id(const std::string& id) const
{
	foreach(widget_ptr w, cells_) {
		if(w) {
			widget_ptr wx = w->get_widget_by_id(id);
			if(wx) {
				return wx;
			}
		}
	}
	return widget::get_widget_by_id(id);
}

widget_ptr grid::get_widget_by_id(const std::string& id)
{
	foreach(widget_ptr w, cells_) {
		if(w) {
			widget_ptr wx = w->get_widget_by_id(id);
			if(wx) {
				return wx;
			}
		}
	}
	return widget::get_widget_by_id(id);
}

std::vector<widget_ptr> grid::get_children() const
{
	return cells_;
}

int show_grid_as_context_menu(grid_ptr grid, widget_ptr draw_widget)
{
	std::vector<widget_ptr> v;
	v.push_back(draw_widget);
	return show_grid_as_context_menu(grid, v);
}

int show_grid_as_context_menu(grid_ptr grid, const std::vector<widget_ptr> draw_widgets)
{
	grid->set_show_background(true);
	grid->allow_selection();
	grid->swallow_clicks();
	int result = -1;
	bool quit = false;
	grid->register_selection_callback(
	  [&](int nitem) {
		result = nitem;
		quit = true;
	  }
	);

	int mousex = 0, mousey = 0;
	input::sdl_get_mouse_state(&mousex, &mousey);

	const int max_x = graphics::screen_width() - grid->width() - 6;
	const int max_y = graphics::screen_height() - grid->height() - 6;

	grid->set_loc(std::min<int>(max_x, mousex), std::min<int>(max_y, mousey));

	while(!quit) {
		SDL_Event event;
		while(input::sdl_poll_event(&event)) {
			bool claimed = grid->process_event(event, false);

			if(claimed) {
				continue;
			}

			switch(event.type) {
			case SDL_KEYDOWN:
				if(event.key.keysym.sym != SDLK_ESCAPE) {
					break;
				}
			case SDL_MOUSEBUTTONDOWN:
			case SDL_QUIT:
				quit = true;
				break;
			}
		}

		get_main_window()->prepare_raster();
		for(widget_ptr w : draw_widgets) {
			w->draw();
		}

		grid->draw();

		gui::draw_tooltip();

		get_main_window()->swap();

		SDL_Delay(20);
	}

	return result;
}

BEGIN_DEFINE_CALLABLE(grid, widget)
	DEFINE_FIELD(children, "[widget]")
		std::vector<variant> v;
	    foreach(widget_ptr w, obj.cells_) {
			v.push_back(variant(w.get()));
		}
		return variant(&v);
	DEFINE_SET_FIELD_TYPE("list")
		obj.reset_contents(value);
		obj.finish_row();
		obj.recalculate_dimensions();
	DEFINE_FIELD(child, "null")
		return variant();
	DEFINE_SET_FIELD_TYPE("map")
		obj.add_col(widget_factory::create(value, obj.get_environment())).finish_row();
		obj.recalculate_dimensions();
	DEFINE_FIELD(selected_row, "int")
		return variant(obj.selected_row_);
END_DEFINE_CALLABLE(grid)

}
