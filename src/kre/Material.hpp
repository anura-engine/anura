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

		const std::vector<TexturePtr>& getTexture() const { return tex_; }
		const std::string& name() const { return name_; }
		bool useFog() const { return use_fog_; }
		bool useLighting() const { return use_lighting_; }
		bool doDepthWrite() const { return do_depth_write_; }
		bool doDepthCheck() const { return do_depth_check_; }
		const BlendMode& getBlendMode() const { return blend_; }

		void setTexture(const TexturePtr& tex);
		void enableLighting(bool en=true);
		void enableFog(bool en=true);
		void enableDepthWrite(bool en=true);
		void enableDepthCheck(bool en=true);
		void setBlendMode(const BlendMode& bm);
		void setBlendMode(BlendModeConstants src, BlendModeConstants dst);

		float width() const;
		float height() const;

		const rectf getNormalisedTextureCoords(const std::vector<TexturePtr>::const_iterator& it);
		template<typename T>
		const rectf getNormalisedTextureCoords(const std::vector<TexturePtr>::const_iterator& it, const Geometry::Rect<T>& r) {
			float w = static_cast<float>((*it)->width());
			float h = static_cast<float>((*it)->height());
			return rectf(static_cast<float>(r.x())/w, static_cast<float>(r.y())/h, static_cast<float>(r.x2())/w, static_cast<float>(r.y2())/h);
		}
		template<typename T>
		const rectf getNormalisedTextureCoords(const Geometry::Rect<T>& r) {
			float w = static_cast<float>(width());
			float h = static_cast<float>(height());
			return rectf(static_cast<float>(r.x())/w, static_cast<float>(r.y())/h, static_cast<float>(r.x2())/w, static_cast<float>(r.y2())/h);
		}

		template<typename T> void setCoords(const Geometry::Rect<T>& r) {
			draw_rect_ = r.template as_type<float>();
		}
		const rectf& getCoords() const { return draw_rect_; }
		
		// Performs the actions to apply the current material to the renderable object.
		// Returns a boolean indicating whether to use lighting or not for this
		// material.
		bool apply();
		void unapply();

		static MaterialPtr createMaterial(const variant& node);
	protected:
		void init(const variant& node);
	private:
		virtual TexturePtr createTexture(const variant& node) = 0;
		virtual void handleApply() = 0;
		virtual void handleUnapply() = 0;

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
