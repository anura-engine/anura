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

#include <map>

#include "../asserts.hpp"
#include "../logger.hpp"
#include "DisplayDevice.hpp"

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

		typedef std::map<std::string, std::function<DisplayDevicePtr()>> DisplayDeviceRegistry;
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

	DisplayDevice::DisplayDevice()
	{
	}

	DisplayDevice::~DisplayDevice()
	{
	}

	void DisplayDevice::SetClearColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
	{
		SetClearColor(r/255.0f, g/255.0f, b/255.0f, a/255.0f);
	}

	DisplayDevicePtr DisplayDevice::Factory(const std::string& type)
	{
		ASSERT_LOG(!get_display_registry().empty(), "No display device drivers registered.");
		auto it = get_display_registry().find(type);
		if(it == get_display_registry().end()) {			
			LOG_WARN("Requested display driver '" << type << "' not found, using default: " << get_display_registry().begin()->first);
			current_display_device() = get_display_registry().begin()->second();
			return get_display_registry().begin()->second();
		}
		current_display_device() = it->second();
		return it->second();
	}

	DisplayDevicePtr DisplayDevice::GetCurrent()
	{
		ASSERT_LOG(current_display_device() != NULL, "display device is NULL");
		return current_display_device();
	}

	void DisplayDevice::RegisterFactoryFunction(const std::string& type, std::function<DisplayDevicePtr()> create_fn)
	{
		auto it = get_display_registry().find(type);
		if(it != get_display_registry().end()) {
			LOG_WARN("Overwriting the Display Device Driver: " << type);
		}
		get_display_registry()[type] = create_fn;
	}

	void DisplayDevice::BlitTexture(const TexturePtr& tex, int dstx, int dsty, int dstw, int dsth, float rotation, int srcx, int srcy, int srcw, int srch)
	{
		GetCurrent()->DoBlitTexture(tex, dstx, dsty, dstw, dsth, rotation, srcx, srcy, srcw, srch);
	}

	AttributeSetPtr DisplayDevice::CreateAttributeSet(bool hardware_hint, bool indexed, bool instanced)
	{
		if(hardware_hint) {
			auto as = DisplayDevice::GetCurrent()->HandleCreateAttributeSet(indexed, instanced);
			if(as) {
				return as;
			}
		}
		return AttributeSetPtr(new AttributeSet(indexed, instanced));
	}

	HardwareAttributePtr DisplayDevice::CreateAttributeBuffer(bool hw_backed, AttributeBase* parent)
	{
		if(hw_backed) {
			auto attrib = DisplayDevice::GetCurrent()->HandleCreateAttribute(parent);
			if(attrib) {
				return attrib;
			}
		}
		return HardwareAttributePtr(new HardwareAttributeImpl(parent));
	}

	RenderTargetPtr DisplayDevice::RenderTargetInstance(size_t width, size_t height, 
		size_t color_plane_count, 
		bool depth, 
		bool stencil, 
		bool use_multi_sampling, 
		size_t multi_samples)
	{
		return GetCurrent()->HandleCreateRenderTarget(width, height, 
			color_plane_count, 
			depth, 
			stencil, 
			use_multi_sampling, 
			multi_samples);
	}

	TexturePtr DisplayDevice::CreateTexture(const SurfacePtr& surface, const variant& node)
	{
		return GetCurrent()->HandleCreateTexture(surface, node);
	}

	TexturePtr DisplayDevice::CreateTexture(const SurfacePtr& surface, Texture::Type type, int mipmap_levels)
	{
		return GetCurrent()->HandleCreateTexture(surface, type, mipmap_levels);
	}

	TexturePtr DisplayDevice::CreateTexture(unsigned width, PixelFormat::PF fmt)
	{
		return GetCurrent()->HandleCreateTexture(width, fmt);
	}

	TexturePtr DisplayDevice::CreateTexture(unsigned width, unsigned height, PixelFormat::PF fmt, Texture::Type type)
	{
		return GetCurrent()->HandleCreateTexture(width, height, fmt, type);
	}

	TexturePtr DisplayDevice::CreateTexture(unsigned width, unsigned height, unsigned depth, PixelFormat::PF fmt)
	{
		return GetCurrent()->HandleCreateTexture(width, height, depth, fmt);
	}

	TexturePtr DisplayDevice::CreateTexture(const std::string& filename, Texture::Type type, int mipmap_levels)
	{
		return GetCurrent()->HandleCreateTexture(filename, type, mipmap_levels);
	}

	MaterialPtr DisplayDevice::CreateMaterial(const variant& node)
	{
		return GetCurrent()->HandleCreateMaterial(node);
	}

	MaterialPtr DisplayDevice::CreateMaterial(const std::string& name, const std::vector<TexturePtr>& textures, const BlendMode& blend, bool fog, bool lighting, bool depth_write, bool depth_check)
	{
		return GetCurrent()->HandleCreateMaterial(name, textures, blend, fog, lighting, depth_write, depth_check);
	}

	RenderTargetPtr DisplayDevice::RenderTargetInstance(const variant& node)
	{
		return DisplayDevice::GetCurrent()->HandleCreateRenderTarget(node);
	}

	bool DisplayDevice::CheckForFeature(DisplayDeviceCapabilties cap)
	{
		return DisplayDevice::GetCurrent()->DoCheckForFeature(cap);
	}

	DisplayDeviceDef::DisplayDeviceDef(const std::vector<AttributeSetPtr>& as)
		: attributes_(as)//, uniforms_(us)
	{
	}

	DisplayDeviceDef::~DisplayDeviceDef()
	{
	}

	void DisplayDeviceDef::SetHint(const std::string& hint_name, const std::string& hint)
	{
		HintList hint_list(1,hint);
		hints_.insert(std::make_pair(hint_name, hint_list));
	}

	void DisplayDeviceDef::SetHint(const std::string& hint_name, const HintList& hint)
	{
		hints_[hint_name] = hint;
	}

	DisplayDeviceData::DisplayDeviceData()
	{
	}

	DisplayDeviceData::~DisplayDeviceData()
	{
	}
}
