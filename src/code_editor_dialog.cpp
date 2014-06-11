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

#ifndef NO_EDITOR
#include <boost/bind.hpp>

#include <algorithm>

#include "border_widget.hpp"
#include "button.hpp"
#include "code_editor_dialog.hpp"
#include "code_editor_widget.hpp"
#include "custom_object.hpp"
#include "custom_object_callable.hpp"
#include "custom_object_type.hpp"
#include "debug_console.hpp"
#include "drag_widget.hpp"
#include "filesystem.hpp"
#include "font.hpp"
#include "formatter.hpp"
#include "formula_function_registry.hpp"
#include "formula_object.hpp"
#include "frame.hpp"
#include "image_widget.hpp"
#include "json_parser.hpp"
#include "label.hpp"
#include "level.hpp"
#include "level_runner.hpp"
#include "module.hpp"
#include "object_events.hpp"
#include "preferences.hpp"
#include "text_editor_widget.hpp"
#include "tile_map.hpp"
#include "tileset_editor_dialog.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"

namespace game_logic
{
void invalidate_class_definition(const std::string& class_name);
}

std::set<level*>& get_all_levels_set();

code_editor_dialog::code_editor_dialog(const rect& r)
  : dialog(r.x(), r.y(), r.w(), r.h()), invalidated_(0), has_error_(false),
    modified_(false), file_contents_set_(true), suggestions_prefix_(-1),
	have_close_buttons_(false)
{
	init();
}

void code_editor_dialog::init()
{
	clear();

	using namespace gui;

	if(!editor_) {
		editor_.reset(new code_editor_widget(width() - 40, height() - (60 + (optional_error_text_area_ ? 170 : 0))));
	}

	button* save_button = new button("Save", boost::bind(&code_editor_dialog::save, this));
	button* increase_font = new button("+", boost::bind(&code_editor_dialog::change_font_size, this, 1));
	button* decrease_font = new button("-", boost::bind(&code_editor_dialog::change_font_size, this, -1));

	save_button_.reset(save_button);

	//std::cerr << "CED: " << x() << "," << y() << "; " << width() << "," << height() << std::endl;
	drag_widget* dragger = new drag_widget(x(), y(), width(), height(),
		drag_widget::DRAG_HORIZONTAL, NULL, 
		boost::bind(&code_editor_dialog::on_drag_end, this, _1, _2), 
		boost::bind(&code_editor_dialog::on_drag, this, _1, _2));

	search_ = new TextEditorWidget(120);
	replace_ = new TextEditorWidget(120);
	const SDL_Color col = {255,255,255,255};
	WidgetPtr find_label(label::create("Find: ", col));
	replace_label_ = label::create("Replace: ", col);
	status_label_ = label::create("Ok", col);
	error_label_ = label::create("", col);
	addWidget(find_label, 42, 12, MOVE_RIGHT);
	addWidget(WidgetPtr(search_), MOVE_RIGHT);
	addWidget(replace_label_, MOVE_RIGHT);
	addWidget(WidgetPtr(replace_), MOVE_RIGHT);
	addWidget(WidgetPtr(save_button), MOVE_RIGHT);

	if(have_close_buttons_) {
		button* save_and_close_button = new button("Save+Close", boost::bind(&code_editor_dialog::save_and_close, this));
		button* abort_button = new button("Abort", boost::bind(&dialog::cancel, this));
		addWidget(WidgetPtr(save_and_close_button), MOVE_RIGHT);
		addWidget(WidgetPtr(abort_button), MOVE_RIGHT);
	}

	addWidget(WidgetPtr(increase_font), MOVE_RIGHT);
	addWidget(WidgetPtr(decrease_font), MOVE_RIGHT);
	addWidget(editor_, find_label->x(), find_label->y() + save_button->height() + 2);
	if(optional_error_text_area_) {
		addWidget(optional_error_text_area_);
	}
	addWidget(status_label_);
	addWidget(error_label_, status_label_->x() + 480, status_label_->y());
	addWidget(WidgetPtr(dragger));

	replace_label_->setVisible(false);
	replace_->setVisible(false);

	if(fname_.empty() == false && fname_[0] == '@') {
		save_button->setVisible(false);
	}

	search_->set_on_tab_handler(boost::bind(&code_editor_dialog::on_tab, this));
	replace_->set_on_tab_handler(boost::bind(&code_editor_dialog::on_tab, this));

	search_->set_on_change_handler(boost::bind(&code_editor_dialog::on_search_changed, this));
	search_->set_on_enter_handler(boost::bind(&code_editor_dialog::on_search_enter, this));
	replace_->set_on_enter_handler(boost::bind(&code_editor_dialog::on_replace_enter, this));


	init_files_grid();
}

