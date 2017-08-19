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

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random/mersenne_twister.hpp>

#include "asserts.hpp"
#include "unit_test.hpp"
#include "uuid.hpp"

namespace 
{
	boost::mt19937* twister_rng() 
	{
		static boost::mt19937 ran;
		ran.seed(static_cast<unsigned>(boost::posix_time::microsec_clock::local_time().time_of_day().total_milliseconds()));
		return &ran;
	}

	UNIT_TEST(serialize_uuid) 
	{
		for(int i = 0; i != 8; ++i) {
			boost::uuids::uuid id = generate_uuid();
			const bool succeeded = id == read_uuid(write_uuid(id));
			CHECK_EQ(succeeded, true);
		}
	}
}

boost::uuids::uuid generate_uuid() 
{
	static boost::uuids::basic_random_generator<boost::mt19937> gen(twister_rng());
	return gen();
}

std::string write_uuid(const boost::uuids::uuid& id) 
{
	char result[33];
	char* ptr = result;
	for(auto num : id) {
		sprintf(ptr, "%02x", static_cast<int>(num));
		ptr += 2;
	}
	return std::string(result, result+32);
}

boost::uuids::uuid read_uuid(const std::string& s) 
{
	boost::uuids::uuid result;

	const std::string& nums = s;
	const char* ptr = nums.c_str();
	ASSERT_LOG(nums.size() == 32, "Trying to deserialize bad UUID: " << nums);
	for(auto itor = result.begin(); itor != result.end(); ++itor) {
		char buf[3];
		buf[0] = *ptr++;
		buf[1] = *ptr++;
		buf[2] = 0;

		*itor = static_cast<uint8_t>(strtol(buf, nullptr, 16));
	}

	return result;
}

boost::uuids::uuid addr_to_uuid(const std::string& s)
{
	auto itor = s.begin();
	if(s.size() > 2 && s[0] == '0' && s[1] == 'x') {
		itor += 2;
	}

	std::string str(itor, s.end());
	const size_t sz = str.size();
	if(sz < 32) {
		str.resize(32);
		std::fill(str.begin() + sz, str.end(), '0');
	} else {
		str.resize(32);
	}

	return read_uuid(str);
}


BENCHMARK(generate_uuid) {
	generate_uuid();
	BENCHMARK_LOOP {
		generate_uuid();
	}
}
