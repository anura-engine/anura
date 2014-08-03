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

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#endif

#include <algorithm>
#include <ctime>
#include "utils.hpp"

#include "kre/WindowManager.hpp"

#include "level.hpp"
#include "filesystem.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "variant.hpp"

int truncate_to_char(int value) 
{ 
	return std::min(std::max(value, 0), 255);
}

void write_autosave()
{
	variant node = Level::current().write();
	if(sound::current_music().empty() == false) {
		node.add_attr(variant("music"), variant(sound::current_music()));
	}
	
	sys::write_file(preferences::auto_save_file_path(), node.write_json());
	sys::write_file(std::string(preferences::auto_save_file_path()) + ".stat", "1");
}

void toggle_fullscreen()
{
	auto wnd = KRE::WindowManager::getMainWindow();
	wnd->setFullscreenMode(wnd->fullscreenMode() == KRE::FullScreenMode::WINDOWED ? KRE::FullScreenMode::FULLSCREEN_WINDOWED : KRE::FullScreenMode::WINDOWED);
}

std::string get_http_datetime() 
{
	time_t rawtime;
	char buffer[128];
	time(&rawtime);
#if defined(_MSC_VER)
	struct tm timeinfo;
	gmtime_s(&timeinfo, &rawtime);
	// RFC 8022 format
	strftime(buffer, 80, "%a, %d %b %Y %X GMT", &timeinfo);
#else
	struct tm* timeinfo;
	timeinfo = gmtime(&rawtime);
	// RFC 8022 format
	strftime(buffer, 80, "%a, %d %b %Y %X GMT", timeinfo);
#endif
	return std::string(buffer);
}

#ifdef _WINDOWS
const __int64 DELTA_EPOCH_IN_MICROSECS= 11644473600000000;

struct timezone2 
{
  __int32  tz_minuteswest; /* minutes W of Greenwich */
  bool  tz_dsttime;     /* type of dst correction */
};

int gettimeofday(struct timeval *tv, struct timezone2 *tz)
{
	if(tv) {
		FILETIME ft;
		__int64 tmpres = 0;
		ZeroMemory(&ft,sizeof(ft));
		GetSystemTimeAsFileTime(&ft);

		tmpres = ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		/*converting file time to unix epoch*/
		tmpres /= 10;  /*convert into microseconds*/
		tmpres -= DELTA_EPOCH_IN_MICROSECS; 
		tv->tv_sec = (__int32)(tmpres * 0.000001);
		tv->tv_usec =(tmpres % 1000000);
	}

    //_tzset(),don't work properly, so we use GetTimeZoneInformation
	if(tz) {
		TIME_ZONE_INFORMATION tz_winapi;
		ZeroMemory(&tz_winapi, sizeof(tz_winapi));
		int rez = GetTimeZoneInformation(&tz_winapi);
		tz->tz_dsttime = (rez == 2) ? true : false;
		tz->tz_minuteswest = tz_winapi.Bias + ((rez == 2) ? tz_winapi.DaylightBias : 0);
	}
	return 0;
}
#endif 
