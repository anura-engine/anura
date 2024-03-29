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
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include "Util.hpp"

#include "asserts.hpp"
#include "unit_test.hpp"

namespace Util
{
	std::vector<std::string> split(const std::string& str, const std::string& delimiters, SplitFlags flags)
	{
		std::vector<std::string> v;
		std::string::size_type start = 0;
		auto pos = str.find_first_of(delimiters, start);
		while(pos != std::string::npos) {
			if(pos != start && flags != SplitFlags::ALLOW_EMPTY_STRINGS) { // ignore empty tokens
				v.emplace_back(str, start, pos - start);
			}
			start = pos + 1;
			pos = str.find_first_of(delimiters, start);
		}
		if(start < str.length()) { // ignore trailing delimiter
			v.emplace_back(str, start, str.length() - start); // add what's left of the string
		}
		return v;
	}
}

UNIT_TEST(split_test_0) {
	const std::vector<std::string> strings_vector = Util::split(
			"permission is hereby granted to use this software for any purpose",
			"aeiou", Util::SplitFlags::NONE);
	const std::vector<std::string> expected_vector {
		"p", "rm", "ss", "n ", "s h", "r", "by gr", "nt", "d t", " ",
		"s", " th", "s s", "ftw", "r", " f", "r ", "ny p", "rp", "s" };
	ASSERT_LOG(strings_vector.size() == expected_vector.size(), "size mismatch");
	unsigned index = 0;
	for (const auto &string : strings_vector) {
		CHECK_EQ(expected_vector[index++], string);
	}
}

UNIT_TEST(split_test_1) {
	const std::vector<std::string> strings_vector = Util::split(
			"aether", "aeiou", Util::SplitFlags::NONE);
	const std::vector<std::string> expected_vector { "th", "r" };
	ASSERT_LOG(strings_vector.size() == expected_vector.size(), "size mismatch");
	unsigned index = 0;
	for (const auto &string : strings_vector) {
		CHECK_EQ(expected_vector[index++], string);
	}
}

//   Does this expose a bug?
UNIT_TEST(split_test_2) {
	const std::vector<std::string> strings_vector = Util::split(
			"aether", "aeiou",
			Util::SplitFlags::ALLOW_EMPTY_STRINGS);
	const std::vector<std::string> expected_vector { "r" };
	for (const auto &string : strings_vector) {
		LOG_INFO(string);
	}
	ASSERT_LOG(strings_vector.size() == expected_vector.size(), "size mismatch");
	unsigned index = 0;
	for (const auto &string : strings_vector) {
		CHECK_EQ(expected_vector[index++], string);
	}
}

//   Does this expose a bug?
UNIT_TEST(split_test_3) {
	const std::vector<std::string> strings_vector = Util::split(
			"entropia aether", "aeiou",
			Util::SplitFlags::ALLOW_EMPTY_STRINGS);
	const std::vector<std::string> expected_vector { "r" };
	for (const auto &string : strings_vector) {
		LOG_INFO(string);
	}
	ASSERT_LOG(strings_vector.size() == expected_vector.size(), "size mismatch");
	unsigned index = 0;
	for (const auto &string : strings_vector) {
		CHECK_EQ(expected_vector[index++], string);
	}
}

//   Does this expose a bug?
UNIT_TEST(split_test_4) {
	const std::vector<std::string> strings_vector = Util::split(
			"materia entropia aether", "aeiou",
			Util::SplitFlags::ALLOW_EMPTY_STRINGS);
	const std::vector<std::string> expected_vector { "r" };
	for (const auto &string : strings_vector) {
		LOG_INFO(string);
	}
	ASSERT_LOG(strings_vector.size() == expected_vector.size(), "size mismatch");
	unsigned index = 0;
	for (const auto &string : strings_vector) {
		CHECK_EQ(expected_vector[index++], string);
	}
}
