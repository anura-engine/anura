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
#ifndef PAUSE_GAME_DIALOG_INCLUDED
#define PAUSE_GAME_DIALOG_INCLUDED

enum PAUSE_GAME_RESULT { PAUSE_GAME_CONTINUE, PAUSE_GAME_CONTROLS, PAUSE_GAME_QUIT, PAUSE_GAME_GO_TO_TITLESCREEN, PAUSE_GAME_GO_TO_LOBBY };

PAUSE_GAME_RESULT show_pause_game_dialog();

struct interrupt_game_exception {
	PAUSE_GAME_RESULT result;
	interrupt_game_exception(PAUSE_GAME_RESULT res=PAUSE_GAME_QUIT) : result(res)
	{}
};

#endif
