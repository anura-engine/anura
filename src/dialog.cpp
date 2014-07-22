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

#include <boost/range/adaptor/reversed.hpp>
#include <iostream>

#include "kre/Canvas.hpp"
#include "kre/Font.hpp"
#include "kre/Texture.hpp"
#include "kre/WindowManager.hpp"

#include "button.hpp"
#include "controls.hpp"
#include "dialog.hpp"
#include "draw_scene.hpp"
#include "framed_gui_element.hpp"
#include "input.hpp"
#include "joystick.hpp"
#include "module.hpp"
#include "tooltip.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "widget_factory.hpp"

namespace gui 
{
	using std::placeholders::_1;

	namespace 
	{
		module::module_file_map& get_dialog_path()
		{
			static module::module_file_map dialog_file_map;
			return dialog_file_map;
		}

		void load_dialog_file_paths(const std::string& path)
		{
			if(get_dialog_path().empty()) {
				module::get_unique_filenames_under_dir(path, &get_dialog_path());
			}
		}
	}

	void reset_dialog_paths()
	{
		get_dialog_path().clear();
	}

	std::string get_dialog_file(const std::string& fname)
	{
		load_dialog_file_paths("data/dialog/");
		module::module_file_map::const_iterator it = module::find(get_dialog_path(), fname);
		ASSERT_LOG(it != get_dialog_path().end(), "DIALOG FILE NOT FOUND: " << fname);
		return it->second;
	}

	Dialog::Dialog(int x, int y, int w, int h)
	  : opened_(false), cancelled_(false), clear_bg_(196), padding_(10),
		add_x_(0), add_y_(0), bg_alpha_(1.0), last_draw_(-1), upscale_frame_(true),
		current_tab_focus_(tab_widgets_.end()), control_lockout_(0)
	{
		setEnvironment();
		setLoc(x,y);
		setDim(w,h);
		forced_dimensions_ = rect(x, y, w, h);
	}

	Dialog::Dialog(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v,e),
		opened_(false), cancelled_(false), 
		add_x_(0), add_y_(0), last_draw_(-1),
		upscale_frame_(v["upscale_frame"].as_bool(true)),
		current_tab_focus_(tab_widgets_.end()), control_lockout_(0)
	{
		forced_dimensions_ = rect(x(), y(), width(), height());
		padding_ = v["padding"].as_int(10);
		if(v.has_key("background_frame")) {
			background_FramedGuiElement_ = v["background_frame"].as_string();
		}
		if(v.has_key("background_draw")) {
			std::string scene = v["background_draw"].as_string();
			if(scene == "last_scene") {
				draw_background_fn_ = std::bind(&Dialog::draw_last_scene);
			}
			// XXX could make this FFL callable. Or could allow any of the background scenes to be drawn. or both.
		}
		clear_bg_ = v["clear_background_alpha"].as_int(196);
		bg_alpha_ = v["background_alpha"].as_int(255) / 255.0f;
		if(v.has_key("cursor")) {
			std::vector<int> vi = v["cursor"].as_list_int();
			setCursor(vi[0], vi[1]);
		}
		if(v.has_key("on_quit")) {
			on_quit_ = std::bind(&Dialog::quitDelegate, this);
			ASSERT_LOG(getEnvironment() != NULL, "environment not set");
			const variant on_quit_value = v["on_quit"];
			if(on_quit_value.is_function()) {
				ASSERT_LOG(on_quit_value.min_function_arguments() == 0, "on_quit_value dialog function should take 0 arguments: " << v.debug_location());
				static const variant fml("fn()");
				ffl_on_quit_.reset(new game_logic::Formula(fml));

				game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
				callable->add("fn", on_quit_value);

				quit_arg_.reset(callable);
			} else {
				ffl_on_quit_ = getEnvironment()->createFormula(v["on_quit"]);
			}
		}
		if(v.has_key("on_close")) {
			on_close_ = std::bind(&Dialog::closeDelegate, this, _1);
			ASSERT_LOG(getEnvironment() != NULL, "environment not set");
			const variant on_close_value = v["on_close"];
			if(on_close_value.is_function()) {
				ASSERT_LOG(on_close_value.min_function_arguments() <= 1 && on_close_value.max_function_arguments() >= 1, "on_close dialog function should take 1 argument: " << v.debug_location());
				static const variant fml("fn(selection)");
				ffl_on_close_.reset(new game_logic::Formula(fml));

				game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
				callable->add("fn", on_close_value);

				close_arg_.reset(callable);
			} else {
				ffl_on_close_ = getEnvironment()->createFormula(v["on_close"]);
			}
		}
		std::vector<variant> children = v["children"].as_list();
		for(const variant& child : children) {
			WidgetPtr w = widget_factory::create(child, e);
			if(w->x() != 0 || w->y() != 0) {
			//if(child.has_key("add_xy")) {
			//	std::vector<int> addxy = child["add_xy"].as_list_int();
			//	addWidget(widget_factory::create(child, e), addxy[0], addxy[1]);
				addWidget(w, w->x(), w->y());
			} else {
				addWidget(w);
			}
		}
		recalculateDimensions();
	}

