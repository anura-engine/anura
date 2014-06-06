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

#include <map>
#include <memory>
#include <string>

#include "AttributeSet.hpp"
#include "Canvas.hpp"
#include "../Color.hpp"
#include "DisplayDeviceFwd.hpp"
#include "Material.hpp"
#include "Renderable.hpp"
#include "RenderTarget.hpp"
#include "../variant.hpp"

namespace KRE
{
	typedef std::vector<std::string> HintList;
	typedef std::map<std::string,HintList> HintMap;
	class DisplayDeviceDef
	{
	public:
		DisplayDeviceDef(const std::vector<AttributeSetPtr>& as/*, const std::vector<UniformSetPtr>& us*/);
		~DisplayDeviceDef();

		const std::vector<AttributeSetPtr>& GetAttributeSet() const { return attributes_; }
		//const std::vector<UniformSetPtr>& GetUniformSet() const { return uniforms_; }

		void SetHint(const std::string& hint_name, const std::string& hint);
		void SetHint(const std::string& hint_name, const HintList& hint);
		HintMap GetHints() const { return hints_; }
	private:
		HintMap hints_;
		const std::vector<AttributeSetPtr>& attributes_;
		//const std::vector<UniformSetPtr>& uniforms_;
	};

	class DisplayDeviceData
	{
	public:
		DisplayDeviceData();
		virtual ~DisplayDeviceData();
	private:
		DisplayDeviceData(const DisplayDeviceData&);
	};

	enum class DisplayDeviceCapabilties
	{
		NPOT_TEXTURES,
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
		enum ClearFlags {
			DISPLAY_CLEAR_COLOR		= 1,
			DISPLAY_CLEAR_DEPTH		= 2,
			DISPLAY_CLEAR_STENCIL	= 4,
			DISPLAY_CLEAR_ALL		= 0xffffffff,
		};

		DisplayDevice();
		virtual ~DisplayDevice();

		virtual DisplayDeviceId ID() const = 0;

		virtual void SetClearColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
		virtual void SetClearColor(float r, float g, float b, float a) = 0;
		virtual void SetClearColor(const Color& color) = 0;

		virtual void Clear(uint32_t clr) = 0;
		virtual void Swap() = 0;

		virtual void Init(size_t width, size_t height) = 0;
		virtual void PrintDeviceInfo() = 0;

		virtual void Render(const RenderablePtr& r) = 0;

		static TexturePtr CreateTexture(const std::string& filename, 
			Texture::Type type=Texture::Type::TEXTURE_2D, 
			int mipmap_levels=0);

		static TexturePtr CreateTexture(const SurfacePtr& surface, const variant& node);
		static TexturePtr CreateTexture(const SurfacePtr& surface, 
			Texture::Type type=Texture::Type::TEXTURE_2D, 
			int mipmap_levels=0);
		static TexturePtr CreateTexture(unsigned width, PixelFormat::PF fmt);
		static TexturePtr CreateTexture(unsigned width, unsigned height, PixelFormat::PF fmt, Texture::Type type=Texture::Type::TEXTURE_2D);
		static TexturePtr CreateTexture(unsigned width, unsigned height, unsigned depth, PixelFormat::PF fmt);

		virtual CanvasPtr GetCanvas() = 0;

		static void BlitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch);

		static MaterialPtr CreateMaterial(const variant& node);
		static MaterialPtr CreateMaterial(const std::string& name, const std::vector<TexturePtr>& textures, const BlendMode& blend=BlendMode(), bool fog=false, bool lighting=false, bool depth_write=false, bool depth_check=false);

		static RenderTargetPtr RenderTargetInstance(size_t width, size_t height, 
			size_t color_plane_count=1, 
			bool depth=false, 
			bool stencil=false, 
			bool use_multi_sampling=false, 
			size_t multi_samples=0);
		static RenderTargetPtr RenderTargetInstance(const variant& node);

		virtual DisplayDeviceDataPtr CreateDisplayDeviceData(const DisplayDeviceDef& def) = 0;

		static AttributeSetPtr CreateAttributeSet(bool hardware_hint=false, bool indexed=false, bool instanced=false);
		static HardwareAttributePtr CreateAttributeBuffer(bool hw_backed, AttributeBase* parent);

		static DisplayDevicePtr Factory(const std::string& type);

		static DisplayDevicePtr GetCurrent();

		static bool CheckForFeature(DisplayDeviceCapabilties cap);

		static void RegisterFactoryFunction(const std::string& type, std::function<DisplayDevicePtr()>);
	private:
		DisplayDevice(const DisplayDevice&);
		virtual AttributeSetPtr HandleCreateAttributeSet(bool indexed, bool instanced) = 0;
		virtual HardwareAttributePtr HandleCreateAttribute(AttributeBase* parent) = 0;

		virtual RenderTargetPtr HandleCreateRenderTarget(size_t width, size_t height, 
			size_t color_plane_count, 
			bool depth, 
			bool stencil, 
			bool use_multi_sampling, 
			size_t multi_samples) = 0;
		virtual RenderTargetPtr HandleCreateRenderTarget(const variant& node) = 0;
		
		virtual TexturePtr HandleCreateTexture(const std::string& filename, Texture::Type type, int mipmap_levels) = 0;
		virtual TexturePtr HandleCreateTexture(const SurfacePtr& surface, const variant& node) = 0;
		virtual TexturePtr HandleCreateTexture(const SurfacePtr& surface, Texture::Type type, int mipmap_levels) = 0;
		virtual TexturePtr HandleCreateTexture(unsigned width, PixelFormat::PF fmt) = 0;
		virtual TexturePtr HandleCreateTexture(unsigned width, unsigned height, PixelFormat::PF fmt, Texture::Type type=Texture::Type::TEXTURE_2D) = 0;
		virtual TexturePtr HandleCreateTexture(unsigned width, unsigned height, unsigned depth, PixelFormat::PF fmt) = 0;

		virtual MaterialPtr HandleCreateMaterial(const variant& node) = 0;
		virtual MaterialPtr HandleCreateMaterial(const std::string& name, const std::vector<TexturePtr>& textures, const BlendMode& blend=BlendMode(), bool fog=false, bool lighting=false, bool depth_write=false, bool depth_check=false) = 0;

		virtual bool DoCheckForFeature(DisplayDeviceCapabilties cap) = 0;

		virtual void DoBlitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch) = 0;
	};

	template<class T>
	struct DisplayDeviceRegistrar
	{
		DisplayDeviceRegistrar(const std::string& type)
		{
			// register the class factory function 
			DisplayDevice::RegisterFactoryFunction(type, []() -> DisplayDevicePtr { return DisplayDevicePtr(new T());});
		}
	};
}
