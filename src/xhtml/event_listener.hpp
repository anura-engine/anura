/*
Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/

#pragma once

#include <memory>

#include "SDL.h"

#include "geometry.hpp"

class EventListener
{
public:
	EventListener();
	virtual ~EventListener();
	bool mouseMotion(bool claimed, const point& p, unsigned keymod, bool in_rect=false);
	bool mouseButtonUp(bool claimed, const point& p, unsigned buttons, unsigned keymod, bool in_rect=false);
	bool mouseButtonDown(bool claimed, const point& p, unsigned buttons, unsigned keymod, bool in_rect=false);
	bool mouseWheel(bool claimed, const point& p, const point& delta, int direction, bool in_rect=false);
	
	bool keyDown(bool claimed, const SDL_Keysym& keysym, bool repeat, bool pressed);
	bool keyUp(bool claimed, const SDL_Keysym& keysym, bool repeat, bool pressed);
	bool textInput(bool claimed, const std::string& text);
	bool textEditing(bool claimed, const std::string& text, int start, int length);
private:
	virtual bool handleMouseMotion(bool claimed, const point& p, unsigned keymod, bool in_rect) = 0;
	virtual bool handleMouseButtonUp(bool claimed, const point& p, unsigned buttons, unsigned keymod, bool in_rect) = 0;
	virtual bool handleMouseButtonDown(bool claimed, const point& p, unsigned buttons, unsigned keymod, bool in_rect) = 0;	
	virtual bool handleMouseWheel(bool claimed, const point& p, const point& delta, int direction, bool in_rect) = 0;
	
	virtual bool handleKeyDown(bool claimed, const SDL_Keysym& keysym, bool repeat, bool pressed) { return claimed; }
	virtual bool handleKeyUp(bool claimed, const SDL_Keysym& keysym, bool repeat, bool pressed) { return claimed; }
	virtual bool handleTextInput(bool claimed, const std::string& text) { return claimed; }
	virtual bool handleTextEditing(bool claimed, const std::string& text, int start, int length) { return claimed; }
};

typedef std::shared_ptr<EventListener> EventListenerPtr;
