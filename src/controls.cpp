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
#ifdef _WIN32
#include <winsock2.h>
#elif defined(__native_client__)
#include <stdint.h>
uint32_t ntohl(uint32_t hl)
{
   return (((hl&0x000000FF)<<24)+((hl&0x0000FF00)<<8)+
   ((hl&0x00FF0000)>>8)+((hl&0xFF000000)>>24));
}
uint32_t htonl(uint32_t nl)
{
   return (((nl&0x000000FF)<<24)+((nl&0x0000FF00)<<8)+
   ((nl&0x00FF0000)>>8)+((nl&0xFF000000)>>24));
}
#else
#include <netinet/in.h>
#endif

#include <assert.h>
#include <boost/cstdint.hpp>

#include <stdio.h>

#include <stack>
#include <vector>

#include "graphics.hpp"
#include "asserts.hpp"
#include "controls.hpp"
#include "foreach.hpp"
#include "joystick.hpp"
#include "level_runner.hpp"
#include "multiplayer.hpp"
#include "preferences.hpp"
#include "iphone_controls.hpp"
#include "variant.hpp"

namespace controls {

const char** control_names()
{
	static const char* names[] = { "up", "down", "left", "right", "attack", "jump", "tongue", NULL };
	return names;
}

namespace {

variant g_user_ctrl_output;

int npackets_received;
int ngood_packets;
int last_packet_size_;

const int MAX_PLAYERS = 8;

struct ControlFrame {
	ControlFrame() : keys(0)
	{}
	bool operator==(const ControlFrame& o) const {
		return keys == o.keys && user == o.user;
	}
	bool operator!=(const ControlFrame& o) const {
		return !(*this == o);
	}

