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

#include "../asserts.hpp"
#include "MaterialOpenGL.hpp"
#include "TextureOpenGL.hpp"

namespace KRE
{
	namespace
	{
		GLenum convert_blend_mode(BlendMode::BlendModeConstants bm)
		{
			switch(bm) {
				case BlendMode::BlendModeConstants::BM_ZERO:					return GL_ZERO;
				case BlendMode::BlendModeConstants::BM_ONE:						return GL_ONE;
				case BlendMode::BlendModeConstants::BM_SRC_COLOR:				return GL_SRC_COLOR;
				case BlendMode::BlendModeConstants::BM_ONE_MINUS_SRC_COLOR:		return GL_ONE_MINUS_SRC_COLOR;
				case BlendMode::BlendModeConstants::BM_DST_COLOR:				return GL_DST_COLOR;
				case BlendMode::BlendModeConstants::BM_ONE_MINUS_DST_COLOR:		return GL_ONE_MINUS_DST_COLOR;
				case BlendMode::BlendModeConstants::BM_SRC_ALPHA:				return GL_SRC_ALPHA;
				case BlendMode::BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA:		return GL_ONE_MINUS_SRC_ALPHA;
				case BlendMode::BlendModeConstants::BM_DST_ALPHA:				return GL_DST_ALPHA;
				case BlendMode::BlendModeConstants::BM_ONE_MINUS_DST_ALPHA:		return GL_ONE_MINUS_DST_ALPHA;
				case BlendMode::BlendModeConstants::BM_CONSTANT_COLOR:			return GL_CONSTANT_COLOR;
				case BlendMode::BlendModeConstants::BM_ONE_MINUS_CONSTANT_COLOR:return GL_ONE_MINUS_CONSTANT_COLOR;
				case BlendMode::BlendModeConstants::BM_CONSTANT_ALPHA:			return GL_CONSTANT_ALPHA;
				case BlendMode::BlendModeConstants::BM_ONE_MINUS_CONSTANT_ALPHA:return GL_ONE_MINUS_CONSTANT_ALPHA;
			}
			ASSERT_LOG(false, "Unrecognised blend mode");
			return GL_ZERO;
		}
	}

	OpenGLMaterial::OpenGLMaterial(const variant& node) 
	{
		Init(node);
	}

	OpenGLMaterial::OpenGLMaterial(const std::string& name, 
		const std::vector<TexturePtr>& textures, 
		const BlendMode& blend, 
		bool fog, 
		bool lighting, 
		bool depth_write, 
		bool depth_check)
		: Material(name, textures, blend, fog, lighting, depth_write, depth_check)
	{
	}

	OpenGLMaterial::~OpenGLMaterial()
	{
	}

	void OpenGLMaterial::HandleApply() 
	{
		auto textures = GetTexture();
		auto n = textures.size()-1;
		for(auto it = textures.rbegin(); it != textures.rend(); ++it) {
			if(n > 0) {
				glActiveTexture(GL_TEXTURE0 + n);
			}
			(*it)->Bind();
		}

		auto& bm = GetBlendMode();
		if(bm.Src() != BlendMode::BlendModeConstants::BM_SRC_ALPHA 
			|| bm.Dst() != BlendMode::BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA) {
			glBlendFunc(convert_blend_mode(bm.Src()), convert_blend_mode(bm.Dst()));
		}

		if(DoDepthCheck()) {
			glEnable(GL_DEPTH_TEST);
		} else {
			if(DoDepthWrite()) {
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_ALWAYS);
			}
		}

		if(UseFog()) {
			// XXXX: todo.
		}
	}

	void OpenGLMaterial::HandleUnapply() 
	{
		if(DoDepthCheck() || DoDepthWrite()) {
			glDisable(GL_DEPTH_TEST);
			if(DoDepthWrite()) {
				glDepthFunc(GL_LESS);
			}
		}

		auto& bm = GetBlendMode();
		if(bm.Src() != BlendMode::BlendModeConstants::BM_SRC_ALPHA 
			|| bm.Dst() != BlendMode::BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA) {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
	}

	TexturePtr OpenGLMaterial::CreateTexture(const variant& node)
	{
		ASSERT_LOG(node.has_key("image") || node.has_key("texture"), "Must have either 'image' or 'texture' attribute.");
		const std::string image_name = node.has_key("image") ? node["image"].as_string() : node["texture"].as_string();
		auto surface = Surface::Create(image_name);
		return TexturePtr(new OpenGLTexture(surface, node));
	}
}
