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

#include <set>

#include "DisplayDevice.hpp"
#include "Texture.hpp"
#include "ShadersGLES2.hpp"

namespace KRE
{
	class DisplayDeviceGLESv2 : public DisplayDevice
	{
	public:
		explicit DisplayDeviceGLESv2(WindowPtr wnd);
		~DisplayDeviceGLESv2();

		DisplayDeviceId ID() const override { return DISPLAY_DEVICE_OPENGLES; }

		void swap() override;
		void clear(ClearFlags clr) override;

		void setClearColor(float r, float g, float b, float a) const override;
		void setClearColor(const Color& color) const override;

		void render(const Renderable* r) const override;

		// Lets us set a default camera if nothing else is configured.
		CameraPtr setDefaultCamera(const CameraPtr& cam) override;
		CameraPtr getDefaultCamera() const override;

		CanvasPtr getCanvas() override;
		ClipScopePtr createClipScope(const rect& r) override;
		StencilScopePtr createStencilScope(const StencilSettings& settings) override;
		ScissorPtr getScissor(const rect& r) override;

		void clearTextures() override;

		EffectPtr createEffect(const variant& node) override;

		void loadShadersFromVariant(const variant& node) override;
		ShaderProgramPtr getShaderProgram(const std::string& name) override;
		ShaderProgramPtr getShaderProgram(const variant& node) override;
		ShaderProgramPtr getDefaultShader() override;
		ShaderProgramPtr createShader(const std::string& name, 
			const std::vector<ShaderData>& shader_data, 
			const std::vector<ActiveMapping>& uniform_map,
			const std::vector<ActiveMapping>& attribute_map) override;
		ShaderProgramPtr createGaussianShader(int radius) override;

		BlendEquationImplBasePtr getBlendEquationImpl() override;

		void init(int width, int height) override;
		void printDeviceInfo() override;

		int queryParameteri(DisplayDeviceParameters param) override;

		void setViewPort(const rect& vp) override;
		void setViewPort(int x, int y, int width, int height) override;
		const rect& getViewPort() const override;
	private:
		DisplayDeviceGLESv2();
		DisplayDeviceGLESv2(const DisplayDeviceGLESv2&);

		AttributeSetPtr handleCreateAttributeSet(bool indexed, bool instanced) override;
		HardwareAttributePtr handleCreateAttribute(AttributeBase* parent) override;

		RenderTargetPtr handleCreateRenderTarget(int width, int height, 
			int color_plane_count, 
			bool depth, 
			bool stencil, 
			bool use_multi_sampling, 
			int multi_samples) override;
		RenderTargetPtr handleCreateRenderTarget(const variant& node) override;
		void doBlitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch) override;

		bool doCheckForFeature(DisplayDeviceCapabilties cap) override;

		TexturePtr handleCreateTexture(const SurfacePtr& surface, TextureType type, int mipmap_levels) override;
		TexturePtr handleCreateTexture(const SurfacePtr& surface, const variant& node) override;

		TexturePtr handleCreateTexture1D(int width, PixelFormat::PF fmt) override;
		TexturePtr handleCreateTexture2D(int width, int height, PixelFormat::PF fmt) override;
		TexturePtr handleCreateTexture3D(int width, int height, int depth, PixelFormat::PF fmt) override;

		TexturePtr handleCreateTextureArray(int count, int width, int height, PixelFormat::PF fmt, TextureType type) override;
		TexturePtr handleCreateTextureArray(const std::vector<SurfacePtr>& surfaces, const variant& node) override;

		bool handleReadPixels(int x, int y, unsigned width, unsigned height, ReadFormat fmt, AttrFormat type, void* data, int stride) override;

		std::set<std::string> extensions_;

		bool seperate_blend_equations_;
		bool have_render_to_texture_;
		bool npot_textures_;
		bool hardware_uniform_buffers_;
		int max_texture_units_;

		int major_version_;
		int minor_version_;
	};
}
