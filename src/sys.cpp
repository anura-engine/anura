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

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_host.h>
#endif


#ifdef _MSC_VER
#include <windows.h>
#include <psapi.h>
#endif

#include "asserts.hpp"
#include "filesystem.hpp"
#include "string_utils.hpp"
#include "sys.hpp"
#include "unit_test.hpp"

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

#ifdef __linux__

#include <malloc.h>

	namespace {
	bool parse_linux_status_value(const char* haystack, const char* stat_name, int* value)
	{
		const char* s = strstr(haystack, stat_name);
		if(s == nullptr) {
			return false;
		}

		while(*s && !util::c_isdigit(*s)) {
			++s;
		}

		if(!*s) {
			return false;
		}

		*value = atoi(s);
		return true;
	}
	}

	bool get_memory_consumption(MemoryConsumptionInfo* info)
	{
		std::string s = read_file("/proc/self/status");
		if(parse_linux_status_value(s.c_str(), "VmSize:", &info->vm_used_kb) == false) {
			return false;
		}

		if(parse_linux_status_value(s.c_str(), "VmRSS:", &info->phys_used_kb) == false) {
			return false;
		}

		struct mallinfo m = mallinfo();

		info->heap_free_kb = m.fordblks/1024;
		info->heap_used_kb = m.uordblks/1024;

		return true;
	}

	int get_heap_object_usable_size(void* ptr) {
		return malloc_usable_size(ptr);
	}

#elif defined(__APPLE__)

	bool get_memory_consumption(MemoryConsumptionInfo* res)
	{
	    struct mach_task_basic_info info;
		mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
		if(task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
			(task_info_t)&info, &infoCount) != KERN_SUCCESS ) {
			return false;
		}

		res->heap_free_kb = 0;
		res->heap_used_kb = 0;

		res->vm_used_kb = info.virtual_size/1024;
		res->phys_used_kb = info.resident_size/1024;
		return true;
	}

	int get_heap_object_usable_size(void* ptr) {
		return 0;
	}

#elif defined(_MSC_VER)

	bool get_memory_consumption(MemoryConsumptionInfo* res)
	{
		PROCESS_MEMORY_COUNTERS counters;
		GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));

		res->heap_free_kb = 0;
		res->heap_used_kb = 0;

		res->vm_used_kb = 0;
		res->phys_used_kb = counters.WorkingSetSize/1024;
		return true;
	}

	int get_heap_object_usable_size(void* ptr) {
		return 0;
	}

#else
//Add additional implementations here.
	bool get_memory_consumption(MemoryConsumptionInfo* info)
	{
		return false;
	}

	int get_heap_object_usable_size(void* ptr) {
		return 0;
	}
#endif
}

COMMAND_LINE_UTILITY(util_test_memory_consumption)
{
	sys::MemoryConsumptionInfo info;
	const bool res = sys::get_memory_consumption(&info);

	ASSERT_LOG(res, "Failed to parse memory consumption");

	printf("Memory consumption: %d virt, %d phys\n", info.vm_used_kb, info.phys_used_kb);
}
