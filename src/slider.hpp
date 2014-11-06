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
#ifndef SLIDER_HPP_INCLUDED
#define SLIDER_HPP_INCLUDED

#include <boost/function.hpp>

#include "image_widget.hpp"
#include "texture.hpp"
#include "widget.hpp"
#include "gui_section.hpp"


namespace gui {
	
//A slider widget. Forwards to a given function whenever its value changes.
class slider : public widget
{
public:
	explicit slider(int width, boost::function<void (double)> onchange, double position=0.0, int scale=2);
	explicit slider(const variant& v, game_logic::formula_callable* e);
	double position() const {return position_;};
	void set_position (double position) {position_ = position;};
	void set_drag_end(boost::function<void (double)> ondragend) { ondragend_ = ondragend; }
		
protected:
	virtual void set_value(const std::string& key, const variant& v);
	virtual variant get_value(const std::string& key) const;
	void init() const;

private:
	bool in_slider(int x, int y) const;
	bool in_button(int x, int y) const;
	void handle_draw() const;
	bool handle_event(const SDL_Event& event, bool claimed);
	void handle_process();
		
	int width_;
	boost::function<void (double)> onchange_;
	boost::function<void (double)> ondragend_;
	bool dragging_;
	double position_;
		
	widget_ptr slider_left_, slider_right_, slider_middle_, slider_button_;

	game_logic::formula_ptr ffl_handler_;
	void change_delegate(double);
	game_logic::formula_ptr ffl_end_handler_;
	void dragend_delegate(double);
};
	
typedef boost::intrusive_ptr<slider> slider_ptr;
	
}

#endif
