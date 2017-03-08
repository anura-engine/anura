#pragma once

#include "intrusive_ptr.hpp"
#include "reference_counted_object.hpp"

namespace ffl
{

template<typename T>
class weak_ptr : public weak_ptr_base {
public:
	explicit weak_ptr(T* obj=nullptr) : weak_ptr_base(obj)
	{}

	void reset(T* obj=nullptr) {
		init(obj);
	}

	ffl::IntrusivePtr<T> get() const {

		auto res = reinterpret_cast<T*>(get_obj_add_ref());
		ffl::IntrusivePtr<T> val(res);
		if(res) {
			res->dec_reference();
		}
		return val;
	}

	weak_ptr(const weak_ptr<T>& p) {
		auto ptr = p.get();
		init(ptr.get());
	}

	const weak_ptr<T>& operator=(const weak_ptr<T>& p) {
		auto ptr = p.get();
		init(ptr.get());
		return *this;
	}
};
}
