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

#include <glm/gtx/quaternion.hpp>

#include "AlignedAllocator.hpp"
#include "RenderQueue.hpp"
#include "SceneFwd.hpp"
#include "ScopeableValue.hpp"
#include "StencilSettings.hpp"
#include "UniformBuffer.hpp"
#include "variant.hpp"

namespace KRE
{
	class Renderable : public ScopeableValue, public AlignedAllocator16
	{
	public:
		explicit Renderable();
		explicit Renderable(size_t order);
		explicit Renderable(const variant& node);
		virtual ~Renderable();

		void setFromVariant(const variant& node);

		void setPosition(const glm::vec3& position);
		void setPosition(float x, float y, float z=0.0f);
		void setPosition(int x, int y, int z=0);
		const glm::vec3& getPosition() const { return position_; }

		void setRotation(float angle, const glm::vec3& axis);
		void setRotation(const glm::quat& rot);
		const glm::quat& getRotation() const { return rotation_; }

		void setScale(float xs, float ys, float zs=1.0f);
		void setScale(const glm::vec3& scale);
		const glm::vec3& getScale() const { return scale_; }

		glm::mat4 getModelMatrix() const;
		bool ignoreGlobalModelMatrix() const { return ignore_global_model_; }
		void useGlobalModelMatrix(bool en=true) { ignore_global_model_ = en; }

		// These are derived paramters, refreshed from parent SceneNode
		void setDerivedModel(const glm::vec3& p, const glm::quat& r, const glm::vec3& s);

		size_t getOrder() const { return order_; }
		void setOrder(size_t o) { order_ = o; }

		const CameraPtr& getCamera() const { return camera_; }
		void setCamera(const CameraPtr& camera);

		const LightPtrList& getLights() const { return lights_; }
		void setLights(const LightPtrList& lights);

		TexturePtr getTexture() const { return texture_; }
		void setTexture(TexturePtr tex);

		const RenderTargetPtr& getRenderTarget() const { return render_target_; }
		void setRenderTarget(const RenderTargetPtr& rt);

		void setShader(ShaderProgramPtr shader);
		ShaderProgramPtr getShader() const { return shader_; }

		void setClipSettings(const StencilSettings& settings, RenderablePtr mask);
		bool hasClipSettings() const { return stencil_mask_ != nullptr; }
		const StencilSettings& getStencilSettings() const { return stencil_settings_; }
		RenderablePtr getStencilMask() const { return stencil_mask_; }

		void addAttributeSet(const AttributeSetPtr& attrset);
		//void addUniformSet(const UniformSetPtr& uniset);
		const std::vector<AttributeSetPtr>& getAttributeSet() const { return attributes_; }
		//const std::vector<UniformSetPtr>& getUniformSet() const { return uniforms_; }
		void clearAttributeSets();
		//void clearUniformSets();
		void addUniformBuffer(UniformBufferBase&& ub);

		bool isEnabled() const { return enabled_; }
		void enable(bool en=true) { enabled_ = en; }
		void disable() { enabled_ = false; }

		virtual void preRender(const WindowPtr& wm) {}
		virtual void postRender(const WindowPtr& wm) {}

		// Called just before rendering this item, after shaders and other variables
		// have been set-up
		virtual void renderBegin() {}
		// Called after draw commands have been sent before anything is torn down.
		virtual void renderEnd() {}
	private:
		virtual void onTextureChanged() {}

		size_t order_;
		glm::vec3 position_;
		glm::quat rotation_;
		glm::vec3 scale_;
		CameraPtr camera_;
		LightPtrList lights_;
		TexturePtr texture_;
		RenderTargetPtr render_target_;
		ShaderProgramPtr shader_;
		
		StencilSettings stencil_settings_;
		RenderablePtr stencil_mask_;

		glm::vec3 derived_position_;
		glm::quat derived_rotation_;
		glm::vec3 derived_scale_;

		std::vector<AttributeSetPtr> attributes_;
		//std::vector<UniformBufferBase> uniforms_;
		bool enabled_;
		bool ignore_global_model_;
	};
}
