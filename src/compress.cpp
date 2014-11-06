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
//#define ZLIB_CONST

#include "asserts.hpp"
#include "compress.hpp"
#include "unit_test.hpp"
#include "zlib.h"

#define CHUNK 16384

namespace zip {

std::vector<char> compress(const std::vector<char>& data, int compression_level)
{
	ASSERT_LOG(compression_level >= -1 && compression_level <= 9, "Compression level must be between -1(default) and 9.");
	if(data.empty()) {
		return data;
	}

	std::vector<char> output(compressBound(data.size()));

	Bytef* dst = reinterpret_cast<Bytef*>(&output[0]);
	uLongf dst_len = output.size();
	const Bytef* src = reinterpret_cast<const Bytef*>(&data[0]);

	const int result = compress2(dst, &dst_len, src, data.size(), compression_level);
	ASSERT_EQ(result, Z_OK);

	output.resize(dst_len);
	return output;
}

std::vector<char> decompress(const std::vector<char>& data)
{
	const unsigned int MAX_OUTPUT_SIZE = 256*1024*1024;

	unsigned int output_size = data.size()*10;
	if(output_size > MAX_OUTPUT_SIZE) {
		output_size = MAX_OUTPUT_SIZE;
	}

	do {
		std::vector<char> output(output_size);

		Bytef* dst = reinterpret_cast<Bytef*>(&output[0]);
		uLongf dst_len = output.size();
		const Bytef* src = reinterpret_cast<const Bytef*>(&data[0]);

		const int result = uncompress(dst, &dst_len, src, data.size());
		if(result == Z_OK) {
			output.resize(dst_len);
			return output;
		}

		output_size *= 2;
	} while(output_size < MAX_OUTPUT_SIZE);

	ASSERT_LOG(false, "COULD NOT DECOMPRESS " << data.size() << " BYTE BUFFER\n");
}

std::vector<char> decompress_known_size(const std::vector<char>& data, int size)
{
	std::vector<char> output(size);

	Bytef* dst = reinterpret_cast<Bytef*>(&output[0]);
	uLongf dst_len = output.size();
	const Bytef* src = reinterpret_cast<const Bytef*>(&data[0]);

	const int result = uncompress(dst, &dst_len, src, data.size());
	ASSERT_LOG(result != Z_MEM_ERROR, "Decompression out of memory");
	ASSERT_LOG(result != Z_BUF_ERROR, "Insufficient space in output buffer");
	ASSERT_LOG(result != Z_DATA_ERROR, "Compression data corrupt");
	ASSERT_LOG(result == Z_OK && dst_len == output.size(), "FAILED TO DECOMPRESS " << data.size() << " BYTES OF DATA TO EXPECTED " << output.size() << " BYTES: " << " result = " << result << " (Z_OK = " << Z_OK << ") OUTPUT " << dst_len);
	return output;
}

}

UNIT_TEST(compression_test)
{
	std::vector<char> data(100000);
	for(int n = 0; n != data.size(); ++n) {
		data[n] = 'A' + rand()%26;
	}

	std::vector<char> compressed = zip::compress(data);
	std::vector<char> uncompressed = zip::decompress(compressed);
	CHECK_EQ(uncompressed.size(), data.size());
	for(int n = 0; n != data.size(); ++n) {
		CHECK_EQ(data[n], uncompressed[n]);
	}
}
