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

#include "custom_object_widget.hpp"
#include "level.hpp"
#include "widget_factory.hpp"

namespace gui
{
	namespace {
		int do_commands_on_process = 0;
	}

	CustomObjectWidget::CustomObjectWidget(const variant& v, game_logic::FormulaCallable* e)
		: Widget(v, e)
	{
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");
		ASSERT_LOG(v.has_key("object") == true, "You must provide an object");
		init(v);
	}

	CustomObjectWidget::~CustomObjectWidget()
	{
	}

	void CustomObjectWidget::init(const variant& v)
	{
		using std::placeholders::_1;

		entity_.reset();
		handleProcess_on_entity_ = v["handleProcess"].as_bool(false);
		if(v["object"].is_string()) {
			// type name, has obj_x, obj_y, facing			
			entity_ = EntityPtr(new CustomObject(v["object"].as_string(), v["obj_x"].as_int(0), v["obj_y"].as_int(0), v["facing"].as_int(1) != 0));
			entity_->finishLoading(NULL);
		} else if(v["object"].is_map()) {
			entity_ = EntityPtr(new CustomObject(v["object"]));
			entity_->finishLoading(NULL);
		} else {
			entity_ = v["object"].try_convert<Entity>();
			ASSERT_LOG(entity_ != NULL, "Couldn't convert 'object' attribue to an entity");
			entity_->finishLoading(NULL);
			entity_->validate_properties();
		}
		if(v.has_key("properties")) {
			ASSERT_LOG(v["properties"].is_map(), "properties field must be a map");
			const variant& properties = v["properties"];
			variant keys = properties.getKeys();
			for(int n = 0; n != keys.num_elements(); ++n) {
				variant value = properties[keys[n]];
				entity_->mutate_value(keys[n].as_string(), value);
			}
		}
		if(v.has_key("commands")) {
			do_commands_on_process = 10;
			commands_handler_ = entity_->createFormula(v["commands"]);
			using namespace game_logic;
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(entity_.get()));
			callable->add("id", variant(id()));
			variant value = commands_handler_->execute(*callable);
			entity_->executeCommand(value);
		}
		if(v.has_key("onClick")) {
			click_handler_ = getEnvironment()->createFormula(v["onClick"]);
			on_click_ = std::bind(&CustomObjectWidget::click, this, _1);
		}
		if(v.has_key("on_mouse_enter")) {
			mouse_enter_handler_ = getEnvironment()->createFormula(v["on_mouse_enter"]);
			on_mouse_enter_ = std::bind(&CustomObjectWidget::mouseEnter, this);
		}
		if(v.has_key("on_mouse_leave")) {
			mouse_leave_handler_ = getEnvironment()->createFormula(v["on_mouse_leave"]);
			on_mouse_leave_ = std::bind(&CustomObjectWidget::mouseLeave, this);
		}
		if(v.has_key("overlay") && v["overlay"].is_null() == false) {
			overlay_ = widget_factory::create(v["overlay"], getEnvironment());
		}
		setDim(entity_->getCurrentFrame().width(), entity_->getCurrentFrame().height());
	}

	void CustomObjectWidget::click(int button)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("id", variant(id()));
			callable->add("object", variant(entity_.get()));
			callable->add("mouse_button", variant(button));
			variant value = click_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "custom_object_widget::click() called without environment!" << std::endl;
		}
	}

	void CustomObjectWidget::mouseEnter()
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("id", variant(id()));
			callable->add("object", variant(entity_.get()));
			variant value = mouse_enter_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "custom_object_widget::mouse_enter() called without environment!" << std::endl;
		}
	}

	void CustomObjectWidget::mouseLeave()
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("id", variant(id()));
			callable->add("object", variant(entity_.get()));
			variant value = mouse_leave_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "custom_object_widget::mouse_leave() called without environment!" << std::endl;
		}
	}

	void CustomObjectWidget::setEntity(EntityPtr e)
	{
		entity_ = e;
	}

	EntityPtr CustomObjectWidget::getEntity()
	{
		return entity_;
	}

	ConstEntityPtr CustomObjectWidget::getEntity() const
	{
		return entity_;
	}

BEGIN_DEFINE_CALLABLE(CustomObjectWidget, Widget)
	DEFINE_FIELD(object, "custom_obj")
		return variant(obj.entity_.get());
	DEFINE_SET_FIELD
		std::map<variant, variant> m;
		m[variant("object")] = value;
		obj.init(variant(&m));
	DEFINE_FIELD(overlay, "map|builtin widget|null")
		return variant(obj.overlay_.get());
	DEFINE_SET_FIELD
		obj.overlay_ = widget_factory::create(value, obj.getEnvironment());
	DEFINE_FIELD(handleProcess, "bool")
		return variant::from_bool(obj.handleProcess_on_entity_);
END_DEFINE_CALLABLE(CustomObjectWidget)

	void CustomObjectWidget::handleDraw() const
	{
		if(entity_) {
			// XXX may need to adjust current model by x,y
			entity_->draw(x(), y());
			entity_->drawLater(x(), y());
		}
		if(overlay_) {
			overlay_->setLoc(x() + width()/2 - overlay_->width()/2, y() + height()/2 - overlay_->height()/2);
			overlay_->draw();
		}
	}

	bool CustomObjectWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		if((event.type == SDL_MOUSEWHEEL) && inWidget(event.button.x, event.button.y)) {
			// skip processing if mousewheel event
			if(entity_) {
				CustomObject* obj = static_cast<CustomObject*>(entity_.get());
				return obj->handle_sdl_event(event, claimed);
			}
		}

		if(event.type == SDL_MOUSEMOTION) {
			const SDL_MouseMotionEvent& e = event.motion;
			if(inWidget(e.x,e.y)) {
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
			if(inWidget(e.x,e.y)) {
				claimed = claimMouseEvents();
			}
		} else if(event.type == SDL_MOUSEBUTTONUP) {
			const SDL_MouseButtonEvent& e = event.button;
			if(inWidget(e.x,e.y)) {
				if(on_click_) {
					on_click_(event.button.button);
				}
				claimed = claimMouseEvents();
			}
		}

		if(entity_) {
			CustomObject* obj = dynamic_cast<CustomObject*>(entity_.get());
			return obj->handle_sdl_event(event, claimed);
		}
		return claimed;
	}

	void CustomObjectWidget::handleProcess()
	{
		Widget::handleProcess();
		if(entity_ && handleProcess_on_entity_) {
			CustomObject* obj = dynamic_cast<CustomObject*>(entity_.get());
			obj->process(Level::current());
		}

		if(overlay_) {
			overlay_->process();
		}

//		if(do_commands_on_process && --do_commands_on_process == 0) {
//			variant value = commands_handler_->execute();
//			entity_->executeCommand(value);
//		}
	}

}
