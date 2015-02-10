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

		void swap() override;
		void clear(ClearFlags clr) override;

		void setClearColor(float r, float g, float b, float a) override;
		void setClearColor(const Color& color) override;

		void render(const Renderable* r) const override;

		CanvasPtr getCanvas() override;
		ClipScopePtr createClipScope(const rect& r) override;
		StencilScopePtr createStencilScope(const StencilSettings& settings) override;
		ScissorPtr getScissor(const rect& r) override;

		EffectPtr createEffect(const variant& node) override;

		void loadShadersFromFile(const variant& node) override;
		ShaderProgramPtr getShaderProgram(const std::string& name) override;

		BlendEquationImplBasePtr getBlendEquationImpl() override;

		void init(size_t width, size_t height) override;
		void printDeviceInfo() override;

		void setViewPort(int x, int y, unsigned width, unsigned height) override;

		virtual DisplayDeviceDataPtr createDisplayDeviceData(const DisplayDeviceDef& def) override;

	private:
		DisplayDeviceOpenGL(const DisplayDeviceOpenGL&);

		AttributeSetPtr handleCreateAttributeSet(bool indexed, bool instanced) override;
		HardwareAttributePtr handleCreateAttribute(AttributeBase* parent) override;

		RenderTargetPtr handleCreateRenderTarget(size_t width, size_t height, 
			size_t color_plane_count, 
			bool depth, 
			bool stencil, 
			bool use_multi_sampling, 
			size_t multi_samples) override;
		RenderTargetPtr handleCreateRenderTarget(const variant& node) override;
		void doBlitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch) override;

		bool doCheckForFeature(DisplayDeviceCapabilties cap) override;

		TexturePtr handleCreateTexture(const variant& node) override;
		TexturePtr handleCreateTexture(const std::string& filename, Texture::Type type, int mipmap_levels) override;
		TexturePtr handleCreateTexture(const SurfacePtr& surface, const variant& node) override;
		TexturePtr handleCreateTexture(const SurfacePtr& surface, Texture::Type type, int mipmap_levels) override;
		TexturePtr handleCreateTexture(unsigned width, PixelFormat::PF fmt) override;
		TexturePtr handleCreateTexture(unsigned width, unsigned height, PixelFormat::PF fmt, Texture::Type type=Texture::Type::TEXTURE_2D) override;
		TexturePtr handleCreateTexture(unsigned width, unsigned height, unsigned depth, PixelFormat::PF fmt) override;
		TexturePtr handleCreateTexture(const SurfacePtr& surface, const SurfacePtr& palette) override;

		MaterialPtr handleCreateMaterial(const variant& node) override;
		MaterialPtr handleCreateMaterial(const std::string& name, const std::vector<TexturePtr>& textures, const BlendMode& blend=BlendMode(), bool fog=false, bool lighting=false, bool depth_write=false, bool depth_check=false) override;

		bool handleReadPixels(int x, int y, unsigned width, unsigned height, ReadFormat fmt, AttrFormat type, void* data) override;
	};
}
