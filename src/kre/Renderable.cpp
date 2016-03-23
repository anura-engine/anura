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

#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "AttributeSet.hpp"
#include "CameraObject.hpp"
#include "DisplayDevice.hpp"
#include "LightObject.hpp"
#include "Renderable.hpp"
#include "RenderTarget.hpp"
#include "Shaders.hpp"
#include "Texture.hpp"
#include "variant_utils.hpp"

namespace KRE
{
	Renderable::Renderable()
		: order_(0),
		  position_(0.0f),
		  rotation_(1.0f, 0.0f, 0.0f, 0.0f),
		  scale_(1.0f),
		  shader_(ShaderProgram::getSystemDefault()),
		  stencil_settings_(false, StencilFace::FRONT_AND_BACK, StencilFunc::EQUAL, 0x01, 0x01, 0xff, StencilOperation::KEEP, StencilOperation::KEEP, StencilOperation::KEEP),
		  stencil_mask_(nullptr),
		  enabled_(true),
		  ignore_global_model_(false),
		  derived_position_(0.0f),
		  derived_rotation_(),
		  derived_scale_(1.0f)
	{
	}

	Renderable::Renderable(size_t order)
		: order_(order), 
		  position_(0.0f),
		  rotation_(1.0f, 0.0f, 0.0f, 0.0f),
		  scale_(1.0f),
		  shader_(ShaderProgram::getSystemDefault()),
		  stencil_settings_(false, StencilFace::FRONT_AND_BACK, StencilFunc::EQUAL, 0x01, 0x01, 0xff, StencilOperation::KEEP, StencilOperation::KEEP, StencilOperation::KEEP),
		  stencil_mask_(nullptr),
		  enabled_(true),
		  ignore_global_model_(false),
		  derived_position_(0.0f),
		  derived_rotation_(),
		  derived_scale_(1.0f)
	{
	}

	Renderable::Renderable(const variant& node)
		: order_(0),
		  position_(0.0f),
		  rotation_(1.0f, 0.0f, 0.0f, 0.0f),
		  scale_(1.0f),
		  shader_(ShaderProgram::getSystemDefault()),
		  stencil_settings_(false, StencilFace::FRONT_AND_BACK, StencilFunc::EQUAL, 0x01, 0x01, 0xff, StencilOperation::KEEP, StencilOperation::KEEP, StencilOperation::KEEP),
		  stencil_mask_(nullptr),
		  enabled_(true),
		  ignore_global_model_(false),
		  derived_position_(0.0f),
		  derived_rotation_(),
		  derived_scale_(1.0f)
	{
		if(!node.is_map()) {
			return;
		}

		setFromVariant(node);
	}

	Renderable::~Renderable()
	{
	}

	void Renderable::setFromVariant(const variant& node)
	{
		if(node.has_key("ignore_global_model")) {
			ignore_global_model_ = node["ignore_global_model"].as_bool(false);
		}
		if(node.has_key("order")) {
			order_ = node["order"].as_int32();
		}
		// XXX set other stuff here, tbd
		if(node.has_key("blend")) {
			setBlendMode(KRE::BlendMode(node["blend"]));
		}
		if(node.has_key("blend_enable")) {
			setBlendState(node["blend_enable"].as_bool());
		}
		if(node.has_key("blend_equation")) {
			setBlendEquation(KRE::BlendEquation(node["blend_equation"]));
		} else if(node.has_key("blend_eq")) {
			setBlendEquation(KRE::BlendEquation(node["blend_eq"]));
		}
		if(node.has_key("rotation")) {
			const variant& rot = node["rotation"];
			if(rot.is_numeric()) {
				// Assume it's a simple rotation around z-axis
				setRotation(rot.as_float(), glm::vec3(0.f,0.f,1.f));
			} else if(rot.is_list()) {
				// Either a single rotation formatted as [angle,[x,y,z]] or a list of three euler angles
				if(rot.num_elements() == 2) {
					ASSERT_LOG(rot[1].is_list() && rot[1].num_elements() == 3,
						"Format for a single rotation is [angle, [x,y,z]]");
					setRotation(rot[0].as_float(), variant_to_vec3(rot[1]));
				} else if(rot.num_elements() == 3) {
					setRotation(rot[0].as_float(), glm::vec3(1.,0.,0.));
					setRotation(rot[1].as_float(), glm::vec3(0.,1.,0.));
					setRotation(rot[2].as_float(), glm::vec3(0.,0.,1.));
				} else {
					ASSERT_LOG(false, "Need a list of three (x/y/z rotations) or 2 elements (angle, [axis])");
				}
			}
		}
		if(node.has_key("translation") || node.has_key("position")) {
			const variant& pos = node.has_key("translation") ? node["translation"] : node["position"];
			ASSERT_LOG(pos.is_list() && (pos.num_elements() == 2 || pos.num_elements() == 3),
				"'translation'/'position' attribute should have 2 [x,y] or 3 [x,y,z] elements.");
			if(pos.num_elements() == 3) {
				setPosition(variant_to_vec3(pos));
			} else {
				setPosition(pos[0].as_float(), pos[1].as_float());
			}
		}
		if(node.has_key("scale")) {
			const variant& sc = node["scale"];
			if(sc.is_numeric()) {
				const float scale = sc.as_float();
				setScale(scale, scale, scale);
			} else if(sc.is_list()) {
				float xs = 1.0f;
				float ys = 1.0f;
				float zs = 1.0f;
				if(sc.num_elements() == 1) {
					xs = sc[0].as_float();
				} else if(sc.num_elements() == 2) {
					xs = sc[0].as_float();
					ys = sc[1].as_float();
				} else if(sc.num_elements() == 3) {
					xs = sc[0].as_float();
					ys = sc[1].as_float();
					zs = sc[2].as_float();
				}
				setScale(xs, ys, zs);
			} else {
				ASSERT_LOG(false, "Scale should be a number of a list of up to three elements.");
			}
		}
		if(node.has_key("color")) {
			setColor(KRE::Color(node["color"]));
		}
		if(node.has_key("texture")) {
			if(node["texture"].is_string() && (node["texture"].as_string() != "fbo" && node["texture"].as_string() != "svg")) {
				texture_ = Texture::createTexture(node["texture"]);
			}
		} else if(node.has_key("image")) {
			if(node["image"].is_string() && (node["image"].as_string() != "fbo" && node["image"].as_string() != "svg")) {
				texture_ = Texture::createTexture(node["image"]);
			}
		}
		if(node.has_key("depth_check")) {
			setDepthEnable(node["depth_check"].as_bool());
		}
		if(node.has_key("depth_write")) {
			setDepthWrite(node["depth_write"].as_bool());
		}
		// XXX add depth function.
		if(node.has_key("use_lighting")) {
			enableLighting(node["use_lighting"].as_bool());
		}
	}

