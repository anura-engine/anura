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

#include <future>
#include <thread>
#include <tuple>

#include "Surface.hpp"
#include "stb_rect_pack.h"

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

		unsigned get_next_id()
		{
			static unsigned id = 1;
			return id++;
		}

		int alpha_strip_threshold = 20;	// 20/255 ~ 7.8%

		const int max_surface_width = 4096;
		const int max_surface_height = 4096;
	}

	Surface::Surface()
		: flags_(SurfaceFlags::NONE),
		  name_(),
		  id_(get_next_id())
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
			surface->name_ = filename;
			get_surface_cache()[filename] = surface;
			surface->init();
			return surface;
		} 
		auto surf = std::get<0>(create_fn_tuple)(filename, fmt, flags, convert);
		surf->name_ = filename;
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
		std::stringstream ss;
		ss << "Surface(" << width << "," << height << "," << bpp << "," << row_pitch << "," << rmask << "," << gmask << "," << amask << ", has data:yes)";
		surf->name_ = ss.str();
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
		std::stringstream ss;
		ss << "Surface(" << width << "," << height << "," << bpp << "," << rmask << "," << gmask << "," << amask << ", has data:no)";
		surf->name_ = ss.str();
		return surf;
	}

	SurfacePtr Surface::create(int width, int height, PixelFormat::PF fmt)
	{
		ASSERT_LOG(get_surface_creator().empty() == false, "No resources registered to create surfaces from pixel format.");
		auto create_fn_tuple = get_surface_creator().begin()->second;
		auto surf = std::get<3>(create_fn_tuple)(width, height, fmt);
		std::stringstream ss;
		ss << "Surface(" << width << "," << height << "," << static_cast<int>(fmt) << ")";
		surf->name_ = ss.str();
		return surf;
	}

	void Surface::init()
	{
		createAlphaMap();
		if(flags_ & SurfaceFlags::STRIP_ALPHA_BORDERS) {
			stripAlphaBorders(alpha_strip_threshold);
		}
	}

	void Surface::createAlphaMap()
	{
		const int npixels = width() * height();
		alpha_map_ = std::make_shared<std::vector<bool>>();
		alpha_map_->resize(npixels);

		if(getPixelFormat()->hasAlphaChannel()) {
			SurfaceLock lck(shared_from_this());
			auto pf = getPixelFormat();
			uint32_t alpha_mask = pf->getAlphaMask();
			if(bytesPerPixel() == 4 && rowPitch() % 4 == 0) {
				// Optimization for a common case. Operates ~25 faster than default case.
				const uint32_t* px = reinterpret_cast<const uint32_t*>(pixels());
				auto it = alpha_map_->begin();
				for(int n = 0; n != npixels; ++n) {
					*it++ = (*px++ & alpha_mask) ? false : true;
				}
			} else {
				std::fill(alpha_map_->begin(), alpha_map_->end(), false);
				int w = width();
				auto& am = *alpha_map_;
				iterateOverSurface([&am, w](int x, int y, int r, int g, int b, int a) {
					if(a == 0) {
						am[x + y * w] = true;
					}
				});
			}
		}

		//if(getPixelFormat()->hasAlphaChannel()) {
			//int w = width();
			//auto& am = alpha_map_;
			//iterateOverSurface([&am, w](int x, int y, int r, int g, int b, int a) {
			//	am[x + y * w] = a == 0 ? true : false;
			//});
		//}
	}

	void Surface::stripAlphaBorders(int threshold)
	{
		if(getPixelFormat()->hasAlphaChannel()) {
			const int w = width();
			const int h = height();
			SurfaceLock lck(shared_from_this());
			auto pf = getPixelFormat();
			uint32_t alpha_mask = pf->getAlphaMask();
			int alpha_shift = pf->getAlphaShift();
			threshold <<= alpha_shift;
			if(bytesPerPixel() == 4 && rowPitch() % 4 == 0) {
				// Optimization for a common case. Operates ~25 faster than default case.
				const int num_pixels = w * h;
				// top border
				uint32_t const* pxt = reinterpret_cast<const uint32_t*>(pixels());
				for(int ndx = 0; ndx < num_pixels; ++ndx) {
					if((*pxt++ & alpha_mask) > static_cast<uint32_t>(threshold)) {
						alpha_borders_[1] = ndx / w;
						break;
					}
				}
				const uint32_t* pxb = &reinterpret_cast<const uint32_t*>(pixels())[w * h];
				// bottom border
				for(int ndx = num_pixels - 1; ndx >= 0; --ndx) {
					if((*(--pxb) & alpha_mask) > static_cast<uint32_t>(threshold)) {
						alpha_borders_[3] = h - 1 - ndx / w;
						break;
					}
				}
				// left border
				const uint32_t* pxl = reinterpret_cast<const uint32_t*>(pixels());
				bool finished = false;
				for(int x = 0; x < w && !finished; ++x) {
					for(int y = 0; y < h && !finished; ++y) {
						if((pxl[x + y*w] & alpha_mask) > static_cast<uint32_t>(threshold)) {
							alpha_borders_[0] = x;
							finished = true;
						}
					}
				}
				// right border
				const uint32_t* pxr = reinterpret_cast<const uint32_t*>(pixels());
				finished = false;
				for(int x = w - 1; x >= 0 && !finished; --x) {
					for(int y = 0; y < h && !finished; ++y) {
						if((pxr[x + y*w] & alpha_mask) > static_cast<uint32_t>(threshold)) {
							alpha_borders_[2] = w - 1 - x;
							finished = true;
						}
					}
				}
			} else {
				ASSERT_LOG(false, "won't apply stripAlphaBorders to non 32-bit RGBA image");
			}
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
		const uint8_t* pix = reinterpret_cast<const uint8_t*>(pixels());
		const uint8_t* p = &pix[x * bpp + y * rowPitch()];
		switch(bpp) {
			case 1: pf_->getRGBA(*p, r, g, b, a); break;
			case 2: pf_->getRGBA(*reinterpret_cast<const uint16_t*>(p), r, g, b, a); break;
			case 3: pf_->getRGBA(*reinterpret_cast<const uint32_t*>(p), r, g, b, a); break;
			case 4: pf_->getRGBA(*reinterpret_cast<const uint32_t*>(p), r, g, b, a); break;
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

	size_t Surface::getColorCount(ColorCountFlags flags)
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
			static auto null_filter = [](const std::string& s) { return s; };
			return null_filter;
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
		ASSERT_LOG(alpha_map_ != nullptr && alpha_map_->size() > 0, "No alpha map found.");
		ASSERT_LOG(x + y* width() < static_cast<int>(alpha_map_->size()), "Index exceeds alpha map size.");
		return (*alpha_map_)[y*width()+x]; 
	}

	std::vector<bool>::const_iterator Surface::getAlphaRow(int x, int y) const 
	{ 
		ASSERT_LOG(alpha_map_ != nullptr && alpha_map_->size() > 0, "No alpha map found.");
		ASSERT_LOG(x + y* width() < static_cast<int>(alpha_map_->size()), "Index exceeds alpha map size.");
		return alpha_map_->begin() + y * width() + x; 
	}

	std::vector<bool>::const_iterator Surface::endAlpha() const 
	{ 
		return alpha_map_->end(); 
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

	int Surface::setAlphaStripThreshold(int threshold)
	{
		int old_thr = alpha_strip_threshold;
		alpha_strip_threshold = threshold;
		return old_thr;
	}

	int Surface::getAlphaStripThreshold()
	{
		return alpha_strip_threshold;
	}

#include "profile_timer.hpp"

	SurfacePtr Surface::packImages(const std::vector<std::string>& filenames, std::vector<rect>* outr, std::vector<std::array<int, 4>>* borders)
	{
		profile::manager pman("fit rects");

		const int max_threads = 8;

		SurfaceFlags flags = SurfaceFlags::NO_CACHE;
		if(borders != nullptr) {
			flags = flags | SurfaceFlags::STRIP_ALPHA_BORDERS;
		}

		std::vector<std::future<void>> futures;

		std::vector<SurfacePtr> images;
		images.resize(filenames.size());
		const int n_incr = images.size() / max_threads;
		for(int n = 0; n < static_cast<int>(images.size()); n += n_incr) {
			int n2 = n + n_incr > static_cast<int>(images.size()) ? images.size() : n + n_incr;
			futures.push_back(std::async([&images, n, n2, &filenames, flags]() {
				for(int ndx = n; ndx != n2; ++ndx) {
					images[ndx] = Surface::create(filenames[ndx], flags);
				}
			}));
		}
		for(auto& f : futures) {
			f.get();
		}

		std::vector<stbrp_node> nodes;
		nodes.resize(max_surface_width);

		const int increment = 128;
		int width = 256;
		int height = 256;

		int nn = 0;

		std::vector<stbrp_rect> rects;
		for(auto& img : images) {
			stbrp_rect r;
			r.id = rects.size();
			r.w = img->width();
			r.h = img->height();
			if(borders != nullptr) {
				r.w -= img->getAlphaBorders()[0] + img->getAlphaBorders()[2];
				r.h -= img->getAlphaBorders()[1] + img->getAlphaBorders()[3];
			}
			rects.emplace_back(r);
		}

		bool packed = false;
		while(!packed) {
			for(auto& r : rects) {
				r.x = r.y = r.was_packed = 0;
			}
			stbrp_context context;
			stbrp_init_target(&context, width, height, nodes.data(), nodes.size());
			stbrp_pack_rects(&context, rects.data(), rects.size());

			packed = true;
			for(auto& r : rects) {
				if(!r.was_packed) {
					if(nn & 1) {
						height += increment;
					} else {
						width += increment;
					}
					++nn;
					if(width > max_surface_width || height > max_surface_height) {
						return nullptr;
					}
					packed = false;
					break;
				}
			}
			
		}

		if(borders != nullptr) {
			borders->resize(images.size());
		}
		outr->resize(images.size());

		auto out = Surface::create(width, height, PixelFormat::PF::PIXELFORMAT_RGBA8888);
		for(auto& r : rects) {
			(*outr)[r.id] = rect(r.x, r.y, r.w, r.h);
			rect alpha_borders(0, 0, r.w, r.h);
			if(borders != nullptr) {
				alpha_borders = rect(images[r.id]->getAlphaBorders()[0], images[r.id]->getAlphaBorders()[1], r.w, r.h);
			}
			out->blitTo(images[r.id], alpha_borders, (*outr)[r.id]);
			if(borders != nullptr) {
				(*borders)[r.id] = images[r.id]->getAlphaBorders();
			}
		}
		return out;
	}
}
