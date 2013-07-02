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

#include <limits>
#include <sstream>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include "asserts.hpp"
#include "color_picker.hpp"
#include "geometry.hpp"
#include "grid_widget.hpp"
#include "label.hpp"
#include "raster.hpp"

namespace 
{
	const char* default_palette[] =
	{
		"black",
		"maroon",
		"green",
		"olive_drab",
		"navy",
		"medium_purple",
		"turquoise",
		"cornsilk",
		"grey",
		"red",
		"lime_green",
		"yellow",
		"blue",
		"purple",
		"aquamarine",
		"white"
	};
}

namespace gui
{
	color_picker::color_picker(const rect& area, boost::function<void (const graphics::color&)>* onchange)
		: selected_palette_color_(-1), hue_(0), saturation_(0), value_(0), alpha_(255),
		red_(255), green_(255), blue_(255), dragging_(false), palette_offset_y_(0), main_color_selected_(1)
	{
		set_loc(area.x(), area.y());
		set_dim(area.w(), area.h());
		if(onchange != NULL) {
			onchange_ = *onchange;
		}

		for(int n = 0; n != sizeof(default_palette)/sizeof(default_palette[0]); ++n) {
			palette_.push_back(graphics::color(default_palette[n]));
		}

		primary_ = graphics::color("black");
		secondary_ = graphics::color("white");

		init();
	}

	color_picker::color_picker(const variant& v, game_logic::formula_callable* e)
		: widget(v, e), selected_palette_color_(-1), hue_(0), saturation_(0), 
		value_(0), alpha_(255), red_(255), green_(255), blue_(255), dragging_(false), palette_offset_y_(0),
		main_color_selected_(1)
	{
		// create delegate for onchange
		ASSERT_LOG(get_environment() != 0, "You must specify a callable environment");

		if(v.has_key("on_change")) {
			const variant on_change_value = v["on_change"];
			if(on_change_value.is_function()) {
				ASSERT_LOG(on_change_value.min_function_arguments() <= 1 && on_change_value.max_function_arguments() >= 1, "on_change color_picker function should take 1 argument: " << v.debug_location());
				static const variant fml("fn(color)");
				change_handler_.reset(new game_logic::formula(fml));

				game_logic::map_formula_callable* callable = new game_logic::map_formula_callable;
				callable->add("fn", on_change_value);

				handler_arg_.reset(callable);
			} else { 
				change_handler_ = get_environment()->create_formula(on_change_value);
			}
			onchange_ = boost::bind(&color_picker::change, this);
		}

		if(v.has_key("palette")) {
			ASSERT_LOG(v["palette"].num_elements() <= 16, "'palette' attribute must have 16 or less elements.");
			for(int n = 0; n != v["palette"].num_elements(); ++n) {
				palette_.push_back(graphics::color(v["palette"][n]));
			}
		} else {
			for(int n = 0; n != sizeof(default_palette)/sizeof(default_palette[0]); ++n) {
				palette_.push_back(graphics::color(default_palette[n]));
			}
		}

		if(v.has_key("primary")) {
			primary_ = graphics::color(v["primary"]);
		} else {
			primary_ = graphics::color("black");
		}
		if(v.has_key("secondary")) {
			secondary_ = graphics::color(v["secondary"]);
		} else {
			secondary_ = graphics::color("white");
		}

		init();
	}

	color_picker::~color_picker()
	{
	}

	void color_picker::set_primary_color(graphics::color color)
	{
		primary_ = color;
		color_updated();
	}

	void color_picker::set_secondary_color(graphics::color color)
	{
		secondary_ = color;
		color_updated();
	}

	void color_picker::color_updated()
	{
		set_text_from_color(main_color_selected_ ? primary_ : secondary_);
		set_sliders_from_color(main_color_selected_ ? primary_ : secondary_);
	}