	void Renderable::setDerivedModel(const glm::vec3& p, const glm::quat& r, const glm::vec3& s)
	{
		derived_position_ = p;
		derived_rotation_ = r;
		derived_scale_ = s;
	}

	void Renderable::setPosition(const glm::vec3& position) 
	{
		position_ = position;
	}

	void Renderable::setPosition(float x, float y, float z) 
	{
		position_ = glm::vec3(x, y, z);
	}

	void Renderable::setPosition(int x, int y, int z) 
	{
		position_ = glm::vec3(float(x), float(y), float(z));
	}

	void Renderable::setRotation(float angle, const glm::vec3& axis) 
	{
		rotation_ = glm::angleAxis(glm::radians(angle), axis);
	}

	void Renderable::setRotation(const glm::quat& rot) 
	{
		rotation_ = rot;
	}

	void Renderable::setScale(float xs, float ys, float zs) 
	{
		scale_ = glm::vec3(xs, ys, zs);
	}

	void Renderable::setScale(const glm::vec3& scale) 
	{
		scale_ = scale;
	}

	glm::mat4 Renderable::getModelMatrix() const 
	{
		return glm::translate(glm::mat4(1.0f), position_ + derived_position_) 
			* glm::toMat4(derived_rotation_ * rotation_) 
			* glm::scale(glm::mat4(1.0f), scale_ * derived_scale_);
	}

	void Renderable::setCamera(const CameraPtr& camera)
	{
		camera_ = camera;
	}

	void Renderable::setLights(const LightPtrList& lights)
	{
		lights_ = lights;
	}

	void Renderable::setTexture(TexturePtr tex)
	{
		texture_ = tex;
		onTextureChanged();
	}

	void Renderable::setRenderTarget(const RenderTargetPtr& rt)
	{
		render_target_ = rt;
	}
	
	void Renderable::clearAttributeSets()
	{
		attributes_.clear();
	}

	void Renderable::addAttributeSet(const AttributeSetPtr& attrset)
	{
		attributes_.emplace_back(attrset);
		if(shader_) {
			shader_->configureActives(attrset);
		} else {
			LOG_WARN("No shader is set when adding an attribute set");
		}
	}

	void Renderable::addUniformBuffer(UniformBufferBase&& ub)
	{
		//uniforms_.emplace_back(std::move(ub));
		//uniforms_.insert(uniforms_.end(), std::move(ub));
		//if(shader_) {
		//	shader_->configureUniforms(ub);
		//}
	}

	void Renderable::setShader(ShaderProgramPtr shader)
	{
		//LOG_DEBUG("Reconfiguring attribute sets as new shader specified.");
		shader_ = shader;
		for(auto& attrset : attributes_) {
			shader_->configureActives(attrset);
		}
		//for(auto& ub : uniforms_) {
		//	shader_->configureUniforms(ub);
		//}
	}

	void Renderable::setClipSettings(const StencilSettings& settings, RenderablePtr mask)
	{
		stencil_settings_ = settings;
		stencil_mask_  = mask;
	}

	//void Renderable::clearUniformSets()
	//{
	//	uniforms_.clear();
	//}

	//void Renderable::addUniformSet(const UniformSetPtr& uniset)
	//{
	//	uniforms_.emplace_back(uniset);
	//}
}
