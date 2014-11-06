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
#ifndef CLIPBOARD_HPP_INCLUDED
#define CLIPBOARD_HPP_INCLUDED

#include <string>
#include "graphics.hpp"

void init_clipboard();

/**
 * Copies text to the clipboard.
 *
 * @param text         The text to copy.
 * @param mouse        Is the selection done by the mouse? On UNIX systems there
 *                     are multiple clipboards and the mouse selection uses a
 *                     different clipboard. Ignored on other systems.
 */
void copy_to_clipboard(const std::string& text, const bool mouse);

/**
 * Copies text from the clipboard.
 *
 * @param mouse        Is the pasting done by the mouse?
 *
 * @returns            String on clipbaord.
 */
std::string copy_from_clipboard(const bool mouse);

bool clipboard_handle_event(const SDL_Event& ev);

//if the clipboard has X-style copy paste using the mouse only.
bool clipboard_has_mouse_area();

#endif
