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

#include <algorithm>

#include "Font.hpp"
#include "WindowManager.hpp"
#include "SDLWrapper.hpp"

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
#include "profile_timer.hpp"
#include "text_editor_widget.hpp"
#include "tile_map.hpp"
#include "tileset_editor_dialog.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"

namespace game_logic
{
void invalidate_class_definition(const std::string& class_name);
}

PREF_INT(code_editor_error_area, 300, "");

std::set<Level*>& get_all_levels_set();

CodeEditorDialog::CodeEditorDialog(const rect& r)
  : Dialog(r.x(), r.y(), r.w(), r.h()), invalidated_(0), has_error_(false),
    modified_(false), file_contents_set_(true), suggestions_prefix_(-1),
	have_close_buttons_(false)
{
	init();
}

void CodeEditorDialog::init()
{
	clear();

	using namespace gui;

	const int EDITOR_BUTTONS_X = 42;
	const int EDITOR_BUTTONS_Y = 12;
	const int Y_SPACING        = 4;

	if(!editor_) {
		editor_.reset(new CodeEditorWidget(width() - 40, height() - (60 + (optional_error_text_area_ ? g_code_editor_error_area : 0))));
	}

	Button* save_button      = new Button("Save", std::bind(&CodeEditorDialog::save, this));
	Button* undo_button      = new Button("Undo", std::bind(&CodeEditorDialog::undo, this));
	Button* redo_button      = new Button("Redo", std::bind(&CodeEditorDialog::redo, this));
	Button* increase_font    = new Button("Increase font size", std::bind(&CodeEditorDialog::changeFontSize, this, 1));
	Button* decrease_font    = new Button("Decrease font size", std::bind(&CodeEditorDialog::changeFontSize, this, -1));
	
	find_next_button_ = new Button("Find next", std::bind(&CodeEditorDialog::on_find_next, this));
	save_button_.reset(save_button);

	using std::placeholders::_1;
	using std::placeholders::_2;

	DragWidget* dragger = new DragWidget(x(), y(), width(), height(),
		DragWidget::Direction::HORIZONTAL, [](int,int){}, 
		std::bind(&CodeEditorDialog::on_drag_end, this, _1, _2), 
		std::bind(&CodeEditorDialog::on_drag, this, _1, _2));

	search_ = new TextEditorWidget(120);
	replace_ = new TextEditorWidget(120);
	const KRE::Color col = KRE::Color::colorWhite();

	WidgetPtr change_font_label(Label::create("Change font size:", col));
	WidgetPtr find_label(Label::create("Find: ", col));
	replace_label_ = Label::create("Replace: ", col);
	status_label_ = Label::create(" ", col);
	error_label_ = Label::create("Ok", col);
	error_label_->setTooltip("No errors detected");

	// Saving and fonts
	addWidget(WidgetPtr(save_button), EDITOR_BUTTONS_X, EDITOR_BUTTONS_Y, MOVE_DIRECTION::RIGHT);

	if(have_close_buttons_) {
		Button* save_and_close_button = new Button("Save+Close", std::bind(&CodeEditorDialog::save_and_close, this));
		Button* abort_button = new Button("Abort", std::bind(&Dialog::cancel, this));
		addWidget(WidgetPtr(save_and_close_button), MOVE_DIRECTION::RIGHT);
		addWidget(WidgetPtr(abort_button), MOVE_DIRECTION::RIGHT);
	}


	addWidget(WidgetPtr(undo_button), MOVE_DIRECTION::RIGHT);
	addWidget(WidgetPtr(redo_button), MOVE_DIRECTION::RIGHT);
	addWidget(WidgetPtr(increase_font), MOVE_DIRECTION::RIGHT);
	addWidget(WidgetPtr(decrease_font), MOVE_DIRECTION::RIGHT);

	// Search and replace
	addWidget(find_label, EDITOR_BUTTONS_X, save_button->y() + save_button->height() + Y_SPACING, MOVE_DIRECTION::RIGHT);
	addWidget(WidgetPtr(search_), MOVE_DIRECTION::RIGHT);
	addWidget(replace_label_, MOVE_DIRECTION::RIGHT);
	addWidget(WidgetPtr(replace_), MOVE_DIRECTION::RIGHT);
	addWidget(WidgetPtr(find_next_button_), MOVE_DIRECTION::RIGHT);

	addWidget(editor_, find_label->x(), search_->y() + search_->height() + Y_SPACING);
	if(optional_error_text_area_) {
		addWidget(optional_error_text_area_);
	}
	addWidget(status_label_);
	addWidget(error_label_, status_label_->x() + 480, status_label_->y());
	addWidget(WidgetPtr(dragger));

	replace_label_->setVisible(false);
	replace_->setVisible(false);
	find_next_button_->setVisible(false);

	if(fname_.empty() == false && fname_[0] == '@') {
		save_button->setVisible(false);
	}

	search_->setOnTabHandler(std::bind(&CodeEditorDialog::on_tab, this));
	replace_->setOnTabHandler(std::bind(&CodeEditorDialog::on_tab, this));

	search_->setOnChangeHandler(std::bind(&CodeEditorDialog::on_search_changed, this));
	search_->setOnEnterHandler(std::bind(&CodeEditorDialog::on_search_enter, this));
	replace_->setOnEnterHandler(std::bind(&CodeEditorDialog::on_replace_enter, this));


	init_files_grid();
}

