/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#ifndef CUSTOM_OBJECT_WIDGET_HPP_INCLUDED
#define CUSTOM_OBJECT_WIDGET_HPP_INCLUDED

#include <boost/intrusive_ptr.hpp>

#include "custom_object.hpp"
#include "custom_object_type.hpp"
#include "widget.hpp"

namespace gui
{
	class custom_object_widget : public widget
	{
	public:
		custom_object_widget(const variant& v, game_logic::FormulaCallable* e);
		virtual ~custom_object_widget();
		void setEntity(EntityPtr e);
		EntityPtr getEntity();
		ConstEntityPtr getEntity() const;
		void init(const variant& v);
	protected:

		void handleDraw() const override;
		bool handleEvent(const SDL_Event& event, bool claimed) override;
		virtual void handleProcess() override;
	private:
		DECLARE_CALLABLE(custom_object_widget);

		void click(int button);
		void mouse_enter();
		void mouse_leave();

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

	typedef boost::intrusive_ptr<custom_object_widget> custom_object_WidgetPtr;
	typedef boost::intrusive_ptr<const custom_object_widget> const_custom_object_WidgetPtr;
}

#endif
