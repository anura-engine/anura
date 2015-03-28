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


#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "asserts.hpp"
#include "haptic.hpp"
#include "joystick.hpp"
#include "preferences.hpp"

#if defined(TARGET_BLACKBERRY)
#include <bps/accelerometer.h>
#include <bps/sensor.h>
#include <bps/bps.h>
#endif

namespace haptic 
{
	namespace 
	{
		std::map<int,std::shared_ptr<SDL_Haptic>> haptic_devices;
		typedef std::map<SDL_Haptic*,std::map<std::string,int>> haptic_effect_table;
		haptic_effect_table& get_effects() {
			static haptic_effect_table res;
			return res;
		}
	}
}

namespace joystick 
{
	namespace 
	{
		std::vector<std::shared_ptr<SDL_Joystick>> joysticks;
		std::map<int,std::shared_ptr<SDL_GameController>> game_controllers;
	}

	Manager::Manager() 
	{
		if(SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
			LOG_ERROR("Unable to initialise joystick subsystem");
		}
		if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
			LOG_ERROR("Unable to initialise game controller subsystem");
		}
		if(SDL_InitSubSystem(SDL_INIT_HAPTIC) != 0) {
			LOG_ERROR("Unable to initialise haptic subsystem");
		}
#if defined(__ANDROID__)
		// We're just going to open 1 joystick on android platform.
		int n = 0; {
#else
		for(int n = 0; n != SDL_NumJoysticks(); ++n) {
#endif
			if(SDL_IsGameController(n)) {
				SDL_GameController *controller = SDL_GameControllerOpen(n);
				if(controller) {
					game_controllers[n] = std::shared_ptr<SDL_GameController>(controller, [](SDL_GameController* p){SDL_GameControllerClose(p);});
				} else {
					LOG_WARN("Couldn't open game controller: " << SDL_GetError());
				}
			} else {
				SDL_Joystick* j = SDL_JoystickOpen(n);
				if(j) {
					if (SDL_JoystickNumButtons(j) == 0) {
						// We're probably dealing with an accellerometer here.
						SDL_JoystickClose(j);

						LOG_INFO("discarding joystick " << n << " for being an accellerometer");
					} else {
						joysticks.push_back(std::shared_ptr<SDL_Joystick>(j, [](SDL_Joystick* js){SDL_JoystickClose(js);}));
					}
				}
			}

			SDL_Haptic *haptic = SDL_HapticOpen(n);
			if(haptic) {
				haptic::haptic_devices[n] = std::shared_ptr<SDL_Haptic>(haptic, [](SDL_Haptic* h){SDL_HapticClose(h);});
				if(SDL_HapticRumbleInit(haptic) != 0) {
					LOG_WARN("Failed to initialise a simple rumble effect");
					haptic::haptic_devices.erase(n);
				}
				// buzz the device when we start.
				if(SDL_HapticRumblePlay(haptic, 0.5, 1000) != 0) {
					LOG_WARN("Failed to play a simple rumble effect");
					haptic::haptic_devices.erase(n);
				}
			}
		}

		LOG_INFO("Initialized " << joysticks.size() << " joysticks");
		LOG_INFO("Initialized " << game_controllers.size() << " game controllers");
		LOG_INFO("Initialized " << haptic::haptic_devices.size() << " haptic devices");
	}

	Manager::~Manager() 
	{
		joysticks.clear();
		game_controllers.clear();
		haptic::get_effects().clear();
		haptic::haptic_devices.clear();

		SDL_QuitSubSystem(SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);

#if defined(TARGET_BLACKBERRY)
		bps_shutdown();
#endif
	}

	bool pump_events(const SDL_Event& ev, bool claimed) 
	{
		if(claimed) {
			return claimed;
		}
		switch(ev.type) {
			case SDL_CONTROLLERDEVICEADDED: {
				auto it = game_controllers.find(ev.cdevice.which);
				if(it != game_controllers.end()) {
					LOG_INFO("replacing game controller at index " << ev.cdevice.which);
					game_controllers.erase(it);
				}
				SDL_GameController *controller = SDL_GameControllerOpen(ev.cdevice.which);
				if(controller) {
					game_controllers[ev.cdevice.which] = std::shared_ptr<SDL_GameController>(controller, [](SDL_GameController* p){SDL_GameControllerClose(p);});
				} else {
					LOG_WARN("Couldn't open game controller: " << SDL_GetError());
				}
				return true;
			}
			case SDL_CONTROLLERDEVICEREMOVED: {
				auto it = game_controllers.find(ev.cdevice.which);
				if(it != game_controllers.end()) {
					game_controllers.erase(it);
				} else {
					LOG_WARN("Controller removed message, no matching controller in list");
				}
				return true;
			}
		}
		return false;
	}

