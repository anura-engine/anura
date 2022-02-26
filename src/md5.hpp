/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.	This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

/* Brutally hacked by John Walker back from ANSI C to K&R (no
   prototypes) to maintain the tradition that Netfone will compile
   with Sun's original "cc". */

/* Added a C++ interface.
*/

#pragma once

#include <stdint.h>
#include <iostream>
#include <sstream>
#include <vector>

namespace md5 {
struct MD5Context {
        uint32_t buf[4];
        uint32_t bits[2];
        uint8_t in[64];
};

extern void MD5Init(struct MD5Context *ctx);
extern void MD5Update(struct MD5Context *ctx, unsigned char *buf, unsigned len);
extern void MD5Final(uint8_t digest[16], struct MD5Context *ctx);
extern void MD5Transform(uint32_t buf[4], uint32_t in[16]);

std::string sum(const std::string& data);
}

class MD5
{
public:
	MD5() {
	}
	virtual ~MD5() {
	}

	static std::string calc(const std::string& s) {
		std::vector<uint8_t> v(s.begin(),s.end());
		std::vector<uint8_t> result = calc(v);
		std::string ss(result.begin(), result.end());
		return ss;
	}

	static std::vector<uint8_t> calc(std::vector<uint8_t> v) {
		struct md5::MD5Context ctx;
		md5::MD5Init(&ctx);
		if(v.size()) {
			md5::MD5Update(&ctx, &v[0], static_cast<int>(v.size()));
		}
		std::vector<uint8_t> result(16);
		md5::MD5Final(&result[0], &ctx);
		return result;
	}
};
