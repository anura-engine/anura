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

#include "graphical_font_label.hpp"
#include "i18n.hpp"
#include "key_button.hpp"
#include "framed_gui_element.hpp"
#include "widget_factory.hpp"

namespace gui 
{
	namespace 
	{
		const int vpadding = 4;
		const int hpadding = 10;

		void stoupper(std::string& s)
		{
			std::string::iterator i = s.begin();
			std::string::iterator end = s.end();

			while (i != end) {
				*i = ::toupper((unsigned char)*i);
				++i;
			}
		}

	}

	std::string KeyButton::getKeyName(key_type key) 
	{
		switch(key) {
		case SDLK_LEFT:
			return std::string(("←"));
		case SDLK_RIGHT:
			return std::string(("→"));
		case SDLK_UP:
			return std::string(("↑"));
		case SDLK_DOWN:
			return std::string(("↓"));
		default:
			break;
		}
		return SDL_GetKeyName(key);
	}

	key_type get_key_sym(const std::string& s)
	{
		if(s == "UP" || s == (("↑"))) {
			return SDLK_UP;
		} else if(s == "DOWN" || s == (("↓"))) {
			return SDLK_DOWN;
		} else if(s == "LEFT" || s == (("←"))) {
			return SDLK_LEFT;
		} else if(s == "RIGHT" || s == (("→"))) {
			return SDLK_RIGHT;
		}

		return SDL_GetKeyFromName(s.c_str());
	}

	KeyButton::KeyButton(key_type key, BUTTON_RESOLUTION buttonResolution)
	  : label_(WidgetPtr(new GraphicalFontLabel(getKeyName(key), "door_label", 2))),
		key_(key), button_resolution_(buttonResolution),
		normal_button_image_set_(FramedGuiElement::get("regular_button")),
		depressed_button_image_set_(FramedGuiElement::get("regular_button_pressed")),
		focus_button_image_set_(FramedGuiElement::get("regular_button_focus")),
		current_button_image_set_(normal_button_image_set_), grab_keys_(false)
	
	{
		setEnvironment();
		setDim(label_->width()+hpadding*2,label_->height()+vpadding*2);
	}

	KeyButton::KeyButton(const variant& v, game_logic::FormulaCallable* e) 
		: Widget(v,e), 	normal_button_image_set_(FramedGuiElement::get("regular_button")),
		depressed_button_image_set_(FramedGuiElement::get("regular_button_pressed")),
		focus_button_image_set_(FramedGuiElement::get("regular_button_focus")),
		current_button_image_set_(normal_button_image_set_), grab_keys_(false)
	{
		std::string key = v["key"].as_string();
		key_ = get_key_sym(key);
		label_ = v.has_key("label") ? widget_factory::create(v["label"], e) : WidgetPtr(new GraphicalFontLabel(key, "door_label", 2));
		button_resolution_ = v["resolution"].as_string_default("normal") == "normal" ? BUTTON_SIZE_NORMAL_RESOLUTION : BUTTON_SIZE_DOUBLE_RESOLUTION;

		setDim(label_->width()+hpadding*2,label_->height()+vpadding*2);
	}

	void KeyButton::handleDraw() const
	{
		label_->setLoc(x()+width()/2 - label_->width()/2,y()+height()/2 - label_->height()/2);
		current_button_image_set_->blit(x(),y(),width(),height(), button_resolution_ != BUTTON_SIZE_NORMAL_RESOLUTION);
		label_->draw();
	}

	bool KeyButton::handleEvent(const SDL_Event& event, bool claimed)
	{
		if(claimed) {
			current_button_image_set_ = normal_button_image_set_;
		}

		if(event.type == SDL_MOUSEMOTION && !grab_keys_) {
			const SDL_MouseMotionEvent& e = event.motion;
			if(current_button_image_set_ == depressed_button_image_set_) {
				//pass
			} else if(inWidget(e.x,e.y)) {
				current_button_image_set_ = focus_button_image_set_;
			} else {
				current_button_image_set_ = normal_button_image_set_;
			}
		} else if(event.type == SDL_MOUSEBUTTONDOWN) {
			const SDL_MouseButtonEvent& e = event.button;
			if(inWidget(e.x,e.y)) {
				current_button_image_set_ = depressed_button_image_set_;
			}
		} else if(event.type == SDL_MOUSEBUTTONUP) {
			const SDL_MouseButtonEvent& e = event.button;
			if(current_button_image_set_ == depressed_button_image_set_) {
				if(inWidget(e.x,e.y)) {
					current_button_image_set_ = focus_button_image_set_;
					grab_keys_ = true;
					dynamic_cast<GraphicalFontLabel*>(label_.get())->setText("...");
					claimed = claimMouseEvents();
				} else {
					current_button_image_set_ = normal_button_image_set_;
				}
			} else if (grab_keys_) {
				dynamic_cast<GraphicalFontLabel*>(label_.get())->setText(getKeyName(key_));
				current_button_image_set_ = normal_button_image_set_;
				grab_keys_ = false;
			}
		}

		if(event.type == SDL_KEYDOWN && grab_keys_) {
			key_ = event.key.keysym.sym;
			if(key_ != SDLK_RETURN && key_ != SDLK_ESCAPE) {
				dynamic_cast<GraphicalFontLabel*>(label_.get())->setText(getKeyName(key_));
				claimed = true;
				current_button_image_set_ = normal_button_image_set_;
				grab_keys_ = false;
			}
		}

		return claimed;
	}

	key_type KeyButton::get_key() {
		return key_;
	}

	BEGIN_DEFINE_CALLABLE(KeyButton, Widget)
		DEFINE_FIELD(key, "int")
			return variant(obj.key_);
	END_DEFINE_CALLABLE(KeyButton)
}
