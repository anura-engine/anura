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
#include "geometry.hpp"
#include "ScopeableValue.hpp"
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

	class Texture : public ScopeableValue
	{
	public:
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
		virtual ~Texture();

		void setAddressModes(AddressMode u, AddressMode v=AddressMode::WRAP, AddressMode w=AddressMode::WRAP, const Color& bc=Color(0.0f,0.0f,0.0f));
		void setAddressModes(const AddressMode uvw[3], const Color& bc=Color(0.0f,0.0f,0.0f));

		void setFiltering(Filtering min, Filtering max, Filtering mip);
		void setFiltering(const Filtering f[3]);

		void setBorderColor(const Color& bc);

		TextureType getType() const { return type_; }
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

		int actualWidth() const { return width_; }
		int actualHeight() const { return height_; }
		int actualDepth() const { return depth_; }

		int width() const { return src_rect_.w(); }
		int height() const { return src_rect_.h(); }
		int depth() const { return surfaces_.size(); }

		int surfaceWidth() const { return surface_width_; }
		int surfaceHeight() const { return surface_height_; }

		virtual void init() = 0;
		virtual void bind() = 0;
		virtual unsigned id(int n = 0) = 0;

		virtual void update(int x, unsigned width, void* pixels) = 0;
		// Less safe version for updating a multi-texture.
		virtual void update(int x, int y, unsigned width, unsigned height, const int* stride, const void* pixels) = 0;
		virtual void update(int x, int y, unsigned width, unsigned height, const std::vector<unsigned>& stride, const void* pixels) = 0;
		virtual void update(int x, int y, int z, unsigned width, unsigned height, unsigned depth, void* pixels) = 0;

		static void rebuildAll();
		static void clearTextures();

		// XXX Need to add a pixel filter function, so when we load the surface we apply the filter.
		static TexturePtr createTexture(const std::string& filename,
			TextureType type=TextureType::TEXTURE_2D, 
			int mipmap_levels=0);
		static TexturePtr createTexture(const variant& node);
		static TexturePtr createTexture(const std::string& filename, const variant& node);
		static TexturePtr createTexture(const SurfacePtr& surface, bool cache);
		static TexturePtr createTexture(const SurfacePtr& surface, bool cache, const variant& node);
		
		static TexturePtr createTexture1D(int width, PixelFormat::PF fmt);
		static TexturePtr createTexture2D(int width, int height, PixelFormat::PF fmt);
		static TexturePtr createTexture3D(int width, int height, int depth, PixelFormat::PF fmt);

		static TexturePtr createTexture2D(int count, int width, int height, PixelFormat::PF fmt);
		static TexturePtr createTexture2D(const std::vector<std::string>& filenames, const variant& node);
		static TexturePtr createTexture2D(const std::vector<SurfacePtr>& surfaces, bool cache);

		/* Functions for creating a texture that only has a single channel and an associated
			secondary texture that is used for doing palette look-ups to get the actual color.
		*/
		static TexturePtr createPalettizedTexture(const std::string& filename);
		static TexturePtr createPalettizedTexture(const std::string& filename, const SurfacePtr& palette);
		static TexturePtr createPalettizedTexture(const SurfacePtr& surf);
		static TexturePtr createPalettizedTexture(const SurfacePtr& surf, const SurfacePtr& palette);

		void addPalette(const SurfacePtr& palette);

		const SurfacePtr& getFrontSurface() const { return surfaces_.front(); }
		std::vector<SurfacePtr> getSurfaces() const { return surfaces_; }

		int getUnpackAlignment() const { return unpack_alignment_; }
		void setUnpackAlignment(int align);

		template<typename N, typename T>
		const geometry::Rect<N> getNormalisedTextureCoords(const geometry::Rect<T>& r) {
			float w = static_cast<float>(surface_width_);
			float h = static_cast<float>(surface_height_);
			return geometry::Rect<N>::from_coordinates(static_cast<N>(r.x())/w, static_cast<N>(r.y())/h, static_cast<N>(r.x2())/w, static_cast<N>(r.y2())/h);		
		}

		template<typename N, typename T>
		const N getNormalisedTextureCoordW(const T& x) {
			return static_cast<N>(x) / static_cast<N>(surface_width_);
		}
		template<typename N, typename T>
		const N getNormalisedTextureCoordH(const T& y) {
			return static_cast<N>(y) / static_cast<N>(surface_height_);
		}

		// Can return nullptr if not-implemented, invalid underlying surface.
		virtual const unsigned char* colorAt(int x, int y) const = 0;

		static void clearCache();

		// Set source rect in un-normalised co-ordinates.
		void setSourceRect(const rect& r);
		// Set source rect in normalised co-ordinates.
		void setSourceRectNormalised(const rectf& r);

		const rectf& getSourceRectNormalised() const { return src_rect_norm_; }
		const rect& getSourceRect() const { return src_rect_; }

		virtual TexturePtr clone() = 0;
	protected:
		explicit Texture(const variant& node, const std::vector<SurfacePtr>& surfaces);
		explicit Texture(const std::vector<SurfacePtr>& surfaces,
			TextureType type=TextureType::TEXTURE_2D, 
			int mipmap_levels=0);
		explicit Texture(int count, 
			int width, 
			int height, 
			int depth,
			PixelFormat::PF fmt, 
			TextureType type);
		// Constrcutor to create paletteized texture from a file name and optional surface.
		explicit Texture(const SurfacePtr& surf, const SurfacePtr& palette);
		void setTextureDimensions(int w, int h, int d=0);
	private:
		virtual void rebuild() = 0;
		virtual void handleAddPalette(const SurfacePtr& palette) = 0;

		TextureType type_;
		int mipmaps_;
		AddressMode address_mode_[3]; // u,v,w
		Filtering filtering_[3]; // minification, magnification, mip
		Color border_color_;
		int max_anisotropy_;
		float lod_bias_;
		Texture();
		std::vector<SurfacePtr> surfaces_;

		int surface_width_;
		int surface_height_;

		// Width/Height/Depth of the created texture -- may be a 
		// different size than the surface if things like only
		// allowing power-of-two textures is in effect.
		int width_;
		int height_;
		int depth_;

		int unpack_alignment_;

		rect src_rect_;
		rectf src_rect_norm_;
	};
}
