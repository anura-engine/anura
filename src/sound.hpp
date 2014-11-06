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
#ifndef SOUND_HPP_INCLUDED
#define SOUND_HPP_INCLUDED

#include "variant.hpp"

#include <string>

namespace sound {

struct manager {
	manager();
	~manager();
};

void init_music(variant node);

bool ok();
bool muted();
void mute(bool flag);

void process();

//preload a sound effect in the cache.
void preload(const std::string& file);

//play a sound. 'object' is the object that is playing the sound. It can be
//used later in stop_sound to specify which object is stopping playing
//the sound.
void play(const std::string& file, const void* object=0, float volume=1.0f, float fade_in_time_=0.0f);

//stop a sound. object refers to the object that started the sound, and is
//the same as the object in play().
void stop_sound(const std::string& file, const void* object=0, float fade_out_time=0.0f);

//stop all looped sounds associated with an object; same object as in play()
//intended to be called in all object's destructors
void stop_looped_sounds(const void* object=0);
void change_volume(const void* object=0, int volume=-1);

//Ways to set the sound and music volumes from the user's perspective.
float get_sound_volume();
void set_sound_volume(float volume);

float get_music_volume();
void set_music_volume(float volume);

//Ways to set the music volume from the game engine's perspective.
void set_engine_music_volume(float volume);
float get_engine_music_volume();

void set_panning(float left, float right);
void update_panning(const void* obj, const std::string& id, float left, float right);
	
// function to play a sound effect over and over in a loop. Will return
// a handle to the sound effect.
int play_looped(const std::string& file, const void* object=0, float volume=1.0f, float fade_in_time_=0.0f);

void play_music(const std::string& file);
void play_music_interrupt(const std::string& file);

const std::string& current_music();

}

#endif
