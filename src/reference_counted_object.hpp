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

#include <atomic>
#include <mutex>
#include <assert.h>

#include "boost/intrusive_ptr.hpp"

//turn on multi-threaded protection for FFL constructs.
//#define MT_FFL

#ifdef MT_FFL
typedef std::atomic_int IntRefCount;
#else
typedef int IntRefCount;
#endif

#ifdef __APPLE__
#define THREAD_LOCAL __thread
#elif defined(WIN32)
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL thread_local
#endif

class reference_counted_object;

class weak_ptr_base
{
public:
	explicit weak_ptr_base(const reference_counted_object* obj=nullptr);
	~weak_ptr_base();
	static void release(reference_counted_object* obj);

protected:
	reference_counted_object* get_obj_add_ref() const;
	void init(const reference_counted_object* obj=nullptr);
	void remove();
private:

	void release_internal();

	const reference_counted_object* obj_;
	weak_ptr_base* next_;
	weak_ptr_base* prev_;

	weak_ptr_base(const weak_ptr_base&);
	void operator=(const weak_ptr_base&);
};

class reference_counted_object
{
public:
	reference_counted_object() : count_(0), weak_(nullptr) {
#ifdef DEBUG_GARBAGE_COLLECTOR
		ptr_count_ = 0;
		variant_count_ = 0;
#endif
	}
	reference_counted_object(const reference_counted_object& /*obj*/) : count_(0), weak_(nullptr) {
#ifdef DEBUG_GARBAGE_COLLECTOR
		ptr_count_ = 0;
		variant_count_ = 0;
#endif
	}
	reference_counted_object& operator=(const reference_counted_object& /*obj*/) {
		return *this;
	}

	void add_reference() const { ++count_; }
	void dec_reference() const { if(--count_ == 0) { delete this; } }
	void dec_ref_norelease() const { --count_; }

	int refcount() const { return count_; }

	friend class weak_ptr_base;

#ifdef DEBUG_GARBAGE_COLLECTOR
	void add_ref_ptr_debug() const { ++ptr_count_; }
	void dec_ref_ptr_debug() const { --ptr_count_; }
	mutable int ptr_count_;

	void add_ref_variant_debug() const { ++variant_count_; }
	void dec_ref_variant_debug() const { --variant_count_; }
	mutable int variant_count_;
#endif

protected:
	void turn_reference_counting_off() { count_ = 1000000; }
	virtual ~reference_counted_object() { if(weak_ != nullptr) { weak_ptr_base::release(this); } }
private:

	mutable IntRefCount count_;
	mutable weak_ptr_base* weak_;
};

struct reference_counted_object_pin_norelease
{
	reference_counted_object* obj_;
	reference_counted_object_pin_norelease(reference_counted_object* obj) : obj_(obj)
	{
		obj_->add_reference();
	}
	~reference_counted_object_pin_norelease()
	{
		obj_->dec_ref_norelease();
	}
};

inline void intrusive_ptr_add_ref(const reference_counted_object* obj) {
#ifdef DEBUG_GARBAGE_COLLECTOR
	obj->add_ref_ptr_debug();
#endif

	obj->add_reference();
}

inline void intrusive_ptr_release(const reference_counted_object* obj) {
#ifdef DEBUG_GARBAGE_COLLECTOR
	obj->dec_ref_ptr_debug();
#endif

	obj->dec_reference();
}

inline void variant_ptr_add_ref(const reference_counted_object* obj) {
#ifdef DEBUG_GARBAGE_COLLECTOR
	obj->add_ref_variant_debug();
#endif

	obj->add_reference();
}

inline void variant_ptr_release(const reference_counted_object* obj) {
#ifdef DEBUG_GARBAGE_COLLECTOR
	obj->dec_ref_variant_debug();
#endif

	obj->dec_reference();
}