	unsigned char keys;
	std::string user;
};

std::vector<ControlFrame> controls[MAX_PLAYERS];

//for each player, the highest confirmed cycle we have
int32_t highest_confirmed[MAX_PLAYERS];

//for each player, the highest confirmed cycle of ours that they have
int32_t remote_highest_confirmed[MAX_PLAYERS];

std::map<int, int> our_checksums;

int starting_cycles;
int nplayers = 1;
int local_player;

int delay;

int first_invalid_cycle_var = -1;

key_type sdlk[NUM_CONTROLS] = {
	SDLK_UP,
	SDLK_DOWN,
	SDLK_LEFT,
	SDLK_RIGHT,
	SDLK_d,
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
	SDLK_b,
	SDLK_a
#else
	SDLK_a,
	SDLK_s
#endif
};

CONTROL_ITEM g_mouse_controls[3] = {
	NUM_CONTROLS,
	NUM_CONTROLS,
	NUM_CONTROLS,
};

//If any of these keys are held, we ignore other keyboard input.
key_type control_keys[] = {
	SDLK_LCTRL,
	SDLK_RCTRL,
	SDLK_LALT,
	SDLK_RALT,
};

int32_t our_highest_confirmed() {
	int32_t res = -1;
	for(int n = 0; n != nplayers; ++n) {
		if(res == -1 || highest_confirmed[n] < res) {
			res = highest_confirmed[n];
		}
	}

	return res;
}

std::stack<ControlFrame> local_control_locks;
}

struct control_backup_scope_impl {
	std::vector<ControlFrame> controls[MAX_PLAYERS];
	int32_t highest_confirmed[MAX_PLAYERS];
	int starting_cycles;
	std::stack<ControlFrame> lock_stack;
};

control_backup_scope::control_backup_scope(int flags) : impl_(new control_backup_scope_impl)
{
	impl_->lock_stack = local_control_locks;

	if(flags & CLEAR_LOCKS) {
		while(!local_control_locks.empty()) { local_control_locks.pop(); }
	}

	impl_->starting_cycles = starting_cycles;
	for(int n = 0; n != MAX_PLAYERS; ++n) {
		impl_->controls[n] = controls[n];
		impl_->highest_confirmed[n] = highest_confirmed[n];
	}
}

control_backup_scope::~control_backup_scope()
{
	restore_state();
}

void control_backup_scope::restore_state()
{
	if(impl_) {
		starting_cycles = impl_->starting_cycles;
		for(int n = 0; n != MAX_PLAYERS; ++n) {
			controls[n] = impl_->controls[n];
			highest_confirmed[n] = impl_->highest_confirmed[n];
		}

		local_control_locks = impl_->lock_stack;
	}
}

void control_backup_scope::cancel()
{
	impl_.reset();
}

int their_highest_confirmed() {
	int32_t res = -1;
	for(int n = 0; n != nplayers; ++n) {
		if(n == local_player) {
			continue;
		}

		if(res == -1 || remote_highest_confirmed[n] < res) {
			res = remote_highest_confirmed[n];
		}
	}

	return res;
}

void new_level(int level_starting_cycles, int level_nplayers, int level_local_player)
{
	std::cerr << "SET STARTING CYCLES: " << level_starting_cycles << "\n";
	starting_cycles = level_starting_cycles;
	nplayers = level_nplayers;
	local_player = level_local_player;
	foreach(std::vector<ControlFrame>& v, controls) {
		v.clear();
	}

	foreach(int32_t& highest, highest_confirmed) {
		highest = 0;
	}

	foreach(int32_t& highest, remote_highest_confirmed) {
		highest = 0;
	}
}


local_controls_lock::local_controls_lock(unsigned char state)
{
	ControlFrame ctrl;
	ctrl.keys = state;
	local_control_locks.push(ctrl);
}

local_controls_lock::~local_controls_lock()
{
	local_control_locks.pop();

	if(local_control_locks.empty()) {
		ignore_current_keypresses();
	}
}


const unsigned char* get_local_control_lock()
{
    if(local_control_locks.empty() == false){
        return &local_control_locks.top().keys;
    }else{
        return nullptr;
    }
}

namespace {
//array of keys which we are ignoring. We ignore keys on the end of a dialog.
//keys will be unignored as soon as they are no longer depressed.
bool key_ignore[NUM_CONTROLS];
}

void ignore_current_keypresses()
{
	const Uint8 *state = SDL_GetKeyboardState(NULL);
	for(int n = 0; n < NUM_CONTROLS; ++n) {
		key_ignore[n] = state[SDL_GetScancodeFromKey(sdlk[n])];
	}
}

void read_until(int ncycle)
{
	if(local_player < 0 || local_player >= nplayers) {
		return;
	}

	while(controls[local_player].size() <= ncycle) {
		read_local_controls();
	}

	while(controls[local_player].size() > ncycle+1) {
		unread_local_controls();
	}
}

int local_controls_end()
{
	if(local_player < 0 || local_player >= nplayers) {
		return 0;
	}

	return controls[local_player].size();
}

void read_local_controls()
{
	if(local_player < 0 || local_player >= nplayers) {
		return;
	}

	if(preferences::no_iphone_controls() == false && level::current().allow_touch_controls() == true) {
		iphone_controls::read_controls();
	}

	ControlFrame state;
	if(local_control_locks.empty()) {
#if !defined(__ANDROID__)
		bool ignore_keypresses = false;
		const Uint8 *key_state = SDL_GetKeyboardState(NULL);
		foreach(const key_type& k, control_keys) {
			if(key_state[SDL_GetScancodeFromKey(k)]) {
				ignore_keypresses = true;
				break;
			}
		}

		Uint32 mouse_buttons = SDL_GetMouseState(NULL, NULL);
		for(int n = 0; n < 3; ++n) {
			if(g_mouse_controls[n] != NUM_CONTROLS && (mouse_buttons&SDL_BUTTON(n+1))) {
				if(!key_ignore[g_mouse_controls[n]]) {
					state.keys |= (1 << g_mouse_controls[n]);
				}
			}
		}

		for(int n = 0; n < NUM_CONTROLS; ++n) {
			if(key_state[SDL_GetScancodeFromKey(sdlk[n])] && !ignore_keypresses) {
				if(!key_ignore[n]) {
					state.keys |= (1 << n);
				}
			} else {
				key_ignore[n] = false;
			}
		}
#endif

		if(preferences::no_iphone_controls() == false && level::current().allow_touch_controls() == true) {
			if(iphone_controls::up()) { state.keys |= (1 << CONTROL_UP);}
			if(iphone_controls::down()) { state.keys |= (1 << CONTROL_DOWN);}
			if(iphone_controls::left()) { state.keys |= (1 << CONTROL_LEFT);}
			if(iphone_controls::right()) { state.keys |= (1 << CONTROL_RIGHT);}
			if(iphone_controls::attack()) { state.keys |= (1 << CONTROL_ATTACK);}
			if(iphone_controls::jump()) { state.keys |= (1 << CONTROL_JUMP);}
			if(iphone_controls::tongue()) { state.keys |= (1 << CONTROL_TONGUE);}
		}

#if !defined(__ANDROID__)
		if(joystick::up()) { state.keys |= (1 << CONTROL_UP);}
		if(joystick::down()) { state.keys |= (1 << CONTROL_DOWN);}
		if(joystick::left()) { state.keys |= (1 << CONTROL_LEFT);}
		if(joystick::right()) { state.keys |= (1 << CONTROL_RIGHT);}
		if(joystick::button(0)) { state.keys |= (1 << CONTROL_ATTACK);}
		if(joystick::button(1)) { state.keys |= (1 << CONTROL_JUMP);}
		if(joystick::button(2)) { state.keys |= (1 << CONTROL_TONGUE);}
#endif

		if(g_user_ctrl_output.is_null() == false) {
			state.user = g_user_ctrl_output.write_json();
		}
	} else {
		//we have the controls locked into a specific state.
		state = local_control_locks.top();
	}

	g_user_ctrl_output = variant();

	controls[local_player].push_back(state);
	highest_confirmed[local_player]++;

	//advance networked player's controls based on the assumption that they
	//just did the same thing as last time; incoming packets will correct
	//any assumptions.
	for(int n = 0; n != nplayers; ++n) {
		while(n != local_player && controls[n].size() < controls[local_player].size()) {
			if(controls[n].empty()) {
				controls[n].push_back(ControlFrame());
			} else {
				controls[n].push_back(controls[n].back());
			}
		}
	}
}

void unread_local_controls()
{
	if(local_player < 0 || local_player >= nplayers || controls[local_player].empty()) {
		return;
	}

	controls[local_player].pop_back();
	highest_confirmed[local_player]--;
}

void get_control_status(int cycle, int player, bool* output, const std::string** user)
{
	--cycle;
	cycle -= starting_cycles;

	cycle -= delay;
	if(cycle < 0) {
		for(int n = 0; n != NUM_CONTROLS; ++n) {
			output[n] = false;
		}
		return;
	}

	ASSERT_INDEX_INTO_VECTOR(cycle, controls[player]);

	unsigned char state = controls[player][cycle].keys;

	for(int n = 0; n != NUM_CONTROLS; ++n) {
		output[n] = (state&(1 << n)) ? true : false;
	}

	if(user) {
		*user = &controls[player][cycle].user;
	}
}

void set_delay(int value)
{
	delay = value;
}

void read_control_packet(const char* buf, size_t len)
{
	++npackets_received;

	if(len < 13) {
		fprintf(stderr, "ERROR: CONTROL PACKET TOO SHORT: %d\n", (int)len);
		return;
	}

	const char* end_buf = buf + len;

	int slot = *buf++;

	if(slot < 0 || slot >= nplayers) {
		fprintf(stderr, "ERROR: BAD SLOT NUMBER: %d/%d\n", slot, nplayers);
		return;
	}

	if(slot == local_player) {
		fprintf(stderr, "ERROR: NETWORK PLAYER SAYS THEY HAVE THE SAME SLOT AS US!\n");
		return;
	}

	int32_t current_cycle;
	memcpy(&current_cycle, buf, 4);
	current_cycle = ntohl(current_cycle);
	buf += 4;

	if(current_cycle < highest_confirmed[slot]) {
		fprintf(stderr, "DISCARDING PACKET -- OUT OF ORDER: %d < %d\n", current_cycle, highest_confirmed[slot]);
		return;
	}

	int32_t checksum;
	memcpy(&checksum, buf, 4);
	checksum = ntohl(checksum);
	buf += 4;

	if(checksum && our_checksums[current_cycle-1]) {
		if(checksum == our_checksums[current_cycle-1]) {
			std::cerr << "CHECKSUM MATCH FOR " << current_cycle << ": " << checksum << "\n";
		} else {
			std::cerr << "CHECKSUM DID NOT MATCH FOR " << current_cycle << ": " << checksum << " VS " << our_checksums[current_cycle-1] << "\n";
		}

	}

	int32_t highest_cycle;
	memcpy(&highest_cycle, buf, 4);
	highest_cycle = ntohl(highest_cycle);
	buf += 4;

	if(highest_cycle > remote_highest_confirmed[slot]) {
		remote_highest_confirmed[slot] = highest_cycle;
	}

	int32_t ncycles;
	memcpy(&ncycles, buf, 4);
	ncycles = ntohl(ncycles);
	buf += 4;

	if(*(end_buf-1) != 0) {
		fprintf(stderr, "bad packet, no null terminator\n");
		return;
	}

	int start_cycle = 1 + current_cycle - ncycles;

	//if we already have data up to this point, don't reprocess it.
	if(start_cycle < highest_confirmed[slot]) {
		int diff = highest_confirmed[slot] - start_cycle;
		ncycles -= diff;

		for(int n = 0; n != diff && buf != end_buf; ++n) {
			++buf;
			if(buf == end_buf) {
				fprintf(stderr, "bad packet, incorrect null termination\n");
				return;
			}
			buf += strlen(buf)+1;
		}

		start_cycle = highest_confirmed[slot];
	}

	for(int cycle = start_cycle; cycle <= current_cycle; ++cycle) {
		if(buf == end_buf) {
			fprintf(stderr, "bad packet, incorrect number of null terminators\n");
			return;
		}
		ControlFrame state;
		state.keys = *buf;

		++buf;
		if(buf == end_buf) {
			fprintf(stderr, "bad packet, incorrect number of null terminators\n");
			return;
		}

		state.user = buf;
		buf += state.user.size()+1;

		if(cycle < controls[slot].size()) {
			if(controls[slot][cycle] != state) {
				fprintf(stderr, "RECEIVED CORRECTION\n");
				controls[slot][cycle] = state;
				if(first_invalid_cycle_var == -1 || first_invalid_cycle_var > cycle) {
					//mark us as invalid back to this point, so game logic
					//will be recalculated from here.
					first_invalid_cycle_var = cycle;
				}
			}
		} else {
			fprintf(stderr, "RECEIVED FUTURE PACKET!\n");
			while(controls[slot].size() <= cycle) {
				controls[slot].push_back(state);
			}
		}
	}

	//extend the current control out to the end, to keep the assumption that
	//controls don't change unless we get an explicit signal
	if(current_cycle < static_cast<int>(controls[slot].size()) - 1) {
		for(int n = current_cycle + 1; n < controls[slot].size(); ++n) {
			controls[slot][n] = controls[slot][current_cycle];
		}
	}

	//mark our highest confirmed cycle for this player
	highest_confirmed[slot] = current_cycle;

	assert(buf == end_buf);

	++ngood_packets;
}

void write_control_packet(std::vector<char>& v)
{
	if(local_player < 0 || local_player >= nplayers) {
		fprintf(stderr, "NO VALID LOCAL PLAYER\n");
		return;
	}

	//write our slot to the packet
	v.push_back(local_player);

	//write our current cycle
	int32_t current_cycle = controls[local_player].size()-1;
	int32_t current_cycle_net = htonl(current_cycle);
	v.resize(v.size() + 4);
	memcpy(&v[v.size()-4], &current_cycle_net, 4);

	//write our checksum of game state
	int32_t checksum = our_checksums[current_cycle-1];
	int32_t checksum_net = htonl(checksum);
	v.resize(v.size() + 4);
	memcpy(&v[v.size()-4], &checksum_net, 4);

	//write our highest confirmed cycle
	int32_t highest_cycle = htonl(our_highest_confirmed());
	v.resize(v.size() + 4);
	memcpy(&v[v.size()-4], &highest_cycle, 4);

	int32_t ncycles_to_write = 1+current_cycle - their_highest_confirmed();

	last_packet_size_ = ncycles_to_write;
	if(ncycles_to_write > controls[local_player].size()) {
		ncycles_to_write = controls[local_player].size();
	}

	int32_t ncycles_to_write_net = htonl(ncycles_to_write);
	v.resize(v.size() + 4);
	memcpy(&v[v.size()-4], &ncycles_to_write_net, 4);

	for(int n = 0; n != ncycles_to_write; ++n) {
		const int index = (controls[local_player].size() - ncycles_to_write) + n;
		v.push_back(controls[local_player][index].keys);
		const char* user = controls[local_player][index].user.c_str();
		v.insert(v.end(), user, user + controls[local_player][index].user.size()+1);
	}

	fprintf(stderr, "WRITE CONTROL PACKET: %d\n", (int)v.size());
}

const variant& user_ctrl_output()
{
	return g_user_ctrl_output;
}

void set_user_ctrl_output(const variant& v)
{
	g_user_ctrl_output = v;
}

int first_invalid_cycle()
{
	return first_invalid_cycle_var;
}

void mark_valid()
{
	first_invalid_cycle_var = -1;
}

int num_players()
{
	return nplayers;
}

int num_errors()
{
	return npackets_received - ngood_packets;
}

int packets_received()
{
	return npackets_received;
}

int cycles_behind()
{
	if(local_player < 0 || local_player >= nplayers) {
		return 0;
	}

	return highest_confirmed[local_player] - our_highest_confirmed();
}

int last_packet_size()
{
	return last_packet_size_;
}

void set_checksum(int cycle, int sum)
{
	our_checksums[cycle] = sum;
}

void debug_dump_controls()
{
	fprintf(stderr, "CONTROLS:");
	for(int n = 0; n < nplayers; ++n) {
		fprintf(stderr, " %d:", n);
		for(int m = 0; m < controls[n].size() && m < highest_confirmed[n]; ++m) {
			char c = controls[n][m].keys;
			fprintf(stderr, "%02x", (int)c);
		}
	}

	fprintf(stderr, "\n");

	for(int n = 0; n < nplayers; ++n) {
		for(int m = 0; m < controls[n].size() && m < highest_confirmed[n]; ++m) {
			fprintf(stderr, "CTRL PLAYER %d CYCLE %d: ", n, m);
			for(int j = 0; j != NUM_CONTROLS; ++j) {
				fprintf(stderr, (1 << j)&controls[n][m].keys ? "1" : "0");
			}
			fprintf(stderr, "\n");
		}
	}
}

void set_mouse_to_keycode(CONTROL_ITEM item, int mouse_button)
{
	--mouse_button;
	if(mouse_button >= 0 && mouse_button < 3) {
		g_mouse_controls[mouse_button] = item;
	}
}

CONTROL_ITEM get_mouse_keycode(int mouse_button)
{
	--mouse_button;
	if(mouse_button >= 0 && mouse_button < 3) {
		return g_mouse_controls[mouse_button];
	}

	return NUM_CONTROLS;
}

void set_keycode(CONTROL_ITEM item, key_type key)
{
	if (item < NUM_CONTROLS) {
		sdlk[item] = key;
	}
}

key_type get_keycode(CONTROL_ITEM item) 
{
	if (item < NUM_CONTROLS) {
		return sdlk[item];
	}
	return SDLK_UNKNOWN;
}
}
