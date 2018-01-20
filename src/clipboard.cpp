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

#include "clipboard.hpp"
#include <algorithm>
#include <iostream>

#if (defined(_X11) || defined(__linux__)) || defined(__APPLE__) && !defined(__ANDROID__) || defined(_WIN32)
#define CLIPBOARD_FUNCS_DEFINED

void copy_to_clipboard(const std::string& text)
{
	SDL_SetClipboardText(text.c_str());
}

std::string copy_from_clipboard(const bool mouse)
{
	char* str = SDL_GetClipboardText();
	std::string result(str);
	SDL_free(str);
	return result;
}

bool clipboard_handleEvent(const SDL_Event& ev)
{
	return false;
}

#endif

#ifdef __BEOS__
#include <Clipboard.h>
#define CLIPBOARD_FUNCS_DEFINED

void copy_to_clipboard(const std::string& text)
{
	BMessage *clip;
	if (be_clipboard->Lock())
	{
		be_clipboard->Clear();
		if ((clip = be_clipboard->Data()))
		{
			clip->AddData("text/plain", B_MIME_TYPE, text.c_str(), text.size()+1);
			be_clipboard->Commit();
		}
		be_clipboard->Unlock();
	}
}

std::string copy_from_clipboard(const bool)
{
	const char* data;
	ssize_t size;
	BMessage *clip = nullptr;
	if (be_clipboard->Lock())
	{
		clip = be_clipboard->Data();
		be_clipboard->Unlock();
	}
	if (clip != nullptr && clip->FindData("text/plain", B_MIME_TYPE, (const void**)&data, &size) == B_OK)
		return (const char*)data;
	else
		return "";
}
#endif


#ifndef CLIPBOARD_FUNCS_DEFINED

void copy_to_clipboard(const std::string& /*text*/)
{
}

std::string copy_from_clipboard(const bool)
{
	return "";
}

bool clipboard_handleEvent(const SDL_Event& )
{
	return false;
}

#endif

void init_clipboard()
{
#if (defined(_X11) || defined(__linux__)) && !defined(__APPLE__) && !defined(__ANDROID__)
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif
}

bool clipboard_has_mouse_area()
{
#if (defined(_X11) || defined(__linux__)) && !defined(__APPLE__) && !defined(__ANDROID__)
	return true;
#else
	return false;
#endif
}

