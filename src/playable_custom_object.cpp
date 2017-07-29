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

#include <set>

#include "asserts.hpp"
#include "collision_utils.hpp"
#include "difficulty.hpp"
#include "formatter.hpp"
#include "preferences.hpp"
#include "input.hpp"
#include "joystick.hpp"
#include "level.hpp"
#include "level_runner.hpp"
#include "playable_custom_object.hpp"
#include "string_utils.hpp"
#include "variant_utils.hpp"
#include "widget.hpp"

#ifndef NO_EDITOR
#include "editor.hpp"
#endif


namespace 
{
	std::set<gui::Widget*>& get_key_handling_widgets()
	{
		static std::set<gui::Widget*> res;
		return res;
	}
}

PlayableCustomObject::PlayableCustomObject(const CustomObject& obj)
	: CustomObject(obj), 
	  player_info_(*this), 
	  difficulty_(0), 
 	  vertical_look_(0),
      underwater_ctrl_x_(0), 
	  underwater_ctrl_y_(0), 
	  underwater_controls_(false),
	  can_interact_(0)
{
}

PlayableCustomObject::PlayableCustomObject(const PlayableCustomObject& obj)
	: CustomObject(obj), 
	  player_info_(obj.player_info_),
      difficulty_(obj.difficulty_),
      save_condition_(obj.save_condition_), 
	  vertical_look_(0),
      underwater_ctrl_x_(0), 
	  underwater_ctrl_y_(0), 
	  underwater_controls_(false),
	  can_interact_(0)
{
	player_info_.setEntity(*this);
}

PlayableCustomObject::PlayableCustomObject(variant node)
	: CustomObject(node), 
	  player_info_(*this, node),
      difficulty_(node["difficulty"].as_int(0)),
      vertical_look_(0), 
	  underwater_ctrl_x_(0), 
	  underwater_ctrl_y_(0),
	  underwater_controls_(node["underwater_controls"].as_bool(false)),
	  can_interact_(0)
{
}

PlayableCustomObject::PlayableCustomObject(const std::string& type, int x, int y, bool face_right, bool deferInitProperties)
	: CustomObject(type, x, y, face_right, deferInitProperties),
	player_info_(*this),
	difficulty_(0),
	vertical_look_(0),
	underwater_ctrl_x_(0),
	underwater_ctrl_y_(0),
	underwater_controls_(false),
	can_interact_(0)
{
}

variant PlayableCustomObject::write() const
{
	variant_builder node;
	node.merge_object(CustomObject::write());
	node.merge_object(player_info_.write());
	node.add("is_human", true);
	if(difficulty_) {
		node.add("difficulty", difficulty_);
	}

	if(underwater_controls_) {
		node.add("underwater_controls", true);
	}
	return node.build();
}

void PlayableCustomObject::saveGame()
{
	save_condition_.reset(); //reset before we clone so the clone doesn't copy it
	save_condition_ = clone();
	save_condition_->addToLevel();
}

EntityPtr PlayableCustomObject::backup() const
{
	return EntityPtr(new PlayableCustomObject(*this));
}

EntityPtr PlayableCustomObject::clone() const
{
	return EntityPtr(new PlayableCustomObject(*this));
}

bool PlayableCustomObject::isActive(const rect& screen_area) const
{
	//player objects are always active.
	return true;
}

bool PlayableCustomObject::onPlatform() const
{
	CollisionInfo stand_info;
	const bool standing = isStanding(Level::current(), &stand_info) != CustomObject::STANDING_STATUS::NOT_STANDING;
	return standing && stand_info.platform;
}

int PlayableCustomObject::walkUpOrDownStairs() const
{
	return controlStatus(controls::CONTROL_DOWN) - controlStatus(controls::CONTROL_UP);
}

