#include "utils.hpp"

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