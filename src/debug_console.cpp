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
#include <algorithm>
#include <list>
#include <numeric>
#include <sstream>

#include <boost/bind.hpp>
#include <boost/function.hpp>

#include "asserts.hpp"
#include "custom_object.hpp"
#include "custom_object_functions.hpp"
#include "custom_object_type.hpp"
#include "draw_scene.hpp"
#include "editor.hpp"
#include "filesystem.hpp"
#include "font.hpp"
#include "formatter.hpp"
#include "foreach.hpp"
#include "debug_console.hpp"
#include "decimal.hpp"
#include "foreach.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "load_level.hpp"
#include "player_info.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "slider.hpp"

namespace debug_console
{

namespace {
int graph_cycle = 0;

struct SampleSet {
	SampleSet() : last_cycle(0) {}
	int last_cycle;
	std::vector<decimal> samples;
};

std::map<std::string, SampleSet> graphs;
typedef std::pair<const std::string, SampleSet> graph_pair;

int round_up_value(decimal value)
{
	if(value == decimal()) {
		return 0;
	}

	int result = 1;
	while(result > 0 && result < value) {
		result *= 10;
	}

	if(result < 0) {
		return value.as_int();
	}

	if(result/5 >= value) {
		return result/5;
	} else if(result/2 >= value) {
		return result/2;
	} else {
		return result;
	}
}
}

void add_graph_sample(const std::string& id, decimal value)
{
	SampleSet& s = graphs[id];
	if(graph_cycle - s.last_cycle >= 1000) {
		s.samples.clear();
	} else {
		for(; s.last_cycle < graph_cycle; ++s.last_cycle) {
			s.samples.push_back(decimal());
		}
	}

	if(s.samples.empty()) {
		s.samples.push_back(decimal());
	}

	s.last_cycle = graph_cycle;
	s.samples.back() += value;
}

void process_graph()
{
	++graph_cycle;
}

void draw_graph()
{
	decimal min_value, max_value;
	foreach(graph_pair& p, graphs) {
		if(p.second.last_cycle - p.second.last_cycle >= 1000) {
			p.second.samples.clear();
		}

		foreach(const decimal& value, p.second.samples) {
			if(value < min_value) {
				min_value = value;
			}

			if(value > max_value) {
				max_value = value;
			}
		}
	}

	if(max_value == min_value) {
		return;
	}

	max_value = decimal::from_int(round_up_value(max_value));
	min_value = decimal::from_int(-round_up_value(-min_value));

	const rect graph_area(50, 60, 500, 200);
	graphics::draw_rect(graph_area, graphics::color(255, 255, 255, 64));

	graphics::draw_rect(rect(graph_area.x(), graph_area.y(), graph_area.w(), 2), graphics::color(255,255,255,255));
	graphics::blit_texture(font::render_text_uncached(formatter() << max_value.as_int(), graphics::color_white(), 14), graph_area.x2() + 4, graph_area.y());

	graphics::draw_rect(rect(graph_area.x(), graph_area.y2(), graph_area.w(), 2), graphics::color(255,255,255,255));
	graphics::blit_texture(font::render_text_uncached(formatter() << min_value.as_int(), graphics::color_white(), 14), graph_area.x2() + 4, graph_area.y2() - 12);

	graphics::color GraphColors[] = {
		graphics::color(255,255,255,255),
		graphics::color(0,0,255,255),
		graphics::color(255,0,0,255),
		graphics::color(0,255,0,255),
		graphics::color(255,255,0,255),
		graphics::color(128,128,128,255),
	};

	int colors_index = 0;
	foreach(const graph_pair& p, graphs) {
		if(p.second.samples.empty()) {
			return;
		}

		const graphics::color& graph_color = GraphColors[colors_index%(sizeof(GraphColors)/sizeof(*GraphColors))];
		graph_color.set_as_current_color();

		const int gap = graph_cycle - p.second.last_cycle;
		int index = (gap + p.second.samples.size()) - 1000;
		int pos = 0;
		if(index < 0) {
			pos -= index;
			index = 0;
		}

		//collect the last 20 y samples to average for the label's position.
		std::vector<GLfloat> y_samples;

		std::vector<GLfloat> points;

		while(index < p.second.samples.size()) {
			decimal value = p.second.samples[index];

			const GLfloat xpos = graph_area.x() + (GLfloat(pos)*graph_area.w())/GLfloat(1000);

			const GLfloat value_ratio = ((value - min_value)/(max_value - min_value)).as_float();
			const GLfloat ypos = graph_area.y2() - graph_area.h()*value_ratio;
			points.push_back(xpos);
			points.push_back(ypos);
			y_samples.push_back(ypos);
			++index;
			++pos;
		}

		if(points.empty()) {
			continue;
		}

		if(y_samples.size() > 20) {
			y_samples.erase(y_samples.begin(), y_samples.end() - 20);
		}

		const GLfloat mean_ypos = std::accumulate(y_samples.begin(), y_samples.end(), 0.0)/y_samples.size();

#if defined(USE_SHADERS)
		{
			gles2::manager gles2_manager(gles2::get_simple_shader());
			gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &points[0]);
			glDrawArrays(GL_LINE_STRIP, 0, points.size()/2);
		}
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

		glVertexPointer(2, GL_FLOAT, 0, &points[0]);
		glDrawArrays(GL_LINE_STRIP, 0, points.size()/2);

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif

		graphics::blit_texture(font::render_text_uncached(p.first, graph_color.as_sdl_color(), 14), points[points.size()-2] + 4, mean_ypos - 6);

		++colors_index;
	}

