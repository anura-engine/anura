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

#pragma once

#include <vector>
#include <memory>
#include <string>

typedef unsigned long key_type;

class variant;

namespace controls 
{
	enum CONTROL_ITEM {
		CONTROL_UP,
		CONTROL_DOWN,
		CONTROL_LEFT,
		CONTROL_RIGHT,
		CONTROL_ATTACK,
		CONTROL_JUMP,
		CONTROL_TONGUE,
		NUM_CONTROLS,
	};

	const char** control_names();

	void set_mouse_to_keycode(CONTROL_ITEM item, int mouse_button);
	CONTROL_ITEM get_mouse_keycode(int mouse_button);

	void set_keycode(CONTROL_ITEM item, key_type key);
	key_type get_keycode(CONTROL_ITEM item);

	void new_level(int starting_cycle, int nplayers, int local_player);

	//an object which can lock controls into a specific state for the duration
	//of its scope.
	class local_controls_lock {
	public:
		explicit local_controls_lock(unsigned char state=0);
		~local_controls_lock();
	};
	
	const unsigned char* get_local_control_lock();
    
	enum { CLEAR_LOCKS = 1 };

	class control_backup_scope_impl;
	class control_backup_scope {
	public:
		explicit control_backup_scope(int flags=0);
		~control_backup_scope();

		void clear_locks();

		void restore_state();
		void cancel();
	private:
		std::unique_ptr<control_backup_scope_impl> impl_;
	};

	void read_until(int ncycle);
	int local_controls_end();
	void read_local_controls();
	void unread_local_controls();
	void ignore_current_keypresses();

	void get_controlStatus(int cycle, int player, bool* output, const std::string** user=nullptr);
	void set_delay(int delay);

	void read_control_packet(const char* buf, size_t len);
	void write_control_packet(std::vector<char>& v);

	const variant& user_ctrl_output();
	void set_user_ctrl_output(const variant& v);

	int first_invalid_cycle();
	void mark_valid();

	unsigned num_players();
	int num_errors();
	int packets_received();
	int cycles_behind();

	int their_highest_confirmed();
	int last_packet_size();

	void set_checksum(int cycle, int sum);

	void debug_dump_controls();
}
