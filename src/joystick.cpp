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
#include "graphics.hpp"

#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "foreach.hpp"
#include "joystick.hpp"
#include "preferences.hpp"

#if defined(TARGET_BLACKBERRY)
#include <bps/accelerometer.h>
#include <bps/sensor.h>
#include <bps/bps.h>
#endif

#include "asserts.hpp"

namespace joystick {

namespace {
std::vector<std::shared_ptr<SDL_Joystick>> joysticks;
#if SDL_VERSION_ATLEAST(2,0,0)
std::map<int,std::shared_ptr<SDL_GameController>> game_controllers;
#endif

const int threshold = 32700;
}

manager::manager() {

	if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
		std::cerr << "ERROR: Unable to initialise game controller subsystem" << std::endl;
	}
	if(SDL_InitSubSystem(SDL_INIT_HAPTIC) != 0) {
		std::cerr << "ERROR: Unable to initialise haptic subsystem" << std::endl;
	}
#if defined(__ANDROID__)
    // We're just going to open 1 joystick on android platform.
	int n = 0; {
#else
	for(int n = 0; n != SDL_NumJoysticks(); ++n) {
#endif
#if SDL_VERSION_ATLEAST(2,0,0)
	    if(SDL_IsGameController(n)) {
			SDL_GameController *controller = SDL_GameControllerOpen(n);
			if(controller) {
				game_controllers[n] = std::shared_ptr<SDL_GameController>(controller, [](SDL_GameController* p){SDL_GameControllerClose(p);});
			} else {
				std::cerr << "WARNING: Couldn't open game controller: " << SDL_GetError() << std::endl;
			}
		} else {
#endif
			SDL_Joystick* j = SDL_JoystickOpen(n);
			if(j) {
				if (SDL_JoystickNumButtons(j) == 0) {
					// We're probably dealing with an accellerometer here.
					SDL_JoystickClose(j);

					std::cerr << "INFO: discarding joystick " << n << " for being an accellerometer\n";
				} else {
					joysticks.push_back(std::shared_ptr<SDL_Joystick>(j, [](SDL_Joystick* js){SDL_JoystickClose(js);}));
				}
			}
#if SDL_VERSION_ATLEAST(2,0,0)
		}
#endif
	}
	if(joysticks.size() > 0) {
		std::cerr << "INFO: Initialized " << joysticks.size() << " joysticks" << std::endl;
	}
#if SDL_VERSION_ATLEAST(2,0,0)
	if(game_controllers.size() > 0) {
		std::cerr << "INFO: Initialized " << game_controllers.size() << " game controllers" << std::endl;
	}
#endif
}

manager::~manager() {
	joysticks.clear();
	game_controllers.clear();

#if defined(TARGET_BLACKBERRY)
	bps_shutdown();
#endif
}

bool pump_events(const SDL_Event& ev, bool claimed) {
	if(claimed) {
		return claimed;
	}
	switch(ev.type) {
		case SDL_CONTROLLERDEVICEADDED: {
			auto it = game_controllers.find(ev.cdevice.which);
			if(it != game_controllers.end()) {
				std::cerr << "INFO: replacing game controller at index " << ev.cdevice.which << std::endl;
				game_controllers.erase(it);
			}
			SDL_GameController *controller = SDL_GameControllerOpen(ev.cdevice.which);
			if(controller) {
				game_controllers[ev.cdevice.which] = std::shared_ptr<SDL_GameController>(controller, [](SDL_GameController* p){SDL_GameControllerClose(p);});
			} else {
				std::cerr << "WARNING: Couldn't open game controller: " << SDL_GetError() << std::endl;
			}
			return true;
		}
		case SDL_CONTROLLERDEVICEREMOVED: {
			auto it = game_controllers.find(ev.cdevice.which);
			if(it != game_controllers.end()) {
				game_controllers.erase(it);
			} else {
				std::cerr << "WARNING: Controller removed message, no matching controller in list" << std::endl;
			}
			return true;
		}
	}
	return false;
}

void update() {
	if(preferences::use_joystick()) {
		SDL_JoystickUpdate();
	}
}

bool up() {
	if(!preferences::use_joystick()) {
		return false;
	}

#if SDL_VERSION_ATLEAST(2,0,0)
	for(auto gc : game_controllers) {
		if (SDL_GameControllerGetAxis(gc.second.get(), SDL_CONTROLLER_AXIS_LEFTY) < -4096*2) {
			return true;
		}
		Uint8 state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_DPAD_UP);
		if(state != 0) {
			return true;
		}
	}
#endif

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

bool down() {
	if(!preferences::use_joystick()) {
		return false;
	}

#if SDL_VERSION_ATLEAST(2,0,0)
	for(auto gc : game_controllers) {
		if (SDL_GameControllerGetAxis(gc.second.get(), SDL_CONTROLLER_AXIS_LEFTY) > 4096*2) {
			return true;
		}
		Uint8 state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_DPAD_DOWN);
		if(state != 0) {
			return true;
		}
	}
#endif

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

bool left() {
	if(!preferences::use_joystick()) {
		return false;
	}

#if SDL_VERSION_ATLEAST(2,0,0)
	for(auto gc : game_controllers) {
		if (SDL_GameControllerGetAxis(gc.second.get(), SDL_CONTROLLER_AXIS_LEFTX) < -4096*2) {
			return true;
		}
		Uint8 state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_DPAD_LEFT);
		if(state != 0) {
			return true;
		}
	}
#endif

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

bool right() {
	if(!preferences::use_joystick()) {
		return false;
	}

#if SDL_VERSION_ATLEAST(2,0,0)
	for(auto gc : game_controllers) {
		if (SDL_GameControllerGetAxis(gc.second.get(), SDL_CONTROLLER_AXIS_LEFTX) > 4096*2) {
			return true;
		}
		Uint8 state = SDL_GameControllerGetButton(gc.second.get(), SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
		if(state != 0) {
			return true;
		}
	}
#endif

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

bool button(int n) {
	if(!preferences::use_joystick()) {
		return false;
	}

#if SDL_VERSION_ATLEAST(2,0,0)
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
			default: continue;
		}
		if(state != 0) {
			return true;
		}
	}
#endif

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

int iphone_tilt() {

#if defined(TARGET_BLACKBERRY)
	double x, y, z;
	const int result = accelerometer_read_forces(&x, &y, &z);
	if(result != BPS_SUCCESS) {
		std::cerr << "READ OF ACCELEROMETER FAILED\n";
		return 0;
	} else {
		return x*1000;
	}
#endif

//#if TARGET_OS_IPHONE
//	return SDL_JoystickGetAxis(joysticks.front(), 1);
//#else
	return 0;
//#endif
}

std::vector<int> get_info() {
	std::vector<int> res;
	res.push_back(joysticks.size());
	for(auto j : joysticks) {
		res.push_back(SDL_JoystickGetAxis(j.get(), 0));
		res.push_back(SDL_JoystickGetAxis(j.get(), 1));
	}
	
	return res;
}

}