	bool color_picker::get_palette_color(int n, graphics::color* color)
	{
		ASSERT_LOG(size_t(n) < palette_.size(), "color_picker::get_palette_color selected color out of range: " << n << " >= " << palette_.size());
		ASSERT_LOG(color != NULL, "color_picker::get_palette_color: NULL color pointer given");
		*color = palette_[size_t(n)];
		return true;
	}

	void color_picker::set_palette_color(int n, const graphics::color& color)
	{
		ASSERT_LOG(size_t(n) < palette_.size(), "color_picker::set_palette_color selected color out of range: " << n << " >= " << palette_.size());
		palette_[size_t(n)] = color;
	}

	void color_picker::handle_process()
	{
	}

	namespace 
	{
		struct rgb
		{
			uint8_t r, g, b;
		};

		struct hsv
		{
			uint8_t h, s, v;
		};

		hsv rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b)
		{
			hsv out;
			uint8_t min_color, max_color, delta;

			min_color = std::min(r, std::min(g, b));
			max_color = std::max(r, std::max(g, b));

			delta = max_color - min_color;
			out.v = max_color;
			if(out.v == 0) {
				out.s = 0;
				out.h = 0;
				return out;
			}

			out.s = uint8_t(255.0 * delta / out.v);
			if(out.s == 0) {
				out.h = 0;
				return out;
			}

			if(r == max_color) {
				out.h = uint8_t(43.0 * (g-b)/delta);
			} else if(g == max_color) {
				out.h = 85 + uint8_t(43.0 * (b-r)/delta);
			} else {
				out.h = 171 + uint8_t(43.0 * (r-g)/delta);
			}
			return out;
		}

		rgb hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v)
		{
			rgb out;
			uint8_t region, remainder, p, q, t;

			if(s == 0) {
				out.r = out.g = out.b = v;
			} else {
				region = h / 43;
				remainder = (h - (region * 43)) * 6; 

				p = (v * (255 - s)) >> 8;
				q = (v * (255 - ((s * remainder) >> 8))) >> 8;
				t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

				switch(region)
				{
					case 0:  out.r = v; out.g = t; out.b = p; break;
					case 1:  out.r = q; out.g = v; out.b = p; break;
					case 2:  out.r = p; out.g = v; out.b = t; break;
					case 3:  out.r = p; out.g = q; out.b = v; break;
					case 4:  out.r = t; out.g = p; out.b = v; break;
					default: out.r = v; out.g = p; out.b = q; break;
				}
			}
			return out;
		}

		void draw_colored_circle(int x, int y, int radius) 
		{
			static std::vector<GLfloat> varray;
			static std::vector<uint8_t> carray;
			if(varray.empty()) {
				varray.push_back(0);
				varray.push_back(0);
				carray.push_back(255); carray.push_back(255); carray.push_back(255); carray.push_back(255);
				for(double angle = 0; angle < M_PI * 2.0; angle += 0.0245436926) {
					varray.push_back(GLfloat(radius*cos(angle)));
					varray.push_back(GLfloat(radius*sin(angle)));
					const rgb cc = hsv_to_rgb(uint8_t(angle*255.0/(M_PI*2.0)), 255, 255);
					carray.push_back(cc.r); carray.push_back(cc.g); carray.push_back(cc.b); carray.push_back(255);
				}
				//repeat the first coordinate to complete the circle.
				varray.push_back(varray[2]);
				varray.push_back(varray[3]);
				const rgb cc = hsv_to_rgb(0, 255, 255);
				carray.push_back(cc.r); carray.push_back(cc.g); carray.push_back(cc.b); carray.push_back(255);
			}

			glPushMatrix();
			glTranslatef(GLfloat(x), GLfloat(y), 0);
#if defined(USE_GLES2)
			gles2::manager gles2_manager(gles2::get_simple_col_shader());
			gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray.front());
			gles2::active_shader()->shader()->color_array(4, GL_UNSIGNED_BYTE, GL_TRUE, 0, &carray.front());
			glDrawArrays(GL_TRIANGLE_FAN, 0, varray.size()/2);
