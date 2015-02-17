#include "ffl_weak_ptr.hpp"

weak_ptr_base::weak_ptr_base(const reference_counted_object* obj)
  : obj_(nullptr), next_(nullptr), prev_(nullptr)
{
	init(obj);
}

void weak_ptr_base::init(const reference_counted_object* obj)
{
	remove();

	obj_ = obj;
	next_ = nullptr;
	prev_ = nullptr;

	if(obj == nullptr) {
		return;
	}

	if(obj->weak_ != nullptr) {
		obj->weak_->prev_ = this;
		this->next_ = obj->weak_;
	}

	obj->weak_ = this;
}

weak_ptr_base::~weak_ptr_base()
{
	remove();
}

void weak_ptr_base::remove()
{
	if(obj_ != nullptr && obj_->weak_ == this) {
		obj_->weak_ = next_;
	}

	if(prev_ != nullptr) {
		prev_->next_ = next_;
	}

	if(next_ != nullptr) {
		next_->prev_ = prev_;
	}

	obj_ = nullptr;
	prev_ = nullptr;
	next_ = nullptr;
}

void weak_ptr_base::release()
{
	if(next_ != nullptr) {
		next_->release();
	}

	obj_ = nullptr;
	prev_ = nullptr;
	next_ = nullptr;
}
