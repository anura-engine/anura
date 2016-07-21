/*
Copyright (C) 2016 by Kristina Simpson <sweet.kristas@gmail.com>

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

#include "event_listener.hpp"

EventListener::EventListener()
{
}

EventListener::~EventListener()
{
}

bool EventListener::mouseMotion(bool claimed, const point& p, unsigned keymod)
{
	return handleMouseMotion(claimed, p, keymod);
}

bool EventListener::mouseButtonUp(bool claimed, const point& p, unsigned buttons, unsigned keymod)
{
	return handleMouseButtonUp(claimed, p, buttons, keymod);
}

bool EventListener::mouseButtonDown(bool claimed, const point& p, unsigned buttons, unsigned keymod)
{
	return handleMouseButtonDown(claimed, p, buttons, keymod);
}

bool EventListener::mouseWheel(bool claimed, const point& p, const point& delta, int direction)
{
	return handleMouseWheel(claimed, p, delta, direction);
}

bool EventListener::keyDown(bool claimed, const SDL_Keysym& keysym, bool repeat, bool pressed)
{
	return handleKeyDown(claimed, keysym, repeat, pressed);
}

bool EventListener::keyUp(bool claimed, const SDL_Keysym& keysym, bool repeat, bool pressed)
{
	return handleKeyUp(claimed, keysym, repeat, pressed);
}

bool EventListener::textInput(bool claimed, const std::string& text)
{
	return handleTextInput(claimed, text);
}

bool EventListener::textEditing(bool claimed, const std::string& text, int start, int length)
{
	return handleTextEditing(claimed, text, start, length);
}

