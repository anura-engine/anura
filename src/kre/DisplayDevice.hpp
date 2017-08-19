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

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "DisplayDeviceFwd.hpp"
#include "geometry.hpp"
#include "PixelFormat.hpp"
#include "Renderable.hpp"
#include "Shaders.hpp"
#include "StencilSettings.hpp"
#include "variant.hpp"

namespace KRE
{
	enum class DisplayDeviceCapabilties {
		NPOT_TEXTURES,
		BLEND_EQUATION_SEPERATE,
		RENDER_TO_TEXTURE,
		SHADERS,
		UNIFORM_BUFFERS,
	};

	enum class DisplayDeviceParameters {
		MAX_TEXTURE_UNITS,
	};

	enum class ClearFlags {
		COLOR		= 1,
		DEPTH		= 2,
		STENCIL		= 4,
		ALL			= 0x7fffffff,
	};

	inline ClearFlags operator|(ClearFlags l, ClearFlags r)
	{
		return static_cast<ClearFlags>(static_cast<int>(l) | static_cast<int>(r));
	}

	inline bool operator&(ClearFlags l, ClearFlags r)
	{
		return (static_cast<int>(l) & static_cast<int>(r)) != 0;
	}

	enum class ReadFormat {
		ALPHA,
		DEPTH,
		STENCIL,
		DEPTH_STENCIL,
		RED,
		GREEN,
		BLUE,
		RG,
		RGB,
		BGR,
		RGBA,
		BGRA,
		RED_INT,
		GREEN_INT,
		BLUE_INT,
		RG_INT,
		RGB_INT,
		BGR_INT,
		RGBA_INT,
		BGRA_INT,
	};

	class DisplayDevice
	{
	public:
		enum DisplayDeviceId {
			// Display device is OpenGL 2.1 compatible, using shaders.
			DISPLAY_DEVICE_OPENGL,
			// Display device is OpenGLES 2.0, using shaders
			DISPLAY_DEVICE_OPENGLES,
			// Display device is OpenGL 1.1, fixed function pipeline
			DISPLAY_DEVICE_OPENGL_FIXED,
			// Display device is whatever SDL wants to use
			DISPLAY_DEVICE_SDL,
			// Display device is Direct3D
			DISPLAY_DEVICE_D3D,
		};

		explicit DisplayDevice(WindowPtr wnd);
		virtual ~DisplayDevice();

		virtual DisplayDeviceId ID() const = 0;

		virtual void setClearColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) const;
		virtual void setClearColor(float r, float g, float b, float a) const = 0;
		virtual void setClearColor(const Color& color) const = 0;

		virtual void clear(ClearFlags clr) = 0;
		virtual void swap() = 0;

		virtual void init(int width, int height) = 0;
		virtual void printDeviceInfo() = 0;

		virtual void render(const Renderable* r) const = 0;

		virtual void clearTextures() = 0;

		static TexturePtr createTexture(const SurfacePtr& surface, TextureType type, int mipmap_levels);
		static TexturePtr createTexture(const SurfacePtr& surface, const variant& node);

		static TexturePtr createTexture1D(int width, PixelFormat::PF fmt);
		static TexturePtr createTexture2D(int width, int height, PixelFormat::PF fmt);
		static TexturePtr createTexture3D(int width, int height, int depth, PixelFormat::PF fmt);

		static TexturePtr createTextureArray(int count, int width, int height, PixelFormat::PF fmt, TextureType type);
		static TexturePtr createTextureArray(const std::vector<SurfacePtr>& surfaces, const variant& node);

		virtual CanvasPtr getCanvas() = 0;

		virtual ClipScopePtr createClipScope(const rect& r) = 0;
		virtual ClipShapeScopePtr createClipShapeScope(const RenderablePtr& r) = 0;
		virtual StencilScopePtr createStencilScope(const StencilSettings& settings) = 0;

		virtual ScissorPtr getScissor(const rect& r) = 0;

		virtual CameraPtr setDefaultCamera(const CameraPtr& cam) = 0;
		virtual CameraPtr getDefaultCamera() const = 0;

