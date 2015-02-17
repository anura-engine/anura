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

#if defined(_MSC_VER) && _MSC_VER < 1800
#include <boost/math/special_functions/round.hpp>
using boost::math::round;
#else
#include <cmath>
using std::round;
#endif

#include <set>
#include "asserts.hpp"
#include "DisplayDevice.hpp"
#include "Texture.hpp"
#include "TextureUtils.hpp"

namespace KRE
{
	namespace 
	{
		typedef std::set<Texture*> TextureRegistryType;
		TextureRegistryType& texture_registry()
		{
			static TextureRegistryType res;
			return res;
		}

		void add_to_texture_registry(Texture* tex) 
		{
			texture_registry().insert(tex);
		}

		void remove_from_texture_registery(Texture* tex)
		{
			auto it = texture_registry().find(tex);
			ASSERT_LOG(it != texture_registry().end(), "tried to erase texture from registry that doesn't exist");
			texture_registry().erase(it);
		}
	}

	Texture::Texture(const variant& node, const std::vector<SurfacePtr>& surfaces)
		: type_(Type::TEXTURE_2D), 
		  mipmaps_(0), 
		  max_anisotropy_(1),
		  lod_bias_(0.0f),
		  surface_width_(-1),
		  surface_height_(-1),
		  surfaces_(surfaces),
		  width_(0),
		  height_(0),
		  depth_(0),
		  unpack_alignment_(4),
		  src_rect_(),
		  src_rect_norm_(0.0f, 0.0f, 1.0f, 1.0f)
	{
		if(surfaces_.size() == 0 && node.is_string()) {
			surfaces_.emplace_back(Surface::create(node.as_string()));
		} else if(surfaces_.size() == 0 && node.has_key("image") && node["image"].is_string()) {
			surfaces_.emplace_back(Surface::create(node["image"].as_string()));
		}
		ASSERT_LOG(surfaces_.size() > 0, "Error no surface.");
		surface_width_ = surfaces_.front()->width();
		surface_height_ = surfaces_.front()->height();

		internalInit();
		if(node.has_key("type")) {
			const std::string& type = node["type"].as_string();
			if(type == "1d") {
				type_ = Type::TEXTURE_1D;
			} else if(type == "2d") {
				type_ = Type::TEXTURE_2D;
			} else if(type == "3d") {
				type_ = Type::TEXTURE_3D;
			} else if(type == "cubic") {
				type_ = Type::TEXTURE_CUBIC;
			} else {
				ASSERT_LOG(false, "Unrecognised texture type '" << type << "'. Valid values are 1d,2d,3d and cubic.");
			}
		}
		if(node.has_key("mipmaps")) {
			ASSERT_LOG(node["mipmaps"].is_int(), "'mipmaps' not an integer type, found: " << node["mipmaps"].to_debug_string());
			mipmaps_ = int(node["mipmaps"].as_int());
		}
		if(node.has_key("lod_bias")) {
			ASSERT_LOG(node["lod_bias"].is_numeric(), "'lod_bias' not a numeric type, found: " << node["lod_bias"].to_debug_string());
			lod_bias_ = node["lod_bias"].as_float();
		}
		if(node.has_key("max_anisotropy")) {
			ASSERT_LOG(node["max_anisotropy"].is_int(), "'max_anisotropy' not an integer type, found: " << node["max_anisotropy"].to_debug_string());
			max_anisotropy_ = int(node["max_anisotropy"].as_int());
		}
		if(node.has_key("filtering")) {
			if(node["filtering"].is_string()) {
				const std::string& filtering = node["filtering"].as_string();
				if(filtering == "none") {
					filtering_[0] = Filtering::POINT;
					filtering_[1] = Filtering::POINT;
					filtering_[2] = Filtering::NONE;
				} else if(filtering == "bilinear") {
					filtering_[0] = Filtering::LINEAR;
					filtering_[1] = Filtering::LINEAR;
					filtering_[2] = Filtering::POINT;
				} else if(filtering == "trilinear") {
					filtering_[0] = Filtering::LINEAR;
					filtering_[1] = Filtering::LINEAR;
					filtering_[2] = Filtering::LINEAR;
				} else if(filtering == "anisotropic") {
					filtering_[0] = Filtering::ANISOTROPIC;
					filtering_[1] = Filtering::ANISOTROPIC;
					filtering_[2] = Filtering::LINEAR;
				} else {
					ASSERT_LOG(false, "'filtering' must be either 'none','bilinear','trilinear' or 'anisotropic'. Found: " << filtering);
				}
			} else if(node["filtering"].is_list()) {
				size_t list_size = node["filtering"].num_elements();
				ASSERT_LOG(list_size == 3, "Size of list for 'filtering' attribute must be 3 elements. Found: " << list_size);
				for(size_t n = 0; n != 3; ++n) {
					ASSERT_LOG(node["filtering"][n].is_string(), "Element " << n << " of filtering is not a string: " << node["filtering"][0].to_debug_string());
					const std::string& f = node["filtering"][n].as_string();
					if(f == "none") {
						filtering_[n] = Filtering::NONE;
					} else if(f == "point") {
						filtering_[n] = Filtering::POINT;
					} else if(f == "linear") {
						filtering_[n] = Filtering::LINEAR;
					} else if(f == "anisotropic") {
						filtering_[n] = Filtering::ANISOTROPIC;
					} else {
						ASSERT_LOG(false, "Filtering element(" << n << ") invalid: " << f);
					}
				}
			} else {
				ASSERT_LOG(false, "'filtering' must be either a string value or list of strings. Found: " << node["filtering"].to_debug_string());
			}
		}
		if(node.has_key("address_mode")) {
			if(node["address_mode"].is_list()) {
				size_t list_size = node["address_mode"].num_elements();
				ASSERT_LOG(list_size >= 1 && list_size <= 3, "Size of list for 'address_mode' attribute must be between 1 and 3 elements. Found: " << list_size);
				size_t n = 0;
				for(; n != list_size; ++n) {
					ASSERT_LOG(node["address_mode"][n].is_string(), "Element " << n << " of 'address_mode' attribute is not a string: " << node["address_mode"][0].to_debug_string());
					const std::string& am = node["address_mode"][n].as_string();
					if(am == "wrap") {
						address_mode_[n] = AddressMode::WRAP;
					} else if(am == "clamp") {
						address_mode_[n] = AddressMode::CLAMP;
					} else if(am == "mirror") {
						address_mode_[n] = AddressMode::MIRROR;
					} else if(am == "border") {
						address_mode_[n] = AddressMode::BORDER;
					} else {
						ASSERT_LOG(false, "address_mode element(" << n << ") invalid: " << am);
					}
				}
				for(; n < 3; ++n) {
					address_mode_[n] = AddressMode::WRAP;
				}
			} else {
				ASSERT_LOG(false, "'filtering' must be a list of strings. Found: " << node["filtering"].to_debug_string());
			}
		}
		if(node.has_key("border_color")) {
			border_color_ = Color(node["border_color"]);
		}
		if(node.has_key("rect")) {
			ASSERT_LOG(node["rect"].is_list(), "'rect' attribute must be a list of numbers.");
			ASSERT_LOG(node["rect"].num_elements() >= 4, "'rect' attribute must have at least 4 elements.");
			setSourceRect(rect(node["rect"]));
		}
	}

