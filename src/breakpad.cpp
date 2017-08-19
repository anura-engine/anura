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
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#ifdef USE_BREAKPAD

#include "exception_handler.h"
#include "client/windows/sender/crash_report_sender.h"

#include "SDL.h"
#include "breakpad.hpp"
#include "preferences.hpp"


#pragma comment(lib, "common")
#pragma comment(lib, "crash_generation_client")
#pragma comment(lib, "crash_report_sender")
#pragma comment(lib, "exception_handler")
#pragma comment(lib, "wininet.lib")

namespace
{
	// XXX Make this a preference option somewhere.
	const std::wstring& get_server_address()
	{
		static std::wstring server_address = L"theargentlark.com";
		return server_address;
	}
}

namespace breakpad
{
	static bool mini_dump_filter_callback(void* context, EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion)
	{
		// XXX pop up a dialog etc.
		return true;
	}

	static bool mini_dump_handler_callback(const wchar_t* dump_path, const wchar_t* minidump_id, void* context, EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion, bool succeeded)
	{
		// Instantiate CrashReportSender, specifying checkpoint file, where it stores information about number of dumps sent per day, to limit itself
		google_breakpad::CrashReportSender sender(L"crash.checkpoint");
		// Set maximum numer of reports per day from one user, or -1 for unlimited reports
		sender.set_max_reports_per_day(5);
		// You can fill this map with additional params, for example, video adapter information or application's inner version, or something
		std::map<std::wstring, std::wstring> params;
		// Callback parameters do not give you an exact name of generated minidump file, but you can reconstruct it
		std::wstringstream wss;
		wss << dump_path
			<< L"\\"
			<< minidump_id
			<< L".dmp";
		// XXX Need to figure out the exact definition of this map, the documentation is very unclear.
		std::map<std::wstring, std::wstring> files;
		files[wss.str()] = wss.str();

		// Finally, send a report
		google_breakpad::ReportResult r = sender.SendCrashReport(get_server_address(), params, files, 0);
 
		// Possibly notify user about success/failure
		if (r == google_breakpad::RESULT_SUCCEEDED) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Crash report", "Crash report was sent. Thank you!", NULL);
		} else {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Crash report", "Could not send crash report. Thank you for trying, though!", NULL);
		}
 
		return false;
	}

	void install()
	{
		std::wstringstream dump_path;
		dump_path << preferences::save_file_path();

		google_breakpad::ExceptionHandler *pHandler = new google_breakpad::ExceptionHandler( 
			  dump_path.str(), 
			  mini_dump_filter_callback, 
			  mini_dump_handler_callback, 
			  0, 
			  google_breakpad::ExceptionHandler::HANDLER_ALL, 
			  MiniDumpNormal, 
			  L"", 
			  0 );
	}
}

#endif // USE_BREAKPAD
