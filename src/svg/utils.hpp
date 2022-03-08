#pragma once

#include <string>
#include <vector>

#define DISALLOW_COPY_AND_ASSIGN(TypeName)                                                                             \
	TypeName(const TypeName&) = delete;                                                                                \
	void operator=(const TypeName&) = delete

#define DISALLOW_COPY_ASSIGN_AND_DEFAULT(TypeName)                                                                     \
	TypeName() = delete;                                                                                               \
	TypeName(const TypeName&) = delete;                                                                                \
	void operator=(const TypeName&) = delete

namespace utils
{
	std::vector<std::string> split(const std::string& str, const std::string& delimiters);
}
