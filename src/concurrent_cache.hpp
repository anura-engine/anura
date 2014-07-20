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

#include <map>
#include <vector>

#include "thread.hpp"

template<typename Key, typename Value>
class ConcurrentCache
{
public:
	typedef std::map<Key, Value> map_type;

	size_t size() const { threading::lock l(mutex_); return map_.size(); }

	const Value& get(const Key& key) {
		threading::lock l(mutex_);
		typename map_type::const_iterator itor = map_.find(key);
		if(itor != map_.end()) {
			return itor->second;
		} else {
			static const Value empty_result = Value();
			return empty_result;
		}
	}

	void put(const Key& key, const Value& value) {
		threading::lock l(mutex_);
		map_[key] = value;
	}

	void erase(const Key& key) {
		threading::lock l(mutex_);
		map_.erase(key);
	}

	int count(const Key& key) const {
		threading::lock l(mutex_);
		return map_.count(key);
	}

	void clear() {
		threading::lock l(mutex_);
		map_.clear();
	}

	std::vector<Key> getKeys() {
		std::vector<Key> result;
		threading::lock l(mutex_);
		for(const auto& i : map_) {
			result.push_back(i.first);
		}

		return result;
	}

	struct lock : public threading::lock {
		explicit lock(ConcurrentCache& cache) : threading::lock(cache.mutex_), cache_(cache) {
		}

		map_type& map() const { return cache_.map_; }

	private:
		ConcurrentCache& cache_;
	};

private:
	map_type map_;
	mutable threading::mutex mutex_;
};
