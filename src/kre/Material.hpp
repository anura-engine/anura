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

#include "Blend.hpp"
#include "Geometry.hpp"
#include "Texture.hpp"
#include "../variant.hpp"

namespace KRE
{
	class Material;
	typedef std::shared_ptr<Material> MaterialPtr;

	class Material
	{
	public:
		Material();
		Material(const std::string& name, const std::vector<TexturePtr>& textures, const BlendMode& blend=BlendMode(), bool fog=false, bool lighting=false, bool depth_write=false, bool depth_check=false);
		virtual ~Material();

		const std::vector<TexturePtr>& GetTexture() const { return tex_; }
		const std::string& Name() const { return name_; }
		bool UseFog() const { return use_fog_; }
		bool UseLighting() const { return use_lighting_; }
		bool DoDepthWrite() const { return do_depth_write_; }
		bool DoDepthCheck() const { return do_depth_check_; }
		const BlendMode& GetBlendMode() const { return blend_; }

		void SetTexture(const TexturePtr& tex);
		void EnableLighting(bool en=true);
		void EnableFog(bool en=true);
		void EnableDepthWrite(bool en=true);
		void EnableDepthCheck(bool en=true);
		void SetBlendMode(const BlendMode& bm);
		void SetBlendMode(BlendModeConstants src, BlendModeConstants dst);

		float width() const;
		float height() const;

		const rectf GetNormalisedTextureCoords(const std::vector<TexturePtr>::const_iterator& it);
		template<typename T>
		const rectf GetNormalisedTextureCoords(const std::vector<TexturePtr>::const_iterator& it, const Geometry::Rect<T>& r) {
			float w = static_cast<float>((*it)->width());
			float h = static_cast<float>((*it)->height());
			return rectf(static_cast<float>(r.x())/w, static_cast<float>(r.y())/h, static_cast<float>(r.x2())/w, static_cast<float>(r.y2())/h);
		}
		template<typename T>
		const rectf GetNormalisedTextureCoords(const Geometry::Rect<T>& r) {
			float w = static_cast<float>(width());
			float h = static_cast<float>(height());
			return rectf(static_cast<float>(r.x())/w, static_cast<float>(r.y())/h, static_cast<float>(r.x2())/w, static_cast<float>(r.y2())/h);
		}

		template<typename T> void SetCoords(const Geometry::Rect<T>& r) {
			draw_rect_ = r.template as_type<float>();
		}
		const rectf& GetCoords() const { return draw_rect_; }
		
		// Performs the actions to apply the current material to the renderable object.
		// Returns a boolean indicating whether to use lighting or not for this
		// material.
		bool Apply();
		void Unapply();

		static MaterialPtr createMaterial(const variant& node);
	protected:
		void Init(const variant& node);
	private:
		virtual TexturePtr CreateTexture(const variant& node) = 0;
		virtual void HandleApply() = 0;
		virtual void HandleUnapply() = 0;

		std::string name_;
		std::vector<TexturePtr> tex_;
		bool use_lighting_;
		bool use_fog_;
		bool do_depth_write_;
		bool do_depth_check_;
		BlendMode blend_;
		rectf draw_rect_;
		Material(const Material&);
		Material& operator=(const Material&);
	};
}
