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

#pragma once

#include <string>
#include <vector>

#include "widget_fwd.hpp"

namespace gui 
{
	std::string get_dialog_file(const std::string& fname);
	void reset_dialog_paths();

	class Dialog : public Widget
	{
	public:
		explicit Dialog(int x, int y, int w, int h);
		explicit Dialog(const variant& v, game_logic::FormulaCallable* e);
		virtual ~Dialog();
		virtual void show_modal();
		void show();

		enum MOVE_DIRECTION { MOVE_DOWN, MOVE_RIGHT };
		Dialog& addWidget(WidgetPtr w, MOVE_DIRECTION dir=MOVE_DOWN);
		Dialog& addWidget(WidgetPtr w, int x, int y,
						MOVE_DIRECTION dir=MOVE_DOWN);
		void removeWidget(WidgetPtr w);
		void replace_widget(WidgetPtr w_old, WidgetPtr w_new);
		void clear();
		int padding() const { return padding_; }
		void setPadding(int pad) { padding_ = pad; }
		void close();
		void cancel() { cancelled_ = true; close(); }

		bool closed() { return !opened_; }
		bool cancelled() { return cancelled_; }
		void setCursor(int x, int y) { add_x_ = x; add_y_ = y; }
		int cursor_x() const { return add_x_; }
		int cursor_y() const { return add_y_; }
		bool processEvent(const SDL_Event& e, bool claimed);
	
		void set_on_quit(std::function<void ()> onquit) { on_quit_ = onquit; }

		void set_background_frame(const std::string& id) { background_FramedGuiElement_ = id; }
		void set_draw_background_fn(std::function<void()> fn) { draw_background_fn_ = fn; }
		void set_upscale_frame(bool upscale=true) { upscale_frame_ = upscale; }

		virtual bool hasFocus() const;
		void set_process_hook(std::function<void()> fn) { on_process_ = fn; }
		static void draw_last_scene();
		virtual WidgetPtr getWidgetById(const std::string& id);
		virtual ConstWidgetPtr getWidgetById(const std::string& id) const;

		void prepare_draw();
		void complete_draw();

		std::vector<WidgetPtr> getChildren() const;

		//add standardized okay/cancel buttons in the bottom right corner.
		void add_ok_and_cancel_buttons();

	protected:
		virtual bool handleEvent(const SDL_Event& event, bool claimed) override;
		virtual bool handleEvent_children(const SDL_Event& event, bool claimed);
		virtual void handleDraw() const override;
		virtual void handleDrawChildren() const;
		void set_clear_bg(bool clear) { clear_bg_ = clear; };
		void set_clear_bg_amount(int amount) { clear_bg_ = amount; }
		int clear_bg() const { return clear_bg_; };

		bool pump_events();

		virtual void handleProcess() override;
		void recalculate_dimensions();
	private:
		DECLARE_CALLABLE(dialog);

		void do_up_event();
		void do_down_event();
		void do_select_event();
	
		SortedWidgetList widgets_;
		TabSortedWidgetList tab_widgets_;
		int control_lockout_;

		TabSortedWidgetList::iterator current_tab_focus_;

		bool opened_;
		bool cancelled_;
		int clear_bg_;
	
		std::function<void()> on_quit_;
		std::function<void(bool)> on_close_;

		void quit_delegate();
		void close_delegate(bool cancelled);

		game_logic::formula_ptr ffl_on_quit_;
		game_logic::formula_ptr ffl_on_close_;

		game_logic::FormulaCallablePtr quit_arg_;
		game_logic::FormulaCallablePtr close_arg_;

		//default padding between widgets
		int padding_;

		//where the next widget will be placed by default
		int add_x_, add_y_;

		KRE::MaterialPtr bg_;
		mutable float bg_alpha_;

		int last_draw_;
		rect forced_dimensions_;

		std::string background_FramedGuiElement_;
		std::function<void()> draw_background_fn_;

		bool upscale_frame_;
	};
}
