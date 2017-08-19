#ifdef DEBUG_GARBAGE_COLLECTOR

#include <set>
#include <vector>

#include "intrusive_ptr.hpp"
#include "reference_counted_object.hpp"

namespace ffl
{

namespace {
std::set<void*> g_all_intrusive_ptr;
}

void registerIntrusivePtr(void* p)
{
	g_all_intrusive_ptr.insert(p);
}

void unregisterIntrusivePtr(void* p)
{
	g_all_intrusive_ptr.erase(p);
}

std::vector<IntrusivePtr<reference_counted_object>*> getAllIntrusivePtrDebug()
{
	std::vector<IntrusivePtr<reference_counted_object>*> result;
	for(void* p : g_all_intrusive_ptr) {
		result.push_back(reinterpret_cast<IntrusivePtr<reference_counted_object>*>(p));
		if(result.back()->get() == nullptr) {
			result.pop_back();
		}
	}

	return result;
}

}

#endif
