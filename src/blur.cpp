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

#include "ColorScope.hpp"

#include "blur.hpp"
#include "custom_object.hpp"
#include "frame.hpp"
#include "variant_utils.hpp"

BlurInfo::BlurInfo(float alpha, float fade, int granularity)
  : alpha_(alpha), fade_(fade), granularity_(granularity)
{
}

void BlurInfo::copySettings(const BlurInfo& o)
{
	alpha_ = o.alpha_;
	fade_ = o.fade_;
	granularity_ = o.granularity_;
}

void BlurInfo::nextFrame(int start_x, int start_y, int end_x, int end_y,
                const Frame* object_frame, int time_in_frame, bool facing,
				bool upside_down, float start_rotate, float rotate) {
	for(BlurFrame& f : frames_) {
		f.fade -= fade_;
	}

	while(!frames_.empty() && frames_.front().fade <= 0.0f) {
		frames_.pop_front();
	}

	for(int n = 0; n < granularity_; ++n) {
		BlurFrame f;
		f.object_frame = object_frame;
		f.x = (start_x*n + end_x*(granularity_ - n))/granularity_;
		f.y = (start_y*n + end_y*(granularity_ - n))/granularity_;
		f.time_in_frame = time_in_frame;
		f.facing = facing;
		f.upside_down = upside_down;
		f.rotate = (start_rotate*n + rotate*(granularity_ - n))/granularity_;
		f.fade = alpha_ + (fade_*(granularity_ - n))/granularity_;
		frames_.push_back(f);
	}
}

void BlurInfo::draw() const
{
	for(const BlurFrame& f : frames_) {
		KRE::ColorScope color_scope(KRE::Color(1.0f, 1.0f, 1.0f, f.fade));
		f.object_frame->draw(nullptr, static_cast<int>(f.x), static_cast<int>(f.y), f.facing, f.upside_down, f.time_in_frame, f.rotate);
	}
}

bool BlurInfo::destroyed() const
{
	return granularity_ == 0 && frames_.empty();
}

namespace {
struct ObjectTempModifier {
	ObjectTempModifier(CustomObject* obj, const std::map<std::string,variant>& properties) : obj_(obj) {
		for(auto p : properties) {
			original_properties_[p.first] = obj_->queryValue(p.first);
		}

		Modify(properties);
	}

	~ObjectTempModifier() {
		Modify(original_properties_);
	}

	void Modify(const std::map<std::string,variant>& props) {
		for(auto p : props) {
			try {
				obj_->mutateValue(p.first, p.second);
			} catch(...) {
				ASSERT_LOG(false, "exception while modifying object: " << p.first << " for blurring");
			}
		}
	}

	CustomObject* obj_;
	std::map<std::string,variant> original_properties_;
};
}

BlurObject::BlurObject(const std::map<std::string,variant>& starting_properties, const std::map<std::string,variant>& ending_properties, int duration, variant easing)
  : start_properties_(starting_properties), end_properties_(ending_properties), duration_(duration), age_(0), easing_(easing)
{
}

BlurObject::~BlurObject()
{
}

void BlurObject::setObject(CustomObject* obj)
{
	obj_ = obj;
}

namespace {
int g_recurse = 0;
struct RecursionProtector
{
	RecursionProtector() {
		++g_recurse;
	}

	~RecursionProtector() {
		--g_recurse;
	}

	bool recursing() const { return g_recurse > 1; }
};
}

void BlurObject::draw(int x, int y) 
{
	RecursionProtector protector;
	if(protector.recursing()) {
		return;
	}

	ASSERT_LOG(obj_.get() != nullptr, "Must set an object before drawing a blur");
	
	decimal ratio = age_ >= duration_ ? decimal(1) : decimal(age_) / decimal(duration_);
	if(easing_.is_function()) {
		std::vector<variant> args;
		args.emplace_back(variant(ratio));
		ratio = easing_(args).as_decimal();
	}

	for(auto p : start_properties_) {
		variant val = p.second;

		if(age_ > 0) {
			auto i = end_properties_.find(p.first);
			if(i != end_properties_.end()) {
				val = interpolate_variants(val, i->second, ratio);
			}
		}

		cur_properties_[p.first] = val;
	}

	ObjectTempModifier modifier(obj_.get(), cur_properties_);

	obj_->draw(x, y);
}

void BlurObject::process()
{
	++age_;
}

bool BlurObject::expired() const
{
	return age_ >= duration_;
}

BEGIN_DEFINE_CALLABLE_NOBASE(BlurObject)
END_DEFINE_CALLABLE(BlurObject)