void code_editor_dialog::add_optional_error_text_area(const std::string& text)
{
	using namespace gui;
	optional_error_text_area_.reset(new TextEditorWidget(width() - 40, 160));
	optional_error_text_area_->setText(text);
	foreach(KnownFile& f, files_) {
		f.editor->setDim(width() - 40, height() - (60 + (optional_error_text_area_ ? 170 : 0)));
	}

	if(editor_) {
		editor_->setDim(width() - 40, height() - (60 + (optional_error_text_area_ ? 170 : 0)));
	}
}

bool code_editor_dialog::jump_to_error(const std::string& text)
{
	if(!editor_) {
		return false;
	}

	const std::string search_for = "At " + fname_ + " ";
	const char* p = strstr(text.c_str(), search_for.c_str());
	if(p) {
		p += search_for.size();
		const int line_num = atoi(p);

		if(line_num > 0) {
			editor_->set_cursor(line_num-1, 0);
		}

		return true;
	} else {
		return false;
	}
}

void code_editor_dialog::init_files_grid()
{
	if(files_grid_) {
		remove_widget(files_grid_);
	}

	if(files_.empty()) {
		return;
	}
	
	using namespace gui;

	files_grid_.reset(new grid(1));
	files_grid_->allow_selection();
	files_grid_->register_selection_callback(boost::bind(&code_editor_dialog::select_file, this, _1));
	foreach(const KnownFile& f, files_) {
		if(f.anim) {
			ImageWidget* img = new ImageWidget(f.anim->img());
			img->setDim(42, 42);
			img->setArea(f.anim->area());

			files_grid_->add_col(WidgetPtr(img));
		} else {
			std::string fname = f.fname;
			while(std::find(fname.begin(), fname.end(), '/') != fname.end()) {
				fname.erase(fname.begin(), std::find(fname.begin(), fname.end(), '/')+1);
			}
			if(fname.size() > 6) {
				fname.resize(6);
			}

			files_grid_->add_col(label::create(fname, graphics::color_white()));
		}
	}

	addWidget(files_grid_, 2, 2);
}

void code_editor_dialog::load_file(std::string fname, bool focus, boost::function<void()>* fn)
{
	if(fname_ == fname) {
		return;
	}

	using namespace gui;

	int index = 0;
	foreach(const KnownFile& f, files_) {
		if(f.fname == fname) {
			break;
		}

		++index;
	}

	if(index == files_.size()) {
		KnownFile f;
		f.fname = fname;
		if(fn) {
			f.op_fn = *fn;
		}
		f.editor.reset(new code_editor_widget(width() - 40, height() - (60 + (optional_error_text_area_ ? 170 : 0))));
		std::string text = json::get_file_contents(fname);
		try {
			file_contents_set_ = true;
			variant doc = json::parse(text, json::JSON_NO_PREPROCESSOR);
			std::cerr << "CHECKING FOR PROTOTYPES: " << doc["prototype"].write_json() << "\n";
			if(doc["prototype"].is_list()) {
				std::map<std::string,std::string> paths;
				module::get_unique_filenames_under_dir("data/object_prototypes", &paths);
				foreach(variant proto, doc["prototype"].as_list()) {
					std::string name = proto.as_string() + ".cfg";
					std::map<std::string,std::string>::const_iterator itor = module::find(paths, name);
					if(itor != paths.end()) {
						load_file(itor->second, false);
					}
				}
			}
		} catch(...) {
			file_contents_set_ = false;
		}

		f.editor->setText(json::get_file_contents(fname));
		f.editor->set_on_change_handler(boost::bind(&code_editor_dialog::on_code_changed, this));
		f.editor->set_onMoveCursor_handler(boost::bind(&code_editor_dialog::onMoveCursor, this));

		foreach(const std::string& obj_type, custom_object_type::get_all_ids()) {
			const std::string* path = custom_object_type::get_object_path(obj_type + ".cfg");
			if(path && *path == fname) {
				try {
					f.anim.reset(new frame(custom_object_type::get(obj_type)->default_frame()));
				} catch(...) {
				}
				break;
			}
		}

		//in case any files have been inserted, update the index.
		index = files_.size();
		files_.push_back(f);
	}

	KnownFile f = files_[index];

	if(editor_) {
		f.editor->setFontSize(editor_->get_font_size());
	}

	if(!focus) {
		return;
	}

	files_.erase(files_.begin() + index);
	files_.insert(files_.begin(), f);

	addWidget(f.editor, editor_->x(), editor_->y());
	remove_widget(editor_);

	editor_ = f.editor;
	op_fn_ = f.op_fn;
	editor_->setFocus(true);

	init_files_grid();

	fname_ = fname;

	if(save_button_) {
		save_button_->setVisible(fname_.empty() || fname_[0] != '@');
	}

	modified_ = editor_->text() != sys::read_file(module::map_file(fname));
	onMoveCursor();
}

void code_editor_dialog::select_file(int index)
{
	if(index < 0 || index >= files_.size()) {
		return;
	}

	std::cerr << "select file " << index << " -> " << files_[index].fname << "\n";

	load_file(files_[index].fname);
}

