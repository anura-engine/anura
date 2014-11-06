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
#ifndef REFERENCE_COUNTED_OBJECT_HPP_INCLUDED
#define REFERENCE_COUNTED_OBJECT_HPP_INCLUDED

#include <assert.h>

#include "boost/intrusive_ptr.hpp"

class reference_counted_object;

class weak_ptr_base
{
public:
	explicit weak_ptr_base(const reference_counted_object* obj=NULL);
	~weak_ptr_base();
	void release();

protected:
	reference_counted_object* get_obj() const { return const_cast<reference_counted_object*>(obj_); }
	void init(const reference_counted_object* obj=NULL);
	void remove();
private:
	const reference_counted_object* obj_;
	weak_ptr_base* next_;
	weak_ptr_base* prev_;

	weak_ptr_base(const weak_ptr_base&);
	void operator=(const weak_ptr_base&);
};

class reference_counted_object
{
public:
	reference_counted_object() : count_(0), weak_(NULL) {}
	reference_counted_object(const reference_counted_object& /*obj*/) : count_(0), weak_(NULL) {}
	reference_counted_object& operator=(const reference_counted_object& /*obj*/) {
		return *this;
	}
	virtual ~reference_counted_object() { if(weak_ != NULL) { weak_->release(); } }

	void add_ref() const { ++count_; }
	void dec_ref() const { if(--count_ == 0) { delete this; } }
	void dec_ref_norelease() const { --count_; }

	int refcount() const { return count_; }

	friend class weak_ptr_base;

protected:
	void turn_reference_counting_off() { count_ = 1000000; }
private:
	mutable int count_;
	mutable weak_ptr_base* weak_;
};

struct reference_counted_object_pin_norelease
{
	reference_counted_object* obj_;
	reference_counted_object_pin_norelease(reference_counted_object* obj) : obj_(obj)
	{
		obj_->add_ref();
	}
	~reference_counted_object_pin_norelease()
	{
		obj_->dec_ref_norelease();
	}
};

inline void intrusive_ptr_add_ref(const reference_counted_object* obj) {
	obj->add_ref();
}

inline void intrusive_ptr_release(const reference_counted_object* obj) {
	obj->dec_ref();
}

typedef boost::intrusive_ptr<reference_counted_object> object_ptr;
typedef boost::intrusive_ptr<const reference_counted_object> const_object_ptr;

#endif
