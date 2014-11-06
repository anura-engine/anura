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
#ifndef HI_RES_TIMER_HPP_INCLUDED
#define HI_RES_TIMER_HPP_INCLUDED

#ifdef linux
#include <stdio.h>
#include <sys/time.h>
#endif

struct hi_res_timer {
	hi_res_timer(const char* str) {
#ifdef linux
		str_ = str;
		gettimeofday(&tv_, 0);
#endif
	};

#ifdef linux
	~hi_res_timer() {
		const int begin = (tv_.tv_sec%1000)*1000000 + tv_.tv_usec;
		gettimeofday(&tv_, 0);
		const int end = (tv_.tv_sec%1000)*1000000 + tv_.tv_usec;
		fprintf(stderr, "TIMER: %s: %dus\n", str_, (end - begin));
	}

	const char* str_;
	timeval tv_;
#endif
};

#endif