bool code_editor_dialog::hasKeyboardFocus() const
{
	return editor_->hasFocus() || search_->hasFocus() || replace_->hasFocus();
}

bool code_editor_dialog::handleEvent(const SDL_Event& event, bool claimed)
{
	if(animation_preview_) {
		claimed = animation_preview_->processEvent(event, claimed) || claimed;
		if(claimed) {
			return claimed;
		}
	}

	if(visualize_widget_) {
		claimed = visualize_widget_->processEvent(event, claimed) || claimed;
		if(claimed) {
			return claimed;
		}
	}

	if(suggestions_grid_) {
		//since an event could cause removal of the suggestions grid,
		//make sure the object doesn't get cleaned up until after the event.
		gui::WidgetPtr suggestions = suggestions_grid_;
		claimed = suggestions->processEvent(event, claimed) || claimed;
		if(claimed) {
			return claimed;
		}
	}

	claimed = claimed || dialog::handleEvent(event, claimed);
	if(claimed) {
		return claimed;
	}

	if(hasKeyboardFocus()) {
		switch(event.type) {
		case SDL_KEYDOWN: {
			if(event.key.keysym.sym == SDLK_f && (event.key.keysym.mod&KMOD_CTRL)) {
				search_->setFocus(true);
				replace_->setFocus(false);
				editor_->setFocus(false);
				return true;
			} else if(event.key.keysym.sym == SDLK_n && (event.key.keysym.mod&KMOD_CTRL) || event.key.keysym.sym == SDLK_F3) {
				editor_->next_search_match();
			} else if(event.key.keysym.sym == SDLK_s && (event.key.keysym.mod&KMOD_CTRL)) {
				save();
				return true;
			} else if(event.key.keysym.sym == SDLK_TAB && (event.key.keysym.mod&KMOD_CTRL) && files_grid_) {
				if(!files_grid_->has_must_select()) {
					files_grid_->must_select(true, 1);
				} else {
					files_grid_->must_select(true, (files_grid_->selection()+1)%files_.size());
				}
			}
			break;
		}
		case SDL_KEYUP: {
			if(files_grid_ && files_grid_->has_must_select() && (event.key.keysym.sym == SDLK_LCTRL || event.key.keysym.sym == SDLK_RCTRL)) {
				select_file(files_grid_->selection());
			}
			break;
		}
		}
	}

	return claimed;
}

void code_editor_dialog::handleDraw_children() const
{
	dialog::handleDraw_children();
	if(animation_preview_) {
		animation_preview_->draw();
	}

	if(visualize_widget_) {
		visualize_widget_->draw();
	}

	if(suggestions_grid_) {
		suggestions_grid_->draw();
	}
}

void code_editor_dialog::change_font_size(int amount)
{
	if(editor_) {
		editor_->change_font_size(amount);
	}
}

