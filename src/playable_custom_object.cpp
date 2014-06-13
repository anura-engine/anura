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
#include "asserts.hpp"
#include "collision_utils.hpp"
#include "difficulty.hpp"
#include "formatter.hpp"
#include "preferences.hpp"
#include "input.hpp"
#include "iphone_controls.hpp"
#include "joystick.hpp"
#include "level.hpp"
#include "level_runner.hpp"
#include "playable_custom_object.hpp"
#include "string_utils.hpp"
#include "variant_utils.hpp"

#ifndef NO_EDITOR
#include "editor.hpp"
#endif

PlayableCustomObject::PlayableCustomObject(const custom_object& obj)
  : custom_object(obj), player_info_(*this), difficulty_(0), vertical_look_(0),
    underwater_ctrl_x_(0), underwater_ctrl_y_(0), underwater_controls_(false),
	can_interact_(0)
{
}

PlayableCustomObject::PlayableCustomObject(const PlayableCustomObject& obj)
  : custom_object(obj), player_info_(obj.player_info_),
    difficulty_(obj.difficulty_),
    save_condition_(obj.save_condition_), vertical_look_(0),
    underwater_ctrl_x_(0), underwater_ctrl_y_(0), underwater_controls_(false),
	can_interact_(0)
{
	player_info_.setEntity(*this);
}

PlayableCustomObject::PlayableCustomObject(variant node)
  : custom_object(node), player_info_(*this, node),
    difficulty_(node["difficulty"].as_int(0)),
    vertical_look_(0), underwater_ctrl_x_(0), underwater_ctrl_y_(0),
	underwater_controls_(node["underwater_controls"].as_bool(false)),
	can_interact_(0)
{
}

