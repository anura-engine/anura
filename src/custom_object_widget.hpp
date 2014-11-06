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
		custom_object_widget(const variant& v, game_logic::formula_callable* e);
		virtual ~custom_object_widget();
		void set_entity(entity_ptr e);
		entity_ptr get_entity();
		const_entity_ptr get_entity() const;
		void init(const variant& v);
	protected:

		void handle_draw() const;
		bool handle_event(const SDL_Event& event, bool claimed);
		virtual void handle_process();
	private:
		DECLARE_CALLABLE(custom_object_widget);

		void click(int button);
		void mouse_enter();
		void mouse_leave();

		boost::function<void (int)> on_click_;
		game_logic::formula_ptr click_handler_;
		boost::function<void ()> on_mouse_enter_;
		game_logic::formula_ptr mouse_enter_handler_;
		boost::function<void ()> on_mouse_leave_;
		game_logic::formula_ptr mouse_leave_handler_;

		game_logic::formula_ptr commands_handler_;

		widget_ptr overlay_;

		entity_ptr entity_;
		bool handle_process_on_entity_;
	};

	typedef boost::intrusive_ptr<custom_object_widget> custom_object_widget_ptr;
	typedef boost::intrusive_ptr<const custom_object_widget> const_custom_object_widget_ptr;
}

#endif
