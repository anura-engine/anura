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
#ifndef STATS_HPP_INCLUDED
#define STATS_HPP_INCLUDED

#include <boost/shared_ptr.hpp>

#include "geometry.hpp"
#include "thread.hpp"
#include "variant.hpp"

#include <string>

void http_upload(const std::string& payload, const std::string& script,
                 const char* hostname=NULL, const char* port=NULL);

namespace stats {

//download stats for a given level.
bool download(const std::string& lvl);

class manager {
public:
	manager();
	~manager();
};

class entry {
public:
	explicit entry(const std::string& type);
	entry(const std::string& type, const std::string& level_id);
	~entry();
	entry& set(const std::string& name, const variant& value);
	entry& add_player_pos();
private:
	entry(const entry& o);
	void operator=(const entry& o) const;

	std::string level_id_;
	std::map<variant, variant> records_;
};

void record_program_args(const std::vector<std::string>& args);

void record(const variant& value);
void record(const variant& value, const std::string& level_id);

void flush();
void flush_and_quit();

}

#endif