void PlayableCustomObject::process(Level& lvl)
{
	prev_ctrl_keys_ = ctrl_keys_;
	ctrl_keys_ = getCtrlKeys();

	if(player_info_.currentLevel() != lvl.id()) {
		player_info_.setCurrentLevel(lvl.id());
	}

	if(can_interact_ > 0) {
		--can_interact_;
	}

	// Additional check to ensure that if a widget has focus we don't
	// pass the event on to the playable object.
	bool process_controls = true;
	for(auto& w : get_key_handling_widgets()) {
		if(w->hasFocus()) {
			process_controls = false;
		}
	}

	if(process_controls) {
		bool controls[controls::NUM_CONTROLS];
		for(int n = 0; n != controls::NUM_CONTROLS; ++n) {
			controls[n] = controlStatus(static_cast<controls::CONTROL_ITEM>(n));
		}

		clearControlStatus();
		readControls(lvl.cycle());

		// XX Need to abstract this to read controls and mappings from global game file.
		static const std::string keys[] = { "up", "down", "left", "right", "attack", "jump", "tongue" };	
		for(int n = 0; n != controls::NUM_CONTROLS; ++n) {
			if(controls[n] != controlStatus(static_cast<controls::CONTROL_ITEM>(n))) {
				if(controls[n]) {
					handleEvent("end_ctrl_" + keys[n]);
				} else {
					handleEvent("ctrl_" + keys[n]);
				}
			}
		}
	}

	CustomObject::process(lvl);

}

namespace 
{
	static const char* ctrl[] = { "ctrl_up", "ctrl_down", "ctrl_left", "ctrl_right", "ctrl_attack", "ctrl_jump", "ctrl_tongue" };
}

variant PlayableCustomObject::getValue(const std::string& key) const
{
	if(key.substr(0, 11) == "difficulty_") {
		return variant(difficulty::from_string(key.substr(11)));		
	} else if(key == "difficulty") {
		return getValueBySlot(CUSTOM_OBJECT_PLAYER_DIFFICULTY);
	} else if(key == "can_interact") {
		return getValueBySlot(CUSTOM_OBJECT_PLAYER_CAN_INTERACT);
	} else if(key == "underwater_controls") {
		return getValueBySlot(CUSTOM_OBJECT_PLAYER_UNDERWATER_CONTROLS);
	} else if(key == "ctrl_mod_key") {
		return getValueBySlot(CUSTOM_OBJECT_PLAYER_CTRL_MOD_KEY);
	} else if(key == "ctrl_mod_keys") {
		return getValueBySlot(CUSTOM_OBJECT_PLAYER_CTRL_MOD_KEYS);
	} else if(key == "ctrl_keys") {
		return getValueBySlot(CUSTOM_OBJECT_PLAYER_CTRL_KEYS);
	} else if(key == "ctrl_mice") {
		return getValueBySlot(CUSTOM_OBJECT_PLAYER_CTRL_MICE);
	} else if(key == "ctrl_tilt") {
		return getValueBySlot(CUSTOM_OBJECT_PLAYER_CTRL_TILT);
	} else if(key == "ctrl_x") {
		return getValueBySlot(CUSTOM_OBJECT_PLAYER_CTRL_X);
	} else if(key == "ctrl_y") {
		return getValueBySlot(CUSTOM_OBJECT_PLAYER_CTRL_Y);
	} else if(key == "control_scheme") {
		return getValueBySlot(CUSTOM_OBJECT_PLAYER_CONTROL_SCHEME);
	}

	for(int n = 0; n < sizeof(ctrl)/sizeof(*ctrl); ++n) {
		if(key == ctrl[n]) {
			return variant(controlStatus(static_cast<controls::CONTROL_ITEM>(n)));
		}
	}

	if(key == "ctrl_user") {
		return controlStatusUser();
	}

	if(key == "player") {
		return variant::from_bool(true);
	} else if(key == "vertical_look") {
		return getValueBySlot(CUSTOM_OBJECT_PLAYER_VERTICAL_LOOK);
	}

	return CustomObject::getValue(key);
}

