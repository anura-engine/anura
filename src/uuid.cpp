#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random/mersenne_twister.hpp>

#include "asserts.hpp"
#include "unit_test.hpp"
#include "uuid.hpp"

namespace {

boost::mt19937* twister_rng() {
	static boost::mt19937 ran;
	ran.seed(boost::posix_time::microsec_clock::local_time().time_of_day().total_milliseconds());
	return &ran;
}

UNIT_TEST(serialize_uuid) {
	for(int i = 0; i != 8; ++i) {
		boost::uuids::uuid id = generate_uuid();
		const bool succeeded = id == read_uuid(write_uuid(id));
		CHECK_EQ(succeeded, true);
	}
}

}

boost::uuids::uuid generate_uuid() {
	static boost::uuids::basic_random_generator<boost::mt19937> gen(twister_rng());
	return gen();
}

std::string write_uuid(const boost::uuids::uuid& id) {
	char result[33];
	char* ptr = result;
	for(auto num : id) {
		sprintf(ptr, "%02x", static_cast<int>(num));
		ptr += 2;
	}
	return std::string(result, result+32);
}

boost::uuids::uuid read_uuid(const std::string& s) {
	boost::uuids::uuid result;

	const std::string& nums = s;
	const char* ptr = nums.c_str();
	ASSERT_LOG(nums.size() == 32, "Trying to deserialize bad UUID: " << nums);
	for(auto itor = result.begin(); itor != result.end(); ++itor) {
		char buf[3];
		buf[0] = *ptr++;
		buf[1] = *ptr++;
		buf[2] = 0;

		*itor = strtol(buf, NULL, 16);
	}

	return result;
}

