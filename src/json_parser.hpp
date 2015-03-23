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

#include <string>
#include "variant.hpp"

namespace json 
{
	void set_file_contents(const std::string& path, const std::string& contents);
	std::string get_file_contents(const std::string& path);

	enum class JSON_PARSE_OPTIONS { NO_PREPROCESSOR, USE_PREPROCESSOR };
	variant parse(const std::string& doc, JSON_PARSE_OPTIONS options=JSON_PARSE_OPTIONS::USE_PREPROCESSOR);
	variant parse_from_file(const std::string& fname, JSON_PARSE_OPTIONS options=JSON_PARSE_OPTIONS::USE_PREPROCESSOR);
	bool file_exists_and_is_valid(const std::string& fname);

	struct ParseError 
	{
		explicit ParseError(const std::string& msg);
		ParseError(const std::string& msg, const std::string& filename, int line, int col);

		std::string errorMessage() const;

		std::string message;
		std::string fname;
		int line, col;
	};
}
