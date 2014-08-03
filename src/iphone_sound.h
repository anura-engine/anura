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

#ifndef IPHONE_SOUND_H_INCLUDED
#define IPHONE_SOUND_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

void iphone_init_music(void (*callback)());
void iphone_fade_in_music(int duration);
void iphone_fade_out_music(int duration);
void iphone_play_music(const char *file, int loops);
void iphone_pause_music();
void iphone_resume_music();
void iphone_kill_music();
float iphone_get_music_volume();
void iphone_set_music_volume(float v);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
