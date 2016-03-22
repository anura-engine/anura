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

#include "asserts.hpp"
#include "profile_timer.hpp"
#include "DisplayDevice.hpp"
#include "TextureOGL.hpp"

namespace KRE
{
	namespace
	{
		const int maximum_palette_variations = 48;

		GLenum GetGLAddressMode(Texture::AddressMode am)
		{
			switch(am) {
				case Texture::AddressMode::WRAP:	return GL_REPEAT;
				case Texture::AddressMode::CLAMP:	return GL_CLAMP_TO_EDGE;
				case Texture::AddressMode::MIRROR:	return GL_MIRRORED_REPEAT;
				case Texture::AddressMode::BORDER:	return GL_CLAMP_TO_BORDER;
			}
			return GL_CLAMP_TO_EDGE;
		}

		GLenum GetGLTextureType(TextureType tt) 
		{
			switch(tt) {
				case TextureType::TEXTURE_1D:		return GL_TEXTURE_1D;
				case TextureType::TEXTURE_2D:		return GL_TEXTURE_2D;
				case TextureType::TEXTURE_3D:		return GL_TEXTURE_3D;
				case TextureType::TEXTURE_CUBIC:	return GL_TEXTURE_CUBE_MAP;
			}
			return GL_TEXTURE_2D;
		}

		typedef std::map<unsigned, std::weak_ptr<GLuint>> texture_id_cache;
		texture_id_cache& get_id_cache()
		{
			static texture_id_cache res;
			return res;
		}

		GLuint& get_current_bound_texture()
		{
			static GLuint res = -1;
			return res;
		}
	}

	OpenGLTexture::OpenGLTexture(const variant& node, const std::vector<SurfacePtr>& surfaces)
		: Texture(node, surfaces),
		  texture_data_(),
		  is_yuv_planar_(false)
	{
		int max_tex_units = DisplayDevice::getCurrent()->queryParameteri(DisplayDeviceParameters::MAX_TEXTURE_UNITS);
		if(max_tex_units > 0) {
			ASSERT_LOG(static_cast<int>(surfaces.size()) < max_tex_units, "Number of surfaces given exceeds maximum number of texture units for this hardware.");
		}
		
		texture_data_.resize(getTextureCount());
		int n = 0;
		for(auto& surf : getSurfaces()) {
			texture_data_[n].surface_format = surf->getPixelFormat()->getFormat();
			createTexture(n);
			init(n);
			++n;
		}
	}

	OpenGLTexture::OpenGLTexture(const std::vector<SurfacePtr>& surfaces, TextureType type, int mipmap_levels)
		: Texture(surfaces, type, mipmap_levels), 
		  texture_data_(),
		  is_yuv_planar_(false)
	{
		int max_tex_units = DisplayDevice::getCurrent()->queryParameteri(DisplayDeviceParameters::MAX_TEXTURE_UNITS);
		if(max_tex_units > 0) {
			ASSERT_LOG(static_cast<int>(surfaces.size()) < max_tex_units, "Number of surfaces given exceeds maximum number of texture units for this hardware.");
		}
		texture_data_.resize(getSurfaces().size());
		int n = 0;
		for(auto& surf : surfaces) {
			texture_data_[n].surface_format = surf->getPixelFormat()->getFormat();
			createTexture(n);
			init(n);
			++n;
		}
	}

	OpenGLTexture::OpenGLTexture(int count, int width, int height, int depth, PixelFormat::PF fmt, TextureType type)
		: Texture(count, width, height, depth, fmt, type),
		  texture_data_(),
		  is_yuv_planar_(fmt == PixelFormat::PF::PIXELFORMAT_YV12 ? true : false)
	{
		int max_tex_units = DisplayDevice::getCurrent()->queryParameteri(DisplayDeviceParameters::MAX_TEXTURE_UNITS);
		if(max_tex_units > 0) {
			ASSERT_LOG(count < max_tex_units, "Number of surfaces given exceeds maximum number of texture units for this hardware.");
		}
		texture_data_.resize(count);
		for(int n = 0; n != count; ++n) {
			texture_data_[n].surface_format = fmt;
			createTexture(n);
			init(n);
		}
	}

