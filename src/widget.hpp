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
#ifndef WIDGET_HPP_INCLUDED
#define WIDGET_HPP_INCLUDED

#include <string>
#include <boost/function.hpp>

#include "formula.hpp"
#include "graphics.hpp"
#include "input.hpp"
#include "tooltip.hpp"
#include "framed_gui_element.hpp"
#include "variant_utils.hpp"

namespace game_logic {
class formula_callable_visitor;
}

namespace gui {

class widget;

typedef boost::intrusive_ptr<widget> widget_ptr;
typedef boost::intrusive_ptr<const widget> const_widget_ptr;

struct color_save_context
{
	color_save_context()
	{
#if defined(USE_GLES2)
		memcpy(current_color, gles2::get_color(), sizeof(current_color));
#else
		glGetFloatv(GL_CURRENT_COLOR, current_color);
#endif
	}
	~color_save_context()
	{
		glColor4f(current_color[0], current_color[1], current_color[2], current_color[3]);
	}
	GLfloat current_color[4];
};

class widget_settings_dialog;
class dialog;
typedef boost::intrusive_ptr<dialog> dialog_ptr;

class widget : public virtual input::listener
{
public:
	enum HORIZONTAL_ALIGN {HALIGN_LEFT, HALIGN_CENTER, HALIGN_RIGHT};
	enum VERTICAL_ALIGN   {VALIGN_TOP,  VALIGN_CENTER, VALIGN_BOTTOM};

	bool process_event(const SDL_Event& event, bool claimed);
	void draw() const;

	virtual void set_loc(int x, int y) { true_x_ = x_ = x; true_y_ = y_ = y; recalc_loc(); }
	virtual void set_dim(int w, int h) { w_ = w; h_ = h; recalc_loc(); }

	int x() const;
	int y() const;
	int width() const;
	int height() const;
	void set_tooltip(const std::string& str, int fontsize=18, const SDL_Color& color=graphics::color_yellow(), const std::string& font="");
	void set_tooltip_text(const std::string& str);
	void set_tooltip_fontsize(int fontsize);
	void set_tooltip_color(const graphics::color& color);
	void set_tooltip_font(const std::string& font);
	std::string tooltip_text() const { return tooltip_text_; }
	int tooltip_fontsize() const { return tooltip_fontsize_; }
	std::string tooltip_font() const { return tooltip_font_; }
	SDL_Color tooltip_color() const { return tooltip_color_; }
	bool visible() { return visible_; }
	void set_visible(bool visible) { visible_ = visible; }
	void set_id(const std::string& new_id) { id_ = new_id; }
	std::string id() const { return id_; }
	bool disabled() const { return disabled_; }
	void enable(bool val=true) { disabled_ = val; }
	bool claim_mouse_events() const { return claim_mouse_events_; }
	void set_claim_mouse_events(bool claim=true) { claim_mouse_events_ = claim; }

	int disabled_opacity() const { return disabled_opacity_; }
	void set_disabled_opacity(int n) { disabled_opacity_ = std::min(255, std::max(n, 0)); }

	bool draw_with_object_shader() const { return draw_with_object_shader_; }
	void set_draw_with_object_shader(bool dwos=true) { draw_with_object_shader_ = dwos; }

	unsigned get_tooltip_delay() const { return tooltip_display_delay_; }
	void set_tooltip_delay(unsigned tooltip_delay) { tooltip_display_delay_ = tooltip_delay; }

	virtual widget_ptr get_widget_by_id(const std::string& id);
	virtual const_widget_ptr get_widget_by_id(const std::string& id) const;

	virtual void handle_draw() const = 0;

	virtual bool has_focus() const { return false; }
	game_logic::formula_callable* get_environment() const { return environ_; }

	void set_zorder(int z) { zorder_ = z; }
	int zorder() const { return zorder_; }

	int get_frame_resolution() const { return resolution_; }
	void set_frame_resolution(int r) { resolution_ = r; }
	void set_frame_set(const std::string& frame);
	std::string frame_set_name() const { return frame_set_name_; }

	int get_alpha() const { return display_alpha_; }
	void set_alpha(int a=256) { display_alpha_ = a; }

	int get_pad_width() const { return pad_w_; }
	int get_pad_height() const { return pad_h_; }
	void set_padding(int pw, int ph) { pad_w_ = pw; pad_h_ = ph; }

	HORIZONTAL_ALIGN halign() const { return align_h_; }
	VERTICAL_ALIGN valign() const { return align_v_; }
	void set_halign(HORIZONTAL_ALIGN h) { align_h_ = h; recalc_loc(); }
	void set_valign(VERTICAL_ALIGN v) { align_v_ = v; recalc_loc(); }

	virtual std::vector<widget_ptr> get_children() const { return std::vector<widget_ptr>(); }

	void process();

	void perform_visit_values(game_logic::formula_callable_visitor& visitor) {
		visit_values(visitor);
	}

	dialog_ptr get_settings_dialog(int x, int y, int w, int h);
	variant write();
protected:
	widget();
	explicit widget(const variant& v, game_logic::formula_callable* e);
	virtual ~widget();

	void normalize_event(SDL_Event* event, bool translate_coords=false);
	virtual bool handle_event(const SDL_Event& event, bool claimed) { return claimed; }
	void set_environment(game_logic::formula_callable* e = 0) { environ_ = e; }
	boost::function<void()> on_process_;
	virtual void handle_process();
	virtual void recalc_loc();
	virtual bool in_widget(int xloc, int yloc) const;
	virtual variant handle_write();
	virtual widget_settings_dialog* settings_dialog(int x, int y, int w, int h);
private:
DECLARE_CALLABLE(widget);
	virtual void visit_values(game_logic::formula_callable_visitor& visitor) {}

	int x_, y_;
	int w_, h_;
	int true_x_;
	int true_y_;
	boost::shared_ptr<gui::tooltip_item> tooltip_;
	bool tooltip_displayed_;
	std::string tooltip_text_;
	int tooltip_fontsize_;
	SDL_Color tooltip_color_;
	std::string tooltip_font_;
	bool visible_;
	game_logic::formula_callable* environ_;
	void process_delegate();
	game_logic::formula_ptr ffl_on_process_;
	// default zorder_ is 0.  A widget *must* have a good reason for wanting
	// higher priority in the draw order.
	int zorder_;
	std::string id_;
	bool disabled_;
	uint8_t disabled_opacity_;
	unsigned tooltip_display_delay_;
	unsigned tooltip_ticks_;
	int display_alpha_;
	int pad_h_;
	int pad_w_;
	bool claim_mouse_events_;
	bool draw_with_object_shader_;

	HORIZONTAL_ALIGN align_h_;
	VERTICAL_ALIGN   align_v_;

	std::string frame_set_name_;
	const_framed_gui_element_ptr frame_set_;
	int resolution_;
};

// Functor to sort widgets by z-ordering.
class widget_sort_zorder
{
public:
    bool operator()(const widget_ptr lhs, const widget_ptr rhs) const;
};

}

#endif
