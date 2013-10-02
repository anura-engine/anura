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
#include <iostream>

#include "button.hpp"
#include "color_picker.hpp"
#include "dropdown_widget.hpp"
#include "font.hpp"
#include "grid_widget.hpp"
#include "i18n.hpp"
#include "input.hpp"
#include "label.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "slider.hpp"
#include "text_editor_widget.hpp"
#include "widget_settings_dialog.hpp"

namespace gui {

namespace {
	const int default_font_size = 14;
}

label::label(const std::string& text, int size, const std::string& font)
	: text_(i18n::tr(text)), border_size_(0), size_(size), down_(false),
	  fixed_width_(false), highlight_color_(graphics::color_red()),
	  highlight_on_mouseover_(false), draw_highlight_(false), font_(font)
{
	set_environment();
	color_.r = color_.g = color_.b = 255;
	recalculate_texture();
}

label::label(const std::string& text, const SDL_Color& color, int size, const std::string& font)
	: text_(i18n::tr(text)), color_(color), border_size_(0),
	  size_(size), down_(false), font_(font),
	  fixed_width_(false), highlight_color_(graphics::color_red()),
	  highlight_on_mouseover_(false), draw_highlight_(false)
{
	set_environment();
	recalculate_texture();
}

label::label(const variant& v, game_logic::formula_callable* e)
	: widget(v,e), fixed_width_(false), down_(false), 
	highlight_color_(graphics::color_red()), draw_highlight_(false),
	font_(v["font"].as_string_default())
{
	text_ = i18n::tr(v["text"].as_string());
	color_ = v.has_key("color") 
		? graphics::color(v["color"]).as_sdl_color() 
		: graphics::color(255,255,255,255).as_sdl_color();
	
	if(v.has_key("border_color")) {
		border_color_.reset(new SDL_Color(graphics::color(v["border_color"]).as_sdl_color()));
		if(v.has_key("border_size")) {
			border_size_ = v["border_size"].as_int();
		} else {
			border_size_ = 2;
		}
	}

	size_ = v.has_key("size") ? v["size"].as_int() : default_font_size;
	if(v.has_key("on_click")) {
		ASSERT_LOG(get_environment() != 0, "You must specify a callable environment");
		ffl_click_handler_ = get_environment()->create_formula(v["on_click"]);
		on_click_ = boost::bind(&label::click_delegate, this);
	}
	if(v.has_key("highlight_color")) {
		highlight_color_ = graphics::color(v["highlight_color"]).as_sdl_color();
	}
	highlight_on_mouseover_ = v["highlight_on_mouseover"].as_bool(false);
	set_claim_mouse_events(v["claim_mouse_events"].as_bool(false));
	recalculate_texture();
}

void label::click_delegate()
{
	if(get_environment()) {
		variant value = ffl_click_handler_->execute(*get_environment());
		get_environment()->execute_command(value);
	} else {
		std::cerr << "label::click() called without environment!" << std::endl;
	}
}

void label::set_color(const SDL_Color& color)
{
	color_ = color;
	recalculate_texture();
}

void label::set_font_size(int size)
{
	size_ = size;
	recalculate_texture();
}

void label::set_font(const std::string& font)
{
	font_ = font;
	recalculate_texture();
}

void label::set_text(const std::string& text)
{
	text_ = i18n::tr(text);
	reformat_text();
	recalculate_texture();
}

std::string& label::current_text() {
	if(fixed_width_) {
		return formatted_;
	}
	return text_;
}

const std::string& label::current_text() const {
	if(fixed_width_) {
		return formatted_;
	}
	return text_;
}

void label::set_fixed_width(bool fixed_width)
{
	fixed_width_ = fixed_width;
	reformat_text();
	recalculate_texture();
}

void label::set_dim(int w, int h) {
	if(w != width() || h != height()) {
		inner_set_dim(w, h);
		reformat_text();
		recalculate_texture();
	}
}

void label::inner_set_dim(int w, int h) {
	widget::set_dim(w, h);
}

void label::reformat_text()
{
	if(fixed_width_) {
		formatted_ = text_;
	}
}

void label::recalculate_texture()
{
	texture_ = font::render_text(current_text(), color_, size_, font_);
	inner_set_dim(texture_.width(),texture_.height());

	if(border_color_.get()) {
		border_texture_ = font::render_text(current_text(), *border_color_, size_, font_);
	}
}

void label::handle_draw() const
{
	if(draw_highlight_) {
		SDL_Rect rect = {x(), y(), width(), height()};
#if SDL_VERSION_ATLEAST(2, 0, 0)
		graphics::draw_rect(rect, highlight_color_, highlight_color_.a);
#else
		graphics::draw_rect(rect, highlight_color_, highlight_color_.unused);
#endif
	}

	if(border_texture_.valid()) {
		graphics::blit_texture(border_texture_, x() - border_size_, y());
		graphics::blit_texture(border_texture_, x() + border_size_, y());
		graphics::blit_texture(border_texture_, x(), y() - border_size_);
		graphics::blit_texture(border_texture_, x(), y() + border_size_);
	}
	graphics::blit_texture(texture_, x(), y());
}

void label::set_texture(graphics::texture t) {
	texture_ = t;
}

bool label::in_label(int xloc, int yloc) const
{
	return xloc > x() && xloc < x() + width() &&
	       yloc > y() && yloc < y() + height();
}

bool label::handle_event(const SDL_Event& event, bool claimed)
{
	if(!on_click_ && !highlight_on_mouseover_) {
		return claimed;
	}
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if((event.type == SDL_MOUSEWHEEL) 
#else
	if((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) 
		&& (event.button.button == SDL_BUTTON_WHEELUP || event.button.button == SDL_BUTTON_WHEELDOWN)
#endif
		&& in_label(event.button.x, event.button.y)) {
		// skip processing if mousewheel event
		return claimed;
	}

	if(event.type == SDL_MOUSEMOTION) {
		const SDL_MouseMotionEvent& e = event.motion;
		if(highlight_on_mouseover_) {
			if(in_label(e.x,e.y)) {
				draw_highlight_ = true;
			} else {
				draw_highlight_ = false;
			}
			claimed = claim_mouse_events();
		}
	} else if(event.type == SDL_MOUSEBUTTONDOWN) {
		const SDL_MouseButtonEvent& e = event.button;
		if(in_label(e.x,e.y)) {
			down_ = true;
			claimed = claim_mouse_events();
		}
	} else if(event.type == SDL_MOUSEBUTTONUP) {
		down_ = false;
		const SDL_MouseButtonEvent& e = event.button;
		if(in_label(e.x,e.y)) {
			if(on_click_) {
				on_click_();
			}
			claimed = claim_mouse_events();
		}
	}
	return claimed;
}

variant label::handle_write()
{
	variant_builder res;
	res.add("type", "label");
	res.add("text", text());
	if(color_.r != 255 || color_.g != 255 || color_.b != 255 || color_.a != 255) {
		res.add("color", graphics::color(color_).write());
	}
	if(size() != default_font_size) {
		res.add("size", size());
	}
	if(font().empty() == false) {
		res.add("font", font());
	}
	if(border_color_) {
		res.add("border_color", graphics::color(*border_color_).write());
		if(border_size_ != 2) {
			res.add("border_size", border_size_);
		}
	}
	if(highlight_on_mouseover_) {
		res.add("highlight_on_mouseover", true);
	}
	if(claim_mouse_events()) {
		res.add("claim_mouse_events", true);
	}
	return res.build();
}

widget_settings_dialog* label::settings_dialog(int x, int y, int w, int h)
{
	widget_settings_dialog* d = widget::settings_dialog(x,y,w,h);

	grid_ptr g(new grid(2));

	text_editor_widget_ptr text_edit = new text_editor_widget(150, 30);
	text_edit->set_text(text());
	text_edit->set_on_user_change_handler([=](){set_text(text_edit->text());});
	g->add_col(new label("Text:", d->text_size(), d->font()))
		.add_col(text_edit);

	g->add_col(new label("Size:", d->text_size(), d->font())).
		add_col(new slider(120, [&](double f){set_font_size(int(f*72.0+6.0));}, (size()-6.0)/72.0, 1));

	std::vector<std::string> fonts = font::get_available_fonts();
	fonts.insert(fonts.begin(), "");
	dropdown_widget_ptr font_list(new dropdown_widget(fonts, 150, 28, dropdown_widget::DROPDOWN_LIST));
	font_list->set_font_size(14);
	font_list->set_dropdown_height(height());
	auto fit = std::find(fonts.begin(), fonts.end(), font());
	font_list->set_selection(fit == fonts.end() ? 0 : fit-fonts.begin());
	font_list->set_on_select_handler([&](int n, const std::string& s){set_font(s);});
	font_list->set_zorder(19);
	g->add_col(new label("Font:", d->text_size(), d->font()))
		.add_col(font_list);
	g->add_col(new label("Color:", d->text_size(), d->font()))
		.add_col(new button(new label("Choose...", d->text_size(), d->font()), [&](){
			int mx, my;
			input::sdl_get_mouse_state(&mx, &my);
			mx = mx + 200 > preferences::actual_screen_width() ? preferences::actual_screen_width()-200 : mx;
			my = my + 600 > preferences::actual_screen_height() ? preferences::actual_screen_height()-600 : my;
			my -= d->y();
			color_picker* cp = new color_picker(rect(0, 0, 200, 600), [&](const graphics::color& color){set_color(color.as_sdl_color());});
			cp->set_primary_color(graphics::color(color_));

			grid_ptr gg = new grid(1);
			gg->allow_selection();
			gg->swallow_clicks();
			gg->set_show_background(true);
			gg->allow_draw_highlight(false);
			gg->register_selection_callback([=](int n){if(n != 0){d->remove_widget(gg); d->init();}});
			gg->set_zorder(100);
			gg->add_col(cp);
			d->add_widget(gg, d->x()-mx-100, my);
	}));

	d->add_widget(g);
	return d;
}


BEGIN_DEFINE_CALLABLE(label, widget)
	DEFINE_FIELD(text, "string")
		return variant(obj.text_);
	DEFINE_SET_FIELD
		if(value.is_null()) {
			obj.set_text("");
		} else {
			obj.set_text(value.as_string());
		}
	DEFINE_FIELD(size, "int")
		return variant(obj.size_);
	DEFINE_SET_FIELD
		obj.set_font_size(value.as_int());
	DEFINE_FIELD(font, "string")
		return variant(obj.font_);
	DEFINE_SET_FIELD
		obj.set_font(value.as_string());
	DEFINE_FIELD(color, "string")
		return variant();
	DEFINE_SET_FIELD
		obj.set_color(graphics::color(value).as_sdl_color());
END_DEFINE_CALLABLE(label)

dialog_label::dialog_label(const std::string& text, const SDL_Color& color, int size)
	: label(text, color, size), progress_(0) {

	recalculate_texture();
}

dialog_label::dialog_label(const variant& v, game_logic::formula_callable* e)
	: label(v, e), progress_(0)
{
	recalculate_texture();
}

void dialog_label::set_progress(int progress)
{
	progress_ = progress;
	recalculate_texture();
}

void dialog_label::recalculate_texture()
{
	label::recalculate_texture();
	stages_ = current_text().size();
	int prog = progress_;
	if(prog < 0) prog = 0;
	if(prog > stages_) prog = stages_;
	std::string txt = current_text().substr(0, prog);

	if(prog > 0) {
		set_texture(font::render_text(txt, color(), size(), font()));
	} else {
		set_texture(graphics::texture());
	}
}

void dialog_label::set_value(const std::string& key, const variant& v)
{
	if(key == "progress") {
		set_progress(v.as_int());
	}
	label::set_value(key, v);
}

variant dialog_label::get_value(const std::string& key) const
{
	if(key == "progress") {
		return variant(progress_);
	}
	return label::get_value(key);
}

label_factory::label_factory(const SDL_Color& color, int size)
   : color_(color), size_(size)
{}

label_ptr label_factory::create(const std::string& text) const
{
	return label_ptr(new label(text,color_,size_));
}

label_ptr label_factory::create(const std::string& text,
                                const std::string& tip) const
{
	const label_ptr res(create(text));
	res->set_tooltip(tip);
	return res;
}

}
