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
#ifndef PLAYABLE_CUSTOM_OBJECT_HPP_INCLUDED
#define PLAYABLE_CUSTOM_OBJECT_HPP_INCLUDED

#include <boost/scoped_ptr.hpp>

#include "controls.hpp"
#include "custom_object.hpp"
#include "player_info.hpp"
#include "variant.hpp"

class level;

class playable_custom_object : public custom_object
{
public:
	playable_custom_object(const custom_object& obj);
	playable_custom_object(const playable_custom_object& obj);
	playable_custom_object(variant node);

	virtual variant write() const;

	virtual player_info* is_human() { return &player_info_; }
	virtual const player_info* is_human() const { return &player_info_; }

	void save_game();
	entity_ptr save_condition() const { return save_condition_; }

	virtual entity_ptr backup() const;
	virtual entity_ptr clone() const;

	virtual int vertical_look() const { return vertical_look_; }

	virtual bool is_active(const rect& screen_area) const;

	bool can_interact() const { return can_interact_ != 0; }

	int difficulty() const { return difficulty_; }

private:
	bool on_platform() const;

	int walk_up_or_down_stairs() const;

	virtual void process(level& lvl);
	variant get_value(const std::string& key) const;	
	void set_value(const std::string& key, const variant& value);

	variant get_player_value_by_slot(int slot) const;
	void set_player_value_by_slot(int slot, const variant& value);

	player_info player_info_;

	int difficulty_;

	entity_ptr save_condition_;

	int vertical_look_;

	int underwater_ctrl_x_, underwater_ctrl_y_;

	bool underwater_controls_;

	int can_interact_;
	
	bool reverse_ab_;

	boost::scoped_ptr<controls::local_controls_lock> control_lock_;

	void operator=(const playable_custom_object);
};

#endif
