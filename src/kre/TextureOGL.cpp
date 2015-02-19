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
#include "TextureOGL.hpp"

namespace KRE
{
	namespace
	{
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
	}

	OpenGLTexture::OpenGLTexture(const variant& node, const std::vector<SurfacePtr>& surfaces)
		: Texture(node, surfaces),
		  format_(GL_RGBA),
		  internal_format_(GL_RGBA),
		  type_(GL_UNSIGNED_BYTE),
		  pixel_format_(PixelFormat::PF::PIXELFORMAT_UNKNOWN),
		  is_yuv_planar_(false)
	{
		texture_id_.resize(getSurfaces().size());
		createTexture(getFrontSurface()->getPixelFormat()->getFormat());
		init();
	}

	OpenGLTexture::OpenGLTexture(const std::vector<SurfacePtr>& surfaces, TextureType type, int mipmap_levels)
		: Texture(surfaces, type, mipmap_levels), 
		  format_(GL_RGBA),
		  internal_format_(GL_RGBA),
		  type_(GL_UNSIGNED_BYTE),
		  pixel_format_(PixelFormat::PF::PIXELFORMAT_UNKNOWN),
		  is_yuv_planar_(false)
	{
		texture_id_.resize(surfaces.size());
		createTexture(getFrontSurface()->getPixelFormat()->getFormat());
		init();
	}

	OpenGLTexture::OpenGLTexture(int count,
		int width, 
		int height, 
		PixelFormat::PF fmt, 
		TextureType type, 
		unsigned depth)
		: Texture(count, width, height, depth, fmt, type),
		  format_(GL_RGBA),
		  internal_format_(GL_RGBA),
		  type_(GL_UNSIGNED_BYTE),
		  pixel_format_(PixelFormat::PF::PIXELFORMAT_UNKNOWN),
		  is_yuv_planar_(false)
	{
		texture_id_.resize(count);
		setTextureDimensions(width, height, depth);
		createTexture(fmt);
		init();
	}

	OpenGLTexture::OpenGLTexture(const SurfacePtr& surf, SurfacePtr palette)
		: Texture(surf, palette),
		  format_(GL_RGBA),
		  internal_format_(GL_RGBA),
		  type_(GL_UNSIGNED_BYTE),
		  pixel_format_(PixelFormat::PF::PIXELFORMAT_UNKNOWN),
		  is_yuv_planar_(false)
	{
		texture_id_.resize(1);
		createTexture(getFrontSurface()->getPixelFormat()->getFormat());
		init();
		ASSERT_LOG(false, "OpenGLTexture -- deal with surfaces with palette surface");
	}

	OpenGLTexture::~OpenGLTexture()
	{
		glDeleteTextures(texture_id_.size(), &texture_id_[0]);
	}

