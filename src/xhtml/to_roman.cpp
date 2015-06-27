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

#include <algorithm>
#include <vector>

#include "to_roman.hpp"

namespace 
{
	std::vector<std::pair<int, std::string>>& get_roman_numerals()
	{
		static std::vector<std::pair<int, std::string>> res;
		if(res.empty()) {
			res.emplace_back(std::make_pair(1000, "M"));
			res.emplace_back(std::make_pair(900, "CM"));
			res.emplace_back(std::make_pair(500, "D"));
			res.emplace_back(std::make_pair(400, "CD"));
			res.emplace_back(std::make_pair(100, "C"));
			res.emplace_back(std::make_pair(90, "XC"));
			res.emplace_back(std::make_pair(50, "L"));
			res.emplace_back(std::make_pair(40, "XL"));
			res.emplace_back(std::make_pair(10, "X"));
			res.emplace_back(std::make_pair(9, "IX"));
			res.emplace_back(std::make_pair(5, "V"));
			res.emplace_back(std::make_pair(4, "IV"));
			res.emplace_back(std::make_pair(1, "I"));
		}
		return res;
	}
}

// Converts
std::string to_roman(int n, bool lower)
{
	std::string res;
	for(auto& roman : get_roman_numerals()) {
		while(n >= roman.first) {
			
			n -= roman.first;
			res += roman.second;
		}
	}
	if(lower) {
		std::transform(res.begin(), res.end(), res.begin(), ::tolower);
	}
	return res;
}