	Dialog::~Dialog()
	{
	}

	void Dialog::recalculateDimensions()
	{
		if(forced_dimensions_.empty()) {
			int new_w = 0;
			int new_h = 0;
			for(WidgetPtr w : widgets_) {
				if((w->x() + w->width()) > new_w) {
					new_w = w->x() + w->width() + padding_ + getPadWidth();
				}
				if((w->y() + w->height()) > new_h) {
					new_h = w->y() + w->height() + padding_ + getPadHeight();
				}
			}
			setDim(new_w, new_h);
		}
	}

	void Dialog::draw_last_scene() 
	{
		::draw_last_scene();
	}

	void Dialog::quitDelegate()
	{
		if(quit_arg_) {
			using namespace game_logic;
			variant value = ffl_on_quit_->execute(*quit_arg_);
			getEnvironment()->createFormula(value);
		} else if(getEnvironment()) {
			variant value = ffl_on_quit_->execute(*getEnvironment());
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "dialog::quitDelegate() called without environment!" << std::endl;
		}
	}

	void Dialog::closeDelegate(bool cancelled)
	{
		using namespace game_logic;
		if(close_arg_) {
			using namespace game_logic;
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(close_arg_.get()));
			callable->add("cancelled", variant::from_bool(cancelled));
			variant value = ffl_on_close_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("cancelled", variant::from_bool(cancelled));
			variant value = ffl_on_close_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "dialog::closeDelegate() called without environment!" << std::endl;
		}
	}

	void Dialog::handleProcess()
	{
		Widget::handleProcess();
		for(WidgetPtr w : widgets_) {
			w->process();
		}

		if(joystick::up() && !control_lockout_) {
			control_lockout_ = 10;
			doUpEvent();
		}
		if(joystick::down() && !control_lockout_) {
			control_lockout_ = 10;
			doDownEvent();
		}
		if((joystick::button(0) || joystick::button(1) || joystick::button(2)) && !control_lockout_) {
			control_lockout_ = 10;
			doSelectEvent();
		}

		if(control_lockout_) {
			--control_lockout_;
		}
	}

	Dialog& Dialog::addWidget(WidgetPtr w, Dialog::MOVE_DIRECTION dir)
	{
		addWidget(w, add_x_, add_y_, dir);
		return *this;
	}

	Dialog& Dialog::addWidget(WidgetPtr w, int x, int y, Dialog::MOVE_DIRECTION dir)
	{
		w->setLoc(x,y);
		widgets_.insert(w);
		if(w->tabStop() >= 0) {
			tab_widgets_.insert(TabSortedWidgetList::value_type(w->tabStop(), w));
		}
		switch(dir) {
		case MOVE_DIRECTION::DOWN:
			add_x_ = x;
			add_y_ = y + w->height() + padding_;
			break;
		case MOVE_DIRECTION::RIGHT:
			add_x_ = x + w->width() + padding_;
			add_y_ = y;
			break;
		}
		recalculateDimensions();
		return *this;
	}

	void Dialog::removeWidget(WidgetPtr w)
	{
		if(!w) {
			return;
		}

		auto it = std::find(widgets_.begin(), widgets_.end(), w);
		if(it != widgets_.end()) {
			widgets_.erase(it);
		}
		auto tw_it = tab_widgets_.find(w->tabStop());
		if(tw_it != tab_widgets_.end()) {
			if(current_tab_focus_ == tw_it) {
				++current_tab_focus_;
			}
			tab_widgets_.erase(tw_it);
		}
		recalculateDimensions();
	}

	void Dialog::clear() { 
		add_x_ = add_y_ = 0;
		widgets_.clear(); 
		tab_widgets_.clear();
		recalculateDimensions();
	}

	void Dialog::replaceWidget(WidgetPtr w_old, WidgetPtr w_new)
	{
		int x = w_old->x();
		int y = w_old->y();
		int w = w_old->width();
		int h = w_old->height();

		auto it = std::find(widgets_.begin(), widgets_.end(), w_old);
		if(it != widgets_.end()) {
			widgets_.erase(it);
		}
		widgets_.insert(w_new);

		auto tw_it = tab_widgets_.find(w_old->tabStop());
		if(tw_it != tab_widgets_.end()) {
			if(current_tab_focus_ == tw_it) {
				++current_tab_focus_;
			}
			tab_widgets_.erase(tw_it);
		}
		if(w_new->tabStop() >= 0) {
			tab_widgets_.insert(TabSortedWidgetList::value_type(w_new->tabStop(), w_new));
		}

		w_new->setLoc(x,y);
		w_new->setDim(w,h);

		recalculateDimensions();
	}

	void Dialog::show() 
	{
		opened_ = true;
		setVisible(true);
	}

	bool Dialog::pumpEvents()
	{
		SDL_Event event;
		bool running = true;
		while(running && input::sdl_poll_event(&event)) {  
			bool claimed = false;
            
			switch(event.type) {
			case SDL_QUIT:
				running = false;
				claimed = true;
				SDL_PushEvent(&event);
				break;

	#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE || defined(__ANDROID__)
				case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_MINIMIZED)
				{
					SDL_Event e;
					while (SDL_WaitEvent(&e))
					{
						if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESTORED)
							break;
					}
				}
				break;
	#endif
			default:
				break;
			}
			claimed = processEvent(event, claimed);
		}

		return running;
	}

	void Dialog::showModal()
	{
		opened_ = true;
		cancelled_ = false;

		// We do an initial lockout on the controller start button to stop the dialog being instantly closed.
		int joystick_lockout = 25;

		while(opened_ && pumpEvents()) {
			Uint32 t = profile::get_tick_time();
			process();
			prepareDraw();
			draw();
			gui::draw_tooltip();
			completeDraw();

			if(joystick_lockout) {
				--joystick_lockout;
			}
			if(joystick::button(4) && !joystick_lockout) {
				cancelled_ = true;
				opened_ = false;
			}

			t = t - profile::get_tick_time();
			if(t < 20) {
				profile::delay(20 - t);
			}
		}
	}

	void Dialog::prepareDraw()
	{
		if(clearBg()) {
			KRE::WindowManager::getMainWindow()->setClearColor(KRE::Color::colorBlack());
			KRE::WindowManager::getMainWindow()->clear(KRE::ClearFlags::COLOR | KRE::ClearFlags::DEPTH);
		}
	}

	void Dialog::completeDraw()
	{
		KRE::WindowManager::getMainWindow()->swap();

		const int end_draw = last_draw_ + 20;
		const int delay_time = std::max<int>(1, end_draw - profile::get_tick_time());

		profile::delay(delay_time);

		last_draw_ = profile::get_tick_time();
	}

	std::vector<WidgetPtr> Dialog::getChildren() const
	{
		std::vector<WidgetPtr> widget_list;
		std::copy(widgets_.begin(), widgets_.end(), std::back_inserter(widget_list));
		return widget_list;
	}

	void Dialog::addOkAndCancelButtons()
	{
		WidgetPtr ok(new Button("Ok", std::bind(&Dialog::close, this)));
		WidgetPtr cancel(new Button("Cancel", std::bind(&Dialog::cancel, this)));
		ok->setDim(cancel->width(), ok->height());
		addWidget(ok, width() - 160, height() - 40);
		addWidget(cancel, width() - 80, height() - 40);
	}

	void Dialog::handleDrawChildren() const 
	{
		for(const WidgetPtr& w : widgets_) {
			w->draw(x(), y());
		}
	}

	void Dialog::handleDraw() const
	{
		auto canvas = KRE::Canvas::getInstance();
		if(clearBg()) {
			canvas->drawSolidRect(rect(x(),y(),width(),height()), KRE::Color(0,0,0,clear_bg_));

			//fade effect for fullscreen dialogs
			if(bg_) {
				if(bg_alpha_ > 0.25f) {
					bg_alpha_ -= 0.05f;
				}
				canvas->blitTexture(bg_, 0.0f, rect(x(),y(),width(),height()), KRE::Color(1.0,1.0,1.0,bg_alpha_));
			}
		}

		if(draw_background_fn_) {
			draw_background_fn_();
		}

		if(background_FramedGuiElement_.empty() == false) {
			canvas->drawSolidRect(rect(x(),y(),width(),height()), KRE::Color(0,0,0,getAlpha() >= 255 ? 204 : getAlpha()));
			ConstFramedGuiElementPtr window(FramedGuiElement::get(background_FramedGuiElement_));
			// XXX may need to apply the alpha here?
			window->blit(x(),y(),width(),height(), upscale_frame_);
		}

		handleDrawChildren();
	}

	bool Dialog::processEvent(const SDL_Event& ev, bool claimed) {
		if (ev.type == SDL_QUIT && on_quit_)
			on_quit_();
		return Widget::processEvent(ev, claimed);
	}

	bool Dialog::handleEventChildren(const SDL_Event &event, bool claimed) {
		SDL_Event ev = event;
		normalizeEvent(&ev, false);
		// We copy the list here to cover the case that event processing causes
		// a widget to get removed and thus the iterator to be invalidated.
		SortedWidgetList wlist = widgets_;
		for(auto w : boost::adaptors::reverse(wlist)) {
			claimed |= w->processEvent(ev, claimed);
		}
		return claimed;
	}

	void Dialog::close() 
	{ 
		opened_ = false; 
		if(on_close_) {
			on_close_(cancelled_);
		}
	}

	void Dialog::doUpEvent()
	{
		if(tab_widgets_.size()) {
			if(current_tab_focus_ == tab_widgets_.end()) {
				--current_tab_focus_;
			} else {			
				current_tab_focus_->second->setFocus(false);
				if(current_tab_focus_ == tab_widgets_.begin()) {
					current_tab_focus_ = tab_widgets_.end();
				} 
				--current_tab_focus_;
			}
			if(current_tab_focus_ != tab_widgets_.end()) {
				current_tab_focus_->second->setFocus(true);
			}
		}
	}

	void Dialog::doDownEvent()
	{
		if(tab_widgets_.size()) {
			if(current_tab_focus_ == tab_widgets_.end()) {
				current_tab_focus_ = tab_widgets_.begin();
			} else {
				current_tab_focus_->second->setFocus(false);
				++current_tab_focus_;
				if(current_tab_focus_ == tab_widgets_.end()) {
					current_tab_focus_ = tab_widgets_.begin();
				}
			}

			if(current_tab_focus_ != tab_widgets_.end()) {
				current_tab_focus_->second->setFocus(true);
			}
		}
	}

	void Dialog::doSelectEvent()
	{
		// Process key as an execute here.
		if(current_tab_focus_ != tab_widgets_.end()) {
			current_tab_focus_->second->doExecute();
		}
	}

	bool Dialog::handleEvent(const SDL_Event& ev, bool claimed)
	{

		claimed |= handleEventChildren(ev, claimed);

		if(!claimed && opened_) {
			if(ev.type == SDL_KEYDOWN) {
				if(ev.key.keysym.sym == controls::get_keycode(controls::CONTROL_ATTACK) 
					|| ev.key.keysym.sym == controls::get_keycode(controls::CONTROL_JUMP)) {
					doSelectEvent();
				}
				if(ev.key.keysym.sym == SDLK_TAB) {
					if(ev.key.keysym.mod & KMOD_SHIFT) {
						doUpEvent();
					} else {
						doDownEvent();
					}
					claimed = true;
				}
				switch(ev.key.keysym.sym) {
					case SDLK_RETURN:
						close();
						cancelled_ = false;
						claimed = true;
						break; 
					case SDLK_ESCAPE:
						close();
						cancelled_ = true;
						claimed = true;
						break;
					case SDLK_DOWN:
						doDownEvent();
						claimed = true;
						break;
					case SDLK_UP:
						doUpEvent();
						claimed = true;
						break;				
					default: break;
				}
			}
		}

		if(!claimed) {
			switch(ev.type) {
			//if it's a mouse button up or down and it's within the dialog area,
			//then we claim it because nobody else should get it.
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP: {
				if(claimMouseEvents() && inWidget(ev.button.x, ev.button.y)) {
					claimed = true;
				}
				break;
			}
			}
		}
		return claimed;
	}

	bool Dialog::hasFocus() const
	{
		for(auto w : widgets_) {
			if(w->hasFocus()) {
				return true;
			}
		}

		return false;
	}

	WidgetPtr Dialog::getWidgetById(const std::string& id)
	{
		for(auto w : widgets_) {
			if(w) {
				WidgetPtr wx = w->getWidgetById(id);
				if(wx) {
					return wx;
				}
			}
		}
		return Widget::getWidgetById(id);
	}

	ConstWidgetPtr Dialog::getWidgetById(const std::string& id) const
	{
		for(auto w : widgets_) {
			if(w) {
				WidgetPtr wx = w->getWidgetById(id);
				if(wx) {
					return wx;
				}
			}
		}
		return Widget::getWidgetById(id);
	}

	BEGIN_DEFINE_CALLABLE(Dialog, Widget)
		DEFINE_FIELD(child, "builtin Widget")
			return variant();
		DEFINE_SET_FIELD
			WidgetPtr w = widget_factory::create(value, obj.getEnvironment());
			obj.addWidget(w, w->x(), w->y());
		DEFINE_FIELD(background_alpha, "decimal")
			return variant(obj.bg_alpha_);
		DEFINE_SET_FIELD
			obj.bg_alpha_ = value.as_float();
	END_DEFINE_CALLABLE(Dialog)

}
