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
#include <boost/bind.hpp>

#include "custom_object_widget.hpp"
#include "level.hpp"
#include "widget_factory.hpp"

namespace gui
{
	namespace {
		int do_commands_on_process = 0;
	}

	custom_object_widget::custom_object_widget(const variant& v, game_logic::formula_callable* e)
		: widget(v, e)
	{
		ASSERT_LOG(get_environment() != 0, "You must specify a callable environment");
		ASSERT_LOG(v.has_key("object") == true, "You must provide an object");
		init(v);
	}

	custom_object_widget::~custom_object_widget()
	{
	}

	void custom_object_widget::init(const variant& v)
	{
		entity_.reset();
		handle_process_on_entity_ = v["handle_process"].as_bool(false);
		if(v["object"].is_string()) {
			// type name, has obj_x, obj_y, facing			
			entity_ = entity_ptr(new custom_object(v["object"].as_string(), v["obj_x"].as_int(0), v["obj_y"].as_int(0), v["facing"].as_int(1)));
			entity_->finish_loading(NULL);
		} else if(v["object"].is_map()) {
			entity_ = entity_ptr(new custom_object(v["object"]));
			entity_->finish_loading(NULL);
		} else {
			entity_ = v["object"].try_convert<entity>();
			ASSERT_LOG(entity_ != NULL, "Couldn't convert 'object' attribue to an entity");
			entity_->finish_loading(NULL);
			entity_->validate_properties();
		}
		if(v.has_key("properties")) {
			ASSERT_LOG(v["properties"].is_map(), "properties field must be a map");
			const variant& properties = v["properties"];
			variant keys = properties.get_keys();
			for(int n = 0; n != keys.num_elements(); ++n) {
				variant value = properties[keys[n]];
				entity_->mutate_value(keys[n].as_string(), value);
			}
		}
		if(v.has_key("commands")) {
			do_commands_on_process = 10;
			commands_handler_ = entity_->create_formula(v["commands"]);
			using namespace game_logic;
			map_formula_callable_ptr callable = map_formula_callable_ptr(new map_formula_callable(entity_.get()));
			callable->add("id", variant(id()));
			variant value = commands_handler_->execute(*callable);
			entity_->execute_command(value);
		}
		if(v.has_key("on_click")) {
			click_handler_ = get_environment()->create_formula(v["on_click"]);
			on_click_ = boost::bind(&custom_object_widget::click, this, _1);
		}
		if(v.has_key("on_mouse_enter")) {
			mouse_enter_handler_ = get_environment()->create_formula(v["on_mouse_enter"]);
			on_mouse_enter_ = boost::bind(&custom_object_widget::mouse_enter, this);
		}
		if(v.has_key("on_mouse_leave")) {
			mouse_leave_handler_ = get_environment()->create_formula(v["on_mouse_leave"]);
			on_mouse_leave_ = boost::bind(&custom_object_widget::mouse_leave, this);
		}
		if(v.has_key("overlay") && v["overlay"].is_null() == false) {
			overlay_ = widget_factory::create(v["overlay"], get_environment());
		}
		set_dim(entity_->current_frame().width(), entity_->current_frame().height());
	}

	void custom_object_widget::click(int button)
	{
		using namespace game_logic;
		if(get_environment()) {
			map_formula_callable_ptr callable = map_formula_callable_ptr(new map_formula_callable(get_environment()));
			callable->add("id", variant(id()));
			callable->add("object", variant(entity_.get()));
			callable->add("mouse_button", variant(button));
			variant value = click_handler_->execute(*callable);
			get_environment()->execute_command(value);
		} else {
			std::cerr << "custom_object_widget::click() called without environment!" << std::endl;
		}
	}

	void custom_object_widget::mouse_enter()
	{
		using namespace game_logic;
		if(get_environment()) {
			map_formula_callable_ptr callable = map_formula_callable_ptr(new map_formula_callable(get_environment()));
			callable->add("id", variant(id()));
			callable->add("object", variant(entity_.get()));
			variant value = mouse_enter_handler_->execute(*callable);
			get_environment()->execute_command(value);
		} else {
			std::cerr << "custom_object_widget::mouse_enter() called without environment!" << std::endl;
		}
	}

