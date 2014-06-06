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

#include "DisplayDevice.hpp"
#include "Material.hpp"
#include "ShadersOpenGL.hpp"

namespace KRE
{
	class DisplayDeviceOpenGL : public DisplayDevice
	{
	public:
		DisplayDeviceOpenGL();
		~DisplayDeviceOpenGL();

		DisplayDeviceId ID() const override { return DISPLAY_DEVICE_OPENGL; }

		void Swap() override;
		void Clear(uint32_t clr) override;

		void SetClearColor(float r, float g, float b, float a) override;
		void SetClearColor(const Color& color) override;

		void Render(const RenderablePtr& r) override;

		CanvasPtr GetCanvas() override;

		void Init(size_t width, size_t height) override;
		void PrintDeviceInfo() override;

		virtual DisplayDeviceDataPtr CreateDisplayDeviceData(const DisplayDeviceDef& def) override;

	private:
		DisplayDeviceOpenGL(const DisplayDeviceOpenGL&);

		AttributeSetPtr HandleCreateAttributeSet(bool indexed, bool instanced) override;
		HardwareAttributePtr HandleCreateAttribute(AttributeBase* parent) override;

		RenderTargetPtr HandleCreateRenderTarget(size_t width, size_t height, 
			size_t color_plane_count, 
			bool depth, 
			bool stencil, 
			bool use_multi_sampling, 
			size_t multi_samples) override;
		RenderTargetPtr HandleCreateRenderTarget(const variant& node) override;
		void DoBlitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch) override;

		bool DoCheckForFeature(DisplayDeviceCapabilties cap) override;

		TexturePtr HandleCreateTexture(const std::string& filename, Texture::Type type, int mipmap_levels) override;
		TexturePtr HandleCreateTexture(const SurfacePtr& surface, const variant& node) override;
		TexturePtr HandleCreateTexture(const SurfacePtr& surface, Texture::Type type, int mipmap_levels) override;
		TexturePtr HandleCreateTexture(unsigned width, PixelFormat::PF fmt) override;
		TexturePtr HandleCreateTexture(unsigned width, unsigned height, PixelFormat::PF fmt, Texture::Type type=Texture::Type::TEXTURE_2D) override;
		TexturePtr HandleCreateTexture(unsigned width, unsigned height, unsigned depth, PixelFormat::PF fmt) override;

		MaterialPtr HandleCreateMaterial(const variant& node) override;
		MaterialPtr HandleCreateMaterial(const std::string& name, const std::vector<TexturePtr>& textures, const BlendMode& blend=BlendMode(), bool fog=false, bool lighting=false, bool depth_write=false, bool depth_check=false) override;
	};
}