void CodeEditorDialog::add_optional_error_text_area(const std::string& text)
{
	using namespace gui;
	optional_error_text_area_.reset(new TextEditorWidget(width() - 40, g_code_editor_error_area-10));
	optional_error_text_area_->setText(text);
	for(KnownFile& f : files_) {
		f.editor->setDim(width() - 40, height() - (60 + (optional_error_text_area_ ? g_code_editor_error_area : 0)));
	}

	if(editor_) {
		editor_->setDim(width() - 40, height() - (60 + (optional_error_text_area_ ? g_code_editor_error_area : 0)));
	}
}

bool CodeEditorDialog::jump_to_error(const std::string& text)
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
			editor_->setCursor(line_num-1, 0);
		}

		return true;
	} else {
		return false;
	}
}

void CodeEditorDialog::init_files_grid()
{
	if(files_grid_) {
		removeWidget(files_grid_);
	}

	if(files_.empty()) {
		return;
	}
	
	using namespace gui;
	using std::placeholders::_1;

	files_grid_.reset(new Grid(1));
	files_grid_->allowSelection();
	files_grid_->registerSelectionCallback(std::bind(&CodeEditorDialog::select_file, this, _1));
	for(const KnownFile& f : files_) {
		if(f.anim) {
			ImageWidget* img = new ImageWidget(f.anim->img());
			img->setDim(42, 42);
			img->setArea(f.anim->area());

			files_grid_->addCol(WidgetPtr(img));
		} else {
			std::string fname = f.fname;
			while(std::find(fname.begin(), fname.end(), '/') != fname.end()) {
				fname.erase(fname.begin(), std::find(fname.begin(), fname.end(), '/')+1);
			}
			if(fname.size() > 6) {
				fname.resize(6);
			}

			files_grid_->addCol(Label::create(fname, KRE::Color::colorWhite()));
		}
	}

	addWidget(files_grid_, 2, 2);
}