variant PlayableCustomObject::getPlayerValueBySlot(int slot) const
{
	switch(slot) {
	case CUSTOM_OBJECT_PLAYER_DIFFICULTY: {
		if(preferences::force_difficulty() != std::numeric_limits<int>::min()) {
			return variant(preferences::force_difficulty());
		}

		return variant(difficulty_);
	}
	case CUSTOM_OBJECT_PLAYER_CAN_INTERACT: {
		return variant(can_interact_);
	}
	case CUSTOM_OBJECT_PLAYER_UNDERWATER_CONTROLS: {
		return variant(underwater_controls_);
	}
	case CUSTOM_OBJECT_PLAYER_CTRL_MOD_KEY: {
		return variant(SDL_GetModState());
	}
	case CUSTOM_OBJECT_PLAYER_CTRL_MOD_KEYS: {
		std::vector<variant> res;
		auto mod_keys = SDL_GetModState();
		if(mod_keys & KMOD_LSHIFT) {
			res.emplace_back("lshift");
		}
		if(mod_keys & KMOD_RSHIFT) {
			res.emplace_back("rshift");
		}
		if(mod_keys & KMOD_LCTRL) {
			res.emplace_back("lctrl");
		}
		if(mod_keys & KMOD_RCTRL) {
			res.emplace_back("lctrl");
		}
		if(mod_keys & KMOD_LALT) {
			res.emplace_back("lalt");
		}
		if(mod_keys & KMOD_RALT) {
			res.emplace_back("ralt");
		}
		if(mod_keys & KMOD_LGUI) {
			res.emplace_back("lgui");
		}
		if(mod_keys & KMOD_RGUI) {
			res.emplace_back("rgui");
		}
		if(mod_keys & KMOD_NUM) {
			res.emplace_back("num");
		}
		if(mod_keys & KMOD_CAPS) {
			res.emplace_back("caps");
		}
		if(mod_keys & KMOD_MODE) {
			res.emplace_back("mode");
		}
		if(mod_keys & (KMOD_LSHIFT | KMOD_RSHIFT)) {
			res.emplace_back("shift");
		}
		if(mod_keys & (KMOD_LCTRL | KMOD_RCTRL)) {
			res.emplace_back("ctrl");
		}
		if(mod_keys & (KMOD_LALT | KMOD_RALT)) {
			res.emplace_back("alt");
		}
		if(mod_keys & (KMOD_LGUI | KMOD_RGUI)) {
			res.emplace_back("gui");
		}
		return variant(&res);
	}
	case CUSTOM_OBJECT_PLAYER_CTRL_KEYS: {
		if(ctrl_keys_.is_null()) {
			std::vector<variant> res;
			return variant(&res);
		}

		return ctrl_keys_;

	}
	case CUSTOM_OBJECT_PLAYER_CTRL_PREV_KEYS: {
		if(prev_ctrl_keys_.is_null()) {
			std::vector<variant> res;
			return variant(&res);
		}

		return prev_ctrl_keys_;

	}
	case CUSTOM_OBJECT_PLAYER_CTRL_MICE: {
		std::vector<variant> info;
		int x, y;
		Uint8 button_state = input::sdl_get_mouse_state(&x, &y);

		info.push_back(variant(x));
		info.push_back(variant(y));

		if(button_state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
			info.push_back(variant("left"));
		}

		if(button_state & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
			info.push_back(variant("right"));
		}

		if(button_state & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
			info.push_back(variant("middle"));
		}

		if(button_state & SDL_BUTTON(SDL_BUTTON_X1)) { //these aren't tested
			info.push_back(variant("x1"));
		}

		if(button_state & SDL_BUTTON(SDL_BUTTON_X2)) {
			info.push_back(variant("x2"));
		}

		std::vector<variant> result;
		result.push_back(variant(&info));
		return variant(&result);
	}
	case CUSTOM_OBJECT_PLAYER_CTRL_TILT: {
		return variant(-joystick::iphone_tilt());
	}
	case CUSTOM_OBJECT_PLAYER_CTRL_X: {
		return variant(underwater_ctrl_x_);
	}
	case CUSTOM_OBJECT_PLAYER_CTRL_Y: {
		return variant(underwater_ctrl_y_);
	}
	case CUSTOM_OBJECT_PLAYER_CONTROL_SCHEME: {
		return variant(preferences::control_scheme());
	}
	case CUSTOM_OBJECT_PLAYER_VERTICAL_LOOK: {
		return variant(vertical_look_);
	}
	case CUSTOM_OBJECT_PLAYER_CONTROL_LOCK: {
        std::vector<variant> result;
        
        const unsigned char* locked_control_frame = controls::get_local_control_lock();
        
        if (locked_control_frame == nullptr) {
            return variant();
        }
        
        for(int i = 0; i < 8; ++i){
            if((*locked_control_frame & (0x01 << i)) ){
                
                result.push_back( variant(ctrl[i]) );
            } else {
                //this key isn't pressed
            }            
        }
       
        return variant(&result);
       
    }
        
	}

	ASSERT_LOG(false, "unknown slot in get_player_value_by_slot: " << slot);
}