variant PlayableCustomObject::write() const
{
	variant_builder node;
	node.merge_object(custom_object::write());
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

bool PlayableCustomObject::on_platform() const
{
	collision_info stand_info;
	const bool standing = isStanding(level::current(), &stand_info);
	return standing && stand_info.platform;
}

int PlayableCustomObject::walkUpOrDownStairs() const
{
	return controlStatus(controls::CONTROL_DOWN) - controlStatus(controls::CONTROL_UP);
}

void PlayableCustomObject::process(level& lvl)
{
	if(player_info_.currentLevel() != lvl.id()) {
		player_info_.setCurrentLevel(lvl.id());
	}

	if(can_interact_ > 0) {
		--can_interact_;
	}

	iphone_controls::set_underwater(underwater_controls_);
	iphone_controls::set_can_interact(can_interact_ != 0);
	iphone_controls::set_on_platform(on_platform());
	iphone_controls::set_standing(isStanding(level::current()));

	float underwater_x, underwater_y;
	if(underwater_controls_ && iphone_controls::water_dir(&underwater_x, &underwater_y)) {
		underwater_ctrl_x_ = underwater_x*1000;
		underwater_ctrl_y_ = underwater_y*1000;
	} else {
		underwater_ctrl_x_ = 0;
		underwater_ctrl_y_ = 0;
	}
	
	reverse_ab_ = preferences::reverse_ab();

	bool controls[controls::NUM_CONTROLS];
	for(int n = 0; n != controls::NUM_CONTROLS; ++n) {
		controls[n] = controlStatus(static_cast<controls::CONTROL_ITEM>(n));
	}

	clearControlStatus();
	readControls(lvl.cycle());
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

	custom_object::process(lvl);

}

namespace {
	static const char* ctrl[] = { "ctrl_up", "ctrl_down", "ctrl_left", "ctrl_right", "ctrl_attack", "ctrl_jump", "ctrl_tongue" };
}

variant PlayableCustomObject::getValue(const std::string& key) const
{
	if(key.substr(0, 11) == "difficulty_") {
		return variant(difficulty::from_string(key.substr(11)));		
	} else if(key == "difficulty") {
		return getValue_by_slot(CUSTOM_OBJECT_PLAYER_DIFFICULTY);
	} else if(key == "can_interact") {
		return getValue_by_slot(CUSTOM_OBJECT_PLAYER_CAN_INTERACT);
	} else if(key == "underwater_controls") {
		return getValue_by_slot(CUSTOM_OBJECT_PLAYER_UNDERWATER_CONTROLS);
	} else if(key == "ctrl_mod_key") {
		return getValue_by_slot(CUSTOM_OBJECT_PLAYER_CTRL_MOD_KEY);
	} else if(key == "ctrl_keys") {
		return getValue_by_slot(CUSTOM_OBJECT_PLAYER_CTRL_KEYS);
	} else if(key == "ctrl_mice") {
		return getValue_by_slot(CUSTOM_OBJECT_PLAYER_CTRL_MICE);
	} else if(key == "ctrl_tilt") {
		return getValue_by_slot(CUSTOM_OBJECT_PLAYER_CTRL_TILT);
	} else if(key == "ctrl_x") {
		return getValue_by_slot(CUSTOM_OBJECT_PLAYER_CTRL_X);
	} else if(key == "ctrl_y") {
		return getValue_by_slot(CUSTOM_OBJECT_PLAYER_CTRL_Y);
	} else if(key == "ctrl_reverse_ab") {
		return getValue_by_slot(CUSTOM_OBJECT_PLAYER_CTRL_REVERSE_AB);
	} else if(key == "control_scheme") {
		return getValue_by_slot(CUSTOM_OBJECT_PLAYER_CONTROL_SCHEME);
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
		return getValue_by_slot(CUSTOM_OBJECT_PLAYER_VERTICAL_LOOK);
	}

	return custom_object::getValue(key);
}

variant PlayableCustomObject::getPlayerValueBySlot(int slot) const
{
	switch(slot) {
	case CUSTOM_OBJECT_PLAYER_DIFFICULTY: {
		if(preferences::force_difficulty() != INT_MIN) {
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
	case CUSTOM_OBJECT_PLAYER_CTRL_KEYS: {
		std::vector<variant> result;
		if(level_runner::getCurrent() && level_runner::getCurrent()->get_debug_console() && level_runner::getCurrent()->get_debug_console()->hasKeyboardFocus()) {
			//the debug console is stealing all keystrokes.
			return variant(&result);
		}

#if !TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
		int ary_length;
		const Uint8* key_state = SDL_GetKeyboardState(&ary_length);

#ifndef NO_EDITOR
		if(level_runner::getCurrent()) {
			const editor* e = level_runner::getCurrent()->get_editor();
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
					result.push_back(variant(k));
				}
			}
		}
#endif
		return variant(&result);
	}
	case CUSTOM_OBJECT_PLAYER_CTRL_MICE: {
		std::vector<variant> result;
		

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
		const int nmice = SDL_GetNumMice();
#else
		const int nmice = 1;
#endif
		for(int n = 0; n != nmice; ++n) {
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
			SDL_SelectMouse(n);
#endif
			std::vector<variant> info;
			int x, y;
			Uint8 button_state = input::sdl_get_mouse_state(&x, &y);
			translate_mouse_coords(&x, &y);

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

			result.push_back(variant(&info));
		}

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
	case CUSTOM_OBJECT_PLAYER_CTRL_REVERSE_AB: {
		return variant::from_bool(reverse_ab_);
	}
	case CUSTOM_OBJECT_PLAYER_CONTROL_SCHEME: {
		return variant(preferences::control_scheme());
	}
	case CUSTOM_OBJECT_PLAYER_VERTICAL_LOOK: {
		return variant(vertical_look_);
	}
	case CUSTOM_OBJECT_PLAYER_CONTROL_LOCK: {
		return variant();
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
			ASSERT_LOG(false, "BAD VALUE WHEN SETTING control_lock KEY. A LIST OR NULL IS REQUIRED: " << value.to_debug_string());
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
		custom_object::setValue(key, value);
	}
}
