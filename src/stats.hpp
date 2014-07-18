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

#pragma once

#include "kre/Geometry.hpp"
#include "thread.hpp"
#include "variant.hpp"

void http_upload(const std::string& payload, 
	const std::string& script, 
	const char* hostname=NULL, 
	const char* port=NULL);

namespace stats 
{
	//download stats for a given level.
	bool download(const std::string& lvl);

	class Manager 
	{
	public:
		Manager();
		~Manager();
	};

	class Entry 
	{
	public:
		explicit Entry(const std::string& type);
		Entry(const std::string& type, const std::string& level_id);
		~Entry();
		Entry& set(const std::string& name, const variant& value);
		Entry& addPlayerPos();
	private:
		Entry(const Entry& o);
		void operator=(const Entry& o) const;

		std::string level_id_;
		std::map<variant, variant> records_;
	};

	void record_program_args(const std::vector<std::string>& args);

	void record(const variant& value);
	void record(const variant& value, const std::string& level_id);

	void flush();
	void flush_and_quit();
}
