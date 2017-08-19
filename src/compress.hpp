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

#include "base64.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "variant.hpp"

namespace zip 
{

	struct CompressionException 
	{
		const char* msg;
	};

	//these are slower. Prefer the vector<char> versions.
	std::string compress(const std::string& data, int compression_level=-1);
	std::string decompress(const std::string& data);

	std::vector<char> compress(const std::vector<char>& data, int compression_level=-1);
	std::vector<char> decompress(const std::vector<char>& data);
	std::vector<char> decompress_known_size(const std::vector<char>& data, int size);

	class CompressedData : public game_logic::FormulaCallable 
	{
		std::vector<char> data_;
	public:
		CompressedData(const std::vector<char>& in_data, int compression_level) {
			data_ = compress(in_data, compression_level);
		}
	private:
		DECLARE_CALLABLE(CompressedData);
	};
	typedef ffl::IntrusivePtr<zip::CompressedData> CompressedDataPtr;

}
