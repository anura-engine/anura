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

	SurfaceLock::SurfaceLock(SurfacePtr surface)
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
		if(!(flags & SurfaceFlags::NO_CACHE)) {
			auto it = get_surface_cache().find(filename);
			if(it != get_surface_cache().end()) {
				return it->second;
			}
			auto surface = std::get<0>(create_fn_tuple)(filename, fmt, flags, convert);
			get_surface_cache()[filename] = surface;
			surface->init();
			return surface;
		} 
		auto surf = std::get<0>(create_fn_tuple)(filename, fmt, flags, convert);
		surf->init();
		return surf;
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
		auto surf = std::get<1>(create_fn_tuple)(width, height, bpp, row_pitch, rmask, gmask, bmask, amask, pixels);
		surf->init();
		return surf;
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
		auto surf = std::get<2>(create_fn_tuple)(width, height, bpp, rmask, gmask, bmask, amask);
		surf->init();
		return surf;
	}

	SurfacePtr Surface::create(int width, int height, PixelFormat::PF fmt)
	{
		ASSERT_LOG(get_surface_creator().empty() == false, "No resources registered to create surfaces from pixel format.");
		auto create_fn_tuple = get_surface_creator().begin()->second;
		auto surf = std::get<3>(create_fn_tuple)(width, height, fmt);
		surf->init();
		return surf;
	}

	void Surface::init()
	{
		createAlphaMap();
	}

	void Surface::createAlphaMap()
	{
		const int npixels = width() * height();
		alpha_map_.resize(npixels);
		std::fill(alpha_map_.begin(), alpha_map_.end(), false);

		if(getPixelFormat()->hasAlphaChannel()) {
			int w = width();
			auto& am = alpha_map_;
			iterateOverSurface([&am, w](int x, int y, int r, int g, int b, int a) {
				am[x + y * w] = a == 0;
			});
		}
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

	Color Surface::getColorAt(int x, int y) const
	{
		int r, g, b, a;
		const int bpp = pf_->bytesPerPixel();
		const unsigned char* pix = reinterpret_cast<const unsigned char*>(pixels());
		const unsigned char* p = &pix[x * bpp + y * rowPitch()];
		switch(bpp) {
			case 1: pf_->getRGBA(*p, r, g, b, a); break;
			case 2: pf_->getRGBA(*reinterpret_cast<const unsigned short*>(p), r, g, b, a); break;
			case 3: pf_->getRGBA(*reinterpret_cast<const unsigned long*>(p), r, g, b, a); break;
			case 4: pf_->getRGBA(*reinterpret_cast<const unsigned long*>(p), r, g, b, a); break;
		}
		return Color(r, g, b, a);
	}

	color_histogram_type Surface::getColorHistogram(ColorCountFlags flags)
	{
		color_histogram_type res;
		iterateOverSurface([&res](int x, int y, int r, int g, int b, int a) {
			//Color color(r, g, b, a);
			color_histogram_type::key_type color = (static_cast<uint32_t>(r) << 24)
				| (static_cast<uint32_t>(g) << 16)
				| (static_cast<uint32_t>(b) << 8)
				| (static_cast<uint32_t>(a));
			//ASSERT_LOG(color >= 18446744072937384447UL, "ugh: " << r << "," << g << "," << b << "," << a);
			auto it = res.find(color);
			if(it == res.end()) {
				res[color] = 1;
			} else {
				it->second += 1;
			}
		});
		//for(auto px : *this) {
		//	Color color(px.red, px.green, px.blue, (flags & ColorCountFlags::IGNORE_ALPHA_VARIATIONS ? 255 : px.alpha));
		//	auto it = res.find(color);
		//	if(it == res.end()) {
		//		LOG_DEBUG("Adding color: " << px.red << "," << px.green << "," << px.blue);
		//		res[color] = 1;
		//	} else {
		//		it->second += 1;
		//	}
		//}
		return res;
	}

	unsigned Surface::getColorCount(ColorCountFlags flags)
	{
		return getColorHistogram(flags).size();
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
		case PixelFormat::PF::PIXELFORMAT_INDEX8:
			return true;
		default: break;
		}
		return false;
	}

	bool Surface::isAlpha(unsigned x, unsigned y) const
	{ 
		ASSERT_LOG(alpha_map_.size() > 0, "No alpha map found.");
		ASSERT_LOG(x + y* width() < static_cast<int>(alpha_map_.size()), "Index exceeds alpha map size.");
		return alpha_map_[y*width()+x]; 
	}

	std::vector<bool>::const_iterator Surface::getAlphaRow(int x, int y) const 
	{ 
		ASSERT_LOG(alpha_map_.size() > 0, "No alpha map found.");
		ASSERT_LOG(x + y* width() < static_cast<int>(alpha_map_.size()), "Index exceeds alpha map size.");
		return alpha_map_.begin() + y*width() + x; 
	}

	std::vector<bool>::const_iterator Surface::endAlpha() const 
	{ 
		return alpha_map_.end(); 
	}

	void Surface::iterateOverSurface(surface_iterator_fn fn)
	{
		iterateOverSurface(0, 0, width(), height(), fn);
	}

	void Surface::iterateOverSurface(rect r, surface_iterator_fn fn)
	{
		iterateOverSurface(r.x(), r.y(), r.h(), r.w(), fn);
	}

	void Surface::iterateOverSurface(int sx, int sy, int sw, int sh, surface_iterator_fn iterator_fn)
	{
		SurfaceLock lck(shared_from_this());
		auto pf = getPixelFormat();
		if(pf->getFormat() == PixelFormat::PF::PIXELFORMAT_INDEX1LSB 
			|| pf->getFormat() == PixelFormat::PF::PIXELFORMAT_INDEX1MSB 
			|| pf->getFormat() == PixelFormat::PF::PIXELFORMAT_INDEX4LSB 
			|| pf->getFormat() == PixelFormat::PF::PIXELFORMAT_INDEX4MSB) {
			int cnt = (pf->getFormat() == PixelFormat::PF::PIXELFORMAT_INDEX1LSB || pf->getFormat() == PixelFormat::PF::PIXELFORMAT_INDEX1MSB) ? 8 : 2;
			for(int y = sy; y != sh; ++y) {
				for(int x = sx; x != sw; ++x) {
					for(int n = 0; n != cnt; ++n) {
						int red = 0, green = 0, blue = 0, alpha = 0;
						const uint8_t* px = &reinterpret_cast<const uint8_t*>(pixels())[x * bytesPerPixel() + y * rowPitch()];
						pf->extractRGBA(px, n, red, green, blue, alpha);
						iterator_fn(x, y, red, green, blue, alpha);
					}
				}
			}
		} else {
			for(int y = sy; y != sh; ++y) {
				for(int x = sx; x != sw; ++x) {
					int red = 0, green = 0, blue = 0, alpha = 0;
					const uint8_t* px = &reinterpret_cast<const uint8_t*>(pixels())[x * bytesPerPixel() + y * rowPitch()];
					pf->extractRGBA(px, 0, red, green, blue, alpha);
					iterator_fn(x, y, red, green, blue, alpha);
				}
			}
		}
	}
}