	void update() 
	{
		if(preferences::use_joystick()) {
			SDL_JoystickUpdate();
		}
	}

	bool up() 
	{
		if(!preferences::use_joystick()) {
			return false;
		}

		for(auto gc : game_controllers) {
			if (SDL_GameControllerGetAxis(gc.second.get(), SDL_CONTROLLER_AXIS_LEFTY) < -4096*2) {
				return true;
			}
			Uint8 state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_DPAD_UP);
			if(state != 0) {
				return true;
			}
		}

		for(auto j : joysticks) {
			Sint16  y = SDL_JoystickGetAxis(j.get(), 1);
			if (y < -4096*2) {
				return true;
			}

			const int nhats = SDL_JoystickNumHats(j.get());
			for(int n = 0; n != nhats; ++n) {
				const Uint8 state = SDL_JoystickGetHat(j.get(), n);
				switch(state) {
				case SDL_HAT_UP:
				case SDL_HAT_RIGHTUP:
				case SDL_HAT_LEFTUP:
						return true;
				}
			}

		}

		return false;
	}

	bool down() 
	{
		if(!preferences::use_joystick()) {
			return false;
		}

		for(auto gc : game_controllers) {
			if (SDL_GameControllerGetAxis(gc.second.get(), SDL_CONTROLLER_AXIS_LEFTY) > 4096*2) {
				return true;
			}
			Uint8 state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_DPAD_DOWN);
			if(state != 0) {
				return true;
			}
		}

		for(auto j : joysticks) {
			Sint16  y = SDL_JoystickGetAxis(j.get(), 1);
			if (y > 4096*2) {
				return true;
			}

			const int nhats = SDL_JoystickNumHats(j.get());
			for(int n = 0; n != nhats; ++n) {
				const Uint8 state = SDL_JoystickGetHat(j.get(), n);
				switch(state) {
				case SDL_HAT_DOWN:
				case SDL_HAT_RIGHTDOWN:
				case SDL_HAT_LEFTDOWN:
						return true;
				}
			}

		}

		return false;
	}

	bool left() 
	{
		if(!preferences::use_joystick()) {
			return false;
		}

		for(auto gc : game_controllers) {
			if (SDL_GameControllerGetAxis(gc.second.get(), SDL_CONTROLLER_AXIS_LEFTX) < -4096*2) {
				return true;
			}
			Uint8 state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_DPAD_LEFT);
			if(state != 0) {
				return true;
			}
		}

		for(auto j : joysticks) {
			Sint16  x = SDL_JoystickGetAxis(j.get(), 0);
			if (x < -4096*2) {
				return true;
			}

			const int nhats = SDL_JoystickNumHats(j.get());
			for(int n = 0; n != nhats; ++n) {
				const Uint8 state = SDL_JoystickGetHat(j.get(), n);
				switch(state) {
				case SDL_HAT_LEFT:
				case SDL_HAT_LEFTDOWN:
				case SDL_HAT_LEFTUP:
						return true;
				}
			}

		}

		return false;
	}

	bool right() 
	{
		if(!preferences::use_joystick()) {
			return false;
		}

		for(auto gc : game_controllers) {
			if (SDL_GameControllerGetAxis(gc.second.get(), SDL_CONTROLLER_AXIS_LEFTX) > 4096*2) {
				return true;
			}
			Uint8 state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
			if(state != 0) {
				return true;
			}
		}

		for(auto j : joysticks) {
			Sint16  x = SDL_JoystickGetAxis(j.get(), 0);
			if (x > 4096*2) {
				return true;
			}

			const int nhats = SDL_JoystickNumHats(j.get());
			for(int n = 0; n != nhats; ++n) {
				const Uint8 state = SDL_JoystickGetHat(j.get(), n);
				switch(state) {
				case SDL_HAT_RIGHT:
				case SDL_HAT_RIGHTDOWN:
				case SDL_HAT_RIGHTUP:
						return true;
				}
			}
		}

		return false;
	}

	bool button(int n) 
	{
		if(!preferences::use_joystick()) {
			return false;
		}

		for(auto gc : game_controllers) {
			Uint8 state = 0;
			switch(n) {
				case 0: { // change attacks
					state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_Y);
					break;
				}
				case 1: { // jump
					state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_A);
					break;
				}
				case 2: { // tongue attack
					state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_B);
					break;
				}
				case 3: { // unassigned inventory?
					state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_Y);
					break;
				}
				case 4: { // 
					state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_START);
					break;
				}
				default: continue;
			}
			if(state != 0) {
				return true;
			}
		}

		int cnt = 0;
		for(auto j : joysticks) {
			if(n >= SDL_JoystickNumButtons(j.get())) {
				continue;
			}
			if(SDL_JoystickGetButton(j.get(), n)) {
				return true;
			}
		}

		return false;
	}

	int iphone_tilt() 
	{
#if defined(TARGET_BLACKBERRY)
		double x, y, z;
		const int result = accelerometer_read_forces(&x, &y, &z);
		if(result != BPS_SUCCESS) {
			LOG_ERROR("READ OF ACCELEROMETER FAILED)";
			return 0;
		} else {
			return x*1000;
		}
#endif

		return 0;
	}

	std::vector<size_t> get_info() 
	{
		std::vector<size_t> res;
		res.push_back(joysticks.size());
		for(auto j : joysticks) {
			res.push_back(SDL_JoystickGetAxis(j.get(), 0));
			res.push_back(SDL_JoystickGetAxis(j.get(), 1));
		}
	
		return res;
	}

}

