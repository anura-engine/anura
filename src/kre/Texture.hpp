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

#pragma once

#include <memory>
#include <string>
#include "Blend.hpp"
#include "Color.hpp"
#include "geometry.hpp"
#include "Surface.hpp"
#include "variant.hpp"

namespace KRE
{
	class Texture;
	typedef std::shared_ptr<Texture> TexturePtr;

	// XX Need to add functionality to encapsulate setting the unpack alignment and other parameters
	// unpack swap bytes
	// unpack lsb first
	// unpack image height
	// unpack skip rows, skip pixels

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
		explicit Texture(const variant& node, const SurfacePtr& surface=nullptr);
		explicit Texture(const SurfacePtr& surface, 
			Type type=Type::TEXTURE_2D, 
			int mipmap_levels=0);
		explicit Texture(unsigned width, 
			unsigned height, 
			unsigned depth,
			PixelFormat::PF fmt, 
			Texture::Type type);
		// Constrcutor to create paletteized texture from a file name and optional surface.
		explicit Texture(const SurfacePtr& surf, const SurfacePtr& palette);
		virtual ~Texture();

		void setAddressModes(AddressMode u, AddressMode v=AddressMode::WRAP, AddressMode w=AddressMode::WRAP, const Color& bc=Color(0.0f,0.0f,0.0f));
		void setAddressModes(const AddressMode uvw[3], const Color& bc=Color(0.0f,0.0f,0.0f));

		void setFiltering(Filtering min, Filtering max, Filtering mip);
		void setFiltering(const Filtering f[3]);

		void setBorderColor(const Color& bc);

		Type getType() const { return type_; }
		int getMipMapLevels() const { return mipmaps_; }
		int getMaxAnisotropy() const { return max_anisotropy_; }
		AddressMode getAddressModeU() const { return address_mode_[0]; }
		AddressMode getAddressModeV() const { return address_mode_[1]; }
		AddressMode getAddressModeW() const { return address_mode_[2]; }
		Filtering getFilteringMin() const { return filtering_[0]; }
		Filtering getFilteringMax() const { return filtering_[1]; }
		Filtering getFilteringMip() const { return filtering_[2]; }
		const Color& getBorderColor() const { return border_color_; }
		float getLodBias() const { return lod_bias_; }

		void internalInit();

		unsigned width() const { return width_; }
		unsigned height() const { return height_; }
		unsigned depth() const { return depth_; }

		unsigned surfaceWidth() const { return surface_width_; }
		unsigned surfacehHeight() const { return surface_height_; }

		virtual void init() = 0;
		virtual void bind() = 0;
		virtual unsigned id() = 0;

		virtual void update(int x, unsigned width, void* pixels) = 0;
		// Less safe version for updating a multi-texture.
		virtual void update(int x, int y, unsigned width, unsigned height, const int* stride, const void* pixels) = 0;
		virtual void update(int x, int y, unsigned width, unsigned height, const std::vector<unsigned>& stride, const void* pixels) = 0;
		virtual void update(int x, int y, int z, unsigned width, unsigned height, unsigned depth, void* pixels) = 0;

		static void rebuildAll();
		static void clearTextures();

		// XXX Need to add a pixel filter function, so when we load the surface we apply the filter.
		static TexturePtr createTexture(const std::string& filename,
			Type type=Type::TEXTURE_2D, 
			int mipmap_levels=0);
		static TexturePtr createTexture(const variant& node);
		static TexturePtr createTexture(const std::string& filename, const variant& node);
		static TexturePtr createTexture(const SurfacePtr& surface, bool cache);
		static TexturePtr createTexture(const SurfacePtr& surface, bool cache, const variant& node);
		
		static TexturePtr createTexture(unsigned width, PixelFormat::PF fmt);
		static TexturePtr createTexture(unsigned width, unsigned height, PixelFormat::PF fmt);
		static TexturePtr createTexture(unsigned width, unsigned height, unsigned depth, PixelFormat::PF fmt);

		/* Functions for creating a texture that only has a single channel and an associated
			secondary texture that is used for doing palette look-ups to get the actual color.
		*/
		static TexturePtr createPalettizedTexture(const std::string& filename);
		static TexturePtr createPalettizedTexture(const std::string& filename, const SurfacePtr& palette);
		static TexturePtr createPalettizedTexture(const SurfacePtr& surf);
		static TexturePtr createPalettizedTexture(const SurfacePtr& surf, const SurfacePtr& palette);

		const SurfacePtr& getSurface() const { return surface_; }

		int getUnpackAlignment() const { return unpack_alignment_; }
		void setUnpackAlignment(int align);

		bool hasBlendMode() const { return blend_mode_ != NULL; }
		const BlendMode getBlendMode() const;
		void setBlendMode(const BlendMode& bm) { blend_mode_.reset(new BlendMode(bm)); }

		template<typename N, typename T>
		const geometry::Rect<N> getNormalisedTextureCoords(const geometry::Rect<T>& r) {
			float w = static_cast<float>(surface_width_);
			float h = static_cast<float>(surface_height_);
			return geometry::Rect<N>(static_cast<N>(r.x())/w, static_cast<N>(r.y())/h, static_cast<N>(r.x2())/w, static_cast<N>(r.y2())/h);		
		}

		// Can return NULL if not-implemented, invalid underlying surface.
		virtual const unsigned char* colorAt(int x, int y) const = 0;
		bool isAlpha(unsigned x, unsigned y) { return alpha_map_[y*width_+x]; }
		std::vector<bool>::const_iterator getAlphaRow(int x, int y) const { return alpha_map_.begin() + y*width_ + x; }
		std::vector<bool>::const_iterator endAlpha() const { return alpha_map_.end(); }

		static void clearCache();
	protected:
		void setTextureDimensions(unsigned w, unsigned h, unsigned d=0);
	private:
		virtual void rebuild() = 0;

		Type type_;
		int mipmaps_;
		AddressMode address_mode_[3]; // u,v,w
		Filtering filtering_[3]; // minification, magnification, mip
		Color border_color_;
		int max_anisotropy_;
		float lod_bias_;
		Texture();
		SurfacePtr surface_;

		std::unique_ptr<BlendMode> blend_mode_;

		std::vector<bool> alpha_map_;
		
		unsigned surface_width_;
		unsigned surface_height_;

		// Width/Height/Depth of the created texture -- may be a 
		// different size than the surface if things like only
		// allowing power-of-two textures is in effect.
		unsigned width_;
		unsigned height_;
		unsigned depth_;

		int unpack_alignment_;
	};
}
