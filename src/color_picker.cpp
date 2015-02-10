/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <limits>
#include <sstream>
#include <boost/lexical_cast.hpp>

#include "Canvas.hpp"
#include "geometry.hpp"

#include "asserts.hpp"
#include "color_picker.hpp"
#include "grid_widget.hpp"
#include "label.hpp"

namespace 
{
	const char* default_palette[] =
	{
		"black",
		"maroon",
		"green",
		"olivedrab",
		"navy",
		"mediumpurple",
		"turquoise",
		"cornsilk",
		"grey",
		"red",
		"limegreen",
		"yellow",
		"blue",
		"purple",
		"aquamarine",
		"white"
	};
}

namespace gui
{
	ColorPicker::ColorPicker(const rect& area)
		: selected_palette_color_(0), hue_(0), saturation_(0), value_(0), alpha_(255),
		red_(255), green_(255), blue_(255), dragging_(false), palette_offset_y_(0), main_color_selected_(1)
	{
		setLoc(area.x(), area.y());
		setDim(area.w(), area.h());

		for(int n = 0; n != sizeof(default_palette)/sizeof(default_palette[0]); ++n) {
			palette_.push_back(KRE::Color(default_palette[n]));
		}

		primary_ = KRE::Color::colorBlack();
		secondary_ = KRE::Color::colorWhite();

		init();
	}

	ColorPicker::ColorPicker(const rect& area, std::function<void (const KRE::Color&)> change_fun)
		: selected_palette_color_(), hue_(0), saturation_(0), value_(0), alpha_(255),
		red_(255), green_(255), blue_(255), dragging_(false), palette_offset_y_(0), main_color_selected_(1),
		onchange_(change_fun)
	{
		setLoc(area.x(), area.y());
		setDim(area.w(), area.h());

		for(int n = 0; n != sizeof(default_palette)/sizeof(default_palette[0]); ++n) {
			palette_.push_back(KRE::Color(default_palette[n]));
		}

		primary_ = KRE::Color::colorBlack();
		secondary_ = KRE::Color::colorWhite();

		init();
	}

	ColorPicker::ColorPicker(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v, e), selected_palette_color_(0), hue_(0), saturation_(0), 
		value_(0), alpha_(255), red_(255), green_(255), blue_(255), dragging_(false), palette_offset_y_(0),
		main_color_selected_(1)
	{
		// create delegate for onchange
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");

		if(v.has_key("on_change")) {
			const variant on_change_value = v["on_change"];
			if(on_change_value.is_function()) {
				ASSERT_LOG(on_change_value.min_function_arguments() <= 1 && on_change_value.max_function_arguments() >= 1, "onChange ColorPicker function should take 1 argument: " << v.debug_location());
				static const variant fml("fn(color)");
				change_handler_.reset(new game_logic::Formula(fml));

				game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
				callable->add("fn", on_change_value);

				handler_arg_.reset(callable);
			} else { 
				change_handler_ = getEnvironment()->createFormula(on_change_value);
			}
			onchange_ = std::bind(&ColorPicker::change, this);
		}

		if(v.has_key("palette")) {
			ASSERT_LOG(v["palette"].num_elements() <= 16, "'palette' attribute must have 16 or less elements.");
			for(int n = 0; n != v["palette"].num_elements(); ++n) {
				palette_.push_back(KRE::Color(v["palette"][n]));
			}
		} else {
			for(int n = 0; n != sizeof(default_palette)/sizeof(default_palette[0]); ++n) {
				palette_.push_back(KRE::Color(default_palette[n]));
			}
		}

		if(v.has_key("primary")) {
			primary_ = KRE::Color(v["primary"]);
		} else {
			primary_ = KRE::Color::colorBlack();
		}
		if(v.has_key("secondary")) {
			secondary_ = KRE::Color(v["secondary"]);
		} else {
			secondary_ = KRE::Color::colorWhite();
		}

		init();
	}

	ColorPicker::~ColorPicker()
	{
	}

	void ColorPicker::setPrimaryColor(KRE::Color color)
	{
		primary_ = color;
		colorUpdated();
	}

