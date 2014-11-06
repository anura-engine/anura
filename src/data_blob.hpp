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
#pragma once
#ifndef DATA_BLOB_HPP_INCLUDED
#define DATA_BLOB_HPP_INCLUDED

#include <boost/shared_ptr.hpp>

#include "SDL.h"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

class data_blob : public game_logic::formula_callable
{
public:
	data_blob(const std::string& key, const std::vector<char>& in_data);
	virtual ~data_blob();
	SDL_RWops* get_rw_ops();
	std::string operator()();
	std::string to_string() const;

private:
	DECLARE_CALLABLE(data_blob);
	struct deleter
	{
		void operator()(SDL_RWops* p) 
		{ 
			SDL_FreeRW(p);
		}
	};

	std::vector<char> data_;
	std::string key_;
	boost::shared_ptr<SDL_RWops> rw_ops_;
};

typedef boost::intrusive_ptr<data_blob> data_blob_ptr;
typedef boost::intrusive_ptr<const data_blob> const_data_blob_ptr;

#endif