void code_editor_dialog::process()
{
	using namespace gui;

	if(invalidated_ && SDL_GetTicks() > invalidated_ + 200) {
		try {
			custom_object::reset_current_debug_error();

#if defined(USE_SHADERS)
			gles2::shader::get_and_clear_runtime_error();
#endif

			has_error_ = true;
			file_contents_set_ = true;
			if(op_fn_) {
				json::parse(editor_->text());
				json::set_file_contents(fname_, editor_->text());

				if(strstr(fname_.c_str(), "/objects/")) {
					custom_object_type::set_file_contents(fname_, editor_->text());
				}

				op_fn_();
			} else if(strstr(fname_.c_str(), "/level/")) {
				json::parse(editor_->text());
				json::set_file_contents(fname_, editor_->text());

				level_runner::get_current()->replay_level_from_start();
				
			} else if(strstr(fname_.c_str(), "/tiles/")) {
				std::cerr << "INIT TILE MAP\n";

				const std::string old_contents = json::get_file_contents(fname_);

				//verify the text is parseable before we bother setting it.
				json::parse(editor_->text());
				json::set_file_contents(fname_, editor_->text());
				const variant tiles_data = json::parse_from_file("data/tiles.cfg");
				tile_map::prepare_rebuild_all();
				try {
					std::cerr << "tile_map::init()\n";
					tile_map::init(tiles_data);
					tile_map::rebuild_all();
					std::cerr << "done tile_map::init()\n";
					editor_dialogs::tileset_editor_dialog::global_tile_update();
					foreach(level* lvl, get_all_levels_set()) {
						lvl->rebuild_tiles();
					}
				} catch(...) {
					json::set_file_contents(fname_, old_contents);
					const variant tiles_data = json::parse_from_file("data/tiles.cfg");
					tile_map::init(tiles_data);
					tile_map::rebuild_all();
					editor_dialogs::tileset_editor_dialog::global_tile_update();
					foreach(level* lvl, get_all_levels_set()) {
						lvl->rebuild_tiles();
					}
					throw;
				}
				std::cerr << "INIT TILE MAP OK\n";
#if defined(USE_SHADERS)
			} else if(strstr(fname_.c_str(), "data/shaders.cfg")) {
				std::cerr << "CODE_EDIT_DIALOG FILE: " << fname_ << std::endl;
				gles2::program::load_shaders(editor_->text());
				foreach(level* lvl, get_all_levels_set()) {
					lvl->shaders_updated();
				}
#endif
			} else if(strstr(fname_.c_str(), "classes/") &&
			          std::equal(fname_.end()-4,fname_.end(),".cfg")) {

				std::cerr << "RELOAD FNAME: " << fname_ << "\n";
				std::string::const_iterator slash = fname_.end()-1;
				while(*slash != '/') {
					--slash;
				}
				std::string::const_iterator end = fname_.end()-4;
				const std::string class_name(slash+1, end);;
				json::parse(editor_->text());
				json::set_file_contents(fname_, editor_->text());
				game_logic::invalidate_class_definition(class_name);
				game_logic::formula_object::try_load_class(class_name);
			} else { 
				std::cerr << "SET FILE: " << fname_ << "\n";
				custom_object_type::set_file_contents(fname_, editor_->text());
			}
			error_label_->setText("Ok");
			error_label_->setTooltip("");

			if(optional_error_text_area_) {
				optional_error_text_area_->setText("No errors");
			}

			has_error_ = false;
		} catch(validation_failure_exception& e) {
			file_contents_set_ = false;
			error_label_->setText("Error");
			error_label_->setTooltip(e.msg);

			if(optional_error_text_area_) {
				optional_error_text_area_->setText(e.msg);
			}
		} catch(json::parse_error& e) {
			file_contents_set_ = false;
			error_label_->setText("Error");
			error_label_->setTooltip(e.error_message());
			if(optional_error_text_area_) {
				optional_error_text_area_->setText(e.error_message());
			}
		} catch(...) {
			file_contents_set_ = false;
			error_label_->setText("Error");
			error_label_->setTooltip("Unknown error");

			if(optional_error_text_area_) {
				optional_error_text_area_->setText("Unknown error");
			}
		}
		invalidated_ = 0;
	} else if(custom_object::current_debug_error()) {
		error_label_->setText("Runtime Error");
		error_label_->setTooltip(*custom_object::current_debug_error());
	}

#if defined(USE_SHADERS)
	const std::string shader_error = gles2::shader::get_and_clear_runtime_error();
	if(shader_error != "") {
		error_label_->setText("Runtime Shader Error");
		error_label_->setTooltip(shader_error);
	}
#endif

	const bool show_replace = editor_->has_search_matches();
	replace_label_->setVisible(show_replace);
	replace_->setVisible(show_replace);

	const int cursor_pos = editor_->row_col_to_text_pos(editor_->cursor_row(), editor_->cursor_col());
	const std::string& text = editor_->currentText();

	const gui::code_editor_widget::ObjectInfo info = editor_->get_current_object();
	const json::Token* selected_token = NULL;
	int token_pos = 0;
	foreach(const json::Token& token, info.tokens) {
		const int begin_pos = token.begin - text.c_str();
		const int end_pos = token.end - text.c_str();
		if(cursor_pos >= begin_pos && cursor_pos <= end_pos) {
			token_pos = cursor_pos - begin_pos;
			selected_token = &token;
			break;
		}
	}

	std::vector<Suggestion> suggestions;
	if(selected_token != NULL) {

		const bool at_end = token_pos == selected_token->end - selected_token->begin;
		std::string str(selected_token->begin, selected_token->end);
		suggestions_prefix_ = 0;
		if(str.size() >= 3 && std::string(str.begin(), str.begin()+3) == "on_" && at_end) {
			const std::string id(str.begin()+3, str.end());
			for(int i = 0; i != NUM_OBJECT_BUILTIN_EVENT_IDS; ++i) {
				const std::string& event_str = get_object_event_str(i);
				if(event_str.size() >= id.size() && std::equal(id.begin(), id.end(), event_str.begin())) {
					Suggestion s = { "on_" + event_str, "", ": \"\",", 3 };
					suggestions.push_back(s);
				}
			}

			static std::vector<std::string> animations;

			if(info.obj.is_map() && info.obj["animation"].is_list()) {
				animations.clear();
				foreach(variant anim, info.obj["animation"].as_list()) {
					if(anim.is_map() && anim["id"].is_string()) {
						animations.push_back(anim["id"].as_string());
					}
				}
			}

			foreach(const std::string& str, animations) {
				static const std::string types[] = {"enter", "end", "leave", "process"};
				foreach(const std::string& type, types) {
					const std::string event_str = type + "_" + str + (type == "process" ? "" : "_anim");
					if(event_str.size() >= id.size() && std::equal(id.begin(), id.end(), event_str.begin())) {
						Suggestion s = { "on_" + event_str, "", ": \"\",", 3 };
						suggestions.push_back(s);
					}
				}
			}

			suggestions_prefix_ = str.size();
		} else if(selected_token->type == json::Token::TYPE_STRING) {
			try {
				const std::string formula_str(selected_token->begin, selected_token->end);
				std::vector<formula_tokenizer::token> tokens;
				std::string::const_iterator i1 = formula_str.begin();
				formula_tokenizer::token t = formula_tokenizer::get_token(i1, formula_str.end());
				while(t.type != formula_tokenizer::TOKEN_INVALID) {
					tokens.push_back(t);
					if(i1 == formula_str.end()) {
						break;
					}

					t = formula_tokenizer::get_token(i1, formula_str.end());
				}

				const formula_tokenizer::token* selected = NULL;
				const std::string::const_iterator itor = formula_str.begin() + token_pos;

				foreach(const formula_tokenizer::token& tok, tokens) {
					if(tok.end == itor) {
						selected = &tok;
						break;
					}
				}

				if(selected && selected->type == formula_tokenizer::TOKEN_IDENTIFIER) {
					const std::string identifier(selected->begin, selected->end);

					static const boost::intrusive_ptr<custom_object_callable> obj_definition(new custom_object_callable);
					for(int n = 0; n != obj_definition->num_slots(); ++n) {
						const std::string id = obj_definition->get_entry(n)->id;
						if(id.size() > identifier.size() && std::equal(identifier.begin(), identifier.end(), id.begin())) {
							Suggestion s = { id, "", "", 0 };
							suggestions.push_back(s);
						}
					}

					std::vector<std::string> helpstrings;
					foreach(const std::string& s, function_helpstrings("core")) {
						helpstrings.push_back(s);
					}
					foreach(const std::string& s, function_helpstrings("custom_object")) {
						helpstrings.push_back(s);
					}

					foreach(const std::string& str, helpstrings) {
						std::string::const_iterator paren = std::find(str.begin(), str.end(), '(');
						std::string::const_iterator colon = std::find(paren, str.end(), ':');
						if(colon == str.end()) {
							continue;
						}

						const std::string id(str.begin(), paren);
						const std::string text(str.begin(), colon);
						if(id.size() > identifier.size() && std::equal(identifier.begin(), identifier.end(), id.begin())) {
							Suggestion s = { id, text, "()", 1 };
							suggestions.push_back(s);
						}
					}

					suggestions_prefix_ = identifier.size();
				}
			} catch(formula_tokenizer::token_error&) {
			}
		}

	}

	std::sort(suggestions.begin(), suggestions.end());

	if(suggestions != suggestions_) {
		suggestions_ = suggestions;
		suggestions_grid_.reset();

		if(suggestions_.empty() == false) {
			grid_ptr suggestions_grid(new grid(1));
			suggestions_grid->register_selection_callback(boost::bind(&code_editor_dialog::select_suggestion, this, _1));
			suggestions_grid->swallow_clicks();
			suggestions_grid->allow_selection(true);
			suggestions_grid->set_show_background(true);
			suggestions_grid->set_max_height(160);
			foreach(const Suggestion& s, suggestions_) {
				suggestions_grid->add_col(WidgetPtr(new label(s.suggestion_text.empty() ? s.suggestion : s.suggestion_text)));
			}

			suggestions_grid_.reset(new BorderWidget(suggestions_grid, graphics::color(255,255,255,255)));
		}
		std::cerr << "SUGGESTIONS: " << suggestions_.size() << ":\n";
		foreach(const Suggestion& suggestion, suggestions_) {
			std::cerr << " - " << suggestion.suggestion << "\n";
		}
	}

	if(suggestions_grid_) {
		const std::pair<int,int> cursor_pos = editor_->char_position_on_screen(editor_->cursor_row(), editor_->cursor_col());
		suggestions_grid_->setLoc(x() + editor_->x() + cursor_pos.second, y() + editor_->y() + cursor_pos.first - suggestions_grid_->height());
		
		if(suggestions_grid_->y() < 10) {
			suggestions_grid_->setLoc(suggestions_grid_->x(), suggestions_grid_->y() + suggestions_grid_->height() + 14);
		}

		if(suggestions_grid_->x() + suggestions_grid_->width() + 20 > graphics::screen_width()) {
			suggestions_grid_->setLoc(graphics::screen_width() - suggestions_grid_->width() - 20, suggestions_grid_->y());
		}
	}

	try {
		editor_->set_highlight_current_object(false);
		if(gui::AnimationPreviewWidget::is_animation(info.obj)) {
			if(!animation_preview_) {
				animation_preview_.reset(new gui::AnimationPreviewWidget(info.obj));
				animation_preview_->setRectHandler(boost::bind(&code_editor_dialog::setAnimationRect, this, _1));
				animation_preview_->setSolidHandler(boost::bind(&code_editor_dialog::moveSolidRect, this, _1, _2));
				animation_preview_->setPadHandler(boost::bind(&code_editor_dialog::setIntegerAttr, this, "pad", _1));
				animation_preview_->setNumFramesHandler(boost::bind(&code_editor_dialog::setIntegerAttr, this, "frames", _1));
				animation_preview_->setFramesPerRowHandler(boost::bind(&code_editor_dialog::setIntegerAttr, this, "frames_per_row", _1));
				animation_preview_->setLoc(x() - 520, y() + 100);
				animation_preview_->setDim(500, 400);
				animation_preview_->init();
			} else {
				animation_preview_->setObject(info.obj);
			}

			editor_->set_highlight_current_object(true);
		} else {
			animation_preview_.reset();
		}
	} catch(type_error&) {
		if(animation_preview_) {
			animation_preview_.reset();
		}
	} catch(frame::error&) {
		// skip
	} catch(validation_failure_exception&) {
		if(animation_preview_) {
			animation_preview_.reset();
		}
	} catch(KRE::ImageLoadError&) {
		if(animation_preview_) {
			animation_preview_.reset();
		}
	}

	if(animation_preview_) {
		animation_preview_->process();
	}

	if(visualize_widget_) {
		visualize_widget_->process();
	}
}

