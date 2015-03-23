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

#if TARGET_OS_IPHONE
#include <mach/mach.h>
#include <mach/mach_host.h>
#endif

#include "sys.hpp"

namespace sys 
{
#if TARGET_OS_IPHONE
	bool get_available_memory(AvailableMemoryInfo* info)
	{
		const mach_port_t host_port = mach_host_self();
		mach_msg_type_number_t host_size = sizeof(vm_statistics_data_t) / sizeof(integer_t);

		vm_size_t pagesize;
		host_page_size(host_port, &pagesize);

		vm_statistics_data_t vm_stat;
		if(host_statistics(host_port, HOST_VM_INFO, (host_info_t)&vm_stat, &host_size) != KERN_SUCCESS) {
			return false;
		}

		if(info != nullptr) {
			info->mem_used_kb = (vm_stat.active_count + vm_stat.inactive_count + vm_stat.wire_count) * (pagesize/1024);

			info->mem_free_kb = vm_stat.free_count * (pagesize/1024);
			info->mem_total_kb = info->mem_used_kb + info->mem_free_kb;
		}

		return true;
	}
	
#else
//Add additional implementations here.

	bool get_available_memory(AvailableMemoryInfo* info)
	{
		return false;
	}

#endif
}
