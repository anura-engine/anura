/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <boost/intrusive_ptr.hpp>

#include "custom_object.hpp"
#include "custom_object_type.hpp"
#include "widget.hpp"

namespace gui
{
	class CustomObjectWidget : public Widget
	{
	public:
		CustomObjectWidget(const variant& v, game_logic::FormulaCallable* e);
		virtual ~CustomObjectWidget();
		void setEntity(EntityPtr e);
		EntityPtr getEntity();
		ConstEntityPtr getEntity() const;
		void init(const variant& v);
	protected:

		void handleDraw() const override;
		bool handleEvent(const SDL_Event& event, bool claimed) override;
		virtual void handleProcess() override;
	private:
		DECLARE_CALLABLE(CustomObjectWidget);

		void click(int button);
		void mouseEnter();
		void mouseLeave();

		std::function<void (int)> on_click_;
		game_logic::formula_ptr click_handler_;
		std::function<void ()> on_mouse_enter_;
		game_logic::formula_ptr mouse_enter_handler_;
		std::function<void ()> on_mouse_leave_;
		game_logic::formula_ptr mouse_leave_handler_;

		game_logic::formula_ptr commands_handler_;

		WidgetPtr overlay_;

		EntityPtr entity_;
		bool handleProcess_on_entity_;
	};

	typedef boost::intrusive_ptr<CustomObjectWidget> CustomObjectWidgetPtr;
}