	Texture::Texture(const std::vector<SurfacePtr>& surfaces, Type type, int mipmap_levels)
		: type_(type), 
		  mipmaps_(mipmap_levels), 
		  max_anisotropy_(1),
		  lod_bias_(0.0f),
		  surfaces_(surfaces),
		  surface_width_(-1),
		  surface_height_(-1),
		  width_(0),
		  height_(0),
		  depth_(0),
		  unpack_alignment_(4),
		  src_rect_(),
		  src_rect_norm_(0.0f, 0.0f, 1.0f, 1.0f)
	{
		surface_width_ = surfaces.front()->width();
		surface_height_ = surfaces.front()->height();
		for(auto s : surfaces) {
			ASSERT_LOG(surface_width_ == s->width(), "Surface width didn't match. When creating a multi-surface texture all the surface widths must be the same. May relax this requirement in the future.");
			ASSERT_LOG(surface_height_ == s->height(), "Surface height didn't match. When creating a multi-surface texture all the surface heights must be the same. May relax this requirement in the future.");
		}
		internalInit();
	}

	Texture::Texture(int count, 
		int width, 
		int height, 
		int depth,
		PixelFormat::PF fmt, 
		Texture::Type type)
		: type_(type), 
		  mipmaps_(0), 
		  max_anisotropy_(1),
		  lod_bias_(0.0f),
		  surface_width_(width),
		  surface_height_(height),
		  width_(width),
		  height_(height),
		  depth_(depth),
		  unpack_alignment_(4),
		  src_rect_(),
		  src_rect_norm_(0.0f, 0.0f, 1.0f, 1.0f)
	{
		internalInit();
	}