	void ColorPicker::setSecondaryColor(KRE::Color color)
	{
		secondary_ = color;
		colorUpdated();
	}

	void ColorPicker::colorUpdated()
	{
		setTextFromColor(main_color_selected_ ? primary_ : secondary_);
		setSlidersFromColor(main_color_selected_ ? primary_ : secondary_);
	}

	bool ColorPicker::getPaletteColor(int n, KRE::Color* color)
	{
		ASSERT_LOG(size_t(n) < palette_.size(), "ColorPicker::getPaletteColor selected color out of range: " << n << " >= " << palette_.size());
		ASSERT_LOG(color != NULL, "ColorPicker::getPaletteColor: NULL color pointer given");
		*color = palette_[size_t(n)];
		return true;
	}

	void ColorPicker::setPaletteColor(int n, const KRE::Color& color)
	{
		ASSERT_LOG(size_t(n) < palette_.size(), "ColorPicker::setPaletteColor selected color out of range: " << n << " >= " << palette_.size());
		palette_[size_t(n)] = color;
	}

	void ColorPicker::handleProcess()
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
			auto canvas = KRE::Canvas::getInstance();

			static std::vector<uint8_t> carray;
			if(carray.empty()) {
				carray.push_back(255); carray.push_back(255); carray.push_back(255); carray.push_back(255);
				for(double angle = 0; angle < M_PI * 2.0; angle += 0.0245436926) {
					const rgb cc = hsv_to_rgb(uint8_t(angle*255.0/(M_PI*2.0)), 255, 255);
					carray.push_back(cc.r); carray.push_back(cc.g); carray.push_back(cc.b); carray.push_back(255);
				}
				//repeat the first coordinate to complete the circle.
				const rgb cc = hsv_to_rgb(0, 255, 255);
				carray.push_back(cc.r); carray.push_back(cc.g); carray.push_back(cc.b); carray.push_back(255);
			}
			canvas->drawSolidCircle(point(x, y), static_cast<float>(radius), carray);
		}
	}

	void ColorPicker::handleDraw() const
	{
		auto canvas = KRE::Canvas::getInstance();
		const rect prect(5, 5, color_box_length_, color_box_length_);
		const rect srect(10+color_box_length_, 5, color_box_length_, color_box_length_);
		const rect prect_border(prect.x()-2, prect.y()-2, prect.w()+4, prect.h()+4);
		const rect srect_border(srect.x()-2, srect.y()-2, srect.w()+4, srect.h()+4);

		if(main_color_selected_) {
			canvas->drawHollowRect(prect_border, KRE::Color::colorWhite());
		} else {
			canvas->drawHollowRect(srect_border, KRE::Color::colorWhite());
		}
		canvas->drawSolidRect(prect, primary_);
		canvas->drawSolidRect(srect, secondary_);

		const int xoffset = wheel_radius_ + 5;
		const int yoffset = color_box_length_ + wheel_radius_ + 20;
		draw_colored_circle(xoffset, yoffset, wheel_radius_);
		const int rx = int((saturation_ / 255.0 * wheel_radius_) * cos(hue_ / 255.0 * M_PI * 2.0));
		const int ry = int((saturation_ / 255.0 * wheel_radius_) * sin(hue_ / 255.0 * M_PI * 2.0));
		const rect selected_color_rect(xoffset + rx, yoffset + ry, 4, 4);
		canvas->drawSolidRect(selected_color_rect, KRE::Color::colorBlack());

		g_->draw();
		copy_to_palette_->draw();

		int cnt = 0;
		for(auto& color : palette_) {
			const rect palette_rect(5 + 22*(cnt%8), palette_offset_y_ + (cnt/8)*22, 20, 20);
			canvas->drawSolidRect(palette_rect, color);
			++cnt;
		}
		if(selected_palette_color_ < palette_.size()) {
			const rect prect_border(5 + 22*(selected_palette_color_%8)-1, palette_offset_y_ + (selected_palette_color_/8)*22-1, 24, 24);
			canvas->drawHollowRect(prect_border, KRE::Color::colorWhite());
		}
	}

	void ColorPicker::processMouseInWheel(int x, int y)
	{
		x -= wheel_radius_ + 5;
		y -= color_box_length_ + wheel_radius_ + 20;
		const double r = sqrt(x*x + y*y);
		const double angle = atan2(y, x);
		if(r <= wheel_radius_) {
			hue_ = uint8_t(angle*255.0/(M_PI*2.0));
			saturation_ = uint8_t(r/wheel_radius_ * 255.0);
			value_ = 255;

			rgb out = hsv_to_rgb(hue_, saturation_, value_);
			red_ = out.r; green_ = out.g; blue_ = out.b;
			if(main_color_selected_) {
				primary_ = KRE::Color(red_, green_, blue_, alpha_);
			} else {
				secondary_ = KRE::Color(red_, green_, blue_, alpha_);
			}

			setTextFromColor(main_color_selected_ ? primary_ : secondary_);
			setSlidersFromColor(main_color_selected_ ? primary_ : secondary_);

			if(onchange_) {
				onchange_(main_color_selected_ ? primary_ : secondary_);
			}
		}
	}

	bool ColorPicker::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(claimed) {
			return claimed;
		}
		SDL_Event ev = event;
		normalizeEvent(&ev);

		if(g_ && g_->processEvent(ev, claimed)) {
			return true;
		}
		if(copy_to_palette_ && copy_to_palette_->processEvent(ev, claimed)) {
			return true;
		}

		if(ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
			const SDL_MouseButtonEvent& button = ev.button;
			dragging_ = true;
			processMouseInWheel(button.x, button.y);

			if(button.x >= 5 && button.x <= color_box_length_+5 && button.y >= 5 && button.y <= color_box_length_+5) {
				main_color_selected_ = 1;
			} else if(button.x >= 10+color_box_length_ && button.x <= 10+color_box_length_*2 && button.y >= 5 && button.y <= color_box_length_+5) {
				main_color_selected_ = 0;
			} else if(button.x >= 5 && button.x < 5 + 22*8 && button.y >= palette_offset_y_ && button.y <= palette_offset_y_ + static_cast<int>(palette_.size()/8*22)) {
				size_t color_ndx = (button.y-palette_offset_y_)/22*8+(button.x-5)/22;
				if(color_ndx < palette_.size()) {
					selected_palette_color_ = color_ndx;
					if(!(SDL_GetModState()&KMOD_CTRL)) {
						if(main_color_selected_) {
							primary_ = palette_[selected_palette_color_];
						} else {
							secondary_ = palette_[selected_palette_color_];
						}
						setSlidersFromColor(main_color_selected_ ? primary_ : secondary_);
						setTextFromColor(main_color_selected_ ? primary_ : secondary_);
					}
				}
			}
		} else if(ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT && dragging_) {
			dragging_ = false;
		} else if(ev.type == SDL_MOUSEMOTION && dragging_) {
			processMouseInWheel(ev.motion.x, ev.motion.y);
		} else if(ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_RIGHT) {
			const SDL_MouseButtonEvent& button = ev.button;
			if(button.x >= 5 && button.x < 5 + 22*8 && button.y >= palette_offset_y_ && button.y <= palette_offset_y_ + static_cast<int>(palette_.size()/8*22)) {
				size_t color_ndx = (button.y-palette_offset_y_)/22*8+(button.x-5)/22;
				if(color_ndx < palette_.size()) {
					selected_palette_color_ = color_ndx;
					if(!(SDL_GetModState()&KMOD_CTRL)) {
						if(main_color_selected_) {
							secondary_ = palette_[selected_palette_color_];
						} else {
							primary_ = palette_[selected_palette_color_];
						}
						setSlidersFromColor(main_color_selected_ ? primary_ : secondary_);
						setTextFromColor(main_color_selected_ ? primary_ : secondary_);
					}
				}
			}
		}

		return false;
	}

	void ColorPicker::init()
	{
		using std::placeholders::_1;

		color_box_length_ = width() / 2 - 20;
		wheel_radius_ = width() / 2 - 10;

		setHSVFromColor(primary_);
		s_.clear();
		t_.clear();

		while(palette_.size() < 16) {
			palette_.push_back(KRE::Color::colorWhite());
		}

		std::vector<LabelPtr> labels;
		const char* label_text[] =
		{
			"R:", "G:", "B:", "H:", "S:", "V:", "A:"
		};

		g_.reset(new Grid(3));
		g_->setLoc(5, color_box_length_ + wheel_radius_*2 + 40);
		for(int n = 0; n != 7; ++n) {
			labels.push_back(new Label(label_text[n], KRE::Color::colorAntiquewhite(), 12, "Montaga-Regular"));
			s_.push_back(new Slider(50, std::bind(&ColorPicker::SliderChange, this, n, _1), 0, 1));
			t_.push_back(new TextEditorWidget(40));
			t_.back()->setOnUserChangeHandler(std::bind(&ColorPicker::textChange, this, n));
			t_.back()->setOnTabHandler(std::bind(&ColorPicker::textTabPressed, this, n));

			g_->addCol(labels.back());
			g_->addCol(s_.back());
			g_->addCol(t_.back());
		}
		palette_offset_y_ = g_->y() + g_->height() + 10;

		copy_to_palette_.reset(new Button(new Label("Set", KRE::Color::colorAntiquewhite(), 12, "Montaga-Regular"), std::bind(&ColorPicker::copyToPaletteFn, this)));
		copy_to_palette_->setLoc(5, palette_offset_y_);
		copy_to_palette_->setTooltip("Set palette color", 12, KRE::Color::colorAntiquewhite(), "Montaga-Regular");

		palette_offset_y_ = copy_to_palette_->y() + copy_to_palette_->height() + 10;

		setSlidersFromColor(main_color_selected_ ? primary_ : secondary_);
		setTextFromColor(main_color_selected_ ? primary_ : secondary_);
	}

	void ColorPicker::copyToPaletteFn()
	{
		std::cerr << "copyToPaletteFn()" << std::endl;
		if(selected_palette_color_ < palette_.size()) {
			palette_[selected_palette_color_] = main_color_selected_ ? primary_ : secondary_;
		}
	}

	void ColorPicker::SliderChange(int n, double p)
	{
		ASSERT_LOG(size_t(n) < s_.size(), "ColorPicker::SliderChange: invalid array access: " << n << " >= " << s_.size());
		if(n >= 0 && n <= 2) {
			switch(n) {
			case 0:  red_ = uint8_t(255.0 * p); break;
			case 1:  green_ = uint8_t(255.0 * p); break;
			default: blue_ = uint8_t(255.0 * p); break;
			}
			hsv out = rgb_to_hsv(red_, green_, blue_);
			hue_ = out.h; saturation_ = out.s; value_ = out.v;
			if(main_color_selected_) {
				primary_ = KRE::Color(red_, green_, blue_, alpha_);
			} else {
				secondary_ = KRE::Color(red_, green_, blue_, alpha_);
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
				primary_ = KRE::Color(red_, green_, blue_, alpha_);
			} else {
				secondary_ = KRE::Color(red_, green_, blue_, alpha_);
			}
		} else {
			// alpha
			alpha_ = uint8_t(255.0 * p);
			if(main_color_selected_) {
				primary_ = KRE::Color(red_, green_, blue_, alpha_);
			} else {
				secondary_ = KRE::Color(red_, green_, blue_, alpha_);
			}
		}
		setTextFromColor(main_color_selected_ ? primary_ : secondary_);
		setSlidersFromColor(main_color_selected_ ? primary_ : secondary_);

		if(onchange_) {
			onchange_(main_color_selected_ ? primary_ : secondary_);
		}
	}

	void ColorPicker::textTabPressed(int n)
	{
		ASSERT_LOG(size_t(n) < t_.size(), "ColorPicker::textChange invalid array access: " << n << " >= " << t_.size());
		t_[n]->setFocus(false);
		if(++n >= 7) {
			n = 0;
		}
		t_[n]->setFocus(true);
	}

	void ColorPicker::textChange(int n)
	{
		ASSERT_LOG(size_t(n) < t_.size(), "ColorPicker::textChange invalid array access: " << n << " >= " << t_.size());
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
			primary_ = KRE::Color(red_, green_, blue_, alpha_);
		} else {
			secondary_ = KRE::Color(red_, green_, blue_, alpha_);
		}
		setTextFromColor(primary_);
		setSlidersFromColor(main_color_selected_ ? primary_ : secondary_);

		if(onchange_) {
			onchange_(main_color_selected_ ? primary_ : secondary_);
		}
	}

	void ColorPicker::setSlidersFromColor(const KRE::Color& c)
	{
		ASSERT_LOG(s_.size() == 7, "Didn't find the correct number of Sliders.");
		s_[0]->setPosition(static_cast<float>(c.r()/255.0));
		s_[1]->setPosition(static_cast<float>(c.g()/255.0));
		s_[2]->setPosition(static_cast<float>(c.b()/255.0));
		hsv out = rgb_to_hsv(c.r_int(), c.g_int(), c.b_int());
		s_[3]->setPosition(static_cast<float>(out.h/255.0));
		s_[4]->setPosition(static_cast<float>(out.s/255.0));
		s_[5]->setPosition(static_cast<float>(out.v/255.0));
		s_[6]->setPosition(static_cast<float>(alpha_/255.0));
	}

	void ColorPicker::setTextFromColor(const KRE::Color& c, int n)
	{
		ASSERT_LOG(t_.size() == 7, "Didn't find the correct number of Sliders.");
		std::stringstream str;
		if(n != 0) {
			str << int(c.r());
			t_[0]->setText(str.str(), false);
		}
		if(n != 1) {
			str.str(std::string()); str << int(c.g());
			t_[1]->setText(str.str(), false);
		}
		if(n != 2) {
			str.str(std::string()); str << int(c.b());
			t_[2]->setText(str.str(), false);
		}
		hsv out = rgb_to_hsv(c.r_int(), c.g_int(), c.b_int());
		if(n != 3) {
			str.str(std::string()); str << int(out.h);
			t_[3]->setText(str.str(), false);
		}
		if(n != 4) {
			str.str(std::string()); str << int(out.s);
			t_[4]->setText(str.str(), false);
		}
		if(n != 5) {
			str.str(std::string()); str << int(out.v);
			t_[5]->setText(str.str(), false);
		}
		if(n != 6) {
			str.str(std::string()); str << int(alpha_);
			t_[6]->setText(str.str(), false);
		}
	}

	void ColorPicker::change()
	{
		using namespace game_logic;
		if(handler_arg_) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(handler_arg_.get()));
			callable->add("color", getPrimaryColor().write());
			variant value = change_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("color", getPrimaryColor().write());
			variant value = change_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "ColorPicker::change() called without environment!" << std::endl;
		}
	}


	void ColorPicker::setHSVFromColor(const KRE::Color& in_color)
	{
		hsv out = rgb_to_hsv(in_color.r_int(), in_color.g_int(), in_color.b_int());
		hue_ = out.h;
		saturation_ = out.s;
		value_ = out.v;
	}

	BEGIN_DEFINE_CALLABLE(ColorPicker, Widget)
	DEFINE_FIELD(primary, "[int,int,int,int]")
		return obj.primary_.write();
	DEFINE_SET_FIELD_TYPE("[int]|string")
		obj.primary_ = KRE::Color(value);
	DEFINE_FIELD(color, "[int,int,int,int]")
		return obj.primary_.write();
	DEFINE_SET_FIELD_TYPE("[int]|string")
		obj.primary_ = KRE::Color(value);
	DEFINE_FIELD(secondary, "[int,int,int,int]")
		return obj.secondary_.write();
	DEFINE_SET_FIELD_TYPE("[int]|string")
		obj.secondary_ = KRE::Color(value);
	END_DEFINE_CALLABLE(ColorPicker)
}
