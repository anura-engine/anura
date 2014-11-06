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
#ifndef LABEL_HPP_INCLUDED
#define LABEL_HPP_INCLUDED

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "color_chart.hpp"
#include "formula_callable_definition.hpp"
#include "graphics.hpp"
#include "texture.hpp"
#include "widget.hpp"

namespace gui {

class label;
class dialog_label;
class dropdown_widget;

typedef boost::intrusive_ptr<label> label_ptr;
typedef boost::intrusive_ptr<const label> const_label_ptr;
typedef boost::intrusive_ptr<dialog_label> dialog_label_ptr;

class label : public widget
{
public:
	static label_ptr create(const std::string& text,
	                        const SDL_Color& color, 
							int size=14, 
							const std::string& font="") 
	{
		return label_ptr(new label(text, color, size, font));
	}
	explicit label(const std::string& text, const SDL_Color& color, int size=14, const std::string& font="");
	explicit label(const std::string& text, int size=14, const std::string& font="");
	explicit label(const variant& v, game_logic::formula_callable* e);

	void set_font_size(int font_size);
	void set_font(const std::string& font);
	void set_color(const SDL_Color& color);
	void set_text(const std::string& text);
	void set_fixed_width(bool fixed_width);
	virtual void set_dim(int x, int y);
	SDL_Color color() { return color_; }
	std::string font() const { return font_; }
	int size() { return size_; }
	std::string text() { return text_; }
	void set_click_handler(boost::function<void()> click) { on_click_ = click; }
	void set_highlight_color(const SDL_Color &col) {highlight_color_ = col;}
	void allow_highlight_on_mouseover(bool val=true) { highlight_on_mouseover_ = val; }
protected:
	label(const label&);
	void operator=(const label&);
	std::string& current_text();
	const std::string& current_text() const;
	virtual void recalculate_texture();
	void set_texture(graphics::texture t);

	virtual bool handle_event(const SDL_Event& event, bool claimed);
	virtual variant handle_write();
	virtual widget_settings_dialog* settings_dialog(int x, int y, int w, int h);

protected:
	DECLARE_CALLABLE(label);
private:

	void handle_draw() const;
	void inner_set_dim(int x, int y);
	void reformat_text();

	std::string text_, formatted_;
	graphics::texture texture_, border_texture_;
	int border_size_;
	SDL_Color color_;
	SDL_Color highlight_color_;
	boost::scoped_ptr<SDL_Color> border_color_;
	int size_;
	std::string font_;
	bool fixed_width_;

	bool in_label(int xloc, int yloc) const;
	boost::function<void()> on_click_;
	game_logic::formula_ptr ffl_click_handler_;
	void click_delegate();
	bool highlight_on_mouseover_;
	bool draw_highlight_;
	bool down_;

	friend class dropdown_widget;
};

class dialog_label : public label
{
public:
	explicit dialog_label(const std::string& text, const SDL_Color& color, int size=18);
	explicit dialog_label(const variant& v, game_logic::formula_callable* e);
	void set_progress(int progress);
	int get_max_progress() { return stages_; }
protected:
	virtual void set_value(const std::string& key, const variant& v);
	virtual variant get_value(const std::string& key) const;
	virtual void recalculate_texture();
private:
	int progress_, stages_;
};

class label_factory
{
public:
	label_factory(const SDL_Color& color, int size);
	label_ptr create(const std::string& text) const;
	label_ptr create(const std::string& text,
			 const std::string& tip) const;
private:
	SDL_Color color_;
	int size_;
};

}

#endif
