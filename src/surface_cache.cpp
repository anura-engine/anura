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

#include <cstdint>

#include "concurrent_cache.hpp"
#include "filesystem.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "surface_cache.hpp"

namespace graphics
{
	namespace
	{
		struct CacheEntry
		{
			KRE::SurfacePtr surf;
			std::string fname;
			int64_t mod_time;
		};

		typedef ConcurrentCache<std::string,CacheEntry> SurfaceMap;
		SurfaceMap& cache()
		{
			static SurfaceMap res;
			return res;
		}

		int64_t get_file_mod_time(const std::string& fn)
		{
			return sys::file_mod_time(fn);
		}

		const std::string image_path = ""; //"images/";
	}

	KRE::SurfacePtr SurfaceCache::get(const std::string& key, bool cache_surface, std::string* full_filename)
	{
		if(cache_surface) {
			KRE::SurfacePtr surf = cache().get(key).surf;
			if(surf == nullptr) {
				CacheEntry entry;
				surf = entry.surf = get(key, false);
				if(entry.fname.empty() == false) {
					entry.mod_time = get_file_mod_time(entry.fname);
				}
				cache().put(key, entry);
			}
			return surf;
		}

		std::string fname = image_path + key;
		KRE::SurfacePtr surf;
		if(key.empty() == false && key[0] == '#') {
			const std::string fname = std::string(preferences::user_data_path()) + "/tmp_images/" + std::string(key.begin()+1, key.end());
			surf = KRE::Surface::create(fname);
			if(full_filename) {
				*full_filename = fname;
			}
		} else if(sys::file_exists(key)) {
			surf = KRE::Surface::create(key);
			if(full_filename) {
				*full_filename = key;
			}
		} else {
			surf = KRE::Surface::create(module::map_file(fname));
			if(full_filename) {
				*full_filename = module::map_file(fname);
			}
		}

		if(surf == nullptr || surf->width() == 0) {
			if(key != "") {
				LOG_INFO("failed to load image '" << key << "'");
			}
			throw LoadImageError();
		}
	return surf;
	}

	void SurfaceCache::invalidateModified(std::vector<std::string>* keys_modified)
	{
		for(const auto& k : cache().getKeys()) {
			CacheEntry entry = cache().get(k);
			const int64_t mod_time = get_file_mod_time(entry.fname);
			if(mod_time != entry.mod_time) {
				cache().erase(k);
				if(keys_modified) {
					keys_modified->emplace_back(k);
				}
			}
		}
	}

	void SurfaceCache::clear()
	{
		cache().clear();
	}

}
