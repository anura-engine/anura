/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#ifndef COMPRESS_HPP_INCLUDED
#define COMPRESS_HPP_INCLUDED

#include <vector>

#include "base64.hpp"
#include "formula_callable.hpp"
#include "variant.hpp"

namespace zip {

struct CompressionException {
	const char* msg;
};

std::vector<char> compress(const std::vector<char>& data, int compression_level=-1);
std::vector<char> decompress(const std::vector<char>& data);
std::vector<char> decompress_known_size(const std::vector<char>& data, int size);

class compressed_data : public game_logic::formula_callable {
	std::vector<char> data_;
public:
	compressed_data(const std::vector<char>& in_data, int compression_level) {
		data_ = compress(in_data, compression_level);
	}
	variant get_value(const std::string& key) const {
		if(key == "encode") {
			std::vector<char> v = base64::b64encode(data_);
			return variant(std::string(v.begin(), v.end()));
		} else if(key == "decompress") {
			std::vector<char> v = decompress(data_);
			return variant(std::string(v.begin(), v.end()));
		}
		return variant();
	}
};
typedef boost::intrusive_ptr<zip::compressed_data> compressed_data_ptr;

}

#endif // COMPRES_HPP_INCLUDED
