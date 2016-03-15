/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include <exception>
#include <iterator>
#include <string>
#include <cstdint>

namespace utils
{
	class utf8_to_codepoint
	{
	public:
		class iterator : public std::iterator<std::forward_iterator_tag, char32_t, std::string::difference_type, const char32_t*, const char32_t&>
		{
		public:
			iterator(std::string::const_iterator it) 
				: it_(it),
				utf8_bitmask_1(0x80),
				utf8_bitmask_2(0x40),
				utf8_bitmask_3(0x20),
				utf8_bitmask_4(0x10)
			{}
			~iterator() {}
			bool operator!= (const iterator& other) const
			{
				return it_ != other.it_;
			}
			char32_t operator*() 
			{
				char32_t codepoint = 0;
				std::string::const_iterator it(it_);
				auto c = *it++;
				if(c & utf8_bitmask_1) {
					if(c & utf8_bitmask_3) {
						if(c & utf8_bitmask_4) {
							// U = (C1 - 240) * 262,144 + (C2 - 128) * 4,096 + (C3 - 128) * 64 + C4 - 128
							codepoint = (c - 240) * 262144;
							c = *it++;
							codepoint += (c - 128) * 4096;
							c = *it++;
							codepoint += (c - 128) * 64;
							c = *it++;
							codepoint += c - 128;
						} else {
							// U = (C1 - 224) * 4,096 + (C2 - 128) * 64 + C3 - 128
							codepoint = (c - 224) * 4096;
							c = *it++;
							codepoint += (c - 128) * 64;
							c = *it++;
							codepoint += c - 128;
						}
					} else {
						// U = (C1 - 192) * 64 + C2 - 128
						codepoint = (c - 192) * 64;
						c = *it++;
						codepoint += c - 128;
					}
				} else {
					// U = C1
					codepoint = c;
				}
				return codepoint;
			}
			iterator& operator++()
			{
				std::string::difference_type offset = 1;
				auto c = *it_;
				if(c & utf8_bitmask_1) {
					if(c & utf8_bitmask_3) {
						if(c & utf8_bitmask_4) {
							offset = 4;
						} else {
							offset = 3;
						}
					} else {
						offset = 2;
					}
				}
				std::advance(it_, offset);
				return *this;
			}
		private:
			std::string::const_iterator it_;
			const uint8_t utf8_bitmask_1;
			const uint8_t utf8_bitmask_2;
			const uint8_t utf8_bitmask_3;
			const uint8_t utf8_bitmask_4;
		};

		iterator begin() const { return iterator(utf8_.begin()); }
		iterator end() const { return iterator(utf8_.end()); }

		utf8_to_codepoint(const std::string& s) : utf8_(s) {}
	private:
		std::string utf8_;
		utf8_to_codepoint();
	};
    
    inline std::string codepoint_to_utf8(const char32_t cp) 
    {
		char utf8_str[4];		// max length of a utf-8 encoded string is 4 bytes, as per RFC-3629
		int n = 1;				// Count of characters pushed into array.

		if(cp <= 0x7f) {
			utf8_str[0] = static_cast<char>(cp);
		} else if(cp <= 0x7ff) {
			utf8_str[0] = static_cast<char>((cp >> 6) & 0x1f)|0xc0;
			utf8_str[1] = static_cast<char>(cp & 0x3f)|0x80;
			++n;
		} else if(cp <= 0xffff) {
			utf8_str[0] = static_cast<char>(cp >> 12)|0xe0;
			utf8_str[1] = static_cast<char>((cp >> 6) & 0x3f)|0x80;
			utf8_str[2] = static_cast<char>(cp & 0x3f)|0x80;
			n += 2;
		} else if(cp <= 0x10ffff) {
			utf8_str[0] = static_cast<char>(cp >> 18)|0xf0;
			utf8_str[1] = static_cast<char>((cp >> 12) & 0x3f)|0x80;
			utf8_str[2] = static_cast<char>((cp >> 6) & 0x3f)|0x80;
			utf8_str[3] = static_cast<char>(cp & 0x3f)|0x80;
			n += 3;
		} else {
			throw std::runtime_error("Unable to convert codepoint value to utf-8 encoded string.");
		}
		return std::string(utf8_str, n);
    }
}
