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
#include "clipboard.hpp"
#include "raster.hpp"
#include <algorithm>
#include <iostream>

#if (defined(_X11) || defined(__linux__)) && !defined(__APPLE__) && !defined(__ANDROID__)
#define CLIPBOARD_FUNCS_DEFINED

void copy_to_clipboard(const std::string& text, const bool mouse)
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
#ifdef _WIN32
#include <windows.h>
#define CLIPBOARD_FUNCS_DEFINED

bool clipboard_handleEvent(const SDL_Event& )
{
	return false;
}

void copy_to_clipboard(const std::string& text, const bool)
{
	if(text.empty()) {
		return;
	}

	if(!OpenClipboard(NULL))
		return;
	EmptyClipboard();

	// Convert newlines
	std::string str;
	str.reserve(text.size());
	std::string::const_iterator last = text.begin();
	while(last != text.end()) {
		if(*last != '\n') {
			str.push_back(*last);
		} else {
			str.append("\r\n");
		}
		++last;
	}

	const HGLOBAL hglb = GlobalAlloc(GMEM_MOVEABLE, (str.size() + 1) * sizeof(TCHAR));
	if(hglb == NULL) {
		CloseClipboard();
		return;
	}
	char* const buffer = reinterpret_cast<char* const>(GlobalLock(hglb));
	strcpy(buffer, str.c_str());
	GlobalUnlock(hglb);
	SetClipboardData(CF_TEXT, hglb);
	CloseClipboard();
}

std::string copy_from_clipboard(const bool)
{
	if(!IsClipboardFormatAvailable(CF_TEXT))
		return "";
	if(!OpenClipboard(NULL))
		return "";

	HGLOBAL hglb = GetClipboardData(CF_TEXT);
	if(hglb == NULL) {
		CloseClipboard();
		return "";
	}
	char const * buffer = reinterpret_cast<char*>(GlobalLock(hglb));
	if(buffer == NULL) {
		CloseClipboard();
		return "";
	}

	// Convert newlines
	std::string str(buffer);
	str.erase(std::remove(str.begin(),str.end(),'\r'),str.end());

	GlobalUnlock(hglb);
	CloseClipboard();
	return str;
}

#endif

#ifdef __BEOS__
#include <Clipboard.h>
#define CLIPBOARD_FUNCS_DEFINED

void copy_to_clipboard(const std::string& text, const bool)
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
	BMessage *clip = NULL;
	if (be_clipboard->Lock())
	{
		clip = be_clipboard->Data();
		be_clipboard->Unlock();
	}
	if (clip != NULL && clip->FindData("text/plain", B_MIME_TYPE, (const void**)&data, &size) == B_OK)
		return (const char*)data;
	else
		return "";
}
#endif

#ifdef __APPLE__
    #include "TargetConditionals.h"
	#include "AvailabilityMacros.h"

    #if TARGET_OS_IPHONE
        //for now, do nothing
    #elif TARGET_OS_MAC
        #define decimal decimal_carbon
        #import <Cocoa/Cocoa.h>
        #undef decimal
        #define CLIPBOARD_FUNCS_DEFINED
        void copy_to_clipboard(const std::string& text, const bool)
        {
            NSString *clipString = [NSString stringWithCString:text.c_str()
                                                encoding:[NSString defaultCStringEncoding]];
			NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
			#if (MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5)
				NSInteger changeCount = [pasteboard clearContents];
			#else
				[pasteboard declareTypes:[NSArray arrayWithObjects:NSStringPboardType, nil] owner:nil];
			#endif
          BOOL OK = [pasteboard setString:clipString forType:NSStringPboardType];
            std::cerr << OK << "okay?\n";
            NSLog(@"%@",clipString);
            
            NSLog(@"%@", [[NSPasteboard generalPasteboard] stringForType:NSStringPboardType]);
        }

        std::string copy_from_clipboard(const bool)
        {
            NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
            NSString *clipString = [pasteboard stringForType:NSStringPboardType];
            std::string finalClipString = std::string([clipString UTF8String]);
            return finalClipString;
        }

        bool clipboard_handleEvent(const SDL_Event& )
        {
            return false;
        }
#endif
#endif

#ifndef CLIPBOARD_FUNCS_DEFINED

void copy_to_clipboard(const std::string& /*text*/, const bool)
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