void code_editor_dialog::change_width(int amount)
{
	int new_width = width() + amount;
	if(new_width < 200) {
		new_width = 200;
	}

	if(new_width > 1000) {
		new_width = 1000;
	}

	amount = new_width - width();
	setLoc(x() - amount, y());
	setDim(new_width, height());


	foreach(KnownFile& f, files_) {
		f.editor->setDim(width() - 40, height() - (60 + (optional_error_text_area_ ? 170 : 0)));
	}
	init();
}

void code_editor_dialog::on_drag(int dx, int dy) 
{
	int new_width = width() + dx;
	int min_width = int(graphics::screen_width() * 0.17);
	int max_width = int(graphics::screen_width() * 0.83);
	//std::cerr << "ON_DRAG: " << dx << ", " << min_width << ", " << max_width << std::endl;
	if(new_width < min_width) { 
		new_width = min_width;
	}

	if(new_width > max_width) {
		new_width = max_width;
	}

	dx = new_width - width();
	setLoc(x() - dx, y());
	setDim(new_width, height());


	foreach(KnownFile& f, files_) {
		f.editor->setDim(width() - 40, height() - (60 + (optional_error_text_area_ ? 170 : 0)));
	}
	//init();
}

void code_editor_dialog::on_drag_end(int x, int y)
{
	init();
}

