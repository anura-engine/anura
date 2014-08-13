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

#include "Material.hpp"
#include "Surface.hpp"
#include "WindowManager.hpp"

namespace KRE
{
	Material::Material()
		: use_lighting_(false),
		use_fog_(false),
		do_depth_write_(false),
		do_depth_check_(false),
		draw_rect_(0.0f, 0.0f, 0.0f, 0.0f)
	{
	}

	Material::Material(const std::string& name, 
		const std::vector<TexturePtr>& textures, 
		const BlendMode& blend, 
		bool fog, 
		bool lighting, 
		bool depth_write, 
		bool depth_check)
		: name_(name),
		tex_(textures),
		blend_(blend),
		use_lighting_(lighting),
		use_fog_(fog),
		do_depth_write_(depth_write),
		do_depth_check_(depth_check),
		draw_rect_(0.0f, 0.0f, 0.0f, 0.0f)
	{
	}

	void Material::init(const variant& node)
	{
		blend_.set(BlendModeConstants::BM_SRC_ALPHA, BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA);

		if(node.is_string()) {
			name_ = node.as_string();
			tex_.emplace_back(DisplayDevice::createTexture(name_));
		} else if(node.is_map()) {
			name_ = node["name"].as_string();
		
			// XXX: technically a material could have multiple technique's and passes -- ignoring for now.
			ASSERT_LOG(node.has_key("technique"), "PSYSTEM2: 'material' must have 'technique' attribute.");
			ASSERT_LOG(node["technique"].has_key("pass"), "PSYSTEM2: 'material' must have 'pass' attribute.");
			const variant& pass = node["technique"]["pass"];
			use_lighting_ = pass["lighting"].as_bool(false);
			use_fog_ = pass["fog_override"].as_bool(false);
			do_depth_write_ = pass["depth_write"].as_bool(true);
			do_depth_check_ = pass["depth_check"].as_bool(true);
			if(pass.has_key("scene_blend")) {
				blend_.set(pass["scene_blend"]);
			}
			if(pass.has_key("texture_unit")) {
				if(pass["texture_unit"].is_map()) {
					tex_.emplace_back(createTexture(pass["texture_unit"]));
				} else if(pass["texture_unit"].is_list()) {
					for(size_t n = 0; n != pass["texture_unit"].num_elements(); ++n) {
						tex_.emplace_back(createTexture(pass["texture_unit"][n]));
					}
				} else {
					ASSERT_LOG(false, "'texture_unit' attribute must be map or list ");
				}
			}
			if(pass.has_key("rect")) {
				draw_rect_ = rectf(pass["rect"]);
			}
		} else {
			ASSERT_LOG(false, "Materials(Textures) must be either a single string filename or a map.");
		}
	}

	Material::~Material()
	{
	}

	void Material::setTexture(const TexturePtr& tex)
	{
		tex_.emplace_back(tex);
	}

	void Material::enableLighting(bool en)
	{
		use_lighting_ = en;
	}

	void Material::enableFog(bool en)
	{
		use_fog_ = en;
	}

	void Material::enableDepthWrite(bool en)
	{
		do_depth_write_ = en;
	}

	void Material::enableDepthCheck(bool en)
	{
		do_depth_check_ = en;
	}

	void Material::setBlendMode(const BlendMode& bm)
	{
		blend_ = bm;
	}

	void Material::setBlendMode(BlendModeConstants src, BlendModeConstants dst)
	{
		blend_.set(src, dst);
	}

	bool Material::apply()
	{
		handleApply();
		return useLighting();
	}

	void Material::unapply()
	{
		handleUnapply();
	}

	MaterialPtr Material::createMaterial(const variant& node)
	{
		return DisplayDevice::getCurrent()->createMaterial(node);
	}

	const rectf Material::getNormalisedTextureCoords(const std::vector<TexturePtr>::const_iterator& it)
	{
		float w = static_cast<float>((*it)->width());
		float h = static_cast<float>((*it)->height());
		if(draw_rect_.x() == 0.0f && draw_rect_.y() == 0.0f && draw_rect_.x2() == 0.0f && draw_rect_.y2() == 0.0f) {
			return rectf(0.0f, 0.0f, 1.0f, 1.0f);
		}
		return rectf(draw_rect_.x()/w, draw_rect_.y()/h, draw_rect_.x2()/w, draw_rect_.y2()/h);
	}
}
