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
	namespace {
		std::set<Texture*>& allTextures() {
			static std::set<Texture*>* value = new std::set<Texture*>;
			return *value;
		}
	}

	const std::set<Texture*>& Texture::getAllTextures() {
		return allTextures();
	}

	Texture::Texture(const variant& node, const std::vector<SurfacePtr>& surfaces)
		: is_paletteized_(false),
		  mix_ratio_(0.0f),
		  mix_palettes_(false)
	{
		palette_[0] = palette_[1] = 0;
		if(node.is_list()) {
			if(surfaces.size() > 0) {
				ASSERT_LOG(surfaces.size() == node.num_elements(), "Number of items in node list must match number of surfaces.");
			}
			texture_params_.resize(node.num_elements());
			for(int n = 0; n != node.num_elements(); ++n) {
				if(surfaces.size() == 0) {
					ASSERT_LOG(node[n].has_key("image") && node[n]["image"].is_string(), "No 'image' attribute found");
					texture_params_[n].surface = Surface::create(node.as_string());
					texture_params_[n].surface_width = texture_params_[n].surface->width();
					texture_params_[n].surface_height = texture_params_[n].surface->height();
				} else {
					texture_params_[n].surface = surfaces[n];
					texture_params_[n].surface_width = texture_params_[n].surface->width();
					texture_params_[n].surface_height = texture_params_[n].surface->height();
				}
				initFromVariant(texture_params_.begin() + n, node[n]);
			}
		} else {
			SurfaceFlags flags = SurfaceFlags::NONE;

			if(node.is_map()) {
				variant flags_list = node["surface_flags"];
				if(flags_list.is_list()) {
					for(const std::string& f : flags_list.as_list_string()) {
						if(f == "NO_CACHE") {
							flags = flags | SurfaceFlags::NO_CACHE;
						} else if(f == "NO_ALPHA_FILTER") {
							flags = flags | SurfaceFlags::NO_ALPHA_FILTER;
						} else {
							ASSERT_LOG(false, "Illegal surface flag: " << f);
						}
					}
				}
			}

			if(surfaces.size() == 0 && node.is_string()) {
				texture_params_.resize(1);
				texture_params_[0].surface = Surface::create(node.as_string(), static_cast<SurfaceFlags>(flags));
				texture_params_[0].surface_width = texture_params_[0].surface->width();
				texture_params_[0].surface_height = texture_params_[0].surface->height();
			} else if(surfaces.size() == 0 && node.has_key("image") && node["image"].is_string()) {
				texture_params_.resize(1);
				texture_params_[0].surface = Surface::create(node["image"].as_string(), flags);
				texture_params_[0].surface_width = texture_params_[0].surface->width();
				texture_params_[0].surface_height = texture_params_[0].surface->height();
			} else if(surfaces.size() == 0 && node.has_key("images") && node["images"].is_list()) {
				texture_params_.resize(node["images"].num_elements());
				int n = 0;
				for(auto s : node["images"].as_list_string()) {
					texture_params_[n].surface = Surface::create(s, flags);
					texture_params_[n].surface_width = texture_params_[n].surface->width();
					texture_params_[n].surface_height = texture_params_[n].surface->height();
				}
			} else if(surfaces.size() > 0) {
				texture_params_.resize(surfaces.size());
				for(int n = 0; n != surfaces.size(); ++n) {
					texture_params_[n].surface = surfaces[n];
					texture_params_[n].surface_width = texture_params_[n].surface->width();
					texture_params_[n].surface_height = texture_params_[n].surface->height();
				}
			}


			ASSERT_LOG(texture_params_.size() > 0, "Error no surfaces.");
			// Assumes that we want to use the same parameters for all surfaces.
			for(auto tp = texture_params_.begin(); tp != texture_params_.end(); ++tp) {
				initFromVariant(tp, node);
			}
		}

		allTextures().insert(this);
	}

	Texture::Texture(const std::vector<SurfacePtr>& surfaces, TextureType type, int mipmap_levels)
		: is_paletteized_(false),
		  mix_ratio_(0.0f),
		  mix_palettes_(false)
	{
		palette_[0] = palette_[1] = 0;
		texture_params_.reserve(surfaces.size());
		for(auto s : surfaces) {
			texture_params_.emplace_back(TextureParams());
			texture_params_.back().surface = s;
			texture_params_.back().surface_width = s->width();
			texture_params_.back().surface_height = s->height();
			texture_params_.back().type = type;
			texture_params_.back().mipmaps = mipmap_levels;
			internalInit(texture_params_.begin() + (texture_params_.size() - 1));
		}
		allTextures().insert(this);
	}

	Texture::Texture(int count, 
		int width, 
		int height, 
		int depth,
		PixelFormat::PF fmt, 
		TextureType type)
		: is_paletteized_(false),
		  mix_ratio_(0.0f),
		  mix_palettes_(false)
	{
		ASSERT_LOG(count > 0, "Insufficient number of textures specified: " << count);
		palette_[0] = palette_[1] = 0;
		texture_params_.resize(count);
		for(int n = 0; n != count; ++n) {
			auto& tp = texture_params_[n];
			tp.surface = Surface::create(width, height, fmt);
			tp.surface_width = width;
			tp.surface_height = height;
			tp.width = width;
			tp.height = height;
			tp.depth = depth;
			tp.type = type;
			internalInit(texture_params_.begin()+n);
		}
		allTextures().insert(this);
	}

	Texture::Texture(const Texture& o)
	  : texture_params_(o.texture_params_),
		is_paletteized_(o.is_paletteized_),
		mix_ratio_(o.mix_ratio_),
		mix_palettes_(o.mix_palettes_),
		palette_row_map_(o.palette_row_map_)
	{
		memcpy(palette_, o.palette_, sizeof(palette_));
		allTextures().insert(this);
	}

	Texture::~Texture()
	{
		allTextures().erase(this);
	}

	void Texture::initFromVariant(texture_params_iterator tp, const variant& node)
	{
		internalInit(tp);
		if(node.has_key("image_type")) {
			const std::string& type = node["image_type"].as_string();
			if(type == "1d") {
				tp->type = TextureType::TEXTURE_1D;
			} else if(type == "2d") {
				tp->type = TextureType::TEXTURE_2D;
			} else if(type == "3d") {
				tp->type = TextureType::TEXTURE_3D;
			} else if(type == "cubic") {
				tp->type = TextureType::TEXTURE_CUBIC;
			} else {
				ASSERT_LOG(false, "Unrecognised texture type '" << type << "'. Valid values are 1d,2d,3d and cubic.");
			}
		}
		if(node.has_key("mipmaps")) {
			ASSERT_LOG(node["mipmaps"].is_int(), "'mipmaps' not an integer type, found: " << node["mipmaps"].to_debug_string());
			tp->mipmaps = node["mipmaps"].as_int32();
		}
		if(node.has_key("lod_bias")) {
			ASSERT_LOG(node["lod_bias"].is_numeric(), "'lod_bias' not a numeric type, found: " << node["lod_bias"].to_debug_string());
			tp->lod_bias = node["lod_bias"].as_float();
		}
		if(node.has_key("max_anisotropy")) {
			ASSERT_LOG(node["max_anisotropy"].is_int(), "'max_anisotropy' not an integer type, found: " << node["max_anisotropy"].to_debug_string());
			tp->max_anisotropy = node["max_anisotropy"].as_int32();
		}
		if(node.has_key("filtering")) {
			if(node["filtering"].is_string()) {
				const std::string& filtering = node["filtering"].as_string();
				if(filtering == "none") {
					tp->filtering[0] = Filtering::POINT;
					tp->filtering[1] = Filtering::POINT;
					tp->filtering[2] = Filtering::NONE;
				} else if(filtering == "bilinear") {
					tp->filtering[0] = Filtering::LINEAR;
					tp->filtering[1] = Filtering::LINEAR;
					tp->filtering[2] = Filtering::POINT;
				} else if(filtering == "trilinear") {
					tp->filtering[0] = Filtering::LINEAR;
					tp->filtering[1] = Filtering::LINEAR;
					tp->filtering[2] = Filtering::LINEAR;
				} else if(filtering == "anisotropic") {
					tp->filtering[0] = Filtering::ANISOTROPIC;
					tp->filtering[1] = Filtering::ANISOTROPIC;
					tp->filtering[2] = Filtering::LINEAR;
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
						tp->filtering[n] = Filtering::NONE;
					} else if(f == "point") {
						tp->filtering[n] = Filtering::POINT;
					} else if(f == "linear") {
						tp->filtering[n] = Filtering::LINEAR;
					} else if(f == "anisotropic") {
						tp->filtering[n] = Filtering::ANISOTROPIC;
					} else {
						ASSERT_LOG(false, "Filtering element(" << n << ") invalid: " << f);
					}
				}
			} else {
				ASSERT_LOG(false, "'filtering' must be either a string value or list of strings. Found: " << node["filtering"].to_debug_string());
			}
		}
		if(node.has_key("address_mode")) {
			if(node["address_mode"].is_string()) {
				const std::string& am = node["address_mode"].as_string();
				if(am == "wrap") {
					tp->address_mode[0] = tp->address_mode[1] = tp->address_mode[2] = AddressMode::WRAP;
				} else if(am == "clamp") {
					tp->address_mode[0] = tp->address_mode[1] = tp->address_mode[2] = AddressMode::CLAMP;
				} else if(am == "mirror") {
					tp->address_mode[0] = tp->address_mode[1] = tp->address_mode[2] = AddressMode::MIRROR;
				} else if(am == "border") {
					tp->address_mode[0] = tp->address_mode[1] = tp->address_mode[2] = AddressMode::BORDER;
				} else {
					ASSERT_LOG(false, "address_mode invalid: " << am << ", valid values are wrap, clamp, mirror, border.");
				}
			} else if(node["address_mode"].is_list()) {
				size_t list_size = node["address_mode"].num_elements();
				ASSERT_LOG(list_size >= 1 && list_size <= 3, "Size of list for 'address_mode' attribute must be between 1 and 3 elements. Found: " << list_size);
				size_t n = 0;
				for(; n != list_size; ++n) {
					ASSERT_LOG(node["address_mode"][n].is_string(), "Element " << n << " of 'address_mode' attribute is not a string: " << node["address_mode"][0].to_debug_string());
					const std::string& am = node["address_mode"][n].as_string();
					if(am == "wrap") {
						tp->address_mode[n] = AddressMode::WRAP;
					} else if(am == "clamp") {
						tp->address_mode[n] = AddressMode::CLAMP;
					} else if(am == "mirror") {
						tp->address_mode[n] = AddressMode::MIRROR;
					} else if(am == "border") {
						tp->address_mode[n] = AddressMode::BORDER;
					} else {
						ASSERT_LOG(false, "address_mode element(" << n << ") invalid: " << am);
					}
				}
				for(; n < 3; ++n) {
					tp->address_mode[n] = AddressMode::WRAP;
				}
			} else {
				ASSERT_LOG(false, "'address_mode' must be a list of strings. Found: " << node["address_mode"].to_debug_string());
			}
		}
		if(node.has_key("border_color")) {
			tp->border_color = Color(node["border_color"]);
		}
		if(node.has_key("rect")) {
			ASSERT_LOG(node["rect"].is_list(), "'rect' attribute must be a list of numbers.");
			ASSERT_LOG(node["rect"].num_elements() >= 4, "'rect' attribute must have at least 4 elements.");
			tp->src_rect = rect(node["rect"]);
			const int n = static_cast<int>(std::distance(texture_params_.begin(), tp));
			tp->src_rect_norm = rectf::from_coordinates(getTextureCoordW(n, tp->src_rect.x1()), 
				getTextureCoordH(n, tp->src_rect.y1()), 
				getTextureCoordW(n, tp->src_rect.x2()), 
				getTextureCoordH(n, tp->src_rect.y2()));
		}
	}

	void Texture::internalInit(texture_params_iterator tp)
	{
		for(auto& am : tp->address_mode) {
			am = AddressMode::WRAP;
		}
		tp->filtering[0] = Filtering::POINT;
		tp->filtering[1] = Filtering::POINT;
		tp->filtering[2] = Filtering::NONE;

		// XXX For reasons (i.e. some video cards are problematic either hardware/drivers)
		// we are forced to use power-of-two textures anyway if we want mip-mapping and
		// address modes other than CLAMP.
		if(!DisplayDevice::checkForFeature(DisplayDeviceCapabilties::NPOT_TEXTURES)) {
			tp->width = next_power_of_two(tp->surface_width);
			tp->height = next_power_of_two(tp->surface_height);
			ASSERT_LOG(tp->type != TextureType::TEXTURE_3D && tp->type != TextureType::TEXTURE_CUBIC, "fixme texture type3d or cubic");
			tp->depth = 0;

			tp->w_ratio = static_cast<float>(tp->width) / static_cast<float>(tp->surface_width);
			tp->h_ratio = static_cast<float>(tp->height) / static_cast<float>(tp->surface_height);
		} else {
			tp->width = tp->surface_width;
			tp->height = tp->surface_height;
			ASSERT_LOG(tp->type != TextureType::TEXTURE_3D && tp->type != TextureType::TEXTURE_CUBIC, "fixme texture type3d or cubic");
			tp->depth = 0;
		}

		tp->src_rect = rect(0, 0, tp->surface_width, tp->surface_height);
		const int n = static_cast<int>(std::distance(texture_params_.begin(), tp));
		tp->src_rect_norm = rectf::from_coordinates(getTextureCoordW(n, tp->src_rect.x1()), 
			getTextureCoordH(n, tp->src_rect.y1()), 
			getTextureCoordW(n, tp->src_rect.x2()), 
			getTextureCoordH(n, tp->src_rect.y2()));
	}

	void Texture::setAddressModes(int n, Texture::AddressMode u, Texture::AddressMode v, Texture::AddressMode w, const Color& bc)
	{
		ASSERT_LOG(n < static_cast<int>(texture_params_.size()), "index exceeds number of textures present.");
		if(n < 0) {
			for(auto& tp : texture_params_) {
				tp.address_mode[0] = u;
				tp.address_mode[1] = v;
				tp.address_mode[2] = w;
				tp.border_color = bc;
			}
		} else {
			texture_params_[n].address_mode[0] = u;
			texture_params_[n].address_mode[1] = v;
			texture_params_[n].address_mode[2] = w;
			texture_params_[n].border_color = bc;
		}
		init(n);
	}

	void Texture::setAddressModes(int n, const Texture::AddressMode uvw[3], const Color& bc)
	{
		ASSERT_LOG(n < static_cast<int>(texture_params_.size()), "index exceeds number of textures present.");
		if(n < 0) {
			for(auto& tp : texture_params_) {
				for(int m = 0; m < 3; ++m) {
					tp.address_mode[m] = uvw[m];
					tp.border_color = bc;
				}
			}
		} else {
			for(int m = 0; m < 3; ++m) {
				texture_params_[n].address_mode[m] = uvw[m];
			}
			texture_params_[n].border_color = bc;
		}
		init(n);
	}

	void Texture::setFiltering(int n, Texture::Filtering min, Texture::Filtering max, Texture::Filtering mip)
	{
		ASSERT_LOG(n < static_cast<int>(texture_params_.size()), "index exceeds number of textures present.");
		if(n < 0) {
			for(auto& tp : texture_params_) {
				tp.filtering[0] = min;
				tp.filtering[1] = max;
				tp.filtering[2] = mip;
				// If you enable bilinear/trilinear/aniso filtering on an image then it must have mipmaps.
				if((min != Texture::Filtering::LINEAR || min != Texture::Filtering::ANISOTROPIC
					|| max == Texture::Filtering::LINEAR || max == Texture::Filtering::ANISOTROPIC
					|| mip == Texture::Filtering::LINEAR) && tp.mipmaps == 0) {
					tp.mipmaps = 2;
				}
			}
		} else {
			texture_params_[n].filtering[0] = min;
			texture_params_[n].filtering[1] = max;
			texture_params_[n].filtering[2] = mip;

			// If you enable bilinear/trilinear/aniso filtering on an image then it must have mipmaps.
			if((min != Texture::Filtering::LINEAR || min != Texture::Filtering::ANISOTROPIC
				|| max == Texture::Filtering::LINEAR || max == Texture::Filtering::ANISOTROPIC
				|| mip == Texture::Filtering::LINEAR) && texture_params_[n].mipmaps == 0) {
				texture_params_[n].mipmaps = 2;
			}
		}
		init(n);
	}

	void Texture::setFiltering(int n, const Texture::Filtering f[3])
	{
		ASSERT_LOG(n < static_cast<int>(texture_params_.size()), "index exceeds number of textures present.");
		if(n < 0) {
			for(auto& tp : texture_params_) {
				for(int m = 0; m < 3; ++m) {
					tp.filtering[m] = f[m];
				}
			}
		} else {
			for(int m = 0; m < 3; ++m) {
				texture_params_[n].filtering[m] = f[m];
			}
		}
		init(n);
	}

	void Texture::clearSurfaces()
	{
		for(auto& tp : texture_params_) {
			tp.surface.reset();
		}
	}

	void Texture::rebuildAll()
	{
		ASSERT_LOG(false, "Texture::rebuildAll()");
	}

	void Texture::setUnpackAlignment(int n, int align)
	{
		ASSERT_LOG(n < static_cast<int>(texture_params_.size()), "index exceeds number of textures present.");
		ASSERT_LOG(align == 1 || align == 2 || align == 4 || align == 8, 
			"texture unpacking alignment must be either 1,2,4 or 8: " << align);
		if(n < 0) {
			for(auto& tp : texture_params_) {
				tp.unpack_alignment = align;
			}
		} else {
			texture_params_[n].unpack_alignment = align;
		}
	}

	void Texture::setSourceRect(int n, const rect& r)
	{
		ASSERT_LOG(n < static_cast<int>(texture_params_.size()), "index exceeds number of textures present.");
		if(n < 0) {
			for(auto tp = texture_params_.begin(); tp != texture_params_.end(); ++tp) {
				tp->src_rect = r;
				const int n = static_cast<int>(std::distance(texture_params_.begin(), tp));
				tp->src_rect_norm = rectf::from_coordinates(getTextureCoordW(n, tp->src_rect.x1()), 
					getTextureCoordH(n, tp->src_rect.y1()), 
					getTextureCoordW(n, tp->src_rect.x2()), 
					getTextureCoordH(n, tp->src_rect.y2()));
			}
		} else {
			texture_params_[n].src_rect = r;
			texture_params_[n].src_rect_norm = rectf::from_coordinates(getTextureCoordW(n, texture_params_[n].src_rect.x1()), 
				getTextureCoordH(n, texture_params_[n].src_rect.y1()), 
				getTextureCoordW(n, texture_params_[n].src_rect.x2()), 
				getTextureCoordH(n, texture_params_[n].src_rect.y2()));
		}
	}

	void Texture::setSourceRectNormalised(int n, const rectf& r)
	{
		ASSERT_LOG(n < static_cast<int>(texture_params_.size()), "index exceeds number of textures present.");
		if(n < 0) {
			for(auto tp = texture_params_.begin(); tp != texture_params_.end(); ++tp) {
				tp->src_rect_norm = r;
				tp->src_rect = rect::from_coordinates(static_cast<int>(round(r.x() * tp->width)),
					static_cast<int>(round(r.y() * tp->height)),
					static_cast<int>(round(r.x2() * tp->width)),
					static_cast<int>(round(r.y2() * tp->height)));
			}
		} else {
			texture_params_[n].src_rect_norm = r;
			texture_params_[n].src_rect = rect::from_coordinates(static_cast<int>(round(r.x() * texture_params_[n].width)),
				static_cast<int>(round(r.y() * texture_params_[n].height)),
				static_cast<int>(round(r.x2() * texture_params_[n].width)),
				static_cast<int>(round(r.y2() * texture_params_[n].height)));
		}
	}

	void Texture::addPalette(int index, const SurfacePtr& palette)
	{
		if(palette == nullptr) {
			LOG_WARN("Ignoring request to add empty palette surface.");
			return;
		}

		ASSERT_LOG((static_cast<int>(texture_params_.size()) == 1 && !is_paletteized_) || (is_paletteized_ && static_cast<int>(texture_params_.size()) == 2), "Currently we only support converting textures to palette versions that have one texture. may life in future.");

		if(!is_paletteized_) {
			palette_[0] = palette_[1] = 0;
			palette_row_map_[-1] = 0;
		}
		is_paletteized_ = true;
		auto it = palette_row_map_.find(index);
		if(it != palette_row_map_.end()) {
			ASSERT_LOG(false, "adding palette at existing location. " << index << " internal: " << it->second << " id: " << id());
			index = it->second;
		} else {
			int size = static_cast<int>(palette_row_map_.size());
			LOG_DEBUG("adding palette '" << palette->getName() << "' at index: " << size << " from: " << index);
			palette_row_map_[index] = size;
			index = size;
		}
		handleAddPalette(index, palette);
	}

	void Texture::setPalette(int index)
	{
		auto it = palette_row_map_.find(index);
		if(it != palette_row_map_.end()) {
			palette_[0] = it->second;
		} else {
			palette_[0] = 0;
		}
		//LOG_DEBUG("Setting palette to " << index << ":" << palette_[0]);
	}

	bool Texture::hasPaletteAt(int index) const
	{
		return palette_row_map_.find(index) != palette_row_map_.end();
	}

	void Texture::setPaletteMixing(int n1, int n2, float ratio)
	{
		auto it = palette_row_map_.find(n1);
		palette_[0] = it != palette_row_map_.end() ? it->second : 0;
		it = palette_row_map_.find(n2);
		palette_[1] = it != palette_row_map_.end() ? it->second : 0;
		mix_ratio_ = ratio;
		mix_palettes_ = true;
	}

	void Texture::clearPaletteMixing()
	{
		mix_palettes_ = false;
	}

	TexturePtr Texture::createTexture(const variant& node)
	{
		return DisplayDevice::createTexture(nullptr, node);
	}

	TexturePtr Texture::createTexture(const std::string& filename, const variant& node)
	{
		return DisplayDevice::createTexture(Surface::create(filename), node);
	}

	TexturePtr Texture::createTexture(const std::string& filename, TextureType type, int mipmap_levels)
	{
		return DisplayDevice::createTexture(Surface::create(filename), type, mipmap_levels);
	}

	TexturePtr Texture::createTexture(const SurfacePtr& surface)
	{
		return DisplayDevice::createTexture(surface, variant());
	}

	TexturePtr Texture::createTexture(const SurfacePtr& surface, const variant& node)
	{
		return DisplayDevice::createTexture(surface, node);
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

	TexturePtr Texture::createTextureArray(int count, int width, int height, PixelFormat::PF fmt, TextureType type)
	{
		return DisplayDevice::createTextureArray(count, width, height, fmt, type);
	}

	TexturePtr Texture::createTextureArray(const std::vector<SurfacePtr>& surfaces, const variant& node)
	{
		return DisplayDevice::createTextureArray(surfaces, node);
	}

	TexturePtr Texture::createFromImage(const std::string& image_data, const variant& node)
	{
		auto surface = Surface::create(image_data, SurfaceFlags::FROM_DATA | SurfaceFlags::NO_CACHE);
		return DisplayDevice::createTexture(surface, node);
	}

	TexturePtr Texture::createFromImage(const std::string& image_data, TextureType type, int mipmap_levels)
	{
		auto surface = Surface::create(image_data, SurfaceFlags::FROM_DATA | SurfaceFlags::NO_CACHE);
		return DisplayDevice::createTexture(surface, type, mipmap_levels);
	}

	void Texture::clearTextures()
	{
		DisplayDevice::getCurrent()->clearTextures();
	}

	void Texture::clearCache()
	{
		clearTextures();
	}

	std::vector<SurfacePtr> Texture::getSurfaces() const
	{
		std::vector<SurfacePtr> res;
		for(auto& tp : texture_params_) {
			res.emplace_back(tp.surface);
		}
		return res;
	}

	void Texture::addSurface(SurfacePtr surf)
	{
		texture_params_.emplace_back(TextureParams());
		texture_params_.back().surface = surf;
		texture_params_.back().surface_width = surf->width();
		texture_params_.back().surface_height = surf->height();
		internalInit(texture_params_.begin() + (texture_params_.size() - 1));
	}

	void Texture::replaceSurface(int n, SurfacePtr surf)
	{
		ASSERT_LOG(n < static_cast<int>(texture_params_.size()), "index out of bounds. " << n << " >= " << texture_params_.size());
		texture_params_[n] = TextureParams();
		texture_params_[n].surface = surf;
		texture_params_[n].surface_width = surf->width();
		texture_params_[n].surface_height = surf->height();
		internalInit(texture_params_.begin() + n);
	}

	Color Texture::mapPaletteColor(const Color& color, int palette)
	{
		if(!isPaletteized()) {
			return color;
		}
		auto it = palette_row_map_.find(palette);
		if(it == palette_row_map_.end()) {
			return color;
		}
		ASSERT_LOG(texture_params_.size() == 2, "Incorrect number of surfaces in texture.");
		auto& surf = texture_params_[1].surface;
		for(int x = 0; x != surf->width(); ++x) {
			if(surf->getColorAt(x, 0) == color) {
				return surf->getColorAt(x, it->second);
			}			
		}
		return color;
	}

	std::vector<std::string> Texture::findImageNames(const variant& node)
	{
		std::vector<std::string> res;
		if(node.is_string()) {
			res.emplace_back(node.as_string());
		} else if(node.is_map()) {
			if(node.has_key("image")) {
				res.emplace_back(node["image"].as_string());
			} else if(node.has_key("texture")) {
				res.emplace_back(node["texture"].as_string());
			}
		} else if(node.is_list()) {
			for(int n = 0; n != node.num_elements(); ++n) {
				if(node[n].is_map()) {
					if(node[n].has_key("image")) {
						res.emplace_back(node[n]["image"].as_string());
					} else if(node[n].has_key("texture")) {
						res.emplace_back(node[n]["texture"].as_string());
					}
				} else if(node[n].is_string()) {
					res.emplace_back(node[n].as_string());
				}
			}
		}
		return res;
	}
}

