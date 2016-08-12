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

#include "widget.hpp"

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
		virtual void showModal();
		void show();

		enum class MOVE_DIRECTION { DOWN, RIGHT };
		Dialog& addWidget(WidgetPtr w, MOVE_DIRECTION dir=MOVE_DIRECTION::DOWN);
		Dialog& addWidget(WidgetPtr w, int x, int y,
						MOVE_DIRECTION dir=MOVE_DIRECTION::DOWN);
		void removeWidget(WidgetPtr w);
		void replaceWidget(WidgetPtr w_old, WidgetPtr w_new);
		void clear();
		int padding() const { return padding_; }
		void setPadding(int pad) { padding_ = pad; }
		void close();
		void cancel() { cancelled_ = true; close(); }

		bool closed() { return !opened_; }
		bool cancelled() { return cancelled_; }
		void setCursor(int x, int y) { add_x_ = x; add_y_ = y; }
		int getCursorX() const { return add_x_; }
		int getCursorY() const { return add_y_; }
		bool processEvent(const point& p, const SDL_Event& e, bool claimed);
	
		void setOnQuit(std::function<void ()> onquit) { on_quit_ = onquit; }

		void setBackgroundFrame(const std::string& id) { background_framed_gui_element_ = id; }
		void setDrawBackgroundFn(std::function<void()> fn) { draw_background_fn_ = fn; }
		void setUpscaleFrame(bool upscale=true) { upscale_frame_ = upscale; }

		virtual bool hasFocus() const override;
		void setProcessHook(std::function<void()> fn) { on_process_ = fn; }
		static void draw_last_scene();
		virtual WidgetPtr getWidgetById(const std::string& id) override;
		virtual ConstWidgetPtr getWidgetById(const std::string& id) const override;

		void prepareDraw();
		void completeDraw();

		std::vector<WidgetPtr> getChildren() const override;

		//add standardized okay/cancel buttons in the bottom right corner.
		void addOkAndCancelButtons();

		virtual WidgetPtr clone() const override;
	protected:
		virtual bool handleEvent(const SDL_Event& event, bool claimed) override;
		virtual bool handleEventChildren(const SDL_Event& event, bool claimed);
		virtual void handleDraw() const override;
		virtual void handleDrawChildren() const;
		void setClearBg(bool clear) { clear_bg_ = clear; };
		void setClearBgAmount(int amount) { clear_bg_ = amount; }
		int clearBg() const { return clear_bg_; };
		void setCloseHook(std::function<bool(bool)> fn) { on_close_hook_ = fn; }

		bool pumpEvents();

		virtual void handleProcess() override;
		void recalculateDimensions();
	private:
		DECLARE_CALLABLE(Dialog);

		void doUpEvent();
		void doDownEvent();
		void doSelectEvent();
	
		SortedWidgetList widgets_;
		TabSortedWidgetList tab_widgets_;
		int control_lockout_;

		TabSortedWidgetList::iterator current_tab_focus_;

		bool opened_;
		bool cancelled_;
		int clear_bg_;
	
		std::function<void()> on_quit_;
		std::function<void(bool)> on_close_;
		std::function<bool(bool)> on_close_hook_;

		void quitDelegate();
		void closeDelegate(bool cancelled);

		game_logic::FormulaPtr ffl_on_quit_;
		game_logic::FormulaPtr ffl_on_close_;

		game_logic::FormulaCallablePtr quit_arg_;
		game_logic::FormulaCallablePtr close_arg_;

		//default padding between widgets
		int padding_;

		//where the next widget will be placed by default
		int add_x_, add_y_;

		KRE::TexturePtr bg_;
		mutable float bg_alpha_;

		int last_draw_;
		rect forced_dimensions_;

		std::string background_framed_gui_element_;
		std::function<void()> draw_background_fn_;

		bool upscale_frame_;
	};
}
