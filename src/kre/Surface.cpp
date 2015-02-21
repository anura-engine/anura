/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <tuple>
#include <unordered_map>

#include "Surface.hpp"

namespace KRE
{
	namespace
	{
		typedef std::map<std::string,std::tuple<SurfaceCreatorFileFn,SurfaceCreatorPixelsFn,SurfaceCreatorMaskFn,SurfaceCreatorFormatFn>> CreatorMap;
		CreatorMap& get_surface_creator()
		{
			static CreatorMap res;
			return res;
		}

		typedef std::map<std::string, SurfacePtr> SurfaceCacheType;
		SurfaceCacheType& get_surface_cache()
		{
			static SurfaceCacheType res;
			return res;
		}
	}

	Surface::Surface()
		: flags_(SurfaceFlags::NONE)
	{
	}

	Surface::~Surface()
	{
	}

	PixelFormatPtr Surface::getPixelFormat()
	{
		return pf_;
	}

	void Surface::setPixelFormat(PixelFormatPtr pf)
	{
		pf_ = pf;
	}

	SurfaceLock::SurfaceLock(const SurfacePtr& surface)
		: surface_(surface)
	{
		surface_->lock();
	}

	SurfaceLock::~SurfaceLock()
	{
		surface_->unlock();
	}

	SurfacePtr Surface::convert(PixelFormat::PF fmt, SurfaceConvertFn convert)
	{
		return handleConvert(fmt, convert);
	}

	void Surface::convertInPlace(PixelFormat::PF fmt, SurfaceConvertFn convert)
	{
		auto surf = handleConvert(fmt, convert);
		*this = *surf.get();
	}

	bool Surface::registerSurfaceCreator(const std::string& name, 
		SurfaceCreatorFileFn file_fn, 
		SurfaceCreatorPixelsFn pixels_fn, 
		SurfaceCreatorMaskFn mask_fn,
		SurfaceCreatorFormatFn format_fn)
	{
		return get_surface_creator().insert(std::make_pair(name,std::make_tuple(file_fn, pixels_fn, mask_fn, format_fn))).second;
	}

	void Surface::unRegisterSurfaceCreator(const std::string& name)
	{
		auto it = get_surface_creator().find(name);
		ASSERT_LOG(it != get_surface_creator().end(), "Unable to find surface creator: " << name);
		get_surface_creator().erase(it);
	}

	SurfacePtr Surface::create(const std::string& filename, SurfaceFlags flags, PixelFormat::PF fmt, SurfaceConvertFn convert)
	{
		ASSERT_LOG(get_surface_creator().empty() == false, "No resources registered to surfaces images from files.");
		auto create_fn_tuple = get_surface_creator().begin()->second;
		if(flags & SurfaceFlags::NO_CACHE) {
			auto it = get_surface_cache().find(filename);
			if(it != get_surface_cache().end()) {
				return it->second;
			}
			auto surface = std::get<0>(create_fn_tuple)(filename, fmt, flags, convert);
			get_surface_cache()[filename] = surface;
			return surface;
		} 
		return std::get<0>(create_fn_tuple)(filename, fmt, flags, convert);
	}

	SurfacePtr Surface::create(int width, 
		int height, 
		int bpp, 
		int row_pitch, 
		uint32_t rmask, 
		uint32_t gmask, 
		uint32_t bmask, 
		uint32_t amask, 
		const void* pixels)
	{
		// XXX no caching as default?
		ASSERT_LOG(get_surface_creator().empty() == false, "No resources registered to create surfaces from pixels.");
		auto create_fn_tuple = get_surface_creator().begin()->second;
		return std::get<1>(create_fn_tuple)(width, height, bpp, row_pitch, rmask, gmask, bmask, amask, pixels);
	}