	Texture::Texture(const SurfacePtr& surf, const SurfacePtr& palette)
		: type_(Type::TEXTURE_2D), 
		  mipmaps_(0), 
		  max_anisotropy_(1),
		  lod_bias_(0.0f),
		  surface_width_(surf->width()),
		  surface_height_(surf->height()),
		  width_(0),
		  height_(0),
		  depth_(0),
		  unpack_alignment_(4),
		  src_rect_(),
		  src_rect_norm_(0.0f, 0.0f, 1.0f, 1.0f)
	{
		ASSERT_LOG(false, "Write the texture palette code");
		internalInit();	
	}
	
	Texture::~Texture()
	{
		remove_from_texture_registery(this);
	}

	void Texture::internalInit()
	{
		add_to_texture_registry(this);

		for(auto& am : address_mode_) {
			am = AddressMode::CLAMP;
		}
		filtering_[0] = Filtering::POINT;
		filtering_[1] = Filtering::POINT;
		filtering_[2] = Filtering::NONE;

		// XXX For reasons (i.e. some video cards are problematic either hardware/drivers)
		// we are forced to use power-of-two textures anyway if we want mip-mapping and
		// address modes other than CLAMP.
		if(!DisplayDevice::checkForFeature(DisplayDeviceCapabilties::NPOT_TEXTURES)) {
			width_ = next_power_of_two(surface_width_);
			height_ = next_power_of_two(surface_height_);
			depth_ = 0;
		} else {
			width_ = surface_width_;
			height_ = surface_height_;
			depth_ = 0;
		}

		src_rect_ = rect(0, 0, width_, height_);

		const int npixels = width_ * height_;
		alpha_map_.resize(surfaces_.size());
		int n = 0;
		for(auto& s : surfaces_) {
			alpha_map_[n].resize(npixels);
			// surfaces with zero for the alpha mask have no alpha channel
			if(s && s->getPixelFormat()->hasAlphaChannel()) {
				auto fmt = s->getPixelFormat();
				const unsigned char* pixels = reinterpret_cast<const unsigned char*>(s->pixels());
				for(int y = 0; y != height_; ++y) {
					const unsigned char* pix = pixels;
					for(int x = 0; x != width_; ++x) {
						bool alpha = false;
						uint32_t pixel_value;
						switch(fmt->bytesPerPixel()) {
						// XXX - these probably need an endian-ness test. 
						// Or moved to Surface.
						case 1: pixel_value = pix[0]; break;						
						case 2: pixel_value = (pix[1] << 8) | pix[0]; break;
						case 4: pixel_value = (pix[3] << 24) | (pix[2] << 16) | (pix[1] << 8) | pix[0]; break;
						}
						auto alpha_value = (((pixel_value & fmt->getAlphaMask()) >> fmt->getAlphaShift()) << fmt->getAlphaLoss());
						alpha_map_[n][x+y*width_] = alpha_value == 0;
						pix += fmt->bytesPerPixel();
					}
					pixels += s->rowPitch();
				}

			} else {
				for(int y = 0; y != height_; ++y) {
					for(int x = 0; x != width_; ++x) {
						alpha_map_[n][x + y*width_] = false;
					}
				}
			}
			++n;
		}
	}

	void Texture::setAddressModes(Texture::AddressMode u, Texture::AddressMode v, Texture::AddressMode w, const Color& bc)
	{
		address_mode_[0] = u;
		address_mode_[1] = v;
		address_mode_[2] = w;
		border_color_ = bc;
		init();
	}

	void Texture::setAddressModes(const Texture::AddressMode uvw[3], const Color& bc)
	{
		for(int n = 0; n < 3; ++n) {
			address_mode_[n] = uvw[n];
		}
		border_color_ = bc;
		init();
	}

	void Texture::setFiltering(Texture::Filtering min, Texture::Filtering max, Texture::Filtering mip)
	{
		filtering_[0] = min;
		filtering_[1] = max;
		filtering_[2] = mip;
		init();
	}

	void Texture::setFiltering(const Texture::Filtering f[3])
	{
		for(int n = 0; n < 3; ++n) {
			filtering_[n] = f[n];
		}
		init();
	}

	void Texture::rebuildAll()
	{
		for(auto tex : texture_registry()) {
			tex->rebuild();
		}
	}