void code_editor_dialog::on_tab()
{
	if(search_->hasFocus()) {
		search_->setFocus(false);
		if(editor_->has_search_matches()) {
			replace_->setFocus(true);
		} else {
			editor_->setFocus(true);
		}
	} else if(replace_->hasFocus()) {
		replace_->setFocus(false);
		editor_->setFocus(true);
	}
}

void code_editor_dialog::on_search_changed()
{
	editor_->set_search(search_->text());
}

void code_editor_dialog::on_search_enter()
{
	search_->setFocus(false);
	replace_->setFocus(false);
	editor_->setFocus(true);
}

void code_editor_dialog::on_replace_enter()
{
	editor_->replace(replace_->text());
}

void code_editor_dialog::on_code_changed()
{
	if(!modified_) {
		modified_ = true;
		onMoveCursor();
	}

	if(!invalidated_) {
		invalidated_ = SDL_GetTicks();
		error_label_->setText("Processing...");
	}
}

namespace {
void visit_potential_formula_str(variant candidate, variant* result, int row, int col)
{
	if(candidate.is_string() && candidate.get_debug_info()) {
		const variant::debug_info* info = candidate.get_debug_info();
		if(row > info->line && row < info->end_line ||
		   row == info->line && col >= info->column && (row < info->end_line || col <= info->end_column) ||
		   row == info->end_line && col <= info->end_column && (row > info->line || col >= info->column)) {
			*result = candidate;
		}
	}
}
}

