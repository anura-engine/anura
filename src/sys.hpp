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

namespace sys 
{
	struct AvailableMemoryInfo 
	{
		int mem_used_kb, mem_free_kb, mem_total_kb;
	};

	bool get_available_memory(AvailableMemoryInfo* info);

	struct MemoryConsumptionInfo
	{
		MemoryConsumptionInfo() : vm_used_kb(0), phys_used_kb(0), heap_free_kb(0), heap_used_kb(0)
		{}
		int vm_used_kb, phys_used_kb;
		int heap_free_kb, heap_used_kb;
	};

	bool get_memory_consumption(MemoryConsumptionInfo* info);

	int get_heap_object_usable_size(void* ptr);
}
