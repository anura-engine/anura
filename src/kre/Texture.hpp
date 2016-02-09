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

#include <array>
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

		void setAddressModes(int n, AddressMode u, AddressMode v=AddressMode::WRAP, AddressMode w=AddressMode::WRAP, const Color& bc=Color(0.0f,0.0f,0.0f));
		void setAddressModes(int n, const AddressMode uvw[3], const Color& bc=Color(0.0f,0.0f,0.0f));

		void setFiltering(int n, Filtering min, Filtering max, Filtering mip);
		void setFiltering(int n, const Filtering f[3]);

		void setBorderColor(int n, const Color& bc);

		TextureType getType(int n) const { return texture_params_[n].type; }
		int getMipMapLevels(int n = 0) const { return texture_params_[n].mipmaps; }
		int getMaxAnisotropy(int n = 0) const { return texture_params_[n].max_anisotropy; }
		AddressMode getAddressModeU(int n = 0) const { return texture_params_[n].address_mode[0]; }
		AddressMode getAddressModeV(int n = 0) const { return texture_params_[n].address_mode[1]; }
		AddressMode getAddressModeW(int n = 0) const { return texture_params_[n].address_mode[2]; }
		Filtering getFilteringMin(int n = 0) const { return texture_params_[n].filtering[0]; }
		Filtering getFilteringMax(int n = 0) const { return texture_params_[n].filtering[1]; }
		Filtering getFilteringMip(int n = 0) const { return texture_params_[n].filtering[2]; }
		const Color& getBorderColor(int n = 0) const { return texture_params_[n].border_color; }
		float getLodBias(int n = 0) const { return texture_params_[n].lod_bias; }

		int actualWidth(int n = 0) const { return texture_params_[n].width; }
		int actualHeight(int n = 0) const { return texture_params_[n].height; }
		int actualDepth(int n = 0) const { return texture_params_[n].depth; }

		int width(int n = 0) const { return texture_params_[n].src_rect.w(); }
		int height(int n = 0) const { return texture_params_[n].src_rect.h(); }
		int depth(int n = 0) const { return 0; }

		int surfaceWidth(int n = 0) const { return texture_params_[n].surface_width; }
		int surfaceHeight(int n = 0) const { return texture_params_[n].surface_height; }

		virtual void clearSurfaces();

		virtual void init(int n) = 0;
		virtual void bind(int binding_point=0) = 0;
		virtual unsigned id(int n = 0) const = 0;

		virtual void update(int n, int x, int width, void* pixels) = 0;
		// Less safe version for updating a multi-texture.
		virtual void update(int n, int x, int y, int width, int height, const void* pixels) = 0;
		virtual void update2D(int n, int x, int y, int width, int height, int stride, const void* pixels) = 0;
		virtual void updateYUV(int x, int y, int width, int height, const std::vector<int>& stride, const std::vector<void*>& pixels) = 0;
		virtual void update(int n, int x, int y, int z, int width, int height, int depth, void* pixels) = 0;

		static void rebuildAll();
		static void clearTextures();

		virtual SurfacePtr extractTextureToSurface(int n = 0) const = 0;

		static TexturePtr createTexture(const variant& node);
		static TexturePtr createTexture(const std::string& filename, TextureType type=TextureType::TEXTURE_2D, int mipmap_levels=0);
		static TexturePtr createTexture(const std::string& filename, const variant& node);
		static TexturePtr createTexture(const SurfacePtr& surface, const variant& node);
		static TexturePtr createTexture(const SurfacePtr& surface);
		
		static TexturePtr createTexture1D(int width, PixelFormat::PF fmt);
		static TexturePtr createTexture2D(int width, int height, PixelFormat::PF fmt);
		static TexturePtr createTexture3D(int width, int height, int depth, PixelFormat::PF fmt);

		static TexturePtr createTextureArray(int count, int width, int height, PixelFormat::PF fmt, TextureType type);
		static TexturePtr createTextureArray(const std::vector<SurfacePtr>& surfaces, const variant& node);

		void addPalette(int index, const SurfacePtr& palette);

		int getTextureCount() const { return static_cast<int>(texture_params_.size()); }
		const SurfacePtr& getFrontSurface() const { return texture_params_.front().surface; }
		const SurfacePtr& getSurface(int n) const { return texture_params_[n].surface; }
		std::vector<SurfacePtr> getSurfaces() const;

		int getUnpackAlignment(int n = 0) const { return texture_params_[n].unpack_alignment; }
		void setUnpackAlignment(int n, int align);

		template<typename N, typename T>
		const geometry::Rect<N> getNormalisedTextureCoords(int n, const geometry::Rect<T>& r) const {
			const float w = texture_params_[n].w_ratio;
			const float h = texture_params_[n].h_ratio;
			return geometry::Rect<N>::from_coordinates(static_cast<N>(r.x())/w, static_cast<N>(r.y())/h, static_cast<N>(r.x2())/w, static_cast<N>(r.y2())/h);		
		}
		template<typename N, typename T>
		const geometry::Rect<N> getNormalizedTextureCoords(int n, const geometry::Rect<T>& r) const {
			return getNormalisedTextureCoords<N,T>(n, r);
		}

		template<typename N, typename T>
		const N getNormalisedTextureCoordW(int n, const T& x) const {
			return static_cast<N>(x) * texture_params_[n].w_ratio;
		}
		template<typename N, typename T>
		const N getNormalisedTextureCoordH(int n, const T& y) const {
			return static_cast<N>(y) * texture_params_[n].h_ratio;
		}
		template<typename N, typename T>
		const N getNormalizedTextureCoordW(int n, const T& x) const {
			return getNormalisedTextureCoordW<N,T>(n, x);
		}
		template<typename N, typename T>
		const N getNormalizedTextureCoordH(int n, const T& y) const {
			return getNormalisedTextureCoordH<N,T>(n, y);
		}

		// Translates a normalised coord to texture range [0.0, 1.0]
		template<typename T>
		float translateCoordW(int n, const T& x) const {
			return static_cast<float>(x) / static_cast<float>(texture_params_[0].width);
		}
		// Translates a normalised coord to texture range [0.0, 1.0]
		template<typename T>
		float translateCoordH(int n, const T& y) const {
			return static_cast<float>(y) / static_cast<float>(texture_params_[0].height);
		}

		// normalise and translate a width value in surface co-ordinates to texture co-ordinates.
		template<typename T>
		float getTextureCoordW(int n, const T& x) const {
			return translateCoordW<float>(n, getNormalisedTextureCoordW<float,T>(n, x));
		}
		// normalise and translate a height value in surface co-ordinates to texture co-ordinates.
		template<typename T>
		float getTextureCoordH(int n, const T& y) const {
			return translateCoordH<float>(n, getNormalisedTextureCoordH<float,T>(n, y));
		}
		// normalise and translate a pair of x, y co-ordinates.
		template<typename T>
		std::pair<float,float> getTextureCoords(int n, const T& x, const T&y) const {
			return std::make_pair<float,float>(translateCoordW<float>(n, getNormalisedTextureCoordW<float,T>(n, x)),
				translateCoordH<float>(n, getNormalisedTextureCoordH<float,T>(n, x)));
		}
		template<typename T>
		rectf getTextureCoords(int n, const geometry::Rect<T>& r) const {
			return rectf::from_coordinates(translateCoordW<float>(n, getNormalisedTextureCoordW<float,T>(n, r.x1())),
				translateCoordH<float>(n, getNormalisedTextureCoordH<float,T>(n, r.y1())),
				translateCoordW<float>(n, getNormalisedTextureCoordW<float,T>(n, r.x2())),
				translateCoordH<float>(n, getNormalisedTextureCoordH<float,T>(n, r.y2())));
		}

		// Can return nullptr if not-implemented, invalid underlying surface.
		virtual const unsigned char* colorAt(int x, int y) const = 0;

		static void clearCache();

		// Set source rect in un-normalised co-ordinates.
		void setSourceRect(int n, const rect& r);
		// Set source rect in normalised co-ordinates.
		void setSourceRectNormalised(int n, const rectf& r);

		const rectf& getSourceRectNormalised(int n = 0) const { return texture_params_[n].src_rect_norm; }
		const rect& getSourceRect(int n = 0) const { return texture_params_[n].src_rect; }

		bool isPaletteized() const { return is_paletteized_; }
		void setPalette(int n);
		void setPaletteMixing(int n1, int n2, float ratio);
		void clearPaletteMixing();
		int getPalette(int n=0) const { return n < 2 ? palette_[n] : palette_[0]; }
		Color mapPaletteColor(const Color& color, int palette);
		float getMixingRatio() const { return mix_ratio_; }
		bool shouldMixPalettes() const { return mix_palettes_; }
		bool hasPaletteAt(int n) const;

		virtual TexturePtr clone() = 0;

		bool operator==(const Texture& other) const {
			if(texture_params_.size() == other.texture_params_.size()) {
				for(int n = 0; n != static_cast<int>(texture_params_.size()); ++n) {
					// XXX how far should we got here to compare the textures?
					if(id(n) != other.id(n)) {
						return false;
					}
				}
				return true;
			}
			return false;
		}
		bool operator!=(const Texture& other) const {
			return !operator==(other);
		}

		// Helper function to extract all the image names out of a node and return them.
		static std::vector<std::string> findImageNames(const variant& node);
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
		void addSurface(SurfacePtr surf);
		void replaceSurface(int n, SurfacePtr surf);
	private:
		Texture();
		virtual void rebuild() = 0;
		virtual void handleAddPalette(int index, const SurfacePtr& palette) = 0;

		struct TextureParams {
			TextureParams()
				: surface(),
				  type(TextureType::TEXTURE_2D),
				  mipmaps(0),
				  address_mode(),
				  filtering(),
				  border_color(),
				  max_anisotropy(1),
				  lod_bias(0.0f),
				  surface_width(-1),
				  surface_height(-1),
				  width(0),
				  height(0),
				  depth(0),
				  unpack_alignment(4),
				  h_ratio(1.0f),
				  w_ratio(1.0f),
				  src_rect(),
				  src_rect_norm(0.0f, 0.0f, 1.0f, 1.0f)
			{
			}
			SurfacePtr surface;
			
			TextureType type;
			int mipmaps;
			std::array<AddressMode, 3> address_mode; // u,v,w
			std::array<Filtering, 3> filtering; // minification, magnification, mip
			Color border_color;
			int max_anisotropy;
			float lod_bias;

			int surface_width;
			int surface_height;

			// Width/Height/Depth of the created texture -- may be a 
			// different size than the surface if things like only
			// allowing power-of-two textures is in effect.
			int width;
			int height;
			int depth;
			
			int unpack_alignment;

			float h_ratio;
			float w_ratio;

			rect src_rect;
			rectf src_rect_norm;
		};
		std::vector<TextureParams> texture_params_;
		typedef std::vector<TextureParams>::iterator texture_params_iterator;

		bool is_paletteized_;
		int palette_[2];
		float mix_ratio_;
		bool mix_palettes_;
		std::map<int,int> palette_row_map_;

		void initFromVariant(texture_params_iterator tp, const variant& node);
		void internalInit(texture_params_iterator tp);
	};
}