	void OpenGLTexture::update(int x, unsigned width, void* pixels)
	{
		ASSERT_LOG(is_yuv_planar_ == false, "1D Texture Update function called on YUV planar format.");
		glBindTexture(GetGLTextureType(getType()), texture_id_[0]);
		ASSERT_LOG(getType() == TextureType::TEXTURE_1D, "Tried to do 1D texture update on non-1D texture");
		if(getUnpackAlignment() != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment());
		}
		glTexSubImage1D(GetGLTextureType(getType()), 0, x, width, format_, type_, pixels);
	}

	// Add a 2D update function which has single stride, but doesn't support planar YUV.

	void OpenGLTexture::update(int x, int y, unsigned width, unsigned height, const int* stride, const void* pixels)
	{
		ASSERT_LOG(false, "XXX: void OpenGLTexture::update(int x, int y, unsigned width, unsigned height, const int* stride, const void* pixels)");
	}

	// Stride is the width of the image surface *in pixels*
	void OpenGLTexture::update(int x, int y, unsigned width, unsigned height, const std::vector<unsigned>& stride, const void* pixels)
	{
		int num_textures = is_yuv_planar_ ? 2 : 0;
		for(int n = num_textures; n >= 0; --n) {
			glBindTexture(GetGLTextureType(getType()), texture_id_[n]);
			if(stride.size() > size_t(n)) {
				glPixelStorei(GL_UNPACK_ROW_LENGTH, stride[n]);
			}
			if(getUnpackAlignment() != 4) {
				glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment());
			}
			switch(getType()) {
				case TextureType::TEXTURE_1D:
					LOG_WARN("Running 2D texture update on 1D texture.");
					ASSERT_LOG(is_yuv_planar_ == false, "Update of 1D Texture in YUV planar mode.");
					glTexSubImage1D(GetGLTextureType(getType()), 0, x, width, format_, type_, pixels);
					break;
				case TextureType::TEXTURE_2D:
					glTexSubImage2D(GetGLTextureType(getType()), 0, x, y, n>0?width/2:width, n>0?height/2:height, format_, type_, pixels);
					break;
				case TextureType::TEXTURE_3D:
					ASSERT_LOG(false, "Tried to do 2D texture update on 3D texture");
				case TextureType::TEXTURE_CUBIC:
					ASSERT_LOG(false, "No support for updating cubic textures yet.");
			}
		
			if(getMipMapLevels() > 0 && getType() > TextureType::TEXTURE_1D) {
				glGenerateMipmap(GetGLTextureType(getType()));
			}
		}
		if(!stride.empty()) {
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	}

	void OpenGLTexture::update(int x, int y, int z, unsigned width, unsigned height, unsigned depth, void* pixels)
	{
		ASSERT_LOG(is_yuv_planar_ == false, "3D Texture Update function called on YUV planar format.");
		glBindTexture(GetGLTextureType(getType()), texture_id_[0]);
		if(getUnpackAlignment() != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment());
		}
		switch(getType()) {
			case TextureType::TEXTURE_1D:
				LOG_WARN("Running 2D texture update on 1D texture. You may get unexpected results.");
				glTexSubImage1D(GetGLTextureType(getType()), 0, x, width, format_, type_, pixels);
				break;
			case TextureType::TEXTURE_2D:
				LOG_WARN("Running 3D texture update on 2D texture. You may get unexpected results.");
				glTexSubImage2D(GetGLTextureType(getType()), 0, x, y, width, height, format_, type_, pixels);
				break;
			case TextureType::TEXTURE_3D:
				glTexSubImage3D(GetGLTextureType(getType()), 0, x, y, z, width, height, depth, format_, type_, pixels);
			case TextureType::TEXTURE_CUBIC:
				ASSERT_LOG(false, "No support for updating cubic textures yet.");
		}
		if(getMipMapLevels() > 0 && getType() > TextureType::TEXTURE_1D) {
			glGenerateMipmap(GetGLTextureType(getType()));
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	}

	void OpenGLTexture::createTexture(const PixelFormat::PF& fmt)
	{
		// Set the pixel format being used.
		pixel_format_ = fmt;

		if(fmt == PixelFormat::PF::PIXELFORMAT_INDEX1LSB
			|| fmt == PixelFormat::PF::PIXELFORMAT_INDEX1MSB
			|| fmt == PixelFormat::PF::PIXELFORMAT_INDEX4LSB
			|| fmt == PixelFormat::PF::PIXELFORMAT_INDEX4MSB
			|| fmt == PixelFormat::PF::PIXELFORMAT_INDEX8) {
			for(auto& s : getSurfaces()) {
				s->convertInPlace(PixelFormat::PF::PIXELFORMAT_ARGB8888);
			}
			pixel_format_ = getFrontSurface()->getPixelFormat()->getFormat();
		}


		// Change the format/internalFormat/type depending on the 
		// data we now about the surface.
		// XXX these need testing for correctness.
		switch(pixel_format_) {
			case PixelFormat::PF::PIXELFORMAT_INDEX1LSB:
			case PixelFormat::PF::PIXELFORMAT_INDEX1MSB:
			case PixelFormat::PF::PIXELFORMAT_INDEX4LSB:
			case PixelFormat::PF::PIXELFORMAT_INDEX4MSB:
			case PixelFormat::PF::PIXELFORMAT_INDEX8:
				ASSERT_LOG(false, "Invalid pixel format given, indexed formats no supported.");
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB332:
				format_ = GL_RGB;
				internal_format_ = GL_R3_G3_B2;
				type_ = GL_UNSIGNED_BYTE_3_3_2;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB444:
				format_ = GL_RGB;
				internal_format_ = GL_RGB4;
				type_ = GL_UNSIGNED_SHORT;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB555:
				format_ = GL_RGB;
				internal_format_ = GL_RGB5;
				type_ = GL_UNSIGNED_SHORT;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGR555:
				format_ = GL_BGR;
				internal_format_ = GL_RGB4;
				type_ = GL_UNSIGNED_SHORT;
				break;
			case PixelFormat::PF::PIXELFORMAT_ARGB4444:
				format_ = GL_BGRA;
				internal_format_ = GL_RGBA4;
				type_ =  GL_UNSIGNED_SHORT_4_4_4_4_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGBA4444:
				format_ = GL_RGBA;
				internal_format_ = GL_RGBA4;
				type_ =  GL_UNSIGNED_SHORT_4_4_4_4;
				break;
			case PixelFormat::PF::PIXELFORMAT_ABGR4444:
				format_ = GL_RGBA;
				internal_format_ = GL_RGBA4;
				type_ =  GL_UNSIGNED_SHORT_4_4_4_4_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGRA4444:
				format_ = GL_BGRA;
				internal_format_ = GL_RGBA4;
				type_ =  GL_UNSIGNED_SHORT_4_4_4_4;
				break;
			case PixelFormat::PF::PIXELFORMAT_ARGB1555:
				format_ = GL_BGRA;
				internal_format_ = GL_RGB5_A1;
				type_ =  GL_UNSIGNED_SHORT_1_5_5_5_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGBA5551:
				format_ = GL_RGBA;
				internal_format_ = GL_RGB5_A1;
				type_ =  GL_UNSIGNED_SHORT_5_5_5_1;
				break;
			case PixelFormat::PF::PIXELFORMAT_ABGR1555:
				format_ = GL_RGBA;
				internal_format_ = GL_RGB5_A1;
				type_ =  GL_UNSIGNED_SHORT_1_5_5_5_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGRA5551:
				format_ = GL_BGRA;
				internal_format_ = GL_RGB5_A1;
				type_ =  GL_UNSIGNED_SHORT_5_5_5_1;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB565:
				format_ = GL_RGB;
				internal_format_ = GL_RGB;
				type_ =  GL_UNSIGNED_SHORT_5_6_5;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGR565:
				format_ = GL_RGB;
				internal_format_ = GL_RGB;
				type_ =  GL_UNSIGNED_SHORT_5_6_5_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB24:
				format_ = GL_RGB;
				internal_format_ = GL_RGB8;
				type_ =  GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGR24:
				format_ = GL_BGR;
				internal_format_ = GL_RGB8;
				type_ =  GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB888:
				format_ = GL_RGB;
				internal_format_ = GL_RGB8;
				type_ =  GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGBX8888:
				format_ = GL_RGB;
				internal_format_ = GL_RGB8;
				type_ =  GL_UNSIGNED_INT_8_8_8_8;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGR888:
				format_ = GL_BGR;
				internal_format_ = GL_RGB8;
				type_ =  GL_UNSIGNED_BYTE;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGRX8888:
				format_ = GL_BGRA;
				internal_format_ = GL_RGB8;
				type_ =  GL_UNSIGNED_INT_8_8_8_8;
				break;
			case PixelFormat::PF::PIXELFORMAT_ARGB8888:
				format_ = GL_BGRA;
				internal_format_ = GL_RGBA8;
				type_ = GL_UNSIGNED_INT_8_8_8_8_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_XRGB8888:
				// XX not sure these are correct or not
				format_ = GL_BGRA;
				internal_format_ = GL_RGB8;
				type_ = GL_UNSIGNED_INT_8_8_8_8_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGBA8888:
				format_ = GL_RGBA;
				internal_format_ = GL_RGBA8;
				type_ = GL_UNSIGNED_INT_8_8_8_8;
				break;
			case PixelFormat::PF::PIXELFORMAT_ABGR8888:
				format_ = GL_RGBA;
				internal_format_ = GL_RGBA8;
				type_ = GL_UNSIGNED_INT_8_8_8_8_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_BGRA8888:
				format_ = GL_BGRA;
				internal_format_ = GL_RGBA8;
				type_ = GL_UNSIGNED_INT_8_8_8_8;
				break;
			case PixelFormat::PF::PIXELFORMAT_ARGB2101010:
				format_ = GL_BGRA;
				internal_format_ = GL_RGBA8;
				type_ = GL_UNSIGNED_INT_2_10_10_10_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_RGB101010:
				format_ = GL_BGRA;
				internal_format_ = GL_RGB10;
				type_ = GL_UNSIGNED_INT_2_10_10_10_REV;
				break;
			case PixelFormat::PF::PIXELFORMAT_YV12:
			case PixelFormat::PF::PIXELFORMAT_IYUV:
				format_ = GL_LUMINANCE;
				internal_format_ = GL_LUMINANCE;
				type_ = GL_UNSIGNED_BYTE;
				is_yuv_planar_ = true;
				ASSERT_LOG(getType() == TextureType::TEXTURE_2D, "YUV style pixel format only supported for 2D textures.");
				break;
			case PixelFormat::PF::PIXELFORMAT_YUY2:
			case PixelFormat::PF::PIXELFORMAT_UYVY:
			case PixelFormat::PF::PIXELFORMAT_YVYU:
				ASSERT_LOG(false, "Still to implement YUV packed format textures");
				break;
			default:
				ASSERT_LOG(false, "Unrecognised pixel format");
		}

		if(is_yuv_planar_) {
			texture_id_.resize(3);
		}
		int num_textures = texture_id_.size();
		glGenTextures(num_textures, &texture_id_[0]);
		for(int n = 0; n != num_textures; ++n) {
			glBindTexture(GetGLTextureType(getType()), texture_id_[n]);

			unsigned w = is_yuv_planar_ && n>0 ? width()/2 : width();
			unsigned h = is_yuv_planar_ && n>0 ? height()/2 : height();
			unsigned d = is_yuv_planar_ && n>0 ? depth()/2 : depth();

			if(getUnpackAlignment() != 4) {
				glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment());
			}

			const void* pixels = n < static_cast<int>(getSurfaces().size()) && getSurfaces()[n] ? getSurfaces()[n]->pixels() : 0;
			switch(getType()) {
				case TextureType::TEXTURE_1D:
					if(pixels == nullptr) {
						glTexImage1D(GetGLTextureType(getType()), 0, internal_format_, w, 0, format_, type_, 0);
					} else {
						glTexImage1D(GetGLTextureType(getType()), 0, internal_format_, getSurfaces()[n]->width(), 0, format_, type_, pixels);
					}
					break;
				case TextureType::TEXTURE_2D:
					if(pixels == nullptr) {
						glTexImage2D(GetGLTextureType(getType()), 0, internal_format_, w, h, 0, format_, type_, 0);
					} else {
						glTexImage2D(GetGLTextureType(getType()), 0, internal_format_, getSurfaces()[n]->width(), getSurfaces()[n]->height(), 0, format_, type_, pixels);
					}
					break;
				case TextureType::TEXTURE_3D:
					// XXX this isn't correct fixme.
					if(pixels == nullptr) {
						glTexImage3D(GetGLTextureType(getType()), 0, internal_format_, w, h, d, 0, format_, type_, 0);
					} else {
						glTexImage3D(GetGLTextureType(getType()), 0, internal_format_, w, h, d, 0, format_, type_, pixels);
					}
					break;
				case TextureType::TEXTURE_CUBIC:
					// If we are using a cubic texture 		
					ASSERT_LOG(false, "Implement texturing of cubic texture target");
			}
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	}

	void OpenGLTexture::init()
	{
		GLenum type = GetGLTextureType(getType());

		for(unsigned n = 0; n != texture_id_.size(); ++n) {
			glBindTexture(type, texture_id_[n]);

			glTexParameteri(type, GL_TEXTURE_WRAP_S, GetGLAddressMode(getAddressModeU()));
			if(getAddressModeU() == AddressMode::BORDER) {
				glTexParameterfv(type, GL_TEXTURE_BORDER_COLOR, getBorderColor().asFloatVector());
			}
			if(getType() > TextureType::TEXTURE_1D) {
				glTexParameteri(type, GL_TEXTURE_WRAP_T, GetGLAddressMode(getAddressModeV()));
				if(getAddressModeV() == AddressMode::BORDER) {
					glTexParameterfv(type, GL_TEXTURE_BORDER_COLOR, getBorderColor().asFloatVector());
				}
			}
			if(getType() > TextureType::TEXTURE_2D) {
				glTexParameteri(type, GL_TEXTURE_WRAP_R, GetGLAddressMode(getAddressModeW()));
				if(getAddressModeW() == AddressMode::BORDER) {
					glTexParameterfv(type, GL_TEXTURE_BORDER_COLOR, getBorderColor().asFloatVector());
				}
			}

			if(getLodBias() > 1e-14 || getLodBias() < -1e-14) {
				glTexParameterf(type, GL_TEXTURE_LOD_BIAS, getLodBias());
			}
			if(getMipMapLevels() > 0) {
				glTexParameteri(type, GL_TEXTURE_BASE_LEVEL, 0);
				glTexParameteri(type, GL_TEXTURE_MAX_LEVEL, getMipMapLevels());
			}

			if(getMipMapLevels() > 0 && getType() > TextureType::TEXTURE_1D) {
				// XXX for OGL >= 1.4 < 3 use: glTexParameteri(type, GL_GENERATE_MIPMAP, GL_TRUE)
				// XXX for OGL < 1.4 manually generate them with glTexImage2D
				// OGL >= 3 use glGenerateMipmap(type);
				glGenerateMipmap(type);
			}

			ASSERT_LOG(getFilteringMin() != Filtering::NONE, "'none' is not a valid choice for the minifying filter.");
			ASSERT_LOG(getFilteringMax() != Filtering::NONE, "'none' is not a valid choice for the maxifying filter.");
			ASSERT_LOG(getFilteringMip() != Filtering::ANISOTROPIC, "'anisotropic' is not a valid choice for the mip filter.");

			if(getFilteringMin() == Filtering::POINT) {
				switch(getFilteringMip()) {
					case Filtering::NONE: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_NEAREST); break;
					case Filtering::POINT: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST); break;
					case Filtering::LINEAR: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR); break;
					case Filtering::ANISOTROPIC: ASSERT_LOG(false, "ANISOTROPIC invalid"); break;
				}
			} else if(getFilteringMin() == Filtering::LINEAR || getFilteringMin() == Filtering::ANISOTROPIC) {
				switch(getFilteringMip()) {
					case Filtering::NONE: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR); break;
					case Filtering::POINT: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST); break;
					case Filtering::LINEAR: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); break;
					case Filtering::ANISOTROPIC: ASSERT_LOG(false, "ANISOTROPIC invalid"); break;
				}
			}

			if(getFilteringMax() == Filtering::POINT) {
				glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			} else {
				glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}

			if(getFilteringMax() == Filtering::ANISOTROPIC || getFilteringMin() == Filtering::ANISOTROPIC) {
				if(GL_EXT_texture_filter_anisotropic) {
					float largest_anisotropy;
					glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largest_anisotropy);
					glTexParameterf(type, GL_TEXTURE_MAX_ANISOTROPY_EXT, largest_anisotropy > getMaxAnisotropy() ? getMaxAnisotropy() : largest_anisotropy);
				}
			}
		}
	}

	void OpenGLTexture::bind() 
	{
		for(int n = static_cast<int>(texture_id_.size()) - 1; n >= 0; --n) {
			glActiveTexture(GL_TEXTURE0 + n);
			glBindTexture(GetGLTextureType(getType()), texture_id_[n]);
		}
	}

	unsigned OpenGLTexture::id(int n)
	{
		ASSERT_LOG(n < static_cast<int>(texture_id_.size()), "Requested texture id outside bounds.");
		return texture_id_[n];
	}

	void OpenGLTexture::rebuild()
	{
		// Delete the old ids
		glDeleteTextures(texture_id_.size(), &texture_id_[0]);

		// Re-create the texture
		createTexture(pixel_format_);
		init();
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
}