	SurfacePtr Surface::create(int width, 
		int height, 
		int bpp, 
		uint32_t rmask, 
		uint32_t gmask, 
		uint32_t bmask, 
		uint32_t amask)
	{
		// XXX no caching as default?
		ASSERT_LOG(get_surface_creator().empty() == false, "No resources registered to create surfaces from masks.");
		auto create_fn_tuple = get_surface_creator().begin()->second;
		return std::get<2>(create_fn_tuple)(width, height, bpp, rmask, gmask, bmask, amask);
	}

	SurfacePtr Surface::create(int width, int height, PixelFormat::PF fmt)
	{
		ASSERT_LOG(get_surface_creator().empty() == false, "No resources registered to create surfaces from pixel format.");
		auto create_fn_tuple = get_surface_creator().begin()->second;
		return std::get<3>(create_fn_tuple)(width, height, fmt);
	}

	void Surface::clearSurfaceCache()
	{
		resetSurfaceCache();
	}

	void Surface::resetSurfaceCache()
	{
		get_surface_cache().clear();
	}

	void Surface::fillRect(const rect& dst_rect, const Color& color)
	{
		// XXX do we need to consider ARGB/RGBA ordering issues here.
		ASSERT_LOG(dst_rect.x1() >= 0 && dst_rect.x1() <= width(), "destination co-ordinates out of bounds: " << dst_rect.x1() << " : (0," << width() << ")");
		ASSERT_LOG(dst_rect.x2() >= 0 && dst_rect.x2() <= width(), "destination co-ordinates out of bounds: " << dst_rect.x2() << " : (0," << width() << ")");
		ASSERT_LOG(dst_rect.y1() >= 0 && dst_rect.y1() <= height(), "destination co-ordinates out of bounds: " << dst_rect.y1() << " : (0," << height() << ")");
		ASSERT_LOG(dst_rect.y2() >= 0 && dst_rect.y2() <= height(), "destination co-ordinates out of bounds: " << dst_rect.y2() << " : (0," << height() << ")");
		unsigned char* pix = reinterpret_cast<unsigned char*>(pixelsWriteable());
		const int bpp = pf_->bytesPerPixel();
		for(int y = dst_rect.x1(); y < dst_rect.x2(); ++y) {
			for(int x = dst_rect.y1(); x < dst_rect.y2(); ++x) {
				unsigned char* p = &pix[(y * width() + x) * bpp];
				// XXX FIXME
				//uint32_t pixels;
				//pf_->encodeRGBA(&pixels, color.r(), color.g(), color.b(), color.a());
				switch(bpp) {
					case 1: p[0] = color.r_int(); break;
					case 2: p[0] = color.r_int(); p[1] = color.g_int(); break;
					case 3: p[0] = color.r_int(); p[1] = color.g_int(); p[2] = color.b_int(); break;
					case 4: p[0] = color.r_int(); p[1] = color.g_int(); p[2] = color.b_int(); p[3] = color.a_int(); break;
				}
			}
		}		
	}

	// Actually creates a histogram of colors.
	// Could be used for other things.
	unsigned Surface::getColorCount(ColorCountFlags flags)
	{
		const unsigned char* pix = reinterpret_cast<const unsigned char*>(pixels());
		const int bpp = pf_->bytesPerPixel();
		std::unordered_map<uint32_t,uint32_t> color_list;
		for(int y = 0; y < height(); ++y) {
			for(int x = 0; x < width(); ++x) {
				const unsigned char* p = pix;
				int r, g, b, a;
				switch(bpp) {
					case 1: pf_->getRGBA(*p, r, g, b, a); break;
					case 2: pf_->getRGBA(*reinterpret_cast<const unsigned short*>(p), r, g, b, a); break;
					case 3: pf_->getRGBA(*reinterpret_cast<const unsigned long*>(p), r, g, b, a); break;
					case 4: pf_->getRGBA(*reinterpret_cast<const unsigned long*>(p), r, g, b, a); break;
				}
				uint32_t col = (r << 24) | (g << 16) | (b << 8) | (flags & ColorCountFlags::IGNORE_ALPHA_VARIATIONS ? 255 : a);
				if(color_list.find(col) == color_list.end()) {
					color_list[col] = 0;
				} else {
					color_list[col]++;
				}
				p += bpp;
			}
			pix += rowPitch();
		}
		return color_list.size();
	}

