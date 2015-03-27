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

#include <map>

#include "asserts.hpp"
#include "AttributeSet.hpp"
#include "Canvas.hpp"
#include "ClipScope.hpp"
#include "DisplayDevice.hpp"
#include "Effects.hpp"
#include "RenderTarget.hpp"
#include "Scissor.hpp"
#include "Shaders.hpp"
#include "StencilScope.hpp"
#include "Texture.hpp"

namespace KRE
{
	namespace 
	{
		// A quick hack to do case insensitive case compare, doesn't support utf-8,
		// doesn't support unicode comparison between code-points.
		// But then it isn't intended to.
		bool icasecmp(const std::string& l, const std::string& r)
		{
			return l.size() == r.size()
				&& equal(l.cbegin(), l.cend(), r.cbegin(),
					[](std::string::value_type l1, std::string::value_type r1)
						{ return toupper(l1) == toupper(r1); });
		}	

		typedef std::map<std::string, std::function<DisplayDevicePtr(WindowPtr)>> DisplayDeviceRegistry;
		DisplayDeviceRegistry& get_display_registry()
		{
			static DisplayDeviceRegistry res;
			return res;
		}

		DisplayDevicePtr& current_display_device()
		{
			static DisplayDevicePtr res;
			return res;
		};
	}

	DisplayDevice::DisplayDevice(WindowPtr wnd)
		: parent_(wnd)
	{
	}

	DisplayDevice::~DisplayDevice()
	{
	}

	void DisplayDevice::setClearColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) const
	{
		setClearColor(r/255.0f, g/255.0f, b/255.0f, a/255.0f);
	}

	DisplayDevicePtr DisplayDevice::factory(const std::string& type, WindowPtr parent)
	{
		ASSERT_LOG(!get_display_registry().empty(), "No display device drivers registered.");
		auto it = get_display_registry().find(type);
		if(it == get_display_registry().end()) {			
			LOG_WARN("Requested display driver '" << type << "' not found, using default: " << get_display_registry().begin()->first);
			current_display_device() = get_display_registry().begin()->second(parent);
			return current_display_device();
		}
		current_display_device() = it->second(parent);
		return current_display_device();
	}

	DisplayDevicePtr DisplayDevice::getCurrent()
	{
		ASSERT_LOG(current_display_device() != nullptr, "display device is nullptr");
		return current_display_device();
	}

	void DisplayDevice::registerFactoryFunction(const std::string& type, std::function<DisplayDevicePtr(WindowPtr)> create_fn)
	{
		auto it = get_display_registry().find(type);
		if(it != get_display_registry().end()) {
			LOG_WARN("Overwriting the Display Device Driver: " << type);
		}
		get_display_registry()[type] = create_fn;
	}

	void DisplayDevice::blitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch)
	{
		getCurrent()->doBlitTexture(tex, dstx, dsty, dstw, dsth, rotation, srcx, srcy, srcw, srch);
	}

	AttributeSetPtr DisplayDevice::createAttributeSet(bool hardware_hint, bool indexed, bool instanced)
	{
		if(hardware_hint) {
			auto as = DisplayDevice::getCurrent()->handleCreateAttributeSet(indexed, instanced);
			if(as) {
				return as;
			}
		}
		return AttributeSetPtr(new AttributeSet(indexed, instanced));
	}

	HardwareAttributePtr DisplayDevice::createAttributeBuffer(bool hw_backed, AttributeBase* parent)
	{
		if(hw_backed) {
			auto attrib = DisplayDevice::getCurrent()->handleCreateAttribute(parent);
			if(attrib) {
				return attrib;
			}
		}
		return std::make_shared<HardwareAttributeImpl>(parent);
	}

	RenderTargetPtr DisplayDevice::renderTargetInstance(int width, int height, 
		int color_plane_count, 
		bool depth, 
		bool stencil, 
		bool use_multi_sampling, 
		int multi_samples)
	{
		return getCurrent()->handleCreateRenderTarget(width, height, 
			color_plane_count, 
			depth, 
			stencil, 
			use_multi_sampling, 
			multi_samples);
	}

	TexturePtr DisplayDevice::createTexture(const SurfacePtr& surface, TextureType type, int mipmap_levels)
	{
		return getCurrent()->handleCreateTexture(surface, type, mipmap_levels);
	}

	TexturePtr DisplayDevice::createTexture(const SurfacePtr& surface, const variant& node)
	{
		return getCurrent()->handleCreateTexture(surface, node);
	}

	TexturePtr DisplayDevice::createTexture1D(int width, PixelFormat::PF fmt)
	{
		return getCurrent()->handleCreateTexture1D(width, fmt);
	}

	TexturePtr DisplayDevice::createTexture2D(int width, int height, PixelFormat::PF fmt)
	{
		return getCurrent()->handleCreateTexture2D(width, height, fmt);
	}

	TexturePtr DisplayDevice::createTexture3D(int width, int height, int depth, PixelFormat::PF fmt)
	{
		return getCurrent()->handleCreateTexture3D(width, height, depth, fmt);
	}

	TexturePtr DisplayDevice::createTextureArray(int count, int width, int height, PixelFormat::PF fmt, TextureType type)
	{
		return getCurrent()->handleCreateTextureArray(count, width, height, fmt, type);
	}

	TexturePtr DisplayDevice::createTextureArray(const std::vector<SurfacePtr>& surfaces, const variant& node)
	{
		return getCurrent()->handleCreateTextureArray(surfaces, node);
	}

	RenderTargetPtr DisplayDevice::renderTargetInstance(const variant& node)
	{
		return DisplayDevice::getCurrent()->handleCreateRenderTarget(node);
	}

	bool DisplayDevice::checkForFeature(DisplayDeviceCapabilties cap)
	{
		return DisplayDevice::getCurrent()->doCheckForFeature(cap);
	}

	WindowPtr DisplayDevice::getParentWindow() const
	{
		auto parent = parent_.lock();
		ASSERT_LOG(parent != nullptr, "parent for display was null");
		return parent;
	}
}
