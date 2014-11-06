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
#ifndef GUI_FORMULA_FUNCTIONS_HPP_INCLUDED
#define GUI_FORMULA_FUNCTIONS_HPP_INCLUDED

#include "custom_object.hpp"
#include "variant.hpp"

#include <boost/intrusive_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include <string>

class frame;
class level;

class gui_algorithm;
typedef boost::intrusive_ptr<gui_algorithm> gui_algorithm_ptr;
typedef boost::intrusive_ptr<frame> frame_ptr;

class gui_algorithm : public game_logic::formula_callable {
public:
	gui_algorithm(variant node);
	~gui_algorithm();

	static gui_algorithm_ptr get(const std::string& key);
	static gui_algorithm_ptr create(const std::string& key);

	void new_level();

	void process(level& lvl);
	void draw(const level& lvl);
	void load(level& lvl);
	bool gui_event(level& lvl, const SDL_Event &event);

	void draw_animation(const std::string& object_name, const std::string& anim, int x, int y, int cycle) const;
	void color(unsigned char r, unsigned char g, unsigned char b, unsigned char a) const;

	frame_ptr get_frame(const std::string& id) const;

	const custom_object* get_object() const { return object_.get(); }

private:
	void set_object(boost::intrusive_ptr<custom_object> obj);

	gui_algorithm(const gui_algorithm&);
	void operator=(const gui_algorithm&);

	variant get_value(const std::string& key) const;
	variant get_value_by_slot(int slot) const;

	void execute_command(variant v);

	const level* lvl_;
	game_logic::formula_ptr draw_formula_, process_formula_, load_formula_;
	int cycle_;
	bool loaded_;

	std::map<std::string, frame_ptr> frames_;

	boost::intrusive_ptr<custom_object> object_;

	variant cached_draw_commands_;

	variant buttons_;
	std::map<std::string, std::map<const int, game_logic::formula_ptr> > button_formulas_;
	std::map<std::string, rect> button_hit_rects_;

	std::vector<gui_algorithm_ptr> includes_;
};

#endif