	glColor4f(1.0,1.0,1.0,1.0);
}

namespace {
static bool screen_output_enabled = true;

std::list<graphics::texture>& messages() {
	static std::list<graphics::texture> message_queue;
	return message_queue;
}

std::set<console_dialog*> consoles_;

const std::string Prompt = "--> ";
}

void enable_screen_output(bool en)
{
	screen_output_enabled = en;
}

void add_message(const std::string& msg)
{
	if(!preferences::debug() || !SDL_WasInit(0)) {
		return;
	}

	if(!consoles_.empty()) {
		foreach(console_dialog* d, consoles_) {
			d->add_message(msg);
		}
		return;
	}

	if(msg.size() > 100) {
		std::string trunc_msg(msg.begin(), msg.begin() + 90);
		trunc_msg += "...";
		add_message(trunc_msg);
		return;
	}

	const SDL_Color col = {255, 255, 255, 255};
	try {
		messages().push_back(font::render_text_uncached(msg, col, 14));
	} catch(font::error& e) {

		std::cerr << "FAILED TO ADD MESSAGE DUE TO FONT RENDERING FAILURE\n";
		return;
	}
	if(messages().size() > 8) {
		messages().pop_front();
	}
}

void draw()
{
	if(messages().empty()) {
		return;
	}
	if(screen_output_enabled == false) {
		return;
	}

	int ypos = 100;
	foreach(const graphics::texture& t, messages()) {
		const SDL_Rect area = {0, ypos-2, int(t.width() + 10), int(t.height() + 5)};
		graphics::draw_rect(area, graphics::color_black(), 128);
		graphics::blit_texture(t, 5, ypos);
		ypos += t.height() + 5;
	}
}

namespace {
std::string console_history_path()
{
	return std::string(preferences::user_data_path()) + "/console-history.cfg";
}
}

console_dialog::console_dialog(level& lvl, game_logic::formula_callable& obj)
   : dialog(0, graphics::screen_height() - 200, 600, 200), lvl_(&lvl), focus_(&obj),
     history_pos_(0)
{
	if(sys::file_exists(console_history_path())) {
		try {
			history_ = json::parse(sys::read_file(console_history_path())).as_list_string();
			history_pos_ = history_.size();
		} catch(...) {
		}
	}

	init();

	consoles_.insert(this);

	text_editor_->set_focus(true);
}

console_dialog::~console_dialog()
{
	consoles_.erase(this);
}

void console_dialog::init()
{
	using namespace gui;
	text_editor_ = new text_editor_widget(width() - 20, height() - 20);
	add_widget(widget_ptr(text_editor_), 10, 10);

	text_editor_->set_on_move_cursor_handler(boost::bind(&console_dialog::on_move_cursor, this));
	text_editor_->set_on_begin_enter_handler(boost::bind(&console_dialog::on_begin_enter, this));
	text_editor_->set_on_enter_handler(boost::bind(&console_dialog::on_enter, this));

	text_editor_->set_text(Prompt);
	text_editor_->set_cursor(0, Prompt.size());
}

void console_dialog::on_move_cursor()
{
	if(text_editor_->cursor_row() < text_editor_->get_data().size()-1) {
		text_editor_->set_cursor(text_editor_->get_data().size()-1, text_editor_->cursor_col());
	}

	if(text_editor_->cursor_col() < Prompt.size() && text_editor_->get_data().back().size() >= Prompt.size()) {
		text_editor_->set_cursor(text_editor_->get_data().size()-1, Prompt.size());
	}
}