void PlayableCustomObject::setPlayerValueBySlot(int slot, const variant& value)
{
	switch(slot) {
	case CUSTOM_OBJECT_PLAYER_DIFFICULTY:
		difficulty_ = value.as_int();
	break;
	case CUSTOM_OBJECT_PLAYER_CAN_INTERACT:
		can_interact_ = value.as_int();
	break;
	case CUSTOM_OBJECT_PLAYER_UNDERWATER_CONTROLS:
		underwater_controls_ = value.as_bool();
	break;
	case CUSTOM_OBJECT_PLAYER_VERTICAL_LOOK:
		vertical_look_ = value.as_int();
	break;
	case CUSTOM_OBJECT_PLAYER_CONTROL_LOCK:
		if(value.is_null()) {
			control_lock_.reset();
		} else if(value.is_list()) {
			unsigned char state = 0;
			for(int n = 0; n != value.num_elements(); ++n) {
				ASSERT_LOG(value[n].is_string(), "MEMBER OF control_lock LIST NOT A STRING");
				const std::string& str = value[n].as_string();
				int control_key = -1;
				for(int m = 0; m != sizeof(ctrl)/sizeof(*ctrl); ++m) {
					if(ctrl[m] == str) {
						control_key = m;
						break;
					}
				}

				ASSERT_LOG(control_key != -1, "ILLEGAL STRING SET FOR control_lock: '" << str << "' LEGAL KEYS ARE ctrl_(up|down|left|right|attack|jump)");
				state |= 1 << control_key;
			}

			//destroy the old one before creating a new control_lock,
			//since control_lock objects must be constructed and destroyed
			//in FIFO order.
			control_lock_.reset();
			control_lock_.reset(new controls::local_controls_lock(state));
		} else {
			ASSERT_LOG(false, "BAD VALUE WHEN SETTING control_lock KEY. A LIST OR nullptr IS REQUIRED: " << value.to_debug_string());
		}
	break;
	}
}

void PlayableCustomObject::setValue(const std::string& key, const variant& value)
{
	if(key == "difficulty") {
		setPlayerValueBySlot(CUSTOM_OBJECT_PLAYER_DIFFICULTY, value);
	} else if(key == "can_interact") {
		setPlayerValueBySlot(CUSTOM_OBJECT_PLAYER_CAN_INTERACT, value);
	} else if(key == "underwater_controls") {
		setPlayerValueBySlot(CUSTOM_OBJECT_PLAYER_UNDERWATER_CONTROLS, value);
	} else if(key == "vertical_look") {
		setPlayerValueBySlot(CUSTOM_OBJECT_PLAYER_VERTICAL_LOOK, value);
	} else if(key == "control_lock") {
		setPlayerValueBySlot(CUSTOM_OBJECT_PLAYER_CONTROL_LOCK, value);
	} else {
		CustomObject::setValue(key, value);
	}
}

void PlayableCustomObject::surrenderReferences(GarbageCollector* collector)
{
	CustomObject::surrenderReferences(collector);

	collector->surrenderPtr(&save_condition_, "SAVE_CONDITION");
}

void PlayableCustomObject::registerKeyboardOverrideWidget(gui::Widget* widget)
{
	LOG_DEBUG("adding widget: " << widget);
	get_key_handling_widgets().emplace(widget);	
}

void PlayableCustomObject::unregisterKeyboardOverrideWidget(gui::Widget* widget)
{
	LOG_DEBUG("removing widget: " << widget);
	auto it = get_key_handling_widgets().find(widget);
	if(it != get_key_handling_widgets().end()) {
		get_key_handling_widgets().erase(it);
	}
}


variant PlayableCustomObject::getCtrlKeys() const
{
	std::vector<variant> result;
	if(LevelRunner::getCurrent() && LevelRunner::getCurrent()->get_debug_console() && LevelRunner::getCurrent()->get_debug_console()->hasKeyboardFocus()) {
		//the debug console is stealing all keystrokes.
		return variant(&result);
	}

	int ary_length;
	const Uint8* key_state = SDL_GetKeyboardState(&ary_length);

#ifndef NO_EDITOR
	if(LevelRunner::getCurrent()) {
		ConstEditorPtr e = LevelRunner::getCurrent()->get_editor();
		if(e && e->hasKeyboardFocus()) {
			//the editor has the focus, so we tell the game there
			//are no keys pressed.
			ary_length = 0;
		}
	}
#endif

	for(int count = 0; count < ary_length; ++count) {
		if(key_state[count]) {				//Returns only keys that are down so the list that ffl has to deal with is small.
			SDL_Keycode k = SDL_GetKeyFromScancode(SDL_Scancode(count));
			if(k < 128 && util::c_isprint(k)) {
				std::string str(1,k);
				result.push_back(variant(str));
			} else {
				const char* name = SDL_GetKeyName(k);
				if(*name) {
					result.push_back(variant(std::string(name)));
				} else {
					result.push_back(variant(k));
				}
			}
		}
	}
	return variant(&result);
}
