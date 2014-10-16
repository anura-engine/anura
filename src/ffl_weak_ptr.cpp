#include "ffl_weak_ptr.hpp"

weak_ptr_base::weak_ptr_base(const reference_counted_object* obj)
  : obj_(NULL), next_(NULL), prev_(NULL)
{
	init(obj);
}

void weak_ptr_base::init(const reference_counted_object* obj)
{
	remove();

	obj_ = obj;
	next_ = NULL;
	prev_ = NULL;

	if(obj == NULL) {
		return;
	}

	if(obj->weak_ != NULL) {
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
	if(obj_ != NULL && obj_->weak_ == this) {
		obj_->weak_ = next_;
	}

	if(prev_ != NULL) {
		prev_->next_ = next_;
	}

	if(next_ != NULL) {
		next_->prev_ = prev_;
	}

	obj_ = NULL;
	prev_ = NULL;
	next_ = NULL;
}

void weak_ptr_base::release()
{
	if(next_ != NULL) {
		next_->release();
	}

	obj_ = NULL;
	prev_ = NULL;
	next_ = NULL;
}