void code_editor_dialog::onMoveCursor()
{
	visualize_widget_.reset();

	status_label_->setText(formatter() << "Line " << (editor_->cursor_row()+1) << " Col " << (editor_->cursor_col()+1) << (modified_ ? " (Modified)" : ""));

	if(file_contents_set_) {
		try {
			variant v = json::parse_from_file(fname_);
			variant formula_str;
			assert(v.is_map());
			visit_variants(v, boost::bind(visit_potential_formula_str, _1, &formula_str, editor_->cursor_row()+1, editor_->cursor_col()+1));

			if(formula_str.is_string()) {

				const variant::debug_info& str_info = *formula_str.get_debug_info();
				const std::set<game_logic::formula*>& formulae = game_logic::formula::get_all();
				int best_result = -1;
				variant result_variant;
				const game_logic::formula* best_formula = NULL;
				foreach(const game_logic::formula* f, formulae) {
					const variant::debug_info* info = f->str_var().get_debug_info();
					if(!info || !info->filename || *info->filename != *str_info.filename) {
						continue;
					}

					variant result;
					visit_potential_formula_str(f->str_var(), &result, editor_->cursor_row()+1, editor_->cursor_col()+1);
					if(result.is_null()) {
						continue;
					}

					const int result_scope = (info->end_line - info->line)*1024 + (info->end_column - info->column);
					if(best_result == -1 || result_scope <= best_result) {
						result_variant = result;
						best_result = result_scope;
						best_formula = f;
					}
				}

				if(best_formula) {
					const variant::debug_info& info = *result_variant.get_debug_info();
					const int text_pos = editor_->row_col_to_text_pos(editor_->cursor_row(), editor_->cursor_col()) - editor_->row_col_to_text_pos(info.line-1, info.column-1);
					//TODO: make the visualize widget good enough to use.
					//visualize_widget_.reset(new gui::formula_visualize_widget(best_formula->expr(), text_pos, editor_->cursor_row()+1, editor_->cursor_col()+1, 20, 20, 500, 400, editor_.get()));
				}
			}

		} catch(...) {
			std::cerr << "ERROR PARSING FORMULA SET\n";
		}
	} else {
		std::cerr << "NO FORMULA SET\n";
	}
}

void code_editor_dialog::setAnimationRect(rect r)
{
	const gui::code_editor_widget::ObjectInfo info = editor_->get_current_object();
	variant v = info.obj;
	if(v.is_null() == false) {
		v.add_attr(variant("rect"), r.write());
		editor_->modify_current_object(v);
		try {
			animation_preview_->setObject(v);
		} catch(frame::error& e) {
		}
	}
}

void code_editor_dialog::moveSolidRect(int dx, int dy)
{
	const gui::code_editor_widget::ObjectInfo info = editor_->get_current_object();
	variant v = info.obj;
	if(v.is_null() == false) {
		variant solid_area = v["solid_area"];
		if(!solid_area.is_list() || solid_area.num_elements() != 4) {
			return;
		}

		foreach(const variant& num, solid_area.as_list()) {
			if(!num.is_int()) {
				return;
			}
		}

		rect area(solid_area);
		area = rect(area.x() + dx, area.y() + dy, area.w(), area.h());
		v.add_attr(variant("solid_area"), area.write());
		editor_->modify_current_object(v);
		try {
			animation_preview_->setObject(v);
		} catch(frame::error& e) {
		}
	}
}

void code_editor_dialog::setIntegerAttr(const char* attr, int value)
{
	const gui::code_editor_widget::ObjectInfo info = editor_->get_current_object();
	variant v = info.obj;
	if(v.is_null() == false) {
		v.add_attr(variant(attr), variant(value));
		editor_->modify_current_object(v);
		try {
			animation_preview_->setObject(v);
		} catch(frame::error& e) {
		}
	}
}

void code_editor_dialog::save()
{
	sys::write_file(module::map_file(fname_), editor_->text());
	status_label_->setText(formatter() << "Saved " << fname_);
	modified_ = false;
}

void code_editor_dialog::save_and_close()
{
	save();
	close();
}

void code_editor_dialog::select_suggestion(int index)
{
	if(index >= 0 && index < suggestions_.size()) {
		std::cerr << "SELECT " << suggestions_[index].suggestion << "\n";
		const std::string& str = suggestions_[index].suggestion;
		if(suggestions_prefix_ >= 0 && suggestions_prefix_ < str.size()) {
			const int col = editor_->cursor_col();
			const std::string insert(str.begin() + suggestions_prefix_, str.end());
			const std::string postfix = suggestions_[index].postfix;
			const std::string row = editor_->get_data()[editor_->cursor_row()];
			const std::string new_row = std::string(row.begin(), row.begin() + editor_->cursor_col()) + insert + postfix + std::string(row.begin() + editor_->cursor_col(), row.end());
			editor_->set_row_contents(editor_->cursor_row(), new_row);
			editor_->set_cursor(editor_->cursor_row(), col + insert.size() + suggestions_[index].postfix_index);
		}
	} else {
		suggestions_grid_.reset();
	}
}

void edit_and_continue_class(const std::string& class_name, const std::string& error)
{
	boost::intrusive_ptr<code_editor_dialog> d(new code_editor_dialog(rect(0,0,graphics::screen_width(),graphics::screen_height())));

	const std::string::const_iterator end_itor = std::find(class_name.begin(), class_name.end(), '.');
	const std::string filename = "data/classes/" + std::string(class_name.begin(), end_itor) + ".cfg";

	d->set_process_hook(boost::bind(&code_editor_dialog::process, d.get()));
	d->add_optional_error_text_area(error);
	d->set_close_buttons();
	d->init();
	d->load_file(filename);
	d->jump_to_error(error);
	d->set_on_quit(boost::bind(&gui::dialog::cancel, d.get()));
	d->show_modal();

	if(d->cancelled()) {
		_exit(0);
	}
}

