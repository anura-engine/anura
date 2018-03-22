/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include "Canvas.hpp"
#include "Font.hpp"
#include "WindowManager.hpp"

#include "asserts.hpp"
#include "button.hpp"
#include "logger.hpp"
#include "custom_object.hpp"
#include "custom_object_functions.hpp"
#include "custom_object_type.hpp"
#include "draw_scene.hpp"
#include "editor.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "debug_console.hpp"
#include "decimal.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "load_level.hpp"
#include "player_info.hpp"
#include "preferences.hpp"
#include "slider.hpp"
#include "utils.hpp"


namespace debug_console
{
	namespace 
	{
		int g_executing_debug_console;

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

	ExecuteDebugConsoleScope::ExecuteDebugConsoleScope() { ++g_executing_debug_console; }
	ExecuteDebugConsoleScope::~ExecuteDebugConsoleScope() { --g_executing_debug_console; }

	bool isExecutingDebugConsoleCommand()
	{
		return g_executing_debug_console > 0;
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
		auto canvas = KRE::Canvas::getInstance();
		decimal min_value, max_value;
		for(graph_pair& p : graphs) {
			if(p.second.last_cycle - p.second.last_cycle >= 1000) {
				p.second.samples.clear();
			}

			for(const decimal& value : p.second.samples) {
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
		canvas->drawSolidRect(graph_area, KRE::Color(255, 255, 255, 64));

		canvas->drawSolidRect(rect(graph_area.x(), graph_area.y(), graph_area.w(), 2), KRE::Color::colorWhite());
		canvas->blitTexture(KRE::Font::getInstance()->renderText(formatter() << max_value.as_int(), KRE::Color::colorWhite(), 14, false), 
			0, rect(graph_area.x2() + 4, graph_area.y()));

		canvas->drawSolidRect(rect(graph_area.x(), graph_area.y2(), graph_area.w(), 2), KRE::Color::colorWhite());
		canvas->blitTexture(KRE::Font::getInstance()->renderText(formatter() << min_value.as_int(), KRE::Color::colorWhite(), 14, false), 
			0, rect(graph_area.x2() + 4, graph_area.y2() - 12));

		KRE::Color GraphColors[] = {
			KRE::Color(255,255,255,255),
			KRE::Color(0,0,255,255),
			KRE::Color(255,0,0,255),
			KRE::Color(0,255,0,255),
			KRE::Color(255,255,0,255),
			KRE::Color(128,128,128,255),
		};

		int colors_index = 0;
		for(const graph_pair& p : graphs) {
			if(p.second.samples.empty()) {
				return;
			}

			const KRE::Color& graph_color = GraphColors[colors_index%(sizeof(GraphColors)/sizeof(*GraphColors))];

			const int gap = graph_cycle - p.second.last_cycle;
			int index = (gap + static_cast<int>(p.second.samples.size())) - 1000;
			int pos = 0;
			if(index < 0) {
				pos -= index;
				index = 0;
			}

			//collect the last 20 y samples to average for the label's position.
			std::vector<float> y_samples;

			std::vector<glm::vec2> points;

			while(static_cast<unsigned>(index) < p.second.samples.size()) {
				decimal value = p.second.samples[index];

				const float xpos = graph_area.x() + (static_cast<float>(pos)*graph_area.w())/1000.0f;

				const float value_ratio = static_cast<float>(((value - min_value)/(max_value - min_value)).as_float());
				const float ypos = graph_area.y2() - graph_area.h()*value_ratio;
				points.emplace_back(xpos, ypos);
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

			const float mean_ypos = std::accumulate(y_samples.begin(), y_samples.end(), 0.0f)/y_samples.size();

			canvas->drawLineStrip(points, 1.0f, graph_color);
			canvas->blitTexture(KRE::Font::getInstance()->renderText(p.first, graph_color, 14), 0, rect(static_cast<int>(points[points.size()-1][0] + 4), static_cast<int>(mean_ypos - 6)));

			++colors_index;
		}
	}

	namespace 
	{
		static bool screen_output_enabled = true;

		std::list<KRE::TexturePtr>& messages() {
			static std::list<KRE::TexturePtr> message_queue;
			return message_queue;
		}

		std::set<ConsoleDialog*> consoles_;

		const std::string Prompt = "--> ";
	}

	void enable_screen_output(bool en)
	{
		screen_output_enabled = en;
	}

	void addMessage(const std::string& msg)
	{
		if(!preferences::debug() || !SDL_WasInit(0)) {
			return;
		}

		if(!consoles_.empty()) {
			for(ConsoleDialog* d : consoles_) {
				d->addMessage(msg);
			}
			return;
		}

		if(msg.size() > 100) {
			std::string trunc_msg(msg.begin(), msg.begin() + 90);
			trunc_msg += "...";
			addMessage(trunc_msg);
			return;
		}

		try {
			messages().push_back(KRE::Font::getInstance()->renderText(msg, KRE::Color::colorWhite(), 14, false));
		} catch(KRE::FontError&) {
			LOG_ERROR("FAILED TO ADD MESSAGE DUE TO FONT RENDERING FAILURE");
			return;
		}
		if(messages().size() > 8) {
			messages().pop_front();
		}
	}

	void clearMessages()
	{
		messages().clear();
	}

	void draw()
	{
		auto canvas = KRE::Canvas::getInstance();
		if(messages().empty()) {
			return;
		}
		if(screen_output_enabled == false) {
			return;
		}

		int ypos = 100;
		for(const KRE::TexturePtr& t : messages()) {
			const SDL_Rect area = {};
			canvas->drawSolidRect(rect(0, ypos-2, t->width() + 10, t->height() + 5), KRE::Color(0,0,0,128));
			canvas->blitTexture(t, 0, rect(5,ypos,0,0));
			ypos += t->height() + 5;
		}
	}

	namespace 
	{
		std::string console_history_path()
		{
			return std::string(preferences::user_data_path()) + "/console-history.cfg";
		}

		PREF_INT_PERSISTENT(console_width, 600, "Width of console in pixels");
		PREF_INT_PERSISTENT(console_height, 200, "Width of console in pixels");
		PREF_INT_PERSISTENT(console_font_size, 14, "Font size of console text");
	}

	ConsoleDialog::ConsoleDialog(Level& lvl, game_logic::FormulaCallable& obj)
		: Dialog(
			0,
			KRE::WindowManager::getMainWindow()->height() -
				util::clamp<uint_fast16_t>(
					g_console_height, g_console_height,
					KRE::WindowManager::getMainWindow()->height()),
			util::clamp<uint_fast16_t>(
				g_console_width, g_console_width,
				KRE::WindowManager::getMainWindow()->width()),
			util::clamp<uint_fast16_t>(
				g_console_height, g_console_height,
				KRE::WindowManager::getMainWindow()->height())
		),
	     text_editor_(nullptr), lvl_(&lvl), focus_(&obj),
		 history_pos_(0), prompt_pos_(0), dragging_(false), resizing_(false)
	{
		if(sys::file_exists(console_history_path())) {
			try {
				std::string file_contents =
						sys::read_file(console_history_path());
				variant parsed_json = json::parse(
						file_contents,
						json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
				history_ = parsed_json.as_list_string();
				history_pos_ = static_cast<int>(history_.size());
			} catch(...) {
			}
		}

		init();

		consoles_.insert(this);

		text_editor_->setFocus(true);
	}

	ConsoleDialog::~ConsoleDialog()
	{
		consoles_.erase(this);
	}

	void ConsoleDialog::init()
	{
		ffl::IntrusivePtr<gui::TextEditorWidget> old_text_editor(text_editor_);

		using namespace gui;
		text_editor_.reset(new TextEditorWidget(width() - 40, height() - 20));
		addWidget(WidgetPtr(text_editor_), 10, 10);

		text_editor_->setOnMoveCursorHandler(std::bind(&ConsoleDialog::onMoveCursor, this));
		text_editor_->setOnBeginEnterHandler(std::bind(&ConsoleDialog::onBeginEnter, this));
		text_editor_->setOnEnterHandler(std::bind(&ConsoleDialog::onEnter, this));

		text_editor_->setSelectAllHandler([](std::string s) {
			const char* p1 = s.c_str();
			const char* p2 = p1 + strlen(p1);

			const char* p = strstr(p1, "--> ");
			while(p != nullptr) {
				const char* next_p = strstr(p+1, "\n--> ");
				if(next_p == nullptr) {
					break;
				}

				p = next_p + 1;
			}

			if(p == nullptr) {
				return std::pair<int,int>(0, static_cast<int>(s.size()));
			}

			p += 4;

			return std::pair<int,int>(static_cast<int>(p - p1), static_cast<int>(p2 - p1));
		});

		if(old_text_editor) {
			text_editor_->setText(old_text_editor->text());
			text_editor_->setCursor(old_text_editor->cursorRow(), old_text_editor->cursorCol());
			text_editor_->setFontSize(g_console_font_size);
		} else {
			text_editor_->setText(Prompt);
			text_editor_->setCursor(0, static_cast<int>(Prompt.size()));
			text_editor_->setFontSize(g_console_font_size);
			prompt_pos_ = 0;
		}

		auto b = new gui::Button("+", std::bind(&ConsoleDialog::changeFontSize, this, 2));
		addWidget(WidgetPtr(b), width() - 30, 20);
		b = new gui::Button("-", std::bind(&ConsoleDialog::changeFontSize, this, -2));
		addWidget(WidgetPtr(b), width() - 30, 40);
	}

	void ConsoleDialog::onMoveCursor()
	{
		if(static_cast<unsigned>(text_editor_->cursorRow()) < prompt_pos_) {
			text_editor_->setCursor(prompt_pos_, text_editor_->cursorCol());
		}

		if(text_editor_->cursorRow() == prompt_pos_ && static_cast<unsigned>(text_editor_->cursorCol()) < Prompt.size() && text_editor_->getData()[prompt_pos_].size() >= Prompt.size()) {
			text_editor_->setCursor(prompt_pos_, Prompt.size());
		}
	}

	std::string ConsoleDialog::getEnteredCommand()
	{
		std::vector<std::string> data = text_editor_->getData();

		std::string ffl(text_editor_->getData().back());
		while(ffl.size() < Prompt.size() || std::equal(Prompt.begin(), Prompt.end(), ffl.begin()) == false) {
			data.pop_back();
			ASSERT_LOG(data.empty() == false, "No prompt found in debug console: " << ffl);
			ffl = data.back() + ffl;
		}

		ffl.erase(ffl.begin(), ffl.begin() + Prompt.size());

		if(ffl.empty() == false && ffl[0] == '!') {
			std::string prefix(ffl.begin()+1, ffl.end());
			for(auto i = history_.rbegin(); i != history_.rend(); ++i) {
				if(i->size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), i->begin())) {
					std::string text = text_editor_->text();
					text.erase(text.end() - ffl.size() - 1, text.end());
					ffl = *i;
					text_editor_->setText(text + ffl);
					break;
				}
			}
		}
		return ffl;
	}

	bool ConsoleDialog::onBeginEnter()
	{
		if(SDL_GetModState()&KMOD_SHIFT) {
			return true;
		}

		if(lvl_->editor_selection().empty() == false) {
			focus_ = lvl_->editor_selection().front();
		}

		std::string ffl = getEnteredCommand();

		text_editor_->setText(text_editor_->text() + "\n" + Prompt);
		text_editor_->setCursor(static_cast<int>(text_editor_->getData().size())-1, Prompt.size());
		prompt_pos_ = text_editor_->getData().size()-1;
		if(ffl.empty() == false) {
			history_.push_back(ffl);
			if(history_.size() > 512) {
				history_.erase(history_.begin(), history_.begin() + history_.size()-384);
			}
			history_pos_ = static_cast<int>(history_.size());
			sys::write_file(console_history_path(), vector_to_variant(history_).write_json());

			assert_recover_scope recover_from_assert;
			try {
				LOG_INFO("EVALUATING: " << ffl);
				variant ffl_variant(ffl);
				std::string filename = "(debug console)";
				variant::debug_info info;
				info.filename = &filename;
				info.line = info.column = 0;
				ffl_variant.setDebugInfo(info);

				Entity* ent = dynamic_cast<Entity*>(focus_.get());

				game_logic::Formula f(ffl_variant, &get_custom_object_functions_symbol_table(), ent ? ent->getDefinition() : nullptr);
				variant v = f.execute(*focus_);
				if(ent) {
					try {
						ExecuteDebugConsoleScope scope;
						ent->executeCommand(v);
					} catch(validation_failure_exception& e) {
						//if this was a failure due to it not being a real command,
						//that's fine, since we just want to output the result.
						if(!strstr(e.msg.c_str(), "COMMAND WAS EXPECTED, BUT FOUND")) {
							throw e;
						}
					}
				}

				std::string output = v.to_debug_string();
				debug_console::addMessage(output);
				LOG_INFO("OUTPUT: " << output);
			} catch(validation_failure_exception& e) {
				debug_console::addMessage("error parsing formula: " + e.msg);
			} catch(type_error& e) {
				debug_console::addMessage("error executing formula: " + e.message);
			}
		}

		return false;
	}

	void ConsoleDialog::onEnter()
	{
	}

	bool ConsoleDialog::hasKeyboardFocus() const
	{
		return text_editor_->hasFocus();
	}

	void ConsoleDialog::addMessage(const std::string& msg)
	{

		const int old_nlines = text_editor_->getData().size();

		std::string m;
		for(std::vector<std::string>::const_iterator i = text_editor_->getData().begin(); i != text_editor_->getData().begin()+prompt_pos_; ++i) {
			m += *i + "\n";
		}

		m += msg + "\n";
		for(std::vector<std::string>::const_iterator i = text_editor_->getData().begin() + prompt_pos_; i != text_editor_->getData().end(); ++i) {
			m += *i;
			if(i+1 != text_editor_->getData().end()) {
				m += "\n";
			}
		}

		auto col = text_editor_->cursorCol();
		text_editor_->setText(m);
		text_editor_->setCursor(static_cast<int>(text_editor_->getData().size())-1, col);

		const int new_nlines = text_editor_->getData().size();
		prompt_pos_ += new_nlines - old_nlines;
	}

	bool ConsoleDialog::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(!claimed) {
			switch(event.type) {
			case SDL_KEYDOWN:
				if(((event.key.keysym.sym == SDLK_UP && text_editor_->cursorRow() == prompt_pos_) ||
				    (event.key.keysym.sym == SDLK_DOWN && text_editor_->cursorRow() == text_editor_->getData().size()-1))
				   && !history_.empty() && hasKeyboardFocus()) {
					if(event.key.keysym.sym == SDLK_UP) {
						if(history_pos_ == history_.size()) {
							std::string ffl = getEnteredCommand();
							if(!ffl.empty()) {
								history_.push_back(ffl);
							}
						}
						--history_pos_;
					} else {
						++history_pos_;
					}

					if(history_pos_ < 0) {
						history_pos_ = static_cast<int>(history_.size());
					} else if(history_pos_ >= static_cast<int>(history_.size())) {
						std::string ffl = getEnteredCommand();
						if(ffl.empty() == false && (history_.empty() || history_.back() != ffl)) {
							history_.push_back(ffl);
						}
						history_pos_ = static_cast<int>(history_.size());
					}

					loadHistory();
					return true;
				}
				break;
			case SDL_MOUSEMOTION: {
				if(dragging_ && resizing_) {
					clear();
					setLoc(x(), y() + event.motion.yrel);
					g_console_width = width() + event.motion.xrel;
					g_console_height = height() - event.motion.yrel;
					setDim(g_console_width, g_console_height);
					init();
					text_editor_->setFocus(true);
					preferences::save_preferences();
					claimed = true;
					return true;
				} else if(dragging_) {
					setLoc(x() + event.motion.xrel, y() + event.motion.yrel);
					claimed = true;
					return true;
				}
				break;
			}
			case SDL_MOUSEBUTTONUP: {
				dragging_ = false;
				resizing_ = false;
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
				dragging_ = false;
				if(event.button.x >= x() && event.button.y >= y() && event.button.x <= x() + width() && event.button.y < y()+18) {
					dragging_ = true;
					if(event.button.x >= x() + width() - 60) {
						resizing_ = true;
					}
					claimed = true;
					return true;
				}
				break;
			}
		}

		return Dialog::handleEvent(event, claimed);
	}

	void ConsoleDialog::changeFontSize(int delta)
	{
		g_console_font_size = std::min<int>(40, std::max<int>(8, g_console_font_size + delta));
		clear();
		init();
		text_editor_->setFocus(true);
		preferences::save_preferences();
	}

	void ConsoleDialog::loadHistory()
	{
		std::string str;
		if(static_cast<unsigned>(history_pos_) < history_.size()) {
			str = history_[history_pos_];
		}

		std::string m;
		for(std::vector<std::string>::const_iterator i = text_editor_->getData().begin(); i != text_editor_->getData().begin() + prompt_pos_; ++i) {
			m += *i + "\n";
		}

		m += Prompt + str;
		text_editor_->setText(m);

		text_editor_->setCursor(static_cast<int>(text_editor_->getData().size())-1, text_editor_->getData().back().size());
	}

	void ConsoleDialog::setFocus(game_logic::FormulaCallablePtr e)
	{
		focus_ = e;
		text_editor_->setFocus(true);
		Entity* ent = dynamic_cast<Entity*>(focus_.get());
		if(ent) {
			addMessage(formatter() << "Selected object: " << ent->getDebugDescription());
		}
	}
}
