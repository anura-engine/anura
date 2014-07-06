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

#include "../asserts.hpp"
#include "../logger.hpp"
#include "TextureOpenGL.hpp"

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

		GLenum GetGLTextureType(Texture::Type tt) 
		{
			switch(tt) {
				case Texture::Type::TEXTURE_1D:		return GL_TEXTURE_1D;
				case Texture::Type::TEXTURE_2D:		return GL_TEXTURE_2D;
				case Texture::Type::TEXTURE_3D:		return GL_TEXTURE_3D;
				case Texture::Type::TEXTURE_CUBIC:	return GL_TEXTURE_CUBE_MAP;
			}
			return GL_TEXTURE_2D;
		}
	}

	OpenGLTexture::OpenGLTexture(const SurfacePtr& surface, const variant& node)
		: Texture(surface, node),
		format_(GL_RGBA),
		internal_format_(GL_RGBA),
		type_(GL_UNSIGNED_BYTE),
		pixel_format_(PixelFormat::PF::PIXELFORMAT_UNKNOWN),
		is_yuv_planar_(false)
	{
		CreateTexture(getSurface()->getPixelFormat()->getFormat());
		Init();
	}

	OpenGLTexture::OpenGLTexture(const SurfacePtr& surface, Type type,  int mipmap_levels)
		: Texture(surface, type, mipmap_levels), 
		format_(GL_RGBA),
		internal_format_(GL_RGBA),
		type_(GL_UNSIGNED_BYTE),
		pixel_format_(PixelFormat::PF::PIXELFORMAT_UNKNOWN),
		is_yuv_planar_(false)
	{
		CreateTexture(getSurface()->getPixelFormat()->getFormat());
		Init();
	}

	OpenGLTexture::OpenGLTexture(unsigned width, 
		unsigned height, 
		PixelFormat::PF fmt, 
		Type type, 
		unsigned depth)
		: Texture(width, height, depth, fmt, type),
		format_(GL_RGBA),
		internal_format_(GL_RGBA),
		type_(GL_UNSIGNED_BYTE),
		pixel_format_(PixelFormat::PF::PIXELFORMAT_UNKNOWN),
		is_yuv_planar_(false)
	{
		SetTextureDimensions(width, height, depth);
		CreateTexture(fmt);
		Init();
	}

	OpenGLTexture::~OpenGLTexture()
	{
		glDeleteTextures(is_yuv_planar_ ? 3 : 1, &texture_id_[0]);
	}

	void OpenGLTexture::Update(int x, unsigned width, void* pixels)
	{
		ASSERT_LOG(is_yuv_planar_ == false, "1D Texture Update function called on YUV planar format.");
		glBindTexture(GetGLTextureType(GetType()), texture_id_[0]);
		ASSERT_LOG(GetType() == Type::TEXTURE_1D, "Tried to do 1D texture update on non-1D texture");
		if(getUnpackAlignment() != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment());
		}
		glTexSubImage1D(GetGLTextureType(GetType()), 0, x, width, format_, type_, pixels);
	}

	// Add a 2D update function which has single stride, but doesn't support planar YUV.

	// Stride is the width of the image surface *in pixels*
	void OpenGLTexture::Update(int x, int y, unsigned width, unsigned height, const std::vector<unsigned>& stride, void* pixels)
	{
		int num_textures = is_yuv_planar_ ? 2 : 0;
		for(int n = num_textures; n >= 0; --n) {
			glBindTexture(GetGLTextureType(GetType()), texture_id_[n]);
			if(stride.size() > size_t(n)) {
				glPixelStorei(GL_UNPACK_ROW_LENGTH, stride[n]);
			}
			if(getUnpackAlignment() != 4) {
				glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment());
			}
			switch(GetType()) {
				case Type::TEXTURE_1D:
					LOG_WARN("Running 2D texture update on 1D texture.");
					ASSERT_LOG(is_yuv_planar_ == false, "Update of 1D Texture in YUV planar mode.");
					glTexSubImage1D(GetGLTextureType(GetType()), 0, x, width, format_, type_, pixels);
					break;
				case Type::TEXTURE_2D:
					glTexSubImage2D(GetGLTextureType(GetType()), 0, x, y, n>0?width/2:width, n>0?height/2:height, format_, type_, pixels);
					break;
				case Type::TEXTURE_3D:
					ASSERT_LOG(false, "Tried to do 2D texture update on 3D texture");
				case Type::TEXTURE_CUBIC:
					ASSERT_LOG(false, "No support for updating cubic textures yet.");
			}
		
			if(GetMipMapLevels() > 0 && GetType() > Type::TEXTURE_1D) {
				glGenerateMipmap(GetGLTextureType(GetType()));
			}
		}
		if(!stride.empty()) {
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	}

	void OpenGLTexture::Update(int x, int y, int z, unsigned width, unsigned height, unsigned depth, void* pixels)
	{
		ASSERT_LOG(is_yuv_planar_ == false, "3D Texture Update function called on YUV planar format.");
		glBindTexture(GetGLTextureType(GetType()), texture_id_[0]);
		if(getUnpackAlignment() != 4) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment());
		}
		switch(GetType()) {
			case Type::TEXTURE_1D:
				LOG_WARN("Running 2D texture update on 1D texture. You may get unexpected results.");
				glTexSubImage1D(GetGLTextureType(GetType()), 0, x, width, format_, type_, pixels);
				break;
			case Type::TEXTURE_2D:
				LOG_WARN("Running 3D texture update on 2D texture. You may get unexpected results.");
				glTexSubImage2D(GetGLTextureType(GetType()), 0, x, y, width, height, format_, type_, pixels);
				break;
			case Type::TEXTURE_3D:
				glTexSubImage3D(GetGLTextureType(GetType()), 0, x, y, z, width, height, depth, format_, type_, pixels);
			case Type::TEXTURE_CUBIC:
				ASSERT_LOG(false, "No support for updating cubic textures yet.");
		}
		if(GetMipMapLevels() > 0 && GetType() > Type::TEXTURE_1D) {
			glGenerateMipmap(GetGLTextureType(GetType()));
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	}

	void OpenGLTexture::CreateTexture(const PixelFormat::PF& fmt)
	{
		// Set the pixel format being used.
		pixel_format_ = fmt;

		// Change the format/internalFormat/type depending on the 
		// data we now about the surface.
		// XXX these need testing for correctness.
		switch(fmt) {
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
				ASSERT_LOG(GetType() == Type::TEXTURE_2D, "YUV style pixel format only supported for 2D textures.");
				break;
			case PixelFormat::PF::PIXELFORMAT_YUY2:
			case PixelFormat::PF::PIXELFORMAT_UYVY:
			case PixelFormat::PF::PIXELFORMAT_YVYU:
				ASSERT_LOG(false, "Still to implement YUV packed format textures");
				break;
			default:
				ASSERT_LOG(false, "Unrecognised pixel format");
		}

		int num_textures = is_yuv_planar_ ? 3 : 1;
		glGenTextures(num_textures, &texture_id_[0]);
		for(int n = 0; n != num_textures; ++n) {
			glBindTexture(GetGLTextureType(GetType()), texture_id_[n]);

			unsigned w = n>0 ? width()/2 : width();
			unsigned h = n>0 ? height()/2 : height();
			unsigned d = n>0 ? depth()/2 : depth();

			if(getUnpackAlignment() != 4) {
				glPixelStorei(GL_UNPACK_ALIGNMENT, getUnpackAlignment());
			}

			const void* pixels = getSurface() ? getSurface()->pixels() : 0;
			switch(GetType()) {
				case Type::TEXTURE_1D:
					glTexImage1D(GetGLTextureType(GetType()), 0, internal_format_, w, 0, format_, type_, pixels);
					break;
				case Type::TEXTURE_2D:
					glTexImage2D(GetGLTextureType(GetType()), 0, internal_format_, w, h, 0, format_, type_, pixels);
					break;
				case Type::TEXTURE_3D:
					glTexImage3D(GetGLTextureType(GetType()), 0, internal_format_, w, h, d, 0, format_, type_, pixels);
					break;
				case Type::TEXTURE_CUBIC:
					// If we are using a cubic texture 		
					ASSERT_LOG(false, "Implement texturing of cubic texture target");
			}
		}
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	}

	void OpenGLTexture::Init()
	{
		GLenum type = GetGLTextureType(GetType());

		unsigned num_textures = is_yuv_planar_ ? 3 : 1;
		for(unsigned n = 0; n != num_textures; ++n) {
			glBindTexture(type, texture_id_[n]);

			glTexParameteri(type, GL_TEXTURE_WRAP_S, GetGLAddressMode(GetAddressModeU()));
			if(GetAddressModeU() == AddressMode::BORDER) {
				glTexParameterfv(type, GL_TEXTURE_BORDER_COLOR, GetBorderColor().asFloatVector());
			}
			if(GetType() > Type::TEXTURE_1D) {
				glTexParameteri(type, GL_TEXTURE_WRAP_T, GetGLAddressMode(GetAddressModeV()));
				if(GetAddressModeV() == AddressMode::BORDER) {
					glTexParameterfv(type, GL_TEXTURE_BORDER_COLOR, GetBorderColor().asFloatVector());
				}
			}
			if(GetType() > Type::TEXTURE_2D) {
				glTexParameteri(type, GL_TEXTURE_WRAP_R, GetGLAddressMode(GetAddressModeW()));
				if(GetAddressModeW() == AddressMode::BORDER) {
					glTexParameterfv(type, GL_TEXTURE_BORDER_COLOR, GetBorderColor().asFloatVector());
				}
			}

			if(GetLodBias() > 1e-14 || GetLodBias() < -1e-14) {
				glTexParameterf(type, GL_TEXTURE_LOD_BIAS, GetLodBias());
			}
			if(GetMipMapLevels() > 0) {
				glTexParameteri(type, GL_TEXTURE_BASE_LEVEL, 0);
				glTexParameteri(type, GL_TEXTURE_MAX_LEVEL, GetMipMapLevels());
			}

			if(GetMipMapLevels() > 0 && GetType() > Type::TEXTURE_1D) {
				// XXX for OGL >= 1.4 < 3 use: glTexParameteri(type, GL_GENERATE_MIPMAP, GL_TRUE)
				// XXX for OGL < 1.4 manually generate them with glTexImage2D
				// OGL >= 3 use glGenerateMipmap(type);
				glGenerateMipmap(type);
			}

			ASSERT_LOG(GetFilteringMin() != Filtering::NONE, "'none' is not a valid choice for the minifying filter.");
			ASSERT_LOG(GetFilteringMax() != Filtering::NONE, "'none' is not a valid choice for the maxifying filter.");
			ASSERT_LOG(GetFilteringMip() != Filtering::ANISOTROPIC, "'anisotropic' is not a valid choice for the mip filter.");

			if(GetFilteringMin() == Filtering::POINT) {
				switch(GetFilteringMip()) {
					case Filtering::NONE: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_NEAREST); break;
					case Filtering::POINT: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST); break;
					case Filtering::LINEAR: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR); break;
					case Filtering::ANISOTROPIC: ASSERT_LOG(false, "ANISOTROPIC invalid"); break;
				}
			} else if(GetFilteringMin() == Filtering::LINEAR || GetFilteringMin() == Filtering::ANISOTROPIC) {
				switch(GetFilteringMip()) {
					case Filtering::NONE: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR); break;
					case Filtering::POINT: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST); break;
					case Filtering::LINEAR: glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); break;
					case Filtering::ANISOTROPIC: ASSERT_LOG(false, "ANISOTROPIC invalid"); break;
				}
			}

			if(GetFilteringMax() == Filtering::POINT) {
				glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			} else {
				glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}

			if(GetFilteringMax() == Filtering::ANISOTROPIC || GetFilteringMin() == Filtering::ANISOTROPIC) {
				if(GL_EXT_texture_filter_anisotropic) {
					float largest_anisotropy;
					glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largest_anisotropy);
					glTexParameterf(type, GL_TEXTURE_MAX_ANISOTROPY_EXT, largest_anisotropy > GetMaxAnisotropy() ? GetMaxAnisotropy() : largest_anisotropy);
				}
			}
		}
	}

	void OpenGLTexture::Bind() 
	{ 
		if(is_yuv_planar_) {
			int num_textures = is_yuv_planar_ ? 2 : 0;
			for(int n = num_textures; n > 0; --n) {
				glActiveTexture(GL_TEXTURE0 + n); 			
				glBindTexture(GetGLTextureType(GetType()), texture_id_[n]);
			}
			glActiveTexture(GL_TEXTURE0);
		}
		glBindTexture(GetGLTextureType(GetType()), texture_id_[0]);
	}

	unsigned OpenGLTexture::ID()
	{
		return texture_id_[0];
	}

	void OpenGLTexture::Rebuild()
	{
		// Delete the old id
		glDeleteTextures(is_yuv_planar_ ? 3 : 1, &texture_id_[0]);

		// Re-create the texture
		CreateTexture(pixel_format_);
		Init();
	}
}