namespace haptic 
{
	void play(const std::string& id, int iters)
	{
		for(auto hd : haptic_devices) {
			auto it = get_effects().find(hd.second.get());
			if(it != get_effects().end()) {
				auto idit = it->second.find(id);
				if(idit == it->second.end()) {
					SDL_HapticRumblePlay(hd.second.get(), 1.0, 750);
				} else {
					SDL_HapticRunEffect(hd.second.get(), idit->second, iters);
				}
			} else {
				SDL_HapticRumblePlay(hd.second.get(), 1.0, 750);
			}
		}
	}

	void stop(const std::string& id)
	{
		for(auto hd : haptic_devices) {
			auto it = get_effects().find(hd.second.get());
			auto idit = it->second.find(id);
			if(idit == it->second.end()) {
				SDL_HapticStopEffect(hd.second.get(), idit->second);
			}
		}
	}

	void stop_all()
	{
		for(auto hd : haptic_devices) {
			SDL_HapticStopAll(hd.second.get());
		}
	}

	HapticEffectCallable::HapticEffectCallable(const std::string& name, const variant& effect)
	{
		load(name, effect);
	}

	HapticEffectCallable::~HapticEffectCallable()
	{
	}

	namespace 
	{
		void get_list3u(Uint16* li, const variant& v) 
		{
			ASSERT_LOG(v.is_list(), "FATAL: Must be list type");
			for(size_t n = 0; n != 3 && n != v.num_elements(); ++n) {
				li[n] = Uint16(v[n].as_int());
			}
		}
		void get_list3s(Sint16* li, const variant& v) 
		{
			ASSERT_LOG(v.is_list(), "FATAL: Must be list type");
			for(size_t n = 0; n != 3 && n != v.num_elements(); ++n) {
				li[n] = Sint16(v[n].as_int());
			}
		}
	}

