/*
	Copyright (C) 2013-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "SDL_opengles2.h"

#include "Texture.hpp"

namespace KRE
{
	class TextureGLESv2 : public Texture
	{
	public:
		explicit TextureGLESv2(const variant& node, const std::vector<SurfacePtr>& surfaces);
		explicit TextureGLESv2(const std::vector<SurfacePtr>& surfaces, TextureType type, int mipmap_levels);
		explicit TextureGLESv2(int count, int width, int height, int depth, PixelFormat::PF fmt, TextureType type);
		virtual ~TextureGLESv2();

		void bind(int binding_point) override;

		void init(int n) override;
		unsigned id(int n) const override;

		void update(int n, int x, int width, void* pixels) override;
		void update(int n, int x, int y, int width, int height, const void* pixels) override;
		void update2D(int n, int x, int y, int width, int height, int stride, const void* pixels) override;
		void updateYUV(int x, int y, int width, int height, const std::vector<int>& stride, const std::vector<void*>& pixels) override;
		void update(int n, int x, int y, int z, int width, int height, int depth, void* pixels) override;

		SurfacePtr extractTextureToSurface(int n) const override;

		const unsigned char* colorAt(int x, int y) const override;

		TexturePtr clone() override;
		static void handleClearTextures();
	private:
		void createTexture(int n);
		void updatePaletteRow(int index, SurfacePtr new_palette_surface, int palette_width, const std::vector<glm::u8vec4>& pixels);
		void rebuild() override;
		void handleAddPalette(int index, const SurfacePtr& palette) override;
		void handleInit(int n);

		// For YUV family textures we need two more texture id's
		// since we hold them in seperate textures.
		// XXX if we're copying a texture we want to use a shared pointer so
		// we don't accidentally delete textures that are potentially still in use.
		// Still deciding whether to use a vector of shared_ptr<GLuint>
		// Whether to store the textures in a registry, with ref-counting.
		// or what we do here.
		struct TextureData {
			TextureData() 
				: id(), 
				  surface_format(PixelFormat::PF::PIXELFORMAT_UNKNOWN), 
				  palette(), 
				  color_index_map(),
				  format(GL_RGBA), 
				  internal_format(GL_RGBA), 
				  type(GL_UNSIGNED_BYTE)
			{
			}
			std::shared_ptr<GLuint> id;
			PixelFormat::PF surface_format;
			std::vector<color_histogram_type::key_type> palette;
			color_histogram_type color_index_map;
			GLenum format;
			GLenum internal_format;
			GLenum type;
		};
		std::vector<TextureData> texture_data_;

		// Set for YUV style textures;
		bool is_yuv_planar_;
	};
}
