#pragma once

#include <string>
#include <vector>


#define DISALLOW_COPY_AND_ASSIGN(TypeName)  \
    TypeName(const TypeName&);              \
    void operator=(const TypeName&)

#define DISALLOW_COPY_ASSIGN_AND_DEFAULT(TypeName)  \
    TypeName();                                     \
    TypeName(const TypeName&);                      \
    void operator=(const TypeName&)

namespace utils
{
	std::vector<std::string> split(const std::string& str, const std::string& delimiters);
}