	void Texture::setTextureDimensions(int w, int h, int d)
	{
		width_ = w;
		height_ = h;
		depth_ = d;
	}

	void Texture::setUnpackAlignment(int align)
	{
		ASSERT_LOG(align == 1 || align == 2 || align == 4 || align == 8, 
			"texture unpacking alignment must be either 1,2,4 or 8: " << align);
		unpack_alignment_ = align;
	}

	void Texture::setSourceRect(const rect& r)
	{
		src_rect_ = r;
		src_rect_norm_ = getNormalisedTextureCoords<float>(src_rect_);
	}

	void Texture::setSourceRectNormalised(const rectf& r)
	{
		src_rect_norm_ = r;
		src_rect_ = rect::from_coordinates(static_cast<int>(round(r.x() * width_)),
			static_cast<int>(round(r.y() * height_)),
			static_cast<int>(round(r.x2() * width_)),
			static_cast<int>(round(r.y2() * height_)));
	}

	TexturePtr Texture::createTexture(const variant& node)
	{
		return DisplayDevice::createTexture(nullptr, true, node);
	}

	TexturePtr Texture::createTexture(const std::string& filename, Type type, int mipmap_levels)
	{
		return DisplayDevice::createTexture(filename, type, mipmap_levels);
	}

	TexturePtr Texture::createTexture(const SurfacePtr& surface, bool cache)
	{
		return DisplayDevice::createTexture(surface, cache, variant());
	}

	TexturePtr Texture::createTexture(const SurfacePtr& surface, bool cache, const variant& node)
	{
		return DisplayDevice::createTexture(surface, cache, node);
	}

	TexturePtr Texture::createPalettizedTexture(const std::string& filename)
	{
		return DisplayDevice::createTexture(Surface::create(filename), SurfacePtr());
	}

	TexturePtr Texture::createPalettizedTexture(const std::string& filename, const SurfacePtr& palette)
	{
		return DisplayDevice::createTexture(Surface::create(filename), palette);
	}

	TexturePtr Texture::createPalettizedTexture(const SurfacePtr& surf)
	{
		return DisplayDevice::createTexture(surf, SurfacePtr());
	}

	TexturePtr Texture::createPalettizedTexture(const SurfacePtr& surf, const SurfacePtr& palette)
	{
		return DisplayDevice::createTexture(surf, palette);
	}

	TexturePtr Texture::createTexture1D(int width, PixelFormat::PF fmt)
	{
		return DisplayDevice::createTexture1D(width, fmt);
	}

	TexturePtr Texture::createTexture2D(int width, int height, PixelFormat::PF fmt)
	{
		return DisplayDevice::createTexture2D(width, height, fmt);
	}

	TexturePtr Texture::createTexture3D(int width, int height, int depth, PixelFormat::PF fmt)
	{
		return DisplayDevice::createTexture3D(width, height, depth, fmt);
	}

	TexturePtr Texture::createTexture2D(int count, int width, int height, PixelFormat::PF fmt)
	{
		return DisplayDevice::createTexture2D(count, width, height, fmt);
	}

	TexturePtr Texture::createTexture2D(const std::vector<std::string>& filenames, const variant& node)
	{
		return DisplayDevice::createTexture2D(filenames, node);
	}

	TexturePtr Texture::createTexture2D(const std::vector<SurfacePtr>& surfaces, bool cache)
	{
		return DisplayDevice::createTexture2D(surfaces, cache);
	}

	bool Texture::isAlpha(unsigned x, unsigned y, int n) const
	{ 
		ASSERT_LOG(n < static_cast<int>(alpha_map_.size()), "Couldn't index into the alpha map for the texture.");
		return alpha_map_[n][y*width_+x]; 
	}

	std::vector<bool>::const_iterator Texture::getAlphaRow(int x, int y, int n) const 
	{ 
		ASSERT_LOG(n < static_cast<int>(alpha_map_.size()), "Couldn't index into the alpha map for the texture.");
		return alpha_map_[n].begin() + y*width_ + x; 
	}

	std::vector<bool>::const_iterator Texture::endAlpha(int n) const 
	{ 
		ASSERT_LOG(n < static_cast<int>(alpha_map_.size()), "Couldn't index into the alpha map for the texture.");
		return alpha_map_[n].end(); 
	}

	void Texture::clearTextures()
	{
		texture_registry().clear();
	}

	void Texture::clearCache()
	{
		clearTextures();
	}
}
