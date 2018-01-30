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
#include <vector>

#include <boost/lexical_cast.hpp>

namespace util
{
	bool c_isalnum(int c);
	bool c_isalpha(int c);
	bool c_isascii(int c); //
	bool c_isblank(int c); //
	bool c_iscntrl(int c); //
	bool c_isdigit(int c);
	bool c_isgraph(int c); //
	bool c_islower(int c);
	bool c_isprint(int c);
	bool c_ispunct(int c); //
	bool c_isspace(int c);
	bool c_isupper(int c); //
	bool c_isxdigit(int c);

	bool c_isnewline(char c);
	bool portable_isspace(char c);
	bool notspace(char c);

	std::string& strip(std::string& str);

	enum { REMOVE_EMPTY = 0x01, STRIP_SPACES = 0x02 };
	std::vector<std::string> split(std::string const &val, char c = ',', int flags = REMOVE_EMPTY | STRIP_SPACES);
	void split(std::string const &val, std::vector<std::string>& res, char c = ',', int flags = REMOVE_EMPTY | STRIP_SPACES);
	std::vector<std::string> split(std::string const &val, std::string const &delim);
	std::string join(const std::vector<std::string>& v, char c=',');

	//splits the string 's' into ints, storing the output in 'output'. s
	//should point to a comma-separated list of integers. output_size should point
	//to the size of 'output'. The number of ints found will be stored in
	//output_size.
	const char* split_into_ints(const char* s, int* output, int* output_size);
	std::vector<int> split_into_vector_int(const std::string& s, char delim=',');

	std::string join_ints(const int* buf, int size);

	bool string_starts_with(const std::string& target, const std::string& prefix);
	std::string strip_string_prefix(const std::string& target, const std::string& prefix);

	template<typename To, typename From>
	std::vector<To> vector_lexical_cast(const std::vector<From>& v) {
		std::vector<To> result;
		result.resize(v.size());
		for(const From& from : v) {
			result.push_back(boost::lexical_cast<To>(from));
		}

		return result;
	}

	bool wildcard_pattern_match(const std::string& pattern, const std::string& str);
	
	std::string word_wrap(std::string msg, unsigned short columns);
	std::string word_wrap(std::string msg, unsigned short columns, const std::string& indent);
	std::string word_wrap(std::string msg, unsigned short columns, const std::string& indent, unsigned short rows, const std::string& trim_msg);
}
