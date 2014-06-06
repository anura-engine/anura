/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "Surface.hpp"

namespace KRE
{
	namespace
	{
		typedef std::map<std::string,SurfaceCreatorFn> CreatorMap;
		CreatorMap& get_surface_creator()
		{
			static CreatorMap res;
			return res;
		}

		typedef std::map<std::string, SurfacePtr> surface_cache_type;
		surface_cache_type& get_surface_cache()
		{
			static surface_cache_type res;
			return res;
		}
	}

	Surface::Surface()
	{
	}

	Surface::~Surface()
	{
	}

	PixelFormatPtr Surface::GetPixelFormat()
	{
		return pf_;
	}

	void Surface::SetPixelFormat(PixelFormatPtr pf)
	{
		pf_ = pf;
	}

	SurfaceLock::SurfaceLock(const SurfacePtr& surface)
		: surface_(surface)
	{
		surface_->Lock();
	}

	SurfaceLock::~SurfaceLock()
	{
		surface_->Unlock();
	}

	SurfacePtr Surface::Convert(PixelFormat::PF fmt, SurfaceConvertFn convert)
	{
		return HandleConvert(fmt, convert);
	}

	bool Surface::RegisterSurfaceCreator(const std::string& name, SurfaceCreatorFn Creator)
	{
		return get_surface_creator().insert(std::make_pair(name, Creator)).second;
	}

	void Surface::UnRegisterSurfaceCreator(const std::string& name)
	{
		auto it = get_surface_creator().find(name);
		ASSERT_LOG(it != get_surface_creator().end(), "Unable to find surface creator: " << name);
		get_surface_creator().erase(it);
	}

	SurfacePtr Surface::Create(const std::string& filename, bool no_cache, PixelFormat::PF fmt, SurfaceConvertFn convert)
	{
		ASSERT_LOG(get_surface_creator().empty() == false, "No resources registered to create images from files.");
		if(!no_cache) {
			auto it = get_surface_cache().find(filename);
			if(it != get_surface_cache().end()) {
				return it->second;
			}
			auto surface = get_surface_creator().begin()->second(filename, fmt, convert);
			get_surface_cache()[filename] = surface;
			return surface;
		} 
		return get_surface_creator().begin()->second(filename, fmt, convert);
	}

	void Surface::ResetSurfaceCache()
	{
		get_surface_cache().clear();
	}

	PixelFormat::PixelFormat()
	{
	}

	PixelFormat::~PixelFormat()
	{
	}
}
