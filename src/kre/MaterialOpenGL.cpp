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

#include "../asserts.hpp"
#include "BlendOGL.hpp"
#include "MaterialOpenGL.hpp"
#include "TextureOpenGL.hpp"

namespace KRE
{
	OpenGLMaterial::OpenGLMaterial(const variant& node) 
	{
		init(node);
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

	void OpenGLMaterial::handleApply() 
	{
		auto textures = getTexture();
		auto n = textures.size()-1;
		for(auto it = textures.rbegin(); it != textures.rend(); ++it) {
			if(n > 0) {
				glActiveTexture(GL_TEXTURE0 + n);
			}
			(*it)->bind();
		}

		blend_mode_manager_.reset(new BlendModeManagerOGL(getBlendMode()));

		if(doDepthCheck()) {
			glEnable(GL_DEPTH_TEST);
		} else {
			if(doDepthWrite()) {
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_ALWAYS);
			}
		}

		if(useFog()) {
			// XXXX: todo.
		}
	}

	void OpenGLMaterial::handleUnapply() 
	{
		blend_mode_manager_.reset();

		if(doDepthCheck() || doDepthWrite()) {
			glDisable(GL_DEPTH_TEST);
			if(doDepthWrite()) {
				glDepthFunc(GL_LESS);
			}
		}
	}

	TexturePtr OpenGLMaterial::createTexture(const variant& node)
	{
		ASSERT_LOG(node.has_key("image") || node.has_key("texture"), "Must have either 'image' or 'texture' attribute.");
		const std::string image_name = node.has_key("image") ? node["image"].as_string() : node["texture"].as_string();
		auto surface = Surface::create(image_name);
		return TexturePtr(new OpenGLTexture(surface, node));
	}
}
