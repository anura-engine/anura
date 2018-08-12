/*
 *  Copyright (C) 2003-2018 David White <davewx7@gmail.com>
 *                20??-2018 Kristina Simpson <sweet.kristas@gmail.com>
 *                2017-2018 galegosimpatico <galegosimpatico@outlook.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not
 *   claim that you wrote the original software. If you use this software
 *   in a product, an acknowledgement in the product documentation would be
 *   appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be
 *   misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any source
 *   distribution.
 */

#include "utils.hpp"

#include "unit_test.hpp"

namespace utils
{
	std::vector<std::string> split(const std::string& str, const std::string& delimiters) 
	{
		std::vector<std::string> v;
		std::string::size_type start = 0;
		auto pos = str.find_first_of(delimiters, start);
		while(pos != std::string::npos) {
			if(pos != start) // ignore empty tokens
				v.emplace_back(str, start, pos - start);
			start = pos + 1;
			pos = str.find_first_of(delimiters, start);
		}
		if(start < str.length()) // ignore trailing delimiter
			v.emplace_back(str, start, str.length() - start); // add what's left of the string
		return v;
	}
}

UNIT_TEST(svg_utils_split) {
	const std::string str("aether");
	const std::string delimiters("t");
	std::vector<std::string> expected_vector;
	expected_vector.emplace_back("ae");
	expected_vector.emplace_back("her");
	const uint_fast8_t expected_vector_size = expected_vector.size();
	const std::vector<std::string> actual_vector = utils::split(str, delimiters);
	CHECK_EQ(expected_vector_size, actual_vector.size());
	for (int i = 0; i < expected_vector_size; i++) {
		const std::string expected = expected_vector[i];
		LOG_DEBUG(expected);
		const std::string actual = actual_vector[i];
		LOG_DEBUG(actual);
		CHECK_EQ(expected, actual);
	}
}