		virtual void loadShadersFromVariant(const variant& node) = 0;
		virtual ShaderProgramPtr getShaderProgram(const std::string& name) = 0;
		virtual ShaderProgramPtr getShaderProgram(const variant& node) = 0;
		virtual ShaderProgramPtr getDefaultShader() = 0;
		virtual ShaderProgramPtr createShader(const std::string& name, 
			const std::vector<ShaderData>& shader_data, 
			const std::vector<ActiveMapping>& uniform_map,
			const std::vector<ActiveMapping>& attribute_map) = 0;
		virtual ShaderProgramPtr createGaussianShader(int radius) = 0;

		virtual int queryParameteri(DisplayDeviceParameters param) = 0;

		virtual BlendEquationImplBasePtr getBlendEquationImpl() = 0;

		virtual EffectPtr createEffect(const variant& node) = 0;

		static void blitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch);

		static RenderTargetPtr renderTargetInstance(int width, int height, 
			int color_plane_count=1, 
			bool depth=false, 
			bool stencil=false, 
			bool use_multi_sampling=false, 
			int multi_samples=0);
		static RenderTargetPtr renderTargetInstance(const variant& node);

		virtual void setViewPort(const rect& vp) = 0;
		virtual void setViewPort(int x, int y, int width, int height) = 0;
		virtual const rect& getViewPort() const = 0;

		template<typename T>
		bool readPixels(int x, int y, unsigned width, unsigned height, ReadFormat fmt, AttrFormat type, std::vector<T>& data, int stride) {
			data.resize(stride * height / sizeof(T));
			return handleReadPixels(x, y, width, height, fmt, type, static_cast<void*>(data.data()), stride);
		}

		WindowPtr getParentWindow() const;

		static AttributeSetPtr createAttributeSet(bool hardware_hint=false, bool indexed=false, bool instanced=false);
		static HardwareAttributePtr createAttributeBuffer(bool hw_backed, AttributeBase* parent);

		static DisplayDevicePtr factory(const std::string& type, WindowPtr wnd);

		static DisplayDevicePtr getCurrent();		

		static bool checkForFeature(DisplayDeviceCapabilties cap);

		static void registerFactoryFunction(const std::string& type, std::function<DisplayDevicePtr(WindowPtr)>);
	private:
		std::weak_ptr<Window> parent_;

		DisplayDevice();
		DisplayDevice(const DisplayDevice&);
		virtual AttributeSetPtr handleCreateAttributeSet(bool indexed, bool instanced) = 0;
		virtual HardwareAttributePtr handleCreateAttribute(AttributeBase* parent) = 0;

		virtual RenderTargetPtr handleCreateRenderTarget(int width, int height, 
			int color_plane_count, 
			bool depth, 
			bool stencil, 
			bool use_multi_sampling, 
			int multi_samples) = 0;
		virtual RenderTargetPtr handleCreateRenderTarget(const variant& node) = 0;

		virtual bool handleReadPixels(int x, int y, unsigned width, unsigned height, ReadFormat fmt, AttrFormat type, void* data, int stride) = 0;
		
		virtual TexturePtr handleCreateTexture(const SurfacePtr& surface, TextureType type, int mipmap_levels) = 0;
		virtual TexturePtr handleCreateTexture(const SurfacePtr& surface, const variant& node) = 0;

		virtual TexturePtr handleCreateTexture1D(int width, PixelFormat::PF fmt) = 0;
		virtual TexturePtr handleCreateTexture2D(int width, int height, PixelFormat::PF fmt) = 0;
		virtual TexturePtr handleCreateTexture3D(int width, int height, int depth, PixelFormat::PF fmt) = 0;

		virtual TexturePtr handleCreateTextureArray(int count, int width, int height, PixelFormat::PF fmt, TextureType type) = 0;
		virtual TexturePtr handleCreateTextureArray(const std::vector<SurfacePtr>& surfaces, const variant& node) = 0;

		virtual bool doCheckForFeature(DisplayDeviceCapabilties cap) = 0;

		virtual void doBlitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch) = 0;
	};

	template<class T>
	struct DisplayDeviceRegistrar
	{
		DisplayDeviceRegistrar(const std::string& type)
		{
			// register the class factory function 
			DisplayDevice::registerFactoryFunction(type, [](WindowPtr wnd) -> DisplayDevicePtr { return DisplayDevicePtr(new T(wnd));});
		}
	};
}
