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

#include "WindowManager.hpp"

#include "asserts.hpp"
#include "controls.hpp"
#include "decimal.hpp"
#include "formula_constants.hpp"
#include "i18n.hpp"
#include "key_button.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"

namespace game_logic
{
	namespace 
	{
		typedef std::map<std::string, variant> constants_map;
		std::vector<constants_map> constants_stack;
	}

	variant get_constant(const std::string& id)
	{
		if(id == "SCREEN_WIDTH") {
			if(KRE::WindowManager::getMainWindow()) {
				return variant(KRE::WindowManager::getMainWindow()->width());
			} else {
				return variant(1024);
			}
		} else if(id == "SCREEN_HEIGHT") {
			if(KRE::WindowManager::getMainWindow()) {
				return variant(KRE::WindowManager::getMainWindow()->height());
			} else {
				return variant(768);
			}
		} else if(id == "TOUCH_SCREEN") {
#if defined(MOBILE_BUILD)
			return variant::from_bool(true);
#else
			return variant::from_bool(false);
#endif
		} else if(id == "LOW_END_SYSTEM") {
#if defined(MOBILE_BUILD)
			return variant(1);
#else
			return variant(0);
#endif
		} else if(id == "HIGH_END_SYSTEM") {
			return variant(!get_constant("LOW_END_SYSTEM").as_bool());
		} else if(id == "TBS_SERVER_ADDRESS") {
			return variant(preferences::get_tbs_uri().host());
		} else if(id == "TBS_SERVER_PORT") {
			return variant(atoi(preferences::get_tbs_uri().port().c_str()));
		} else if(id == "USERNAME") {
			return variant(preferences::get_username());
		} else if(id == "PASSWORD") {
			return variant(preferences::get_password());
		} else if(id == "UP_KEY") {
			return variant(gui::KeyButton::getKeyName(controls::get_keycode(controls::CONTROL_UP)));
		} else if(id == "DOWN_KEY") {
			return variant(gui::KeyButton::getKeyName(controls::get_keycode(controls::CONTROL_DOWN)));
		} else if(id == "LEFT_KEY") {
			return variant(gui::KeyButton::getKeyName(controls::get_keycode(controls::CONTROL_LEFT)));
		} else if(id == "RIGHT_KEY") {
			return variant(gui::KeyButton::getKeyName(controls::get_keycode(controls::CONTROL_RIGHT)));
		} else if(id == "JUMP_KEY") {
			return variant(gui::KeyButton::getKeyName(controls::get_keycode(controls::CONTROL_JUMP)));
		} else if(id == "TONGUE_KEY") {
			return variant(gui::KeyButton::getKeyName(controls::get_keycode(controls::CONTROL_TONGUE)));
		} else if(id == "ATTACK_KEY") {
			return variant(gui::KeyButton::getKeyName(controls::get_keycode(controls::CONTROL_ATTACK)));
		} else if(id == "LOCALE") {
			return variant(i18n::get_locale());
		} else if(id == "EPSILON") {
			return variant(decimal::epsilon());
		} else if(id == "HEX_DIRECTIONS") {
			std::vector<variant> v;
			v.push_back(variant("n"));
			v.push_back(variant("ne"));
			v.push_back(variant("se"));
			v.push_back(variant("s"));
			v.push_back(variant("sw"));
			v.push_back(variant("nw"));
			return variant(&v);
		} else if(id == "BUILD_OPTIONS") {
			std::vector<variant> v;
			for(auto bo : preferences::get_build_options()) {
				v.push_back(variant(bo));
			}
			return variant(&v);
	} else if(id == "MODULE_NAME") {
		return variant(module::get_module_name());
	} else if(id == "MODULE_PRETTY_NAME") {
		return variant(module::get_module_pretty_name());
	} else if(id == "MODULE_OPTIONS") {
		return preferences::get_module_settings();
	} else if(id == "MODULE_VERSION") {
		return variant(module::get_module_version());
	} else if(id == "MODULE_PATH") {
		return variant(module::get_module_path());
	}

		for(auto i = constants_stack.rbegin(); i != constants_stack.rend(); ++i) {
			constants_map& m = *i;
			constants_map::const_iterator itor = m.find(id);
			if(itor != m.end()) {
				return itor->second;
			}
		}

		ASSERT_LOG(false, "Unknown constant accessed: " << id);

		return variant();
	}

	ConstantsLoader::ConstantsLoader(variant node) : same_as_base_(false)
	{
		constants_map m;
		if(node.is_null() == false) {
			for(variant key : node.getKeys().as_list()) {
				const std::string& attr = key.as_string();
				if(std::find_if(attr.begin(), attr.end(), util::c_islower) != attr.end()) {
					//only all upper case are loaded as consts
					continue;
				}

				m[attr] = node[key];
			}
		}

		if(constants_stack.empty() == false && constants_stack.back() == m) {
			same_as_base_ = true;
		}

		constants_stack.push_back(m);
	}

	ConstantsLoader::~ConstantsLoader() NOEXCEPT(false)
	{
		ASSERT_EQ(constants_stack.empty(), false);
		constants_stack.pop_back();
	}
}

UNIT_TEST(get_constant_0) {
	const variant screen_width_constant =
			game_logic::get_constant("SCREEN_WIDTH");
	CHECK_EQ(variant(1024), screen_width_constant);
}

UNIT_TEST(get_constant_1) {
	const variant screen_height_constant =
			game_logic::get_constant("SCREEN_HEIGHT");
	CHECK_EQ(variant(768), screen_height_constant);
}

UNIT_TEST(get_constant_2) {
	const variant touch_screen_constant =
    game_logic::get_constant("TOUCH_SCREEN");
#ifdef MOBILE_BUILD
	CHECK_EQ(variant::from_bool(true), touch_screen_constant);
#else
	CHECK_EQ(variant::from_bool(false), touch_screen_constant);
#endif
}

UNIT_TEST(get_constant_3) {
	const variant low_end_system_constant =
			game_logic::get_constant("LOW_END_SYSTEM");
#ifdef MOBILE_BUILD
	CHECK_EQ(variant(1), low_end_system_constant);
#else
	CHECK_EQ(variant(0), low_end_system_constant);
#endif
}

UNIT_TEST(get_constant_4) {
	const variant high_end_system_constant =
			game_logic::get_constant("HIGH_END_SYSTEM");
#ifdef MOBILE_BUILD
	CHECK_EQ(variant(false), high_end_system_constant);
#else
	CHECK_EQ(variant(true), high_end_system_constant);
#endif
}

UNIT_TEST(get_constant_5) {
	const variant tbs_server_address_constant =
			game_logic::get_constant("TBS_SERVER_ADDRESS");
	const variant tbs_server_address_preference =
			variant(preferences::get_tbs_uri().host());
	CHECK_EQ(tbs_server_address_preference, tbs_server_address_constant);
}
