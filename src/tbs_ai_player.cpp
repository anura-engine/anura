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
#include <algorithm>
#include <numeric>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "tbs_ai_player.hpp"
#include "tbs_game.hpp"

namespace tbs {

ai_player* ai_player::create(game& g, int nplayer)
{
	return NULL; //new default_ai_player(g, nplayer);
}

ai_player::ai_player(const game& g, int nplayer)
  : game_(g), nplayer_(nplayer)
{}

ai_player::~ai_player()
{}

}