	void custom_object_widget::mouse_leave()
	{
		using namespace game_logic;
		if(get_environment()) {
			map_formula_callable_ptr callable = map_formula_callable_ptr(new map_formula_callable(get_environment()));
			callable->add("id", variant(id()));
			callable->add("object", variant(entity_.get()));
			variant value = mouse_leave_handler_->execute(*callable);
			get_environment()->execute_command(value);
		} else {
			std::cerr << "custom_object_widget::mouse_leave() called without environment!" << std::endl;
		}
	}

	void custom_object_widget::set_entity(entity_ptr e)
	{
		entity_ = e;
	}

	entity_ptr custom_object_widget::get_entity()
	{
		return entity_;
	}

	const_entity_ptr custom_object_widget::get_entity() const
	{
		return entity_;
	}

BEGIN_DEFINE_CALLABLE(custom_object_widget, widget)
	DEFINE_FIELD(object, "custom_obj")
		return variant(obj.entity_.get());
	DEFINE_SET_FIELD
		std::map<variant, variant> m;
		m[variant("object")] = value;
		obj.init(variant(&m));
	DEFINE_FIELD(overlay, "map|builtin widget|null")
		return variant(obj.overlay_.get());
	DEFINE_SET_FIELD
		obj.overlay_ = widget_factory::create(value, obj.get_environment());
	DEFINE_FIELD(handle_process, "bool")
		return variant::from_bool(obj.handle_process_on_entity_);
END_DEFINE_CALLABLE(custom_object_widget)

	void custom_object_widget::handle_draw() const
	{
		if(entity_) {
			glPushMatrix();
			glTranslatef(GLfloat(x() & ~1), GLfloat(y() & ~1), 0.0);
			entity_->draw(x(), y());
			entity_->draw_later(x(), y());
			glPopMatrix();
		}
		if(overlay_) {
			overlay_->set_loc(x() + width()/2 - overlay_->width()/2, y() + height()/2 - overlay_->height()/2);
			overlay_->draw();
		}
	}

	bool custom_object_widget::handle_event(const SDL_Event& event, bool claimed)
	{
		if((event.type == SDL_MOUSEWHEEL) && in_widget(event.button.x, event.button.y)) {
			// skip processing if mousewheel event
			if(entity_) {
				custom_object* obj = static_cast<custom_object*>(entity_.get());
				return obj->handle_sdl_event(event, claimed);
			}
		}

		if(event.type == SDL_MOUSEMOTION) {
			const SDL_MouseMotionEvent& e = event.motion;
			if(in_widget(e.x,e.y)) {
				if(on_mouse_enter_) {
					on_mouse_enter_();
				}
			} else {
				if(on_mouse_leave_) {
					on_mouse_leave_();
				}
			}
		} else if(event.type == SDL_MOUSEBUTTONDOWN) {
			const SDL_MouseButtonEvent& e = event.button;
			if(in_widget(e.x,e.y)) {
				claimed = claim_mouse_events();
			}
		} else if(event.type == SDL_MOUSEBUTTONUP) {
			const SDL_MouseButtonEvent& e = event.button;
			if(in_widget(e.x,e.y)) {
				if(on_click_) {
					on_click_(event.button.button);
				}
				claimed = claim_mouse_events();
			}
		}

		if(entity_) {
			custom_object* obj = static_cast<custom_object*>(entity_.get());
			return obj->handle_sdl_event(event, claimed);
		}
		return claimed;
	}

	void custom_object_widget::handle_process()
	{
		widget::handle_process();
		if(entity_ && handle_process_on_entity_) {
			custom_object* obj = static_cast<custom_object*>(entity_.get());
			obj->process(level::current());
		}

		if(overlay_) {
			overlay_->process();
		}

//		if(do_commands_on_process && --do_commands_on_process == 0) {
//			variant value = commands_handler_->execute();
//			entity_->execute_command(value);
//		}
	}

}