	namespace 
	{
		std::map<FileFilterType, file_filter>& get_file_filter_map()
		{
			static std::map<FileFilterType, file_filter> res;
			return res;
		}

		alpha_filter alpha_filter_fn = nullptr;
	}

	void Surface::setFileFilter(FileFilterType type, file_filter fn)
	{
		get_file_filter_map()[type] = fn;
	}

	file_filter Surface::getFileFilter(FileFilterType type)
	{
		auto it = get_file_filter_map().find(type);
		if(it == get_file_filter_map().end()) {
			return [](const std::string& s) { return s; };
		}
		return it->second;
	}

	void Surface::setAlphaFilter(alpha_filter fn)
	{
		alpha_filter_fn = fn;
	}

	alpha_filter Surface::getAlphaFilter()
	{
		return alpha_filter_fn;
	}

	void Surface::clearAlphaFilter()
	{
		alpha_filter_fn = nullptr;
	}

	SurfaceIterator::SurfaceIterator(SurfacePtr surface) 
		: surface_(surface),
		  x_(0),
		  y_(0),
		  index_(0),
		  pixels_(nullptr)
	{
		pixels_ = reinterpret_cast<const unsigned char*>(surface_->pixels());
	}

	SurfaceIterator::SurfaceIterator()
		: surface_(nullptr),
		  x_(-1),
		  y_(-1),
		  index_(0),
		  pixels_(nullptr)
	{
	}


	SimpleColor SurfaceIterator::dereference() const 
	{
		SurfaceLock lck(surface_);
		SimpleColor res;
		int offs = y_ * surface_->rowPitch() + x_ * surface_->getPixelFormat()->bytesPerPixel();
		std::tie(offs, index_incr_) = surface_->getPixelFormat()->extractRGBA(&pixels_[offs], index_, res.red, res.green, res.blue, res.alpha);
		res.x = x_;
		res.y = y_;
		return res;
	}

	bool SurfaceIterator::equal(SurfaceIterator const& other) const 
	{
		return x_ == other.x_ && y_ == other.y_;
	}

	void SurfaceIterator::increment() 
	{
		if(index_incr_ != 0) {
			index_ = index_incr_;
		} else {
			if(++x_ >= surface_->width()) {
				if(++y_ >= surface_->height()) {
					// indicate we reached the end of the surface.
					y_ = x_ = -1;
				} else {
					// reset x to start new row.
					x_ = 0;
				}
			}
		}
	}

	void SurfaceIterator::decrement() 
	{
		if(--x_ <= 0) {
			if(--y_ <= 0) {
				y_ = x_ = 0;
			} else {
				x_ = surface_->width() - 1;
			}
		}
	}

	void SurfaceIterator::advance(std::ptrdiff_t n) 
	{
		x_ += n;
		while(x_ >= surface_->width()) {
			if(++y_ >= surface_->height()) {
				// indicate we reached the end of the surface.
				y_ = x_ = -1;
				break;
			} else {
				x_ -= surface_->width();
			}			
		}
	}

	PixelFormat::PixelFormat()
	{
	}

	PixelFormat::~PixelFormat()
	{
	}

	bool PixelFormat::isIndexedFormat(PixelFormat::PF pf)
	{
		switch(pf) {
		case PixelFormat::PF::PIXELFORMAT_INDEX1LSB:
		case PixelFormat::PF::PIXELFORMAT_INDEX1MSB:
		case PixelFormat::PF::PIXELFORMAT_INDEX4LSB:
		case PixelFormat::PF::PIXELFORMAT_INDEX4MSB:
			return true;
		default: break;
		}
		return false;
	}
}
