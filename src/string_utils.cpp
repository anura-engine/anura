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

#include "string_utils.hpp"
#include "unit_test.hpp"

#include <algorithm>
#include <stdio.h>

namespace util
{
	bool c_isalnum(int c)
	{
		return ::isalnum(static_cast<unsigned char>(c)) != 0;
	}

	bool c_isalpha(int c)
	{
		return ::isalpha(static_cast<unsigned char>(c)) != 0;
	}

	bool c_isascii(int c)
	{
		return isascii(static_cast<unsigned char>(c)) != 0;
	}

	bool c_isblank(int c)
	{
	#if defined(_MSC_VER)
		return ((MB_CUR_MAX > 1 ? _isctype(c,_BLANK) : __chvalidchk(c, _BLANK))) != 0;
	#else
		return ::isblank(static_cast<unsigned char>(c)) != 0;
	#endif
	}

	bool c_iscntrl(int c)
	{
		return ::iscntrl(static_cast<unsigned char>(c)) != 0;
	}

	bool c_isdigit(int c)
	{
		return ::isdigit(static_cast<unsigned char>(c)) != 0;
	}

	bool c_isgraph(int c)
	{
		return ::isgraph(static_cast<unsigned char>(c)) != 0;
	}

	bool c_islower(int c)
	{
		return ::islower(static_cast<unsigned char>(c)) != 0;
	}

	bool c_isprint(int c)
	{
		return ::isprint(static_cast<unsigned char>(c)) != 0;
	}

	bool c_ispunct(int c)
	{
		return ::ispunct(static_cast<unsigned char>(c)) != 0;
	}

	bool c_isspace(int c)
	{
		return ::isspace(static_cast<unsigned char>(c)) != 0;
	}

	bool c_isupper(int c)
	{
		return ::isupper(static_cast<unsigned char>(c)) != 0;
	}

	bool c_isxdigit(int c)
	{
		return ::isxdigit(static_cast<unsigned char>(c)) != 0;
	}

	bool c_isnewline(char c)
	{
		return c == '\r' || c == '\n';
	}

	bool portable_isspace(char c)
	{
		return c_isnewline(c) || c_isspace(c);
	}

	bool notspace(char c)
	{
		return !portable_isspace(c);
	}

	std::string &strip(std::string &str)
	{
		std::string::iterator it = std::find_if(str.begin(), str.end(), notspace);
		str.erase(str.begin(), it);
		str.erase(std::find_if(str.rbegin(), str.rend(), notspace).base(), str.end());

		return str;
	}

	std::vector<std::string> split(std::string const &val, const std::string& delim)
	{
		std::vector<std::string> result;
		if(delim.empty()) {
			for(char c : val) {
				result.push_back(std::string(1, c));
			}

			return result;
		}

		const char* ptr = val.c_str();
		for(;;) {
			const char* end = strstr(ptr, delim.c_str());
			if(end == nullptr) {
				result.push_back(std::string(ptr));
				return result;
			}

			result.push_back(std::string(ptr, end));
			ptr = end + delim.size();
		}

		return result;
	}

	std::vector<std::string> split(std::string const &val, char c, int flags)
	{
		std::vector<std::string> res;
		split(val, res, c, flags);
		return res;
	}

	void split(std::string const &val, std::vector<std::string>& res, char c, int flags)
	{
		std::string::const_iterator i1 = val.begin();
		std::string::const_iterator i2 = val.begin();

		while (i2 != val.end()) {
			if (*i2 == c) {
				std::string new_val(i1, i2);
				if (flags & STRIP_SPACES)
					strip(new_val);
				if (!(flags & REMOVE_EMPTY) || !new_val.empty())
					res.push_back(new_val);
				++i2;
				if (flags & STRIP_SPACES) {
					while (i2 != val.end() && *i2 == ' ')
						++i2;
				}

				i1 = i2;
			} else {
				++i2;
			}
		}

		std::string new_val(i1, i2);
		if (flags & STRIP_SPACES)
			strip(new_val);
		if (!(flags & REMOVE_EMPTY) || !new_val.empty())
			res.push_back(new_val);
	}

	std::string join(const std::vector<std::string>& v, char j)
	{
		std::string res;
		for(int n = 0; n != v.size(); ++n) {
			if(n != 0) {
				res.push_back(j);
			}

			res += v[n];
		}

		return res;
	}

	const char* split_into_ints(const char* s, int* output, int* output_size)
	{
		char* endptr = nullptr;
		int index = 0;
		for(;;) {
			int result = strtol(s, &endptr, 10);
			if(endptr == s) {
				break;
			}

			if(index < *output_size) {
				output[index] = result;
			}

			++index;

			if(*endptr != ',') {
				break;
			}

			s = endptr+1;
		}

		*output_size = index;
		return endptr;
	}

