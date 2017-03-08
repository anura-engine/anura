#include "ffl_weak_ptr.hpp"

#ifdef MT_FFL
namespace {
	std::mutex& global_weak_ptr_mutex() {
		static std::mutex m;
		return m;
	}
}
#endif

weak_ptr_base::weak_ptr_base(const reference_counted_object* obj)
  : obj_(nullptr), next_(nullptr), prev_(nullptr)
{
	init(obj);
}

void weak_ptr_base::init(const reference_counted_object* obj)
{
#ifdef MT_FFL
	std::lock_guard<std::mutex> guard(global_weak_ptr_mutex());
#endif

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
#ifdef MT_FFL
	std::lock_guard<std::mutex> guard(global_weak_ptr_mutex());
#endif
	remove();
}

reference_counted_object* weak_ptr_base::get_obj_add_ref() const
{

#ifdef MT_FFL
	std::lock_guard<std::mutex> guard(global_weak_ptr_mutex());
#endif

	reference_counted_object* obj = const_cast<reference_counted_object*>(obj_);

	if(obj) {
		if(++obj->count_ > 1) {
			return obj;
		}

		--obj->count_;
	}

	return nullptr;
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

void weak_ptr_base::release(reference_counted_object* obj)
{
#ifdef MT_FFL
	std::lock_guard<std::mutex> guard(global_weak_ptr_mutex());
#endif

	if(obj->weak_) {
		obj->weak_->release_internal();
	}
}

void weak_ptr_base::release_internal()
{
	if(next_ != nullptr) {
		next_->release_internal();
	}

	obj_ = nullptr;
	prev_ = nullptr;
	next_ = nullptr;
}
