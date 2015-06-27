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

#include <functional>
#include <set>
#include <unordered_map>
#include <vector>

#include "RenderFwd.hpp"
#include "SceneFwd.hpp"
#include "variant.hpp"

namespace KRE
{
	typedef std::function<SceneObjectPtr(const std::string&)> ObjectTypeFunction;

	class SceneNode : public std::enable_shared_from_this<SceneNode>
	{
	public:
		explicit SceneNode(std::weak_ptr<SceneGraph> sg);
		explicit SceneNode(std::weak_ptr<SceneGraph> sg, const variant& node);
		virtual ~SceneNode();
		void attachNode(const SceneNodePtr& node);
		void removeNode(const SceneNodePtr& node);
		void attachLight(size_t ref, const LightPtr& obj);
		void attachCamera(const CameraPtr& obj);
		void attachObject(const SceneObjectPtr& obj);
		void removeObject(const SceneObjectPtr& obj);
		void attachRenderTarget(const RenderTargetPtr& obj);
		const CameraPtr& getCamera() const { return camera_; }
		const LightPtrList& getLights() const { return lights_; }
		const RenderTargetPtr getRenderTarget() const { return render_target_; }
		void renderNode(const RenderManagerPtr& renderer, SceneNodeParams* rp);
		std::shared_ptr<SceneGraph> getParentGraph();
		std::shared_ptr<SceneNode> getParent();
		virtual void process(float);
		virtual void notifyNodeAttached(std::weak_ptr<SceneNode> parent);
		void setNodeName(const std::string& s) { name_ = s; }
		const std::string& getNodeName() const { return name_; }

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

		void clear() { objects_.clear(); }

		static void registerObjectType(const std::string& type, ObjectTypeFunction fn);
	private:
		// No default or assignment constructors
		SceneNode();
		void operator=(const SceneNode&);

		std::string name_;
		std::weak_ptr<SceneGraph> scene_graph_;
		std::weak_ptr<SceneNode> parent_;
		std::set<SceneObjectPtr> objects_;
		LightPtrList lights_;
		CameraPtr camera_;
		RenderTargetPtr render_target_;
		glm::vec3 position_;
		glm::quat rotation_;
		glm::vec3 scale_;

		friend std::ostream& operator<<(std::ostream& os, const SceneNode& node);
	};

	template<class T>
	struct SceneObjectRegistrar
	{
		SceneObjectRegistrar(const std::string& type)
		{
			// register the class factory function 
			SceneNode::registerObjectType(type, [](const std::string& type) -> std::shared_ptr<T> { return std::make_shared<T>(type); });
		}
	};

	std::ostream& operator<<(std::ostream& os, const SceneNode& node);
}