	std::vector<int> split_into_vector_int(const std::string& s, char delim)
	{
		std::vector<std::string> v = util::split(s, delim);
		std::vector<int> result(v.size());
		for(int n = 0; n != v.size(); ++n) {
			result[n] = atoi(v[n].c_str());
		}

		return result;
	}

	std::string join_ints(const int* ints, int size)
	{
		std::string result;
		char buf[256];
		for(int n = 0; n != size; ++n) {
			if(n != 0) {
				result += ",";
			}

			sprintf(buf, "%d", ints[n]);
			result += buf;
		}

		return result;
	}

	bool string_starts_with(const std::string& target, const std::string& prefix) {
		if(target.length() < prefix.length()) {
			return false;
		}
		std::string target_pfx =  target.substr(0,prefix.length());
		return target_pfx == prefix;
	}

	std::string strip_string_prefix(const std::string& target, const std::string& prefix) {
		if(target.length() < prefix.length()) {
			return "";
		}
		return target.substr(prefix.length());
	}

	bool wildcard_pattern_match(std::string::const_iterator p1, std::string::const_iterator p2, std::string::const_iterator i1, std::string::const_iterator i2)
	{
		if(i1 == i2) {
			while(p1 != p2) {
				if(*p1 != '*') {
					return false;
				}
				++p1;
			}

			return true;
		}

		if(p1 == p2) {
			return false;
		}

		if(*p1 == '*') {
			++p1;
			if(p1 == p2) {
				return true;
			}

			while(i1 != i2) {
				if(wildcard_pattern_match(p1, p2, i1, i2)) {
					return true;
				}

				++i1;
			}

			return false;
		}

		if(*p1 != *i1) {
			return false;
		}

		return wildcard_pattern_match(p1+1, p2, i1+1, i2);
	}

	bool wildcard_pattern_match(const std::string& pattern, const std::string& str)
	{
		return wildcard_pattern_match(pattern.begin(), pattern.end(), str.begin(), str.end());
	}
	
	std::string word_wrap(std::string msg, unsigned short columns) {
		return word_wrap(msg, columns, "\n    ");
	}
	std::string word_wrap(std::string msg, unsigned short columns, const std::string& indent) {
		return word_wrap(msg, columns, indent, 65535, "(message snipped at 65535 lines)");
	}
	std::string word_wrap(std::string msg, unsigned short columns, const std::string& indent, unsigned short rows, const std::string& trim_msg) {
		std::string res;
		int chars_in_line = 0, lines = 0;
		
		size_t index = 0;
		size_t max_index = msg.length();
		
		for(char c : msg) {
			++chars_in_line;
			++index;
			
			if(lines > rows) {
				res.append(trim_msg);
				break;
			}
			
			//Hard wrap. (Pre-existing.)
			if(c == '\n') {
				chars_in_line = 0;
				++lines;
				res.push_back('\n');
			}
			
			//Soft wrap, either we've ran out of characters on this line or we can't fit another word on this line.
			else if(chars_in_line > columns || (c == ' ' && (chars_in_line + std::min(msg.find_first_of(" \n", index), max_index) - index > columns))) {
				res.push_back('\n'); //Indent wrapped lines, so it's easier to read \n-deliminated "paragraphs".
				res.append(indent);
				chars_in_line = 4;
				++lines;
			}
			
			else {
				res.push_back(c);
			}
		}
		
		return res;
	}
}

UNIT_TEST(test_wildcard_matches)
{
	CHECK_EQ(util::wildcard_pattern_match("abc", "abc"), true);
	CHECK_EQ(util::wildcard_pattern_match("abc", "abcd"), false);
	CHECK_EQ(util::wildcard_pattern_match("abc*", "abcd"), true);
	CHECK_EQ(util::wildcard_pattern_match("*", "abcwj;def"), true);
	CHECK_EQ(util::wildcard_pattern_match("**", "abcwj;def"), true);
	CHECK_EQ(util::wildcard_pattern_match("*x", "abcwj;def"), false);
	CHECK_EQ(util::wildcard_pattern_match("abc*def", "abcwj;def"), true);
	CHECK_EQ(util::wildcard_pattern_match("abc*def", "abcwj;eef"), false);
}

UNIT_TEST(test_split_into_ints)
{
	int buf[6];
	int buf_size = 6;
	const char* str = "4,18,7,245";
	const char* res = util::split_into_ints(str, buf, &buf_size);
	CHECK_EQ(buf_size, 4);
	CHECK_EQ(res, str + strlen(str));
	CHECK_EQ(buf[0], 4);
	CHECK_EQ(buf[1], 18);
	CHECK_EQ(buf[2], 7);
	CHECK_EQ(buf[3], 245);

	buf[1] = 0;
	buf_size = 1;
	res = util::split_into_ints(str, buf, &buf_size);
	CHECK_EQ(buf_size, 4);
	CHECK_EQ(res, str + strlen(str));
	CHECK_EQ(buf[0], 4);
	CHECK_EQ(buf[1], 0);
}
