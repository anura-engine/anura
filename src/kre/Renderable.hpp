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

#include "AttributeSet.hpp"
#include "Material.hpp"
#include "RenderQueue.hpp"
#include "SceneFwd.hpp"
#include "../variant.hpp"

namespace KRE
{
	class Renderable
	{
	public:
		Renderable();
		explicit Renderable(size_t order);
		explicit Renderable(const variant& node);
		virtual ~Renderable();

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

		void setColor(float r, float g, float b, float a=1.0);
		void setColor(int r, int g, int b, int a=255);
		void setColor(const Color& color);
		const Color& getColor() const { return color_; }
		bool isColorSet() const { return color_set_; }

		size_t getOrder() const { return order_; }
		void setOrder(size_t o) { order_ = o; }

		const CameraPtr& getCamera() const { return camera_; }
		void setCamera(const CameraPtr& camera);

		const LightPtrList& getLights() const { return lights_; }
		void setLights(const LightPtrList& lights);

		const MaterialPtr& getMaterial() const { return material_; }
		void setMaterial(const MaterialPtr& material);

		const BlendEquation& getBlendEquation() const { return blend_eqn_; }
		void setBlendEquation(const BlendEquation& eqn) { blend_eqn_ = eqn; }

		const BlendMode& getBlendMode() const { return blend_mode_; }
		void setBlendMode(const BlendMode& bm) { blend_mode_ = bm; }
		void setBlendMode(BlendModeConstants src, BlendModeConstants dst) { blend_mode_.set(src, dst); }

		const RenderTargetPtr& getRenderTarget() const { return render_target_; }
		void setRenderTarget(const RenderTargetPtr& rt);

		void setDisplayData(const DisplayDevicePtr& dd, const DisplayDeviceDef& def);
		const DisplayDeviceDataPtr& getDisplayData() const { return display_data_; }

		void addAttributeSet(const AttributeSetPtr& attrset);
		//void addUniformSet(const UniformSetPtr& uniset);
		const std::vector<AttributeSetPtr>& getAttributeSet() const { return attributes_; }
		//const std::vector<UniformSetPtr>& getUniformSet() const { return uniforms_; }
		void clearAttributeSets();
		//void clearUniformSets();

		virtual void preRender(const WindowManagerPtr& wm) {}
		virtual void postRender(const WindowManagerPtr& wm) {}

		// Called just before rendering this item, after shaders and other variables
		// have been set-up
		virtual void renderBegin() {}
		// Called after draw commands have been sent before anything is torn down.
		virtual void renderEnd() {}
	private:
		size_t order_;
		glm::vec3 position_;
		glm::quat rotation_;
		glm::vec3 scale_;
		CameraPtr camera_;
		LightPtrList lights_;
		MaterialPtr material_;
		RenderTargetPtr render_target_;
		Color color_;
		bool color_set_;
		BlendEquation blend_eqn_;
		BlendMode blend_mode_;

		std::vector<AttributeSetPtr> attributes_;
		//std::vector<UniformSetPtr> uniforms_;

		DisplayDeviceDataPtr display_data_;
		Renderable(const Renderable&);
	};
}
