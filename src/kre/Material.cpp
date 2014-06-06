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

	void Material::Init(const variant& node)
	{
		name_ = node["name"].as_string();
		blend_.Set(BlendMode::BlendModeConstants::BM_SRC_ALPHA, BlendMode::BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA);
		
		// XXX: technically a material could have multiple technique's and passes -- ignoring for now.
		ASSERT_LOG(node.has_key("technique"), "PSYSTEM2: 'material' must have 'technique' attribute.");
		ASSERT_LOG(node["technique"].has_key("pass"), "PSYSTEM2: 'material' must have 'pass' attribute.");
		const variant& pass = node["technique"]["pass"];
		use_lighting_ = pass["lighting"].as_bool(false);
		use_fog_ = pass["fog_override"].as_bool(false);
		do_depth_write_ = pass["depth_write"].as_bool(true);
		do_depth_check_ = pass["depth_check"].as_bool(true);
		if(pass.has_key("scene_blend")) {
			blend_.Set(pass["scene_blend"]);
		}
		if(pass.has_key("texture_unit")) {
			if(pass["texture_unit"].is_map()) {
				tex_.emplace_back(CreateTexture(pass["texture_unit"]));
			} else if(pass["texture_unit"].is_list()) {
				for(size_t n = 0; n != pass["texture_unit"].num_elements(); ++n) {
					tex_.emplace_back(CreateTexture(pass["texture_unit"][n]));
				}
			} else {
				ASSERT_LOG(false, "'texture_unit' attribute must be map or list ");
			}
		}
		if(pass.has_key("rect")) {
			draw_rect_ = rectf(pass["rect"]);
		}
	}

	Material::~Material()
	{
	}

	void Material::SetTexture(const TexturePtr& tex)
	{
		tex_.emplace_back(tex);
	}

	void Material::EnableLighting(bool en)
	{
		use_lighting_ = en;
	}

	void Material::EnableFog(bool en)
	{
		use_fog_ = en;
	}

	void Material::EnableDepthWrite(bool en)
	{
		do_depth_write_ = en;
	}

	void Material::EnableDepthCheck(bool en)
	{
		do_depth_check_ = en;
	}

	void Material::SetBlendMode(const BlendMode& bm)
	{
		blend_ = bm;
	}

	void Material::SetBlendMode(BlendMode::BlendModeConstants src, BlendMode::BlendModeConstants dst)
	{
		blend_.Set(src, dst);
	}

	bool Material::Apply()
	{
		HandleApply();
		return UseLighting();
	}

	void Material::Unapply()
	{
		HandleUnapply();
	}

	const rectf Material::GetNormalisedTextureCoords(const std::vector<TexturePtr>::const_iterator& it)
	{
		float w = (*it)->Width();
		float h = (*it)->Height();
		if(draw_rect_.x() == 0.0f && draw_rect_.y() == 0.0f && draw_rect_.x2() == 0.0f && draw_rect_.y2() == 0.0f) {
			return rectf(0.0f, 0.0f, 1.0f, 1.0f);
		}
		return rectf(draw_rect_.x()/w, draw_rect_.y()/h, draw_rect_.x2()/w, draw_rect_.y2()/h);
	}

	namespace
	{
		BlendMode::BlendModeConstants parse_blend_string(const std::string& s)
		{
			if(s == "zero") {
				return BlendMode::BlendModeConstants::BM_ZERO;
			} else if(s == "one") {
				return BlendMode::BlendModeConstants::BM_ONE;
			} else if(s == "src_color") {
				return BlendMode::BlendModeConstants::BM_SRC_COLOR;
			} else if(s == "one_minus_src_color") {
				return BlendMode::BlendModeConstants::BM_ONE_MINUS_SRC_COLOR;
			} else if(s == "dst_color") {
				return BlendMode::BlendModeConstants::BM_DST_COLOR;
			} else if(s == "one_minus_dst_color") {
				return BlendMode::BlendModeConstants::BM_ONE_MINUS_DST_COLOR;
			} else if(s == "src_alpha") {
				return BlendMode::BlendModeConstants::BM_SRC_ALPHA;
			} else if(s == "one_minus_src_alpha") {
				return BlendMode::BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA;
			} else if(s == "dst_alpha") {
				return BlendMode::BlendModeConstants::BM_DST_ALPHA;
			} else if(s == "one_minus_dst_alpha") {
				return BlendMode::BlendModeConstants::BM_ONE_MINUS_DST_ALPHA;
			} else if(s == "const_color") {
				return BlendMode::BlendModeConstants::BM_CONSTANT_COLOR;
			} else if(s == "one_minus_const_color") {
				return BlendMode::BlendModeConstants::BM_ONE_MINUS_CONSTANT_COLOR;
			} else if(s == "const_alpha") {
				return BlendMode::BlendModeConstants::BM_CONSTANT_ALPHA;
			} else if(s == "one_minus_const_alpha") {
				return BlendMode::BlendModeConstants::BM_ONE_MINUS_CONSTANT_ALPHA;
			} else {
				ASSERT_LOG(false, "parse_blend_string: Unrecognised value: " << s);
			}
		}
	}

	void BlendMode::Set(const variant& node) 
	{
		if(node.is_string()) {
			const std::string& blend = node.as_string();
			if(blend == "add") {
				Set(BlendModeConstants::BM_ONE, BlendModeConstants::BM_ONE);
			} else if(blend == "alpha_blend") {
				Set(BlendModeConstants::BM_SRC_ALPHA, BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA);
			} else if(blend == "colour_blend") {
				Set(BlendModeConstants::BM_SRC_COLOR, BlendModeConstants::BM_ONE_MINUS_SRC_COLOR);
			} else if(blend == "modulate") {
				Set(BlendModeConstants::BM_DST_COLOR, BlendModeConstants::BM_ZERO);
			} else if(blend == "src_colour one") {
				Set(BlendModeConstants::BM_SRC_COLOR, BlendModeConstants::BM_ONE);
			} else if(blend == "src_colour zero") {
				Set(BlendModeConstants::BM_SRC_COLOR, BlendModeConstants::BM_ZERO);
			} else if(blend == "src_colour dest_colour") {
				Set(BlendModeConstants::BM_SRC_COLOR, BlendModeConstants::BM_DST_COLOR);
			} else if(blend == "dest_colour one") {
				Set(BlendModeConstants::BM_DST_COLOR, BlendModeConstants::BM_ONE);
			} else if(blend == "dest_colour src_colour") {
				Set(BlendModeConstants::BM_DST_COLOR, BlendModeConstants::BM_SRC_COLOR);
			} else {
				ASSERT_LOG(false, "BlendMode: Unrecognised scene_blend mode " << blend);
			}
		} else if(node.is_list() && node.num_elements() >= 2) {
			ASSERT_LOG(node[0].is_string() && node[1].is_string(), 
				"BlendMode: Blend mode must be specified by a list of two strings.");
			Set(parse_blend_string(node[0].as_string()), parse_blend_string(node[1].as_string()));
		} else {
			ASSERT_LOG(false, "BlendMode: Setting blend requires either a string or a list of greater than two elements." << node.type_as_string());
		}
	}
}