void edit_and_continue_fn(const std::string& filename, const std::string& error, boost::function<void()> fn)
{
	boost::intrusive_ptr<code_editor_dialog> d(new code_editor_dialog(rect(0,0,graphics::screen_width(),graphics::screen_height())));

	d->set_process_hook(boost::bind(&code_editor_dialog::process, d.get()));
	d->add_optional_error_text_area(error);
	d->set_close_buttons();
	d->init();
	d->load_file(filename, true, &fn);
	const bool result = d->jump_to_error(error);
	if(!result) {
		const char* fname = strstr(error.c_str(), "\nAt ");
		if(fname) {
			fname += 4;
			const char* end_fname = strstr(fname, " ");
			if(end_fname) {
				const std::string file(fname, end_fname);
				d->load_file(file, true, &fn);
				d->jump_to_error(error);
			}
		}
	}
	d->show_modal();

	if(d->cancelled() || d->has_error()) {
		_exit(0);
	}
}

namespace {
void try_fix_assert()
{
}
}

void edit_and_continue_assert(const std::string& msg, boost::function<void()> fn)
{
	const std::vector<CallStackEntry>& stack = get_expression_call_stack();
	std::vector<CallStackEntry> reverse_stack = stack;
	std::reverse(reverse_stack.begin(), reverse_stack.end());
	if(stack.empty() || !level::current_ptr()) {
		return;
	}

	int w, h;
	get_main_window()->auto_window_size(w,h);
	get_main_window()->set_window_size(w,h);

	using namespace gui;

	using debug_console::ConsoleDialog;

	boost::intrusive_ptr<ConsoleDialog> console(new ConsoleDialog(level::current(), *const_cast<game_logic::FormulaCallable*>(stack.back().callable)));

	grid_ptr call_grid(new grid(1));
	call_grid->set_max_height(graphics::screen_height() - console->y());
	call_grid->allow_selection();
	call_grid->must_select();
	foreach(const CallStackEntry& entry, reverse_stack) {
		std::string str = entry.expression->str();
		std::string::iterator i = std::find(str.begin(), str.end(), '\n');
		if(i != str.end()) {
			str.erase(i, str.end());
		}
		call_grid->add_col(WidgetPtr(new label(str)));
	}

	call_grid->setLoc(console->x() + console->width() + 6, console->y());
	call_grid->setDim(graphics::screen_width() - call_grid->x(), graphics::screen_height() - call_grid->y());

	boost::intrusive_ptr<code_editor_dialog> d(new code_editor_dialog(rect(graphics::screen_width()/2,0,graphics::screen_width()/2,console->y())));

	d->set_close_buttons();
	d->show();
	d->init();

	const variant::debug_info* debug_info = stack.back().expression->parent_formula().get_debug_info();
	if(debug_info && debug_info->filename) {
		if(!fn) {
			fn = boost::function<void()>(try_fix_assert);
		}
		d->load_file(*debug_info->filename, true, &fn);
		d->jump_to_error(msg);
	}

	std::vector<WidgetPtr> widgets;
	widgets.push_back(d);
	widgets.push_back(console);
	widgets.push_back(call_grid);

	bool quit = false;
	while(!quit && !d->closed()) {

		SDL_Event event;
		while(input::sdl_poll_event(&event)) {
			switch(event.type) {
				case SDL_QUIT: {
					quit = true;
					break;
				}
			}

			bool swallowed = false;
			foreach(WidgetPtr w, widgets) {
				if(!swallowed) {
					swallowed = w->processEvent(event, swallowed) || swallowed;
				}
			}
		}

		d->process();

		console->prepare_draw();
		foreach(WidgetPtr w, widgets) {
			w->draw();
		}
		console->complete_draw();
	}

	if(quit || d->cancelled() || d->has_error()) {
		_exit(0);
	}
}

COMMAND_LINE_UTILITY(codeedit)
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* wnd = SDL_CreateWindow("Code Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 600, 600, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	SDL_GLContext ctx = SDL_GL_CreateContext(wnd);
#ifdef USE_SHADERS
	glViewport(0, 0, 600, 600);
	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#else
	glShadeModel(GL_SMOOTH);
	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

	const font::manager font_manager;
	graphics::texture::manager texture_manager;

	variant gui_node = json::parse_from_file("data/gui.cfg");
	GuiSection::init(gui_node);

	FramedGuiElement::init(gui_node);

	code_editor_dialog d(rect(0,0,600,600));
	std::cerr << "CREATE DIALOG\n";
	if(args.empty() == false) {
		d.load_file(args[0]);
	}

	d.show_modal();

	SDL_GL_DeleteContext(ctx);
	SDL_DestroyWindow(wnd);
}

#endif // NO_EDITOR