	OpenGLTexture::~OpenGLTexture()
	{
		//int n = 0;
		//for(auto& td : texture_data_) {
		//	LOG_DEBUG("Release TexturePtr(" << n << "): id: " << *td.id << ", use_count: " << td.id.use_count());
		//	++n;
		//}
	}

	void OpenGLTexture::update(int n, int x, int width, void* pixels)
	{
		auto& td = texture_data_[n];
		ASSERT_LOG(is_yuv_planar_ == false, "Use updateYUV to update a YUV texture.");
		glBindTexture(GetGLTextureType(getType(n)), *td.id);
		get_current_bound_texture() = *td.id;
		ASSERT_LOG(getType(n) == TextureType::TEXTURE_1D, "Tried to do 1D texture update on non-1D texture");
		if(getUnpackAlignment(n) != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment(n));
		}
		glTexSubImage1D(GetGLTextureType(getType(n)), 0, x, width, td.format, td.type, pixels);
		if(getUnpackAlignment(n) != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		}
	}

	// Add a 2D update function which has single stride, but doesn't support planar YUV.
	void OpenGLTexture::update2D(int n, int x, int y, int width, int height, int stride, const void* pixels)
	{
		ASSERT_LOG(is_yuv_planar_ == false, "Use updateYUV to update a YUV texture.");
		auto& td = texture_data_[n];
		glBindTexture(GetGLTextureType(getType(n)), *td.id);
		get_current_bound_texture() = *td.id;
		ASSERT_LOG(getType(n) == TextureType::TEXTURE_2D, "Tried to do 2D texture update on non-2D texture: " << static_cast<int>(getType(n)));
		if(getUnpackAlignment(n) != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment(n));
		}
		//glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
		glTexSubImage2D(GetGLTextureType(getType(n)), 0, x, y, width, height, td.format, td.type, pixels);
		//glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		if(getUnpackAlignment(n) != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		}
	}

	void OpenGLTexture::update(int n, int x, int y, int width, int height, const void* pixels)
	{
		ASSERT_LOG(is_yuv_planar_ == false, "Use updateYUV to update a YUV texture.");
		auto& td = texture_data_[n];
		glBindTexture(GetGLTextureType(getType(n)), *td.id);
		get_current_bound_texture() = *td.id;
		ASSERT_LOG(getType(n) == TextureType::TEXTURE_2D, "Tried to do 2D texture update on non-2D texture: " << static_cast<int>(getType(n)));
		if(getUnpackAlignment(n) != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment(n));
		}
		glTexSubImage2D(GetGLTextureType(getType(n)), 0, x, y, width, height, td.format, td.type, pixels);
		if(getUnpackAlignment(n) != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		}
	}

	// Stride is the width of the image surface *in pixels*
	void OpenGLTexture::updateYUV(int x, int y, int width, int height, const std::vector<int>& stride, const std::vector<void*>& pixels)
	{
		ASSERT_LOG(is_yuv_planar_, "updateYUV called on non YUV planar texture.");
		for(int n = 2; n >= 0; --n) {
			auto& td = texture_data_[n];
			glActiveTexture(GL_TEXTURE0 + n);
			glBindTexture(GetGLTextureType(getType(n)), *td.id);
			get_current_bound_texture() = *td.id;
			if(static_cast<int>(stride.size()) > n) {
				glPixelStorei(GL_UNPACK_ROW_LENGTH, stride[n]);
			}
			if(getUnpackAlignment(n) != 4) {
				glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment(n));
			}
			switch(getType(n)) {
				case TextureType::TEXTURE_1D:
					LOG_WARN("Running 2D texture update on 1D texture.");
					ASSERT_LOG(is_yuv_planar_ == false, "Update of 1D Texture in YUV planar mode.");
					glTexSubImage1D(GL_TEXTURE_1D, 0, x, width, td.format, td.type, pixels[n]);
					break;
				case TextureType::TEXTURE_2D:
					glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, n>0?width/2:width, n>0?height/2:height, td.format, td.type, pixels[n]);
					break;
				case TextureType::TEXTURE_3D:
					ASSERT_LOG(false, "Tried to do 2D texture update on 3D texture");
				case TextureType::TEXTURE_CUBIC:
					ASSERT_LOG(false, "No support for updating cubic textures yet.");
			}
		
			if(getMipMapLevels(n) > 0 && getType(n) > TextureType::TEXTURE_1D) {
				glGenerateMipmap(GetGLTextureType(getType(n)));
			}
		}
		if(!stride.empty()) {
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		}
		if(getUnpackAlignment(0) != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		}
	}

	void OpenGLTexture::update(int n, int x, int y, int z, int width, int height, int depth, void* pixels)
	{
		ASSERT_LOG(is_yuv_planar_ == false, "3D Texture Update function called on YUV planar format.");
		auto& td = texture_data_[n];
		glBindTexture(GetGLTextureType(getType(n)), *td.id);
		get_current_bound_texture() = *td.id;
		if(getUnpackAlignment(n) != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment());
		}
		switch(getType(n)) {
			case TextureType::TEXTURE_1D:
				LOG_WARN("Running 2D texture update on 1D texture. You may get unexpected results.");
				glTexSubImage1D(GL_TEXTURE_1D, 0, x, width, td.format, td.type, pixels);
				break;
			case TextureType::TEXTURE_2D:
				LOG_WARN("Running 3D texture update on 2D texture. You may get unexpected results.");
				glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, td.format, td.type, pixels);
				break;
			case TextureType::TEXTURE_3D:
				glTexSubImage3D(GL_TEXTURE_3D, 0, x, y, z, width, height, depth, td.format, td.type, pixels);
			case TextureType::TEXTURE_CUBIC:
				ASSERT_LOG(false, "No support for updating cubic textures yet.");
		}
		if(getMipMapLevels(n) > 0 && getType(n) > TextureType::TEXTURE_1D) {
			glGenerateMipmap(GetGLTextureType(getType(n)));
		}
		if(getUnpackAlignment(n) != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		}
	}

	void OpenGLTexture::updatePaletteRow(int index, SurfacePtr new_palette_surface, int palette_width, const std::vector<glm::u8vec4>& pixels)
	{
		// write altered pixel data to texture.
		update(1, 0, index, palette_width, 1, &pixels[0]);
		// write altered pixel data to surface.
		unsigned char* px = reinterpret_cast<unsigned char*>(new_palette_surface->pixelsWriteable());
		memcpy(&px[index * new_palette_surface->rowPitch()], &pixels[0], pixels.size() * sizeof(glm::u8vec4));
	}

	void OpenGLTexture::handleAddPalette(int index, const SurfacePtr& palette)
	{
		//profile::manager pman("handleAddPalette");
		ASSERT_LOG(is_yuv_planar_ == false, "Can't create a palette for a YUV surface.");
		ASSERT_LOG(index < maximum_palette_variations, "index of (" << index << ") exceeds the maximum soft palette limit: " << maximum_palette_variations);

		if(PixelFormat::isIndexedFormat(getFrontSurface()->getPixelFormat()->getFormat())) {
			// Is already an indexed format.
			// Which means that texture_data_[0].palette should be already valid.
			const auto num_colors = texture_data_[0].palette.size();
			ASSERT_LOG(num_colors > 0, "Indexed data format but no palette present. createTexture() probably not called.");
			if(texture_data_[0].color_index_map.empty()) {
				ASSERT_LOG(static_cast<int>(texture_data_.size()) == 1, "programmer bug");
				for(size_t n = 0; n != num_colors; ++n) {
					texture_data_[0].color_index_map[texture_data_[0].palette[n]] = static_cast<int>(n);
				}
			}
		} else {
			// Create a new indexed surface.
			int sw = surfaceWidth();
			int sh = surfaceHeight();
			auto surf = Surface::create(sw, sh, PixelFormat::PF::PIXELFORMAT_INDEX8);
			int rp = surf->rowPitch();

			std::vector<uint8_t> new_pixels;
			new_pixels.resize(rp * sh);

			auto& td = texture_data_[0];
			td.palette.clear();
			getSurface(0)->iterateOverSurface([&new_pixels, rp, &td](int x, int y, int r, int g, int b, int a){
				color_histogram_type::key_type color = (static_cast<uint32_t>(r) << 24)
					| (static_cast<uint32_t>(g) << 16)
					| (static_cast<uint32_t>(b) << 8)
					| (static_cast<uint32_t>(a));

				auto it = td.color_index_map.find(color);
				if(it == td.color_index_map.end()) {
					const auto index = td.palette.size();
					td.color_index_map[color] = static_cast<uint8_t>(index);
					new_pixels[x + y * rp] = static_cast<uint8_t>(index);
					td.palette.emplace_back(color);
				} else {
					new_pixels[x + y * rp] = static_cast<uint8_t>(it->second);
				}
				ASSERT_LOG(td.palette.size() < 256, "Can't convert surface to palettized version. Too many colors in source image > 256");
			});
			surf->writePixels(&new_pixels[0], static_cast<int>(new_pixels.size()));
			surf->setAlphaMap(getSurface(0)->getAlphaMap());

			//LOG_DEBUG("adding palette '" << palette->getName() << "' to: " << getSurface(0)->getName() << ". " << td.color_index_map.size() << " colors in map");

			//LOG_INFO("handleAddPalette: Color count: " << td.palette.size());

			// save old palette
			auto old_palette = std::move(texture_data_[0].palette);
			auto histogram = std::move(texture_data_[0].color_index_map);

			// Set the surface to our new one.
			replaceSurface(0, surf);
			// Reset the existing data so we can re-create it.
			texture_data_[0] = TextureData();
			texture_data_[0].surface_format = PixelFormat::PF::PIXELFORMAT_INDEX8;
			texture_data_[0].color_index_map = std::move(histogram);
			texture_data_[0].palette = std::move(old_palette);
			createTexture(0);
			init(0);
		}

		SurfacePtr new_palette_surface;
		const auto palette_width = texture_data_[0].palette.size();

		if(texture_data_.size() > 1) {
			// Already have a palette texture we can use.
			new_palette_surface = getSurface(1);
			ASSERT_LOG(new_palette_surface != nullptr, "There was no palette surface found, when there should have been.");
		} else {
			texture_data_.resize(2);
			// We create a surface with <maximum_palette_variations> rows, this allows for a maximum of <maximum_palette_variations> palettes.
			new_palette_surface = Surface::create(static_cast<int>(texture_data_[0].palette.size()), maximum_palette_variations, PixelFormat::PF::PIXELFORMAT_RGBA8888);
			addSurface(new_palette_surface);
			texture_data_[1].surface_format = new_palette_surface->getPixelFormat()->getFormat();
			createTexture(1);
			init(1);

			// Add the original data as row 0 here.
			std::vector<glm::u8vec4> new_pixels;
			new_pixels.reserve(palette_width);
			for(auto& color : texture_data_[0].palette) {
				new_pixels.emplace_back((color >> 24) & 0xff, (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
			}
			updatePaletteRow(0, new_palette_surface, static_cast<int>(palette_width), new_pixels);
		}

		// Create altered pixel data and update the surface/texture.
		std::vector<glm::u8vec4> new_pixels;
		new_pixels.reserve(palette_width);
		// Set the new pixel data same as current data.
		for(auto color : texture_data_[0].palette) {
			new_pixels.emplace_back((color >> 24) & 0xff, (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
		}
		int colors_mapped = 0;
		if(palette->width() > palette->height()) {
			for(int x = 0; x != palette->width(); ++x) {
				Color normal_color = palette->getColorAt(x, 0);
				Color mapped_color = palette->getColorAt(x, 1);

				if(normal_color.ai() == 0) {
					continue;
				}

				//auto it = texture_data_[0].color_index_map.find(normal_color);
				auto it = texture_data_[0].color_index_map.find(normal_color.asRGBA());
				if(it != texture_data_[0].color_index_map.end()) {
					// Found the color in the color map
					new_pixels[it->second] = mapped_color.as_u8vec4();
					++colors_mapped;
				}
			}
		} else {
			for(int y = 0; y != palette->height(); ++y) {
				Color normal_color = palette->getColorAt(0, y);
				Color mapped_color = palette->getColorAt(1, y);

				if(normal_color.ai() == 0) {
					continue;
				}

				auto it = texture_data_[0].color_index_map.find(normal_color.asRGBA());
				if(it != texture_data_[0].color_index_map.end()) {
					// Found the color in the color map
					new_pixels[it->second] = mapped_color.as_u8vec4();
					++colors_mapped;
				}
			}
		}
		//LOG_INFO("Mapped " << colors_mapped << " out of " << palette_width << " colors from palette");

		updatePaletteRow(index, new_palette_surface, static_cast<int>(palette_width), new_pixels);
	}

	void OpenGLTexture::createTexture(int n)
	{
		auto& td = texture_data_[n];
		auto surf = n < static_cast<int>(getSurfaces().size()) ? getSurfaces()[n] : SurfacePtr();

		// Change the format/internalFormat/type depending on the 
		// data we now about the surface.
		// XXX these need testing for correctness.
		switch(td.surface_format) {
			case PixelFormat::PF::PIXELFORMAT_INDEX1LSB:
			case PixelFormat::PF::PIXELFORMAT_INDEX1MSB:
			case PixelFormat::PF::PIXELFORMAT_INDEX4LSB:
			case PixelFormat::PF::PIXELFORMAT_INDEX4MSB:
				ASSERT_LOG(false, "Need to deal with a transform for indexed 1-bit and 4-bit surfaces.");
				break;
			case PixelFormat::PF::PIXELFORMAT_INDEX8:
				if(texture_data_[n].palette.size() == 0) {
					//texture_data_[n].palette = getSurface(n)->getPalette();
					auto& palette = getSurface(n)->getPalette();
					texture_data_[n].palette.reserve(palette.size());
					for(auto& color : palette) {
						texture_data_[n].palette.emplace_back(color.asRGBA());	
					}
					//ASSERT_LOG(false, "Need to create a palette surface for 8-bit native index formats. Or translate to RGBA.");
				}
				td.format = GL_RED;
				td.internal_format = GL_RGBA;
				td.type = GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_R8:
				td.format = GL_RED;
				td.internal_format = GL_RGBA;
				td.type = GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB332:
				td.format = GL_RGB;
				td.internal_format = GL_R3_G3_B2;
				td.type = GL_UNSIGNED_BYTE_3_3_2;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB444:
				td.format = GL_RGB;
				td.internal_format = GL_RGB4;
				td.type = GL_UNSIGNED_SHORT;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB555:
				td.format = GL_RGB;
				td.internal_format = GL_RGB5;
				td.type = GL_UNSIGNED_SHORT;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGR555:
				td.format = GL_BGR;
				td.internal_format = GL_RGB5;
				td.type = GL_UNSIGNED_SHORT;
				break;
			case PixelFormat::PF::PIXELFORMAT_ARGB4444:
				td.format = GL_BGRA;
				td.internal_format = GL_RGBA4;
				td.type =  GL_UNSIGNED_SHORT_4_4_4_4_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGBA4444:
				td.format = GL_RGBA;
				td.internal_format = GL_RGBA4;
				td.type =  GL_UNSIGNED_SHORT_4_4_4_4;
				break;
			case PixelFormat::PF::PIXELFORMAT_ABGR4444:
				td.format = GL_RGBA;
				td.internal_format = GL_RGBA4;
				td.type =  GL_UNSIGNED_SHORT_4_4_4_4_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGRA4444:
				td.format = GL_BGRA;
				td.internal_format = GL_RGBA4;
				td.type =  GL_UNSIGNED_SHORT_4_4_4_4;
				break;
			case PixelFormat::PF::PIXELFORMAT_ARGB1555:
				td.format = GL_BGRA;
				td.internal_format = GL_RGB5_A1;
				td.type =  GL_UNSIGNED_SHORT_1_5_5_5_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGBA5551:
				td.format = GL_RGBA;
				td.internal_format = GL_RGB5_A1;
				td.type =  GL_UNSIGNED_SHORT_5_5_5_1;
				break;
			case PixelFormat::PF::PIXELFORMAT_ABGR1555:
				td.format = GL_RGBA;
				td.internal_format = GL_RGB5_A1;
				td.type =  GL_UNSIGNED_SHORT_1_5_5_5_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGRA5551:
				td.format = GL_BGRA;
				td.internal_format = GL_RGB5_A1;
				td.type =  GL_UNSIGNED_SHORT_5_5_5_1;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB565:
				td.format = GL_RGB;
				td.internal_format = GL_RGB;
				td.type =  GL_UNSIGNED_SHORT_5_6_5;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGR565:
				td.format = GL_RGB;
				td.internal_format = GL_RGB;
				td.type =  GL_UNSIGNED_SHORT_5_6_5_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB24:
				td.format = GL_RGB;
				td.internal_format = GL_RGB8;
				td.type =  GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGR24:
				td.format = GL_BGR;
				td.internal_format = GL_RGB8;
				td.type =  GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB888:
				td.format = GL_RGB;
				td.internal_format = GL_RGB8;
				td.type =  GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGBX8888:
				td.format = GL_RGBA;
				td.internal_format = GL_RGB8;
				td.type =  GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGR888:
				td.format = GL_BGR;
				td.internal_format = GL_RGB8;
				td.type =  GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGRX8888:
				td.format = GL_BGRA;
				td.internal_format = GL_RGB8;
				td.type =  GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_ARGB8888:
				td.format = GL_BGRA;
				td.internal_format = GL_RGBA8;
				td.type = GL_UNSIGNED_INT_8_8_8_8_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_XRGB8888:
				// XX not sure these are correct or not
				td.format = GL_BGRA;
				td.internal_format = GL_RGB8;
				td.type = GL_UNSIGNED_INT_8_8_8_8_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGBA8888:
				td.format = GL_RGBA;
				td.internal_format = GL_RGBA8;
				td.type = GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_ABGR8888:
				td.format = GL_RGBA;
				td.internal_format = GL_RGBA8;
				td.type = GL_UNSIGNED_INT_8_8_8_8_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGRA8888:
				td.format = GL_BGRA;
				td.internal_format = GL_RGBA;
				td.type = GL_UNSIGNED_BYTE;//GL_UNSIGNED_INT_8_8_8_8;
				break;
			case PixelFormat::PF::PIXELFORMAT_ARGB2101010:
				td.format = GL_BGRA;
				td.internal_format = GL_RGB10_A2;
				td.type = GL_UNSIGNED_INT_2_10_10_10_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB101010:
				td.format = GL_BGRA;
				td.internal_format = GL_RGB10;
				td.type = GL_UNSIGNED_INT_2_10_10_10_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_YV12:
			case PixelFormat::PF::PIXELFORMAT_IYUV:
				td.format = GL_LUMINANCE;
				td.internal_format = GL_LUMINANCE;
				td.type = GL_UNSIGNED_BYTE;
				is_yuv_planar_ = true;
				ASSERT_LOG(getType(n) == TextureType::TEXTURE_2D, "YUV style pixel format only supported for 2D textures.");
				break;
			case PixelFormat::PF::PIXELFORMAT_YUY2:
			case PixelFormat::PF::PIXELFORMAT_UYVY:
			case PixelFormat::PF::PIXELFORMAT_YVYU:
				ASSERT_LOG(false, "Still to implement YUV packed format textures");
				break;
			default:
				ASSERT_LOG(false, "Unrecognised pixel format");
		}

		if(surf != nullptr) {
			auto it = get_id_cache().find(surf->id());
			if(it != get_id_cache().end()) {
				auto cached_id = it->second.lock();
				if(cached_id != nullptr) {
					texture_data_[n].id = cached_id;
					return;
				}
				// if we couldn't lock the id fall through and create a new one
			}
		}

		GLuint new_id;
		glGenTextures(1, &new_id);
		auto id_ptr = std::shared_ptr<GLuint>(new GLuint(new_id), [](GLuint* id) { glDeleteTextures(1, id); delete id; });
		td.id = id_ptr;
		if(surf) {
			get_id_cache()[surf->id()] = id_ptr;
		}

		glBindTexture(GetGLTextureType(getType(n)), *td.id);
		get_current_bound_texture() = *td.id;

		unsigned w = is_yuv_planar_ && n>0 ? surfaceWidth(n)/2 : surfaceWidth(n);
		unsigned h = is_yuv_planar_ && n>0 ? surfaceHeight(n)/2 : surfaceHeight(n);
		unsigned d = is_yuv_planar_ && n>0 ? actualDepth(n)/2 : actualDepth(n);

		if(getUnpackAlignment(n) != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment(n));
		}

		const void* pixels = surf != nullptr ? surf->pixels() : 0;
		switch(getType(n)) {
			case TextureType::TEXTURE_1D:
				if(pixels == nullptr) {
					glTexImage1D(GL_TEXTURE_1D, 0, td.internal_format, w, 0, td.format, td.type, 0);
				} else {
					glTexImage1D(GL_TEXTURE_1D, 0, td.internal_format, surf->width(), 0, td.format, td.type, pixels);
				}
				break;
			case TextureType::TEXTURE_2D:
				if(pixels == nullptr) {
					glTexImage2D(GL_TEXTURE_2D, 0, td.internal_format, w, h, 0, td.format, td.type, 0);
				} else {
					glTexImage2D(GL_TEXTURE_2D, 0, td.internal_format, surf->width(), surf->height(), 0, td.format, td.type, pixels);
				}
				break;
			case TextureType::TEXTURE_3D:
				// XXX this isn't correct fixme.
				if(pixels == nullptr) {
					glTexImage3D(GL_TEXTURE_3D, 0, td.internal_format, w, h, d, 0, td.format, td.type, 0);
				} else {
					glTexImage3D(GL_TEXTURE_3D, 0, td.internal_format, w, h, d, 0, td.format, td.type, pixels);
				}
				break;
			case TextureType::TEXTURE_CUBIC:
				// If we are using a cubic texture 		
				ASSERT_LOG(false, "Implement texturing of cubic texture target");
		}

		if(getUnpackAlignment(n) != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		}
	}
	
	void OpenGLTexture::init(int n)
	{
		if(n < 0) {
			for(int m = 0; m != texture_data_.size(); ++m) {
				handleInit(m);
			}
		} else {
			handleInit(n);
		}
	}

	void OpenGLTexture::handleInit(int n)
	{
		auto& td = texture_data_[n];
		GLenum type = GetGLTextureType(getType(n));

		glBindTexture(type, *td.id);
		get_current_bound_texture() = *td.id;

		glTexParameteri(type, GL_TEXTURE_WRAP_S, GetGLAddressMode(getAddressModeU(n)));
		if(getAddressModeU(n) == AddressMode::BORDER) {
			glTexParameterfv(type, GL_TEXTURE_BORDER_COLOR, getBorderColor(n).asFloatVector());
		}
		if(getType(n) > TextureType::TEXTURE_1D) {
			glTexParameteri(type, GL_TEXTURE_WRAP_T, GetGLAddressMode(getAddressModeV(n)));
			if(getAddressModeV(n) == AddressMode::BORDER) {
				glTexParameterfv(type, GL_TEXTURE_BORDER_COLOR, getBorderColor(n).asFloatVector());
			}
		}
		if(getType(n) > TextureType::TEXTURE_2D) {
			glTexParameteri(type, GL_TEXTURE_WRAP_R, GetGLAddressMode(getAddressModeW(n)));
			if(getAddressModeW(n) == AddressMode::BORDER) {
				glTexParameterfv(type, GL_TEXTURE_BORDER_COLOR, getBorderColor(n).asFloatVector());
			}
		}

		if(getLodBias(n) > 1e-14 || getLodBias(n) < -1e-14) {
			glTexParameterf(type, GL_TEXTURE_LOD_BIAS, getLodBias(n));
		}
		if(getMipMapLevels(n) > 0) {
			glTexParameteri(type, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(type, GL_TEXTURE_MAX_LEVEL, getMipMapLevels(n));
		}

		if(getMipMapLevels(n) > 0 && getType(n) > TextureType::TEXTURE_1D) {
			// XXX for OGL >= 1.4 < 3 use: glTexParameteri(type, GL_GENERATE_MIPMAP, GL_TRUE)
			// XXX for OGL < 1.4 manually generate them with glTexImage2D
			// OGL >= 3 use glGenerateMipmap(type);
			glGenerateMipmap(type);
		}

		ASSERT_LOG(getFilteringMin(n) != Filtering::NONE, "'none' is not a valid choice for the minifying filter.");
		ASSERT_LOG(getFilteringMax(n) != Filtering::NONE, "'none' is not a valid choice for the maxifying filter.");
		ASSERT_LOG(getFilteringMip(n) != Filtering::ANISOTROPIC, "'anisotropic' is not a valid choice for the mip filter.");

		if(getFilteringMin() == Filtering::POINT) {
			switch(getFilteringMip(n)) {
				case Filtering::NONE: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_NEAREST); break;
				case Filtering::POINT: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST); break;
				case Filtering::LINEAR: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR); break;
				case Filtering::ANISOTROPIC: ASSERT_LOG(false, "ANISOTROPIC invalid"); break;
			}
		} else if(getFilteringMin(n) == Filtering::LINEAR || getFilteringMin(n) == Filtering::ANISOTROPIC) {
			switch(getFilteringMip(n)) {
				case Filtering::NONE: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR); break;
				case Filtering::POINT: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST); break;
				case Filtering::LINEAR: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); break;
				case Filtering::ANISOTROPIC: ASSERT_LOG(false, "ANISOTROPIC invalid"); break;
			}
		}

		if(getFilteringMax(n) == Filtering::POINT) {
			glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		} else {
			glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

		if(getFilteringMax(n) == Filtering::ANISOTROPIC || getFilteringMin(n) == Filtering::ANISOTROPIC) {
			if(GL_EXT_texture_filter_anisotropic) {
				float largest_anisotropy;
				glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largest_anisotropy);
				glTexParameterf(type, GL_TEXTURE_MAX_ANISOTROPY_EXT, largest_anisotropy > getMaxAnisotropy(n) ? getMaxAnisotropy(n) : largest_anisotropy);
			}
		}
	}

	void OpenGLTexture::bind(int binding_point) 
	{
		// XXX fix this fore multiple texture binding.
		if(get_current_bound_texture() == *texture_data_[0].id) {
			return;
		}
		int n = static_cast<int>(texture_data_.size() - 1);
		for(auto it = texture_data_.rbegin(); it != texture_data_.rend(); ++it, --n) {
			glActiveTexture(GL_TEXTURE0 + n + binding_point);
			glBindTexture(GetGLTextureType(getType(n)), *it->id);
		}
		if(binding_point == 0) {
			get_current_bound_texture() = *texture_data_[0].id;
		}
	}

	unsigned OpenGLTexture::id(int n) const
	{
		ASSERT_LOG(n < static_cast<int>(texture_data_.size()), "Requested texture id outside bounds.");
		return *texture_data_[n].id;
	}

	void OpenGLTexture::rebuild()
	{
		// Delete the old ids
		int num_tex = static_cast<int>(texture_data_.size());
		texture_data_.clear();
		texture_data_.resize(num_tex);

		// Re-create the texture
		for(int n = 0; n != num_tex; ++n) {
			createTexture(n);
			init(n);
		}
	}

	const unsigned char* OpenGLTexture::colorAt(int x, int y) const 
	{
		if(getFrontSurface() == nullptr) {
			// We could probably try a glTexImage fall-back here. But ugh, slow.
			return nullptr;
		}
		auto s = getFrontSurface();
		const unsigned char* pixels = reinterpret_cast<const unsigned char*>(s->pixels());
		return (pixels + (y*s->width() + x)*s->getPixelFormat()->bytesPerPixel());
	}

	TexturePtr OpenGLTexture::clone()
	{
		return TexturePtr(new OpenGLTexture(*this));
	}

	void OpenGLTexture::handleClearTextures()
	{
		get_id_cache().clear();
	}

	SurfacePtr OpenGLTexture::extractTextureToSurface(int n) const
	{
		auto& td = texture_data_[n];
		std::vector<uint8_t> new_data;
		
		const int stride = actualWidth() * 4;
		const int h = actualHeight();

		new_data.resize(h * stride);
		std::fill(new_data.begin(), new_data.end(), 0xcd);
		glBindTexture(GetGLTextureType(getType(n)), *td.id);
		get_current_bound_texture() = *td.id;
		glGetTexImage(GetGLTextureType(getType(n)), 
			0,
			GL_BGRA,
			GL_UNSIGNED_INT_8_8_8_8_REV,
			new_data.data());
		GLenum ok = glGetError();
		if(ok != GL_NONE) {
			LOG_ERROR("Unable to read pixels from texture, error was: " << ok);
			return nullptr;
		}
		//std::vector<uint8_t> data;
		//data.resize(new_data.size());
		//uint8_t* cp_data = data.data();
		
		//for(auto it = new_data.begin() + (h-1)*stride; it != new_data.begin(); it -= stride) {
		//	std::copy(it, it + stride, cp_data);
		//	cp_data += stride;
		//}

		return Surface::create(actualWidth(), actualHeight(), 32, stride, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000, new_data.data());
	}
}