void CodeEditorDialog::load_file(std::string fname, bool focus, std::function<void()>* fn)
{
	if(fname_ == fname) {
		return;
	}

	using namespace gui;

	int index = 0;
	for(const KnownFile& f : files_) {
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
		f.editor.reset(new CodeEditorWidget(width() - 40, height() - (60 + (optional_error_text_area_ ? g_code_editor_error_area : 0))));
		std::string text = json::get_file_contents(fname);
		try {
			file_contents_set_ = true;
			variant doc = json::parse(text, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
			LOG_INFO("CHECKING FOR PROTOTYPES: " << doc["prototype"].write_json());
			if(doc["prototype"].is_list()) {
				std::map<std::string,std::string> paths;
				module::get_unique_filenames_under_dir("data/object_prototypes", &paths);
				for(variant proto : doc["prototype"].as_list()) {
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
		f.editor->setOnChangeHandler(std::bind(&CodeEditorDialog::on_code_changed, this));
		f.editor->setOnMoveCursorHandler(std::bind(&CodeEditorDialog::onMoveCursor, this));

		for(const std::string& obj_type : CustomObjectType::getAllIds()) {
			const std::string* path = CustomObjectType::getObjectPath(obj_type + ".cfg");
			if(path && *path == fname) {
				try {
					f.anim.reset(new Frame(CustomObjectType::get(obj_type)->defaultFrame()));
				} catch(...) {
				}
				break;
			}
		}

		//in case any files have been inserted, update the index.
		index = static_cast<int>(files_.size());
		files_.push_back(f);
	}

	KnownFile f = files_[index];

	if(editor_) {
		f.editor->setFontSize(editor_->getFontSize());
	}

	if(!focus) {
		return;
	}

	files_.erase(files_.begin() + index);
	files_.insert(files_.begin(), f);

	addWidget(f.editor, editor_->x(), editor_->y());
	removeWidget(editor_);

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

void CodeEditorDialog::select_file(int index)
{
	if(index < 0 || static_cast<unsigned>(index) >= files_.size()) {
		return;
	}

	LOG_INFO("select file " << index << " -> " << files_[index].fname);

	load_file(files_[index].fname);
}

bool CodeEditorDialog::hasKeyboardFocus() const
{
	return editor_->hasFocus() || search_->hasFocus() || replace_->hasFocus();
}

bool CodeEditorDialog::handleEvent(const SDL_Event& event, bool claimed)
{
	if(animation_preview_) {
		claimed = animation_preview_->processEvent(getPos(), event, claimed) || claimed;
		if(claimed) {
			return claimed;
		}
	}

	if(visualize_widget_) {
		claimed = visualize_widget_->processEvent(getPos(), event, claimed) || claimed;
		if(claimed) {
			return claimed;
		}
	}

	if(suggestions_grid_) {
		//since an event could cause removal of the suggestions grid,
		//make sure the object doesn't get cleaned up until after the event.
		gui::WidgetPtr suggestions = suggestions_grid_;
		claimed = suggestions->processEvent(getPos(), event, claimed) || claimed;
		if(claimed) {
			return claimed;
		}
	}

	claimed = claimed || Dialog::handleEvent(event, claimed);
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
			} else if((event.key.keysym.sym == SDLK_n && (event.key.keysym.mod&KMOD_CTRL)) || event.key.keysym.sym == SDLK_F3) {
				editor_->nextSearchMatch();
			} else if(event.key.keysym.sym == SDLK_s && (event.key.keysym.mod&KMOD_CTRL)) {
				save();
				return true;
			} else if(event.key.keysym.sym == SDLK_TAB && (event.key.keysym.mod&KMOD_CTRL) && files_grid_) {
				if(!files_grid_->hasMustSelect()) {
					files_grid_->mustSelect(true, 1);
				} else {
					files_grid_->mustSelect(true, (files_grid_->selection()+1)%files_.size());
				}
			}
			break;
		}
		case SDL_KEYUP: {
			if(files_grid_ && files_grid_->hasMustSelect() && (event.key.keysym.sym == SDLK_LCTRL || event.key.keysym.sym == SDLK_RCTRL)) {
				select_file(files_grid_->selection());
			}
			break;
		}
		}
	}

	return claimed;
}

void CodeEditorDialog::handleDrawChildren() const
{
	Dialog::handleDrawChildren();
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

void CodeEditorDialog::undo()
{
	if(editor_) {
		editor_->undo();
	}
}

void CodeEditorDialog::redo()
{
	if(editor_) {
		editor_->redo();
	}
}

void CodeEditorDialog::changeFontSize(int amount)
{
	if(editor_) {
		editor_->changeFontSize(amount);
	}
}

void CodeEditorDialog::process()
{
	using namespace gui;

	using std::placeholders::_1;
	using std::placeholders::_2;

	sys::pump_file_modifications();

	if(invalidated_ && profile::get_tick_time() > invalidated_ + 200) {
		try {
			CustomObject::resetCurrentDebugError();

			has_error_ = true;
			file_contents_set_ = true;
			if(op_fn_) {
				json::parse(editor_->text());
				json::set_file_contents(fname_, editor_->text());

				if(strstr(fname_.c_str(), "/objects/")) {
					CustomObjectType::setFileContents(fname_, editor_->text());
				}

				op_fn_();
			} else if(strstr(fname_.c_str(), "/level/")) {
				json::parse(editor_->text());
				json::set_file_contents(fname_, editor_->text());

				LevelRunner::getCurrent()->replay_level_from_start();
				
			} else if(strstr(fname_.c_str(), "/tiles/")) {
				LOG_INFO("INIT TILE MAP");

				const std::string old_contents = json::get_file_contents(fname_);

				//verify the text is parseable before we bother setting it.
				json::parse(editor_->text());
				json::set_file_contents(fname_, editor_->text());
				const variant tiles_data = json::parse_from_file("data/tiles.cfg");
				TileMap::prepareRebuildAll();
				try {
					LOG_INFO("tile_map::init()");
					TileMap::init(tiles_data);
					TileMap::rebuildAll();
					LOG_INFO("done tile_map::init()");
					editor_dialogs::TilesetEditorDialog::globalTileUpdate();
					for(Level* lvl : get_all_levels_set()) {
						lvl->rebuildTiles();
					}
				} catch(...) {
					json::set_file_contents(fname_, old_contents);
					const variant tiles_data = json::parse_from_file("data/tiles.cfg");
					TileMap::init(tiles_data);
					TileMap::rebuildAll();
					editor_dialogs::TilesetEditorDialog::globalTileUpdate();
					for(Level* lvl : get_all_levels_set()) {
						lvl->rebuildTiles();
					}
					throw;
				}
				LOG_INFO("INIT TILE MAP OK");
			} else if(strstr(fname_.c_str(), "data/shaders.cfg")) {
				LOG_INFO("CODE_EDIT_DIALOG FILE: " << fname_);
				//ASSERT_LOG(false, "XXX edited shaders file fixme");

				variant node = json::parse(editor_->text());
				KRE::ShaderProgram::loadFromVariant(node);
				for(Level* lvl : get_all_levels_set()) {
					lvl->shadersUpdated();
				}
			} else if(strstr(fname_.c_str(), "classes/") &&
			          std::equal(fname_.end()-4,fname_.end(),".cfg")) {

				LOG_INFO("RELOAD FNAME: " << fname_);
				std::string::const_iterator slash = fname_.end()-1;
				while(*slash != '/') {
					--slash;
				}
				std::string::const_iterator end = fname_.end()-4;
				const std::string class_name(slash+1, end);;
				json::parse(editor_->text());
				json::set_file_contents(fname_, editor_->text());
				game_logic::invalidate_class_definition(class_name);
				game_logic::FormulaObject::tryLoadClass(class_name);
			} else { 
				LOG_INFO("SET FILE: " << fname_);
				CustomObjectType::setFileContents(fname_, editor_->text());
			}
			error_label_->setText("Ok");
			error_label_->setTooltip("No errors detected");

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
		} catch(json::ParseError& e) {
			file_contents_set_ = false;
			error_label_->setText("Error");
			error_label_->setTooltip(e.errorMessage());
			if(optional_error_text_area_) {
				optional_error_text_area_->setText(e.errorMessage());
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
	} else if(CustomObject::currentDebugError()) {
		error_label_->setText("Runtime Error");
		error_label_->setTooltip(*CustomObject::currentDebugError());
	}

	const bool show_replace = editor_->hasSearchMatches();
	replace_label_->setVisible(show_replace);
	replace_->setVisible(show_replace);
	find_next_button_->setVisible(show_replace);

	const int cursor_pos = static_cast<int>(editor_->rowColToTextPos(editor_->cursorRow(), editor_->cursorCol()));
	const std::string& text = editor_->currentText();

	const gui::CodeEditorWidget::ObjectInfo info = editor_->getCurrentObject();
	const json::Token* selected_token = nullptr;
	int token_pos = 0;
	for(const json::Token& token : info.tokens) {
		const auto begin_pos = token.begin - text.c_str();
		const auto end_pos = token.end - text.c_str();
		if(cursor_pos >= begin_pos && cursor_pos <= end_pos) {
			token_pos = static_cast<int>(cursor_pos - begin_pos);
			selected_token = &token;
			break;
		}
	}

	std::vector<Suggestion> suggestions;
	if(selected_token != nullptr) {

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
				for(variant anim : info.obj["animation"].as_list()) {
					if(anim.is_map() && anim["id"].is_string()) {
						animations.push_back(anim["id"].as_string());
					}
				}
			}

			for(const std::string& str : animations) {
				static const std::string types[] = {"enter", "end", "leave", "process"};
				for(const std::string& type : types) {
					const std::string event_str = type + "_" + str + (type == "process" ? "" : "_anim");
					if(event_str.size() >= id.size() && std::equal(id.begin(), id.end(), event_str.begin())) {
						Suggestion s = { "on_" + event_str, "", ": \"\",", 3 };
						suggestions.push_back(s);
					}
				}
			}

			suggestions_prefix_ = static_cast<int>(str.size());
		} else if(selected_token->type == json::Token::TYPE::STRING) {
			try {
				const std::string formula_str(selected_token->begin, selected_token->end);
				std::vector<formula_tokenizer::Token> tokens;
				std::string::const_iterator i1 = formula_str.begin();
				formula_tokenizer::Token t = formula_tokenizer::get_token(i1, formula_str.end());
				while(t.type != formula_tokenizer::FFL_TOKEN_TYPE::INVALID) {
					tokens.push_back(t);
					if(i1 == formula_str.end()) {
						break;
					}

					t = formula_tokenizer::get_token(i1, formula_str.end());
				}

				const formula_tokenizer::Token* selected = nullptr;
				const std::string::const_iterator itor = formula_str.begin() + token_pos;

				for(const formula_tokenizer::Token& tok : tokens) {
					if(tok.end == itor) {
						selected = &tok;
						break;
					}
				}

				if(selected && selected->type == formula_tokenizer::FFL_TOKEN_TYPE::IDENTIFIER) {
					const std::string identifier(selected->begin, selected->end);

					static const ffl::IntrusivePtr<CustomObjectCallable> obj_definition(new CustomObjectCallable);
					for(int n = 0; n != obj_definition->getNumSlots(); ++n) {
						const std::string id = obj_definition->getEntry(n)->id;
						if(id.size() > identifier.size() && std::equal(identifier.begin(), identifier.end(), id.begin())) {
							Suggestion s = { id, "", "", 0 };
							suggestions.push_back(s);
						}
					}

					std::vector<std::string> helpstrings;
					for(const std::string& s : function_helpstrings("core")) {
						helpstrings.push_back(s);
					}
					for(const std::string& s : function_helpstrings("custom_object")) {
						helpstrings.push_back(s);
					}

					for(const std::string& str : helpstrings) {
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

					suggestions_prefix_ = static_cast<int>(identifier.size());
				}
			} catch(formula_tokenizer::TokenError&) {
			}
		}

	}

	std::sort(suggestions.begin(), suggestions.end());

	if(suggestions != suggestions_) {
		suggestions_ = suggestions;
		suggestions_grid_.reset();

		if(suggestions_.empty() == false) {
			GridPtr suggestions_grid(new Grid(1));
			suggestions_grid->registerSelectionCallback(std::bind(&CodeEditorDialog::select_suggestion, this, _1));
			suggestions_grid->swallowClicks();
			suggestions_grid->allowSelection(true);
			suggestions_grid->setShowBackground(true);
			suggestions_grid->setMaxHeight(160);
			for(const Suggestion& s : suggestions_) {
				suggestions_grid->addCol(WidgetPtr(new Label(s.suggestion_text.empty() ? s.suggestion : s.suggestion_text)));
			}

			suggestions_grid_.reset(new BorderWidget(suggestions_grid, KRE::Color::colorWhite()));
		}
		LOG_INFO("SUGGESTIONS: " << suggestions_.size() << ":");
		for(const Suggestion& suggestion : suggestions_) {
			LOG_INFO(" - " << suggestion.suggestion);
		}
	}

	if(suggestions_grid_) {
		const auto cursor_pos = editor_->charPositionOnScreen(editor_->cursorRow(), editor_->cursorCol());
		suggestions_grid_->setLoc(static_cast<int>(x() + editor_->x() + cursor_pos.second), static_cast<int>(y() + editor_->y() + cursor_pos.first - suggestions_grid_->height()));
		
		if(suggestions_grid_->y() < 10) {
			suggestions_grid_->setLoc(suggestions_grid_->x(), suggestions_grid_->y() + suggestions_grid_->height() + 14);
		}

		auto wnd_w = KRE::WindowManager::getMainWindow()->width();
		if(suggestions_grid_->x() + suggestions_grid_->width() + 20 > wnd_w) {
			suggestions_grid_->setLoc(wnd_w - suggestions_grid_->width() - 20, suggestions_grid_->y());
		}
	}

	try {
		editor_->setHighlightCurrentObject(false);
		if(gui::AnimationPreviewWidget::is_animation(info.obj)) {
			if(!animation_preview_) {
				animation_preview_.reset(new gui::AnimationPreviewWidget(info.obj));
				animation_preview_->setRectHandler(std::bind(&CodeEditorDialog::setAnimationRect, this, _1));
				animation_preview_->setSolidHandler(std::bind(&CodeEditorDialog::moveSolidRect, this, _1, _2));
				animation_preview_->setPadHandler(std::bind(&CodeEditorDialog::setIntegerAttr, this, "pad", _1));
				animation_preview_->setNumFramesHandler(std::bind(&CodeEditorDialog::setIntegerAttr, this, "frames", _1));
				animation_preview_->setFramesPerRowHandler(std::bind(&CodeEditorDialog::setIntegerAttr, this, "frames_per_row", _1));
				animation_preview_->setLoc(x() - 520, y() + 100);
				animation_preview_->setDim(500, 400);
				animation_preview_->init();
			} else {
				animation_preview_->setObject(info.obj);
			}

			editor_->setHighlightCurrentObject(true);
		} else {
			animation_preview_.reset();
		}
	} catch(type_error&) {
		if(animation_preview_) {
			animation_preview_.reset();
		}
	} catch(Frame::Error&) {
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

void CodeEditorDialog::change_width(int amount)
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


	for(KnownFile& f : files_) {
		f.editor->setDim(width() - 40, height() - (60 + (optional_error_text_area_ ? g_code_editor_error_area : 0)));
	}
	init();
}

void CodeEditorDialog::on_drag(int dx, int dy) 
{
	auto wnd_w = KRE::WindowManager::getMainWindow()->width();
	int new_width = width() + dx;
	int min_width = int(wnd_w * 0.17);
	int max_width = int(wnd_w * 0.83);
	//LOG_INFO("ON_DRAG: " << dx << ", " << min_width << ", " << max_width);
	if(new_width < min_width) { 
		new_width = min_width;
	}

	if(new_width > max_width) {
		new_width = max_width;
	}

	dx = new_width - width();
	setLoc(x() - dx, y());
	setDim(new_width, height());


	for(KnownFile& f : files_) {
		f.editor->setDim(width() - 40, height() - (60 + (optional_error_text_area_ ? g_code_editor_error_area : 0)));
	}
	//init();
}

void CodeEditorDialog::on_drag_end(int x, int y)
{
	init();
}

void CodeEditorDialog::on_tab()
{
	if(search_->hasFocus()) {
		search_->setFocus(false);
		if(editor_->hasSearchMatches()) {
			replace_->setFocus(true);
		} else {
			editor_->setFocus(true);
		}
	} else if(replace_->hasFocus()) {
		replace_->setFocus(false);
		editor_->setFocus(true);
	}
}

void CodeEditorDialog::on_search_changed()
{
	editor_->setSearch(search_->text());
}

void CodeEditorDialog::on_search_enter()
{
	search_->setFocus(false);
	replace_->setFocus(false);
	editor_->setFocus(true);
}

void CodeEditorDialog::on_find_next()
{
	editor_->nextSearchMatch();
	search_->setFocus(false);
	replace_->setFocus(false);
	editor_->setFocus(true);
}

void CodeEditorDialog::on_replace_enter()
{
	editor_->replace(replace_->text());
}

void CodeEditorDialog::on_code_changed()
{
	if(!modified_) {
		modified_ = true;
		onMoveCursor();
	}

	if(!invalidated_) {
		invalidated_ = profile::get_tick_time();
		error_label_->setText("Processing...");
	}
}

namespace {
void visit_potential_formula_str(variant candidate, variant* result, int row, int col)
{
	if(candidate.is_string() && candidate.get_debug_info()) {
		const variant::debug_info* info = candidate.get_debug_info();
		if((row > info->line && row < info->end_line) ||
		   (row == info->line && col >= info->column && (row < info->end_line || col <= info->end_column)) ||
		   (row == info->end_line && col <= info->end_column && (row > info->line || col >= info->column))) {
			*result = candidate;
		}
	}
}
}

void CodeEditorDialog::onMoveCursor()
{
	using std::placeholders::_1;

	visualize_widget_.reset();

	status_label_->setText(formatter() << "Line " << (editor_->cursorRow()+1) << " Col " << (editor_->cursorCol()+1) << (modified_ ? " (Modified)" : ""));

	if(file_contents_set_) {
		try {
			variant v = json::parse_from_file(fname_);
			variant formula_str;
			assert(v.is_map());
			visitVariants(v, std::bind(visit_potential_formula_str, _1, &formula_str, static_cast<int>(editor_->cursorRow()+1), static_cast<int>(editor_->cursorCol()+1)));

			if(formula_str.is_string()) {

				const variant::debug_info& str_info = *formula_str.get_debug_info();
				const std::set<game_logic::Formula*>& formulae = game_logic::Formula::getAll();
				int best_result = -1;
				variant result_variant;
				const game_logic::Formula* best_formula = nullptr;
				for(const game_logic::Formula* f : formulae) {
					const variant::debug_info* info = f->strVal().get_debug_info();
					if(!info || !info->filename || *info->filename != *str_info.filename) {
						continue;
					}

					variant result;
					visit_potential_formula_str(f->strVal(), &result, static_cast<int>(editor_->cursorRow()+1), static_cast<int>(editor_->cursorCol()+1));
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
					//TODO: make the visualize widget good enough to use.
					//visualize_widget_.reset(new gui::formula_visualize_widget(best_formula->expr(), text_pos, editor_->cursorRow()+1, editor_->cursorCol()+1, 20, 20, 500, 400, editor_.get()));
				}
			}

		} catch(...) {
			LOG_INFO("ERROR PARSING FORMULA SET");
		}
	} else {
		LOG_INFO("NO FORMULA SET");
	}
}

void CodeEditorDialog::setAnimationRect(rect r)
{
	const gui::CodeEditorWidget::ObjectInfo info = editor_->getCurrentObject();
	variant v = info.obj;
	if(v.is_null() == false) {
		v.add_attr(variant("rect"), r.write());
		editor_->modifyCurrentObject(v);
		try {
			animation_preview_->setObject(v);
		} catch(Frame::Error&) {
		}
	}
}

void CodeEditorDialog::moveSolidRect(int dx, int dy)
{
	const gui::CodeEditorWidget::ObjectInfo info = editor_->getCurrentObject();
	variant v = info.obj;
	if(v.is_null() == false) {
		variant solid_area = v["solid_area"];
		if(!solid_area.is_list() || solid_area.num_elements() != 4) {
			return;
		}

		for(const variant& num : solid_area.as_list()) {
			if(!num.is_int()) {
				return;
			}
		}

		rect area(solid_area);
		area = rect(area.x() + dx, area.y() + dy, area.w(), area.h());
		v.add_attr(variant("solid_area"), area.write());
		editor_->modifyCurrentObject(v);
		try {
			animation_preview_->setObject(v);
		} catch(Frame::Error&) {
		}
	}
}

void CodeEditorDialog::setIntegerAttr(const char* attr, int value)
{
	const gui::CodeEditorWidget::ObjectInfo info = editor_->getCurrentObject();
	variant v = info.obj;
	if(v.is_null() == false) {
		v.add_attr(variant(attr), variant(value));
		editor_->modifyCurrentObject(v);
		try {
			animation_preview_->setObject(v);
		} catch(Frame::Error&) {
		}
	}
}

void CodeEditorDialog::save()
{
	sys::write_file(module::map_file(fname_), editor_->text());
	status_label_->setText(formatter() << "Saved " << fname_);
	modified_ = false;
}

void CodeEditorDialog::save_and_close()
{
	save();
	close();
}

void CodeEditorDialog::select_suggestion(int index)
{
	if(index >= 0 && static_cast<unsigned>(index) < suggestions_.size()) {
		LOG_INFO("SELECT " << suggestions_[index].suggestion);
		const std::string& str = suggestions_[index].suggestion;
		if(suggestions_prefix_ >= 0 && static_cast<unsigned>(suggestions_prefix_) < str.size()) {
			const int col = static_cast<int>(editor_->cursorCol());
			const std::string insert(str.begin() + suggestions_prefix_, str.end());
			const std::string postfix = suggestions_[index].postfix;
			const std::string row = editor_->getData()[editor_->cursorRow()];
			const std::string new_row = std::string(row.begin(), row.begin() + editor_->cursorCol()) + insert + postfix + std::string(row.begin() + editor_->cursorCol(), row.end());
			editor_->setRowContents(editor_->cursorRow(), new_row);
			editor_->setCursor(editor_->cursorRow(), col + static_cast<int>(insert.size()) + suggestions_[index].postfix_index);
		}
	} else {
		suggestions_grid_.reset();
	}
}

void edit_and_continue_class(const std::string& class_name, const std::string& error)
{
	auto wnd_w = KRE::WindowManager::getMainWindow()->width();
	auto wnd_h = KRE::WindowManager::getMainWindow()->height();
	ffl::IntrusivePtr<CodeEditorDialog> d(new CodeEditorDialog(rect(0,0,wnd_w,wnd_h)));

	const std::string::const_iterator end_itor = std::find(class_name.begin(), class_name.end(), '.');
	const std::string filename = "data/classes/" + std::string(class_name.begin(), end_itor) + ".cfg";

	d->setProcessHook(std::bind(&CodeEditorDialog::process, d.get()));
	d->add_optional_error_text_area(error);
	d->set_close_buttons();
	d->init();
	d->load_file(filename);
	d->jump_to_error(error);
	d->setOnQuit(std::bind(&gui::Dialog::cancel, d.get()));
	d->showModal();

	if(d->cancelled()) {
		_exit(0);
	}
}

void edit_and_continue_fn(const std::string& filename, const std::string& error, std::function<void()> fn)
{
	auto wnd_w = KRE::WindowManager::getMainWindow()->width();
	auto wnd_h = KRE::WindowManager::getMainWindow()->height();
	ffl::IntrusivePtr<CodeEditorDialog> d(new CodeEditorDialog(rect(0,0,wnd_w,wnd_h)));

	d->setProcessHook(std::bind(&CodeEditorDialog::process, d.get()));
	d->add_optional_error_text_area(error);
	d->set_close_buttons();
	d->init();
	d->load_file(filename, true, &fn);

	std::string real_filename = module::map_file(filename);

	std::function<void()> reload_dialog_fn = std::bind(&CodeEditorDialog::close, d.get());
	const int file_mod_handle = sys::notify_on_file_modification(real_filename, reload_dialog_fn);

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
	d->showModal();

	sys::remove_notify_on_file_modification(file_mod_handle);

	SDL_Event event;
	while(input::sdl_poll_event(&event)) {
		switch(event.type) {
		case SDL_QUIT:
			_exit(0);
		}
	}

	if(d->cancelled() || d->has_error()) {
		_exit(0);
	}
}

namespace 
{
	void try_fix_assert()
	{
	}
}

void edit_and_continue_assert(const std::string& msg, std::function<void()> fn)
{
	const std::vector<CallStackEntry>& stack = get_expression_call_stack();
	std::vector<CallStackEntry> reverse_stack = stack;
	std::reverse(reverse_stack.begin(), reverse_stack.end());
	if(stack.empty() || !Level::getCurrentPtr()) {
		assert(false);
	}

	auto wnd = KRE::WindowManager::getMainWindow();

	int w, h;
	wnd->autoWindowSize(w,h);
	wnd->setWindowSize(w,h);

	using namespace gui;

	using debug_console::ConsoleDialog;

	ffl::IntrusivePtr<ConsoleDialog> console(new ConsoleDialog(Level::current(), *const_cast<game_logic::FormulaCallable*>(stack.back().callable)));

	GridPtr call_grid(new Grid(1));
	call_grid->setMaxHeight(wnd->height() - console->y());
	call_grid->allowSelection();
	call_grid->mustSelect();
	for(const CallStackEntry& entry : reverse_stack) {
		std::string str = entry.expression->str();
		std::string::iterator i = std::find(str.begin(), str.end(), '\n');
		if(i != str.end()) {
			str.erase(i, str.end());
		}
		call_grid->addCol(WidgetPtr(new Label(str)));
	}

	call_grid->setLoc(console->x() + console->width() + 6, console->y());
	call_grid->setDim(wnd->width() - call_grid->x(), wnd->height() - call_grid->y());

	ffl::IntrusivePtr<CodeEditorDialog> d(new CodeEditorDialog(rect(wnd->width()/2,0,wnd->width()/2,console->y())));

	d->set_close_buttons();
	d->show();
	d->init();

	const variant::debug_info* debug_info = stack.back().expression->getParentFormula().get_debug_info();
	if(debug_info && debug_info->filename) {
		if(!fn) {
			fn = std::function<void()>(try_fix_assert);
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
					_exit(0);
					break;
				}
			}

			bool swallowed = false;
			for(WidgetPtr w : widgets) {
				if(!swallowed) {
					swallowed = w->processEvent(point(), event, swallowed) || swallowed;
				}
			}
		}

		d->process();

		console->prepareDraw();
		for(WidgetPtr w : widgets) {
			w->draw();
		}
		console->completeDraw();
	}

	if(quit || d->cancelled() || d->has_error()) {
		_exit(0);
	}
}

COMMAND_LINE_UTILITY(codeedit)
{
	SDL::SDL_ptr manager(new SDL::SDL());

	variant_builder hints;
	hints.add("renderer", "opengl");
	hints.add("title", "Anura auto-update");
	hints.add("clear_color", "black");

	KRE::WindowManager wm("SDL");
	auto wnd = wm.createWindow(800, 600, hints.build());

	variant gui_node = json::parse_from_file("data/gui.cfg");
	GuiSection::init(gui_node);

	FramedGuiElement::init(gui_node);

	CodeEditorDialog d(rect(0,0,600,600));
	LOG_INFO("CREATE DIALOG");
	if(args.empty() == false) {
		d.load_file(args[0]);
	}

	d.showModal();
}

#endif // NO_EDITOR