#else
			glDisable(GL_TEXTURE_2D);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glVertexPointer(2, GL_FLOAT, 0, &varray.front());
			glColorPointer(4, GL_UNSIGNED_BYTE, 0, &carray.front());
			glDrawArrays(GL_TRIANGLE_FAN, 0, varray.size()/2);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glEnable(GL_TEXTURE_2D);
#endif
			glPopMatrix();
		}
	}

	void color_picker::handle_draw() const
	{
		glPushMatrix();
		glTranslatef(GLfloat(x() & ~1), GLfloat(y() & ~1), 0.0);

		const rect prect(5, 5, color_box_length_, color_box_length_);
		const rect srect(10+color_box_length_, 5, color_box_length_, color_box_length_);
		const rect prect_border(prect.x()-2, prect.y()-2, prect.w()+4, prect.h()+4);
		const rect srect_border(srect.x()-2, srect.y()-2, srect.w()+4, srect.h()+4);

		if(main_color_selected_) {
			graphics::draw_hollow_rect(prect_border.sdl_rect(), graphics::color(255,255,255).as_sdl_color());
		} else {
			graphics::draw_hollow_rect(srect_border.sdl_rect(), graphics::color(255,255,255).as_sdl_color());
		}
		graphics::draw_rect(prect, primary_);
		graphics::draw_rect(srect, secondary_);

		const int xoffset = wheel_radius_ + 5;
		const int yoffset = color_box_length_ + wheel_radius_ + 20;
		draw_colored_circle(xoffset, yoffset, wheel_radius_);
		const int rx = int((saturation_ / 255.0 * wheel_radius_) * cos(hue_ / 255.0 * M_PI * 2.0));
		const int ry = int((saturation_ / 255.0 * wheel_radius_) * sin(hue_ / 255.0 * M_PI * 2.0));
		const rect selected_color_rect(xoffset + rx, yoffset + ry, 4, 4);
		graphics::draw_rect(selected_color_rect, graphics::color("black"));

		g_->draw();
		copy_to_palette_->draw();

		int cnt = 0;
		for(auto& color : palette_) {
			const rect palette_rect(5 + 22*(cnt%8), palette_offset_y_ + (cnt/8)*22, 20, 20);
			graphics::draw_rect(palette_rect, color);
			++cnt;
		}
		if(selected_palette_color_ >= 0 && selected_palette_color_ < palette_.size()) {
			const rect prect_border(5 + 22*(selected_palette_color_%8)-1, palette_offset_y_ + (selected_palette_color_/8)*22-1, 24, 24);
			graphics::draw_hollow_rect(prect_border.sdl_rect(), graphics::color(255,255,255).as_sdl_color());
		}
		glPopMatrix();
	}

	void color_picker::process_mouse_in_wheel(int x, int y)
	{
		x -= wheel_radius_ + 5;
		y -= color_box_length_ + wheel_radius_ + 20;
		const double r = sqrt(x*x + y*y);
		const double angle = atan2(y, x);
		if(r <= wheel_radius_) {
			hue_ = uint8_t(angle*255.0/(M_PI*2.0));
			saturation_ = uint8_t(r/wheel_radius_ * 255.0);

			rgb out = hsv_to_rgb(hue_, saturation_, value_);
			red_ = out.r; green_ = out.g; blue_ = out.b;
			if(main_color_selected_) {
				primary_ = graphics::color(red_, green_, blue_, alpha_);
			} else {
				secondary_ = graphics::color(red_, green_, blue_, alpha_);
			}

			set_text_from_color(main_color_selected_ ? primary_ : secondary_);
			set_sliders_from_color(main_color_selected_ ? primary_ : secondary_);

			if(onchange_) {
				onchange_(main_color_selected_ ? primary_ : secondary_);
			}
		}
	}

	bool color_picker::handle_event(const SDL_Event& event, bool claimed)
	{
		if(claimed) {
			return claimed;
		}
		SDL_Event ev = event;
		normalize_event(&ev);

		if(g_ && g_->process_event(ev, claimed)) {
			return true;
		}
		if(copy_to_palette_ && copy_to_palette_->process_event(ev, claimed)) {
			return true;
		}

		if(ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
			const SDL_MouseButtonEvent& button = ev.button;
			dragging_ = true;
			process_mouse_in_wheel(button.x, button.y);

			if(button.x >= 5 && button.x <= color_box_length_+5 && button.y >= 5 && button.y <= color_box_length_+5) {
				main_color_selected_ = 1;
			} else if(button.x >= 10+color_box_length_ && button.x <= 10+color_box_length_*2 && button.y >= 5 && button.y <= color_box_length_+5) {
				main_color_selected_ = 0;
			} else if(button.x >= 5 && button.x < 5 + 22*8 && button.y >= palette_offset_y_ && button.y <= palette_offset_y_ + palette_.size()/8*22) {
				size_t color_ndx = (button.y-palette_offset_y_)/22*8+(button.x-5)/22;
				if(color_ndx < palette_.size()) {
					selected_palette_color_ = color_ndx;
					if(!(SDL_GetModState()&KMOD_CTRL)) {
						if(main_color_selected_) {
							primary_ = palette_[selected_palette_color_];
						} else {
							secondary_ = palette_[selected_palette_color_];
						}
					}
				}
			}
		} else if(ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT && dragging_) {
			dragging_ = false;
		} else if(ev.type == SDL_MOUSEMOTION && dragging_) {
			process_mouse_in_wheel(ev.motion.x, ev.motion.y);
		} else if(ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_RIGHT) {
			const SDL_MouseButtonEvent& button = ev.button;
			if(button.x >= 5 && button.x < 5 + 22*8 && button.y >= palette_offset_y_ && button.y <= palette_offset_y_ + palette_.size()/8*22) {
				size_t color_ndx = (button.y-palette_offset_y_)/22*8+(button.x-5)/22;
				if(color_ndx < palette_.size()) {
					selected_palette_color_ = color_ndx;
					if(!(SDL_GetModState()&KMOD_CTRL)) {
						if(main_color_selected_) {
							secondary_ = palette_[selected_palette_color_];
						} else {
							primary_ = palette_[selected_palette_color_];
						}
					}
				}
			}
		}

		return false;
	}

	void color_picker::init()
	{
		color_box_length_ = width() / 2 - 20;
		wheel_radius_ = width() / 2 - 10;

		set_hsv_from_color(primary_);
		s_.clear();
		t_.clear();

		while(palette_.size() < 16) {
			palette_.push_back(graphics::color("white"));
		}

		std::vector<label_ptr> labels;
		const char* label_text[] =
		{
			"R:", "G:", "B:", "H:", "S:", "V:", "A:"
		};

		g_.reset(new grid(3));
		g_->set_loc(5, color_box_length_ + wheel_radius_*2 + 40);
		for(int n = 0; n != 7; ++n) {
			labels.push_back(new label(label_text[n], graphics::color("antique_white").as_sdl_color(), 12, "Montaga-Regular"));
			s_.push_back(new slider(50, boost::bind(&color_picker::slider_change, this, n, _1), 0, 1));
			t_.push_back(new text_editor_widget(40));
			t_.back()->set_on_change_handler(boost::bind(&color_picker::text_change, this, n));
			t_.back()->set_on_tab_handler(boost::bind(&color_picker::text_tab_pressed, this, n));

			g_->add_col(labels.back());
			g_->add_col(s_.back());
			g_->add_col(t_.back());
		}
		palette_offset_y_ = g_->y() + g_->height() + 10;

		copy_to_palette_.reset(new button(new label("Set", graphics::color("antique_white").as_sdl_color(), 12, "Montaga-Regular"), boost::bind(&color_picker::copy_to_palette_fn, this)));
		copy_to_palette_->set_loc(5, palette_offset_y_);
		copy_to_palette_->set_tooltip("Set palette color", 12, graphics::color("antique_white").as_sdl_color(), "Montaga-Regular");

		palette_offset_y_ = copy_to_palette_->y() + copy_to_palette_->height() + 10;

		set_sliders_from_color(main_color_selected_ ? primary_ : secondary_);
		set_text_from_color(main_color_selected_ ? primary_ : secondary_);
	}

	void color_picker::copy_to_palette_fn()
	{
		std::cerr << "copy_to_palette_fn()" << std::endl;
		if(selected_palette_color_ >= 0 &&  size_t(selected_palette_color_) < palette_.size()) {
			palette_[selected_palette_color_] = main_color_selected_ ? primary_ : secondary_;
		}
	}

	void color_picker::slider_change(int n, double p)
	{
		ASSERT_LOG(size_t(n) < s_.size(), "color_picker::slider_change: invalid array access: " << n << " >= " << s_.size());
		if(n >= 0 && n <= 2) {
			switch(n) {
			case 0:  red_ = uint8_t(255.0 * p); break;
			case 1:  green_ = uint8_t(255.0 * p); break;
			default: blue_ = uint8_t(255.0 * p); break;
			}
			hsv out = rgb_to_hsv(red_, green_, blue_);
			hue_ = out.h; saturation_ = out.s; value_ = out.v;
			if(main_color_selected_) {
				primary_ = graphics::color(red_, green_, blue_, alpha_);
			} else {
				secondary_ = graphics::color(red_, green_, blue_, alpha_);
			}
		} else if(n >= 3 && n <= 5) {
			switch(n) {
			case 3:  hue_ = uint8_t(255.0 * p); break;
			case 4:  saturation_ = uint8_t(255.0 * p); break;
			default: value_ = uint8_t(255.0 * p); break;
			}
			rgb out = hsv_to_rgb(hue_, saturation_, value_);
			red_ = out.r; green_ = out.g; blue_ = out.b;
			if(main_color_selected_) {
				primary_ = graphics::color(red_, green_, blue_, alpha_);
			} else {
				secondary_ = graphics::color(red_, green_, blue_, alpha_);
			}
		} else {
			// alpha
			alpha_ = uint8_t(255.0 * p);
			if(main_color_selected_) {
				primary_ = graphics::color(red_, green_, blue_, alpha_);
			} else {
				secondary_ = graphics::color(red_, green_, blue_, alpha_);
			}
		}
		set_text_from_color(main_color_selected_ ? primary_ : secondary_);
		set_sliders_from_color(main_color_selected_ ? primary_ : secondary_);

		if(onchange_) {
			onchange_(main_color_selected_ ? primary_ : secondary_);
		}
	}

	void color_picker::text_tab_pressed(int n)
	{
		ASSERT_LOG(size_t(n) < t_.size(), "color_picker::text_change invalid array access: " << n << " >= " << t_.size());
		t_[n]->set_focus(false);
		if(++n >= 7) {
			n = 0;
		}
		t_[n]->set_focus(true);
	}

	void color_picker::text_change(int n)
	{
		ASSERT_LOG(size_t(n) < t_.size(), "color_picker::text_change invalid array access: " << n << " >= " << t_.size());
		int val;
		switch(n) {
			case 0:  val = red_; break;
			case 1:  val = green_; break;
			case 2:  val = blue_; break;
			case 3:  val = hue_; break;
			case 4:  val = saturation_; break;
			case 5:  val = value_; break;
			default: val = alpha_; break;
		}
		try {
			val = std::max(0, std::min(boost::lexical_cast<int>(t_[n]->text()), 255));
		} catch(boost::bad_lexical_cast&) {
			// passing on it, keep default.
		}
		switch(n) {
			case 0:  red_ = val; break;
			case 1:  green_ = val; break;
			case 2:  blue_ = val; break;
			case 3:  hue_ = val; break;
			case 4:  saturation_ = val; break;
			case 5:  value_ = val; break;
			default: alpha_ = val; break;
		}
		if(n <= 2) {
			hsv out = rgb_to_hsv(red_, green_, blue_);
			hue_ = out.h; saturation_ = out.s; value_ = out.v;
		} else if(n <= 5) {
			rgb out = hsv_to_rgb(hue_, saturation_, value_);
			red_ = out.r; green_ = out.g; blue_ = out.b;
		}
		if(main_color_selected_) {
			primary_ = graphics::color(red_, green_, blue_, alpha_);
		} else {
			secondary_ = graphics::color(red_, green_, blue_, alpha_);
		}
		//set_text_from_color(primary_, n);
		set_sliders_from_color(main_color_selected_ ? primary_ : secondary_);

		if(onchange_) {
			onchange_(main_color_selected_ ? primary_ : secondary_);
		}
	}

	void color_picker::set_sliders_from_color(const graphics::color& c)
	{
		ASSERT_LOG(s_.size() == 7, "Didn't find the correct number of sliders.");
		s_[0]->set_position(c.r()/255.0);
		s_[1]->set_position(c.g()/255.0);
		s_[2]->set_position(c.b()/255.0);
		hsv out = rgb_to_hsv(c.r(), c.g(), c.b());
		s_[3]->set_position(out.h/255.0);
		s_[4]->set_position(out.s/255.0);
		s_[5]->set_position(out.v/255.0);
		s_[6]->set_position(alpha_/255.0);
	}

	void color_picker::set_text_from_color(const graphics::color& c, int n)
	{
		ASSERT_LOG(t_.size() == 7, "Didn't find the correct number of sliders.");
		std::stringstream str;
		if(n != 0) {
			str << int(c.r());
			t_[0]->set_text(str.str(), true);
		}
		if(n != 1) {
			str.str(std::string()); str << int(c.g());
			t_[1]->set_text(str.str(), true);
		}
		if(n != 2) {
			str.str(std::string()); str << int(c.b());
			t_[2]->set_text(str.str(), true);
		}
		hsv out = rgb_to_hsv(c.r(), c.g(), c.b());
		if(n != 3) {
			str.str(std::string()); str << int(out.h);
			t_[3]->set_text(str.str(), true);
		}
		if(n != 4) {
			str.str(std::string()); str << int(out.s);
			t_[4]->set_text(str.str(), true);
		}
		if(n != 5) {
			str.str(std::string()); str << int(out.v);
			t_[5]->set_text(str.str(), true);
		}
		if(n != 6) {
			str.str(std::string()); str << int(alpha_);
			t_[6]->set_text(str.str(), true);
		}
	}

	void color_picker::change()
	{
		using namespace game_logic;
		if(handler_arg_) {
			map_formula_callable_ptr callable = map_formula_callable_ptr(new map_formula_callable(handler_arg_.get()));
			callable->add("color", get_primary_color().write());
			variant value = change_handler_->execute(*callable);
			get_environment()->execute_command(value);
		} else if(get_environment()) {
			map_formula_callable_ptr callable = map_formula_callable_ptr(new map_formula_callable(get_environment()));
			callable->add("color", get_primary_color().write());
			variant value = change_handler_->execute(*callable);
			get_environment()->execute_command(value);
		} else {
			std::cerr << "color_picker::change() called without environment!" << std::endl;
		}
	}


	void color_picker::set_hsv_from_color(const graphics::color& in_color)
	{
		hsv out = rgb_to_hsv(in_color.r(), in_color.g(), in_color.b());
		hue_ = out.h;
		saturation_ = out.s;
		value_ = out.v;
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(color_picker)
	DEFINE_FIELD(primary, "[int,int,int,int]")
		return obj.primary_.write();
	DEFINE_SET_FIELD_TYPE("[int]|string")
		obj.primary_ = graphics::color(value);
	DEFINE_FIELD(color, "[int,int,int,int]")
		return obj.primary_.write();
	DEFINE_SET_FIELD_TYPE("[int]|string")
		obj.primary_ = graphics::color(value);
	DEFINE_FIELD(secondary, "[int,int,int,int]")
		return obj.secondary_.write();
	DEFINE_SET_FIELD_TYPE("[int]|string")
		obj.secondary_ = graphics::color(value);
	END_DEFINE_CALLABLE(color_picker)
}