	void HapticEffectCallable::load(const std::string& name, const variant& eff) 
	{
		SDL_HapticEffect effect;
		SDL_memset(&effect, 0, sizeof(effect));

		// convert from our variant map to an SDL_HapticEffect structure.
		ASSERT_LOG(eff.has_key("type"), "FATAL: haptic effects must have 'type' key.");
		ASSERT_LOG(eff["type"].is_string(), "FATAL: 'type' key must be a string.");
		std::string type = eff["type"].as_string();

		Uint32 length = eff["length"].as_int();
		Uint16 delay = Uint16(eff["delay"].as_int());

		Uint16 button = 0;
		if(eff.has_key("button")) {
			button = Uint16(eff["button"].as_int());
		}
		Uint16 interval = 0;
		if(eff.has_key("interval")) {
			interval = Uint16(eff["interval"].as_int());
		}

		Uint16 attack_length = 0;
		if(eff.has_key("attack_length")) {
			attack_length = Uint16(eff["attack_length"].as_int());
		}
		Uint16 attack_level = 0;
		if(eff.has_key("attack_level")) {
			attack_level = Uint16(eff["attack_level"].as_int());
		}
		Uint16 fade_length = 0;
		if(eff.has_key("fade_length")) {
			fade_length = Uint16(eff["fade_length"].as_int());
		}
		Uint16 fade_level = 0;
		if(eff.has_key("fade_level")) {
			fade_level = Uint16(eff["fade_level"].as_int());
		}

		SDL_HapticDirection direction;
		if(eff.has_key("direction")) {
			const std::string& dir = eff["direction"].as_string();
			if(dir == "polar") {
				direction.type = SDL_HAPTIC_POLAR;
				direction.dir[0] =  eff["direction_rotation0"].as_int();
			} else if(dir == "cartesian") {
				direction.type = SDL_HAPTIC_CARTESIAN;
				direction.dir[0] =  eff["direction_x"].as_int();
				direction.dir[1] =  eff["direction_y"].as_int();
				if(eff.has_key("direction_z")) {
					direction.dir[2] =  eff["direction_z"].as_int();
				}
			} else if(dir == "sepherical") {
				direction.type = SDL_HAPTIC_SPHERICAL;
				direction.dir[0] =  eff["direction_rotation0"].as_int();
				if(eff.has_key("direction_rotation1")) {
					direction.dir[1] =  eff["direction_rotation1"].as_int();
				}
			} else {
				ASSERT_LOG(false, "FATAL: Unknown direction value '" << dir << "'");
			}
		}

		if(type == "constant") {
			effect.type = SDL_HAPTIC_CONSTANT;
			effect.constant.length = eff["level"].as_int();
			effect.constant.attack_length = attack_length;
			effect.constant.attack_level = attack_level;
			effect.constant.fade_length = fade_length;
			effect.constant.fade_level = fade_level;
			effect.constant.button = button;
			effect.constant.interval = interval;
			effect.constant.length = length;
			effect.constant.delay = delay;
		} else if(type == "sine" || type == "sqaure" || type == "triangle" || type == "sawtooth_up" || type == "sawtooth_down") {
			if(type == "sine") {
				effect.type = SDL_HAPTIC_SINE;
			//} else if(type == "sqaure") {
			//	effect.type = SDL_HAPTIC_SQUARE;
			} else if(type == "triangle") {
				effect.type = SDL_HAPTIC_TRIANGLE;
			} else if(type == "sawtooth_up") {
				effect.type = SDL_HAPTIC_SAWTOOTHUP;
			} else if(type == "sawtooth_down") {
				effect.type = SDL_HAPTIC_SAWTOOTHDOWN;
			}
			effect.periodic.period = eff["period"].as_int();
			effect.periodic.magnitude = eff["magnitude"].as_int();
			if(eff.has_key("offset")) {
				effect.periodic.offset = eff["offset"].as_int();
			}
			if(eff.has_key("phase")) {
				effect.periodic.phase = eff["phase"].as_int();
			}
			effect.periodic.attack_length = attack_length;
			effect.periodic.attack_level = attack_level;
			effect.periodic.fade_length = fade_length;
			effect.periodic.fade_level = fade_level;
			effect.periodic.button = button;
			effect.periodic.interval = interval;
			effect.periodic.length = length;
			effect.periodic.delay = delay;
		} else if(type == "spring" || type == "damper" || type == "inertia" || type == "friction") {
			if(type == "spring") {
				effect.type = SDL_HAPTIC_SPRING;
			} else if(type == "damper") {
				effect.type = SDL_HAPTIC_DAMPER;
			} else if(type == "inertia") {
				effect.type = SDL_HAPTIC_INERTIA;
			} else if(type == "friction") {
				effect.type = SDL_HAPTIC_FRICTION;
			}
			effect.condition.button = button;
			effect.condition.interval = interval;
			effect.condition.length = length;
			effect.condition.delay = delay;
			get_list3u(effect.condition.right_sat, eff["right_saturation"]);
			get_list3u(effect.condition.left_sat, eff["left_saturation"]);
			get_list3s(effect.condition.right_coeff, eff["right_coefficient"]);
			get_list3s(effect.condition.left_coeff, eff["left_coefficient"]);
			get_list3u(effect.condition.deadband, eff["deadband"]);
			get_list3s(effect.condition.center, eff["center"]);
		} else if(type == "ramp") {
			effect.type = SDL_HAPTIC_RAMP;
			effect.ramp.start = eff["start"].as_int();
			effect.ramp.start = eff["end"].as_int();
			effect.ramp.attack_length = attack_length;
			effect.ramp.attack_level = attack_level;
			effect.ramp.fade_length = fade_length;
			effect.ramp.fade_level = fade_level;
			effect.ramp.button = button;
			effect.ramp.interval = interval;
		} else if(type == "custom") {
			effect.type = SDL_HAPTIC_CUSTOM;
		}
		
		for(auto hd : haptic_devices) {
			int id = SDL_HapticNewEffect(hd.second.get(), &effect);
			if(id >= 0) {
				auto it = get_effects().find(hd.second.get());
				if(it != get_effects().end()) {
					it->second[name] = id;
				} else {
					std::map<std::string,int> m;
					m[name] = id;
					get_effects()[hd.second.get()] = m;
				}
			} else {
				LOG_WARN("error creating haptic effect(" << name << "): " << SDL_GetError());
			}
		}
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(HapticEffectCallable)
		DEFINE_FIELD(dummy, "int")
		return variant(0);
	END_DEFINE_CALLABLE(HapticEffectCallable)
}