bool console_dialog::on_begin_enter()
{
	if(lvl_->editor_selection().empty() == false) {
		focus_ = lvl_->editor_selection().front();
	}

	std::vector<std::string> data = text_editor_->get_data();

	std::string ffl(text_editor_->get_data().back());
	while(ffl.size() < Prompt.size() || std::equal(Prompt.begin(), Prompt.end(), ffl.begin()) == false) {
		data.pop_back();
		ASSERT_LOG(data.empty() == false, "No prompt found in debug console: " << ffl);
		ffl = data.back() + ffl;
	}

	ffl.erase(ffl.begin(), ffl.begin() + Prompt.size());
	text_editor_->set_text(text_editor_->text() + "\n" + Prompt);
	text_editor_->set_cursor(text_editor_->get_data().size()-1, Prompt.size());
	if(!ffl.empty()) {
		history_.push_back(ffl);
		if(history_.size() > 128) {
			history_.erase(history_.begin(), history_.begin() + history_.size()-96);
		}
		history_pos_ = history_.size();
		sys::write_file(console_history_path(), vector_to_variant(history_).write_json());

		assert_recover_scope recover_from_assert;
		try {
			std::cerr << "EVALUATING: " << ffl << "\n";
			variant ffl_variant(ffl);
			std::string filename = "(debug console)";
			variant::debug_info info;
			info.filename = &filename;
			info.line = info.column = 0;
			ffl_variant.set_debug_info(info);

			entity* ent = dynamic_cast<entity*>(focus_.get());

			game_logic::formula f(ffl_variant, &get_custom_object_functions_symbol_table(), ent ? ent->get_definition() : NULL);
			variant v = f.execute(*focus_);
			if(ent) {
				try {
					ent->execute_command(v);
				} catch(validation_failure_exception& e) {
					//if this was a failure due to it not being a real command,
					//that's fine, since we just want to output the result.
					if(!strstr(e.msg.c_str(), "COMMAND WAS EXPECTED, BUT FOUND")) {
						throw e;
					}
				}
			}

			std::string output = v.to_debug_string();
			debug_console::add_message(output);
			std::cerr << "OUTPUT: " << output << std::endl;
		} catch(validation_failure_exception& e) {
			debug_console::add_message("error parsing formula: " + e.msg);
		} catch(type_error& e) {
			debug_console::add_message("error executing formula: " + e.message);
		}
	}

	return false;
}

void console_dialog::on_enter()
{
}

bool console_dialog::has_keyboard_focus() const
{
	return text_editor_->has_focus();
}

void console_dialog::add_message(const std::string& msg)
{

	std::string m;
	for(std::vector<std::string>::const_iterator i = text_editor_->get_data().begin(); i != text_editor_->get_data().end()-1; ++i) {
		m += *i + "\n";
	}

	m += msg + "\n";
	m += text_editor_->get_data().back();

	int col = text_editor_->cursor_col();
	text_editor_->set_text(m);
	text_editor_->set_cursor(text_editor_->get_data().size()-1, col);
}

bool console_dialog::handle_event(const SDL_Event& event, bool claimed)
{
	if(!claimed && has_keyboard_focus()) {
		switch(event.type) {
		case SDL_KEYDOWN:
			if((event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_DOWN) && !history_.empty()) {
				if(event.key.keysym.sym == SDLK_UP) {
					--history_pos_;
				} else {
					++history_pos_;
				}

				if(history_pos_ < 0) {
					history_pos_ = history_.size();
				} else if(history_pos_ > history_.size()) {
					history_pos_ = history_.size();
				}

				load_history();
				return true;
			}
			break;
		}
	}

	return dialog::handle_event(event, claimed);
}

void console_dialog::load_history()
{
	std::string str;
	if(history_pos_ < history_.size()) {
		str = history_[history_pos_];
	}

	std::string m;
	for(std::vector<std::string>::const_iterator i = text_editor_->get_data().begin(); i != text_editor_->get_data().end()-1; ++i) {
		m += *i + "\n";
	}

	m += Prompt + str;
	text_editor_->set_text(m);

	text_editor_->set_cursor(text_editor_->get_data().size()-1, text_editor_->get_data().back().size());
}

void console_dialog::set_focus(game_logic::formula_callable_ptr e)
{
	focus_ = e;
	text_editor_->set_focus(true);
	entity* ent = dynamic_cast<entity*>(focus_.get());
	if(ent) {
		add_message(formatter() << "Selected object: " << ent->debug_description());
	}
}

}
