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

#pragma once

#include <memory>
#include <string>
#include "Color.hpp"
#include "Geometry.hpp"
#include "Surface.hpp"
#include "variant.hpp"

namespace KRE
{
	inline unsigned next_power_of_2(unsigned n) {
		--n;
		n = n|(n >> 1);
		n = n|(n >> 2);
		n = n|(n >> 4);
		n = n|(n >> 8);
		n = n|(n >> 16);
		++n;
		return n;
	}

	class Texture
	{
	public:
		enum class Type {
			TEXTURE_1D,
			TEXTURE_2D,
			TEXTURE_3D,
			TEXTURE_CUBIC,
		};
		enum class AddressMode {
			WRAP,
			CLAMP,
			MIRROR,
			BORDER,
		};
		enum class Filtering {
			NONE,
			POINT,
			LINEAR,
			ANISOTROPIC,
		};
		Texture(const SurfacePtr& surface, const variant& node);
		Texture(const SurfacePtr& surface, 
			Type type=Type::TEXTURE_2D, 
			int mipmap_levels=0);
		Texture(unsigned width, 
			unsigned height, 
			unsigned depth,
			PixelFormat::PF fmt, 
			Texture::Type type);
		virtual ~Texture();

		void SetAddressModes(AddressMode u, AddressMode v=AddressMode::WRAP, AddressMode w=AddressMode::WRAP, const Color& bc=Color(0.0f,0.0f,0.0f));
		void SetAddressModes(const AddressMode uvw[3], const Color& bc=Color(0.0f,0.0f,0.0f));

		void SetFiltering(Filtering min, Filtering max, Filtering mip);
		void SetFiltering(const Filtering f[3]);

		void SetBorderColor(const Color& bc);

		Type GetType() const { return type_; }
		int GetMipMapLevels() const { return mipmaps_; }
		int GetMaxAnisotropy() const { return max_anisotropy_; }
		AddressMode GetAddressModeU() const { return address_mode_[0]; }
		AddressMode GetAddressModeV() const { return address_mode_[1]; }
		AddressMode GetAddressModeW() const { return address_mode_[2]; }
		Filtering GetFilteringMin() const { return filtering_[0]; }
		Filtering GetFilteringMax() const { return filtering_[1]; }
		Filtering GetFilteringMip() const { return filtering_[2]; }
		const Color& GetBorderColor() const { return border_color_; }
		float GetLodBias() const { return lod_bias_; }

		void InternalInit();

		unsigned Width() const { return width_; }
		unsigned Height() const { return height_; }
		unsigned Depth() const { return depth_; }

		unsigned SurfaceWidth() const { return surface_width_; }
		unsigned SurfacehHeight() const { return surface_height_; }

		virtual void Init() = 0;
		virtual void Bind() = 0;
		virtual unsigned ID() = 0;

		virtual void Update(int x, unsigned width, void* pixels) = 0;
		virtual void Update(int x, int y, unsigned width, unsigned height, const std::vector<unsigned>& stride, void* pixels) = 0;
		virtual void Update(int x, int y, int z, unsigned width, unsigned height, unsigned depth, void* pixels) = 0;

		static void RebuildAll();
	protected:
		const SurfacePtr& GetSurface() const { return surface_; }
		void SetTextureDimensions(unsigned w, unsigned h, unsigned d=0);
	private:
		virtual void Rebuild() = 0;

		Type type_;
		int mipmaps_;
		AddressMode address_mode_[3]; // u,v,w
		Filtering filtering_[3]; // minification, magnification, mip
		Color border_color_;
		int max_anisotropy_;
		float lod_bias_;
		Texture();
		SurfacePtr surface_;
		
		unsigned surface_width_;
		unsigned surface_height_;

		// Width/Height/Depth of the created texture -- may be a 
		// different size than the surface if things like only
		// allowing power-of-two textures is in effect.
		unsigned width_;
		unsigned height_;
		unsigned depth_;
	};

	typedef std::shared_ptr<Texture> TexturePtr;
}
