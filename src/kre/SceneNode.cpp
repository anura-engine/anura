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

#include "asserts.hpp"
#include "CameraObject.hpp"
#include "LightObject.hpp"
#include "RenderManager.hpp"
#include "RenderTarget.hpp"
#include "SceneGraph.hpp"
#include "SceneNode.hpp"
#include "SceneObject.hpp"
#include "variant_utils.hpp"

namespace KRE
{
	namespace
	{
		typedef std::map<std::string, ObjectTypeFunction> SceneObjectFactoryLookupTable;
		SceneObjectFactoryLookupTable& get_object_factory()
		{
			static SceneObjectFactoryLookupTable res;
			return res;
		}

		const glm::vec3 x_axis(1.0f, 0.0f, 0.0f);
		const glm::vec3 y_axis(0.0f, 1.0f, 0.0f);
		const glm::vec3 z_axis(0.0f, 0.0f, 1.0f);
	}

	SceneNode::SceneNode(std::weak_ptr<SceneGraph> sg)
		: scene_graph_(sg),
		  position_(0.0f),
		  rotation_(1.0f, 0.0f, 0.0f, 0.0f),
		  scale_(1.0f)
	{
		ASSERT_LOG(scene_graph_.lock() != nullptr, "scene_graph_ was null.");
	}

	SceneNode::SceneNode(std::weak_ptr<SceneGraph> sg, const variant& node)
		: scene_graph_(sg),
		  parent_(),
		  position_(0.0f),
		  rotation_(1.0f, 0.0f, 0.0f, 0.0f),
		  scale_(1.0f)
	{
		ASSERT_LOG(scene_graph_.lock() != nullptr, "scene_graph_ was null.");
		if(node.has_key("camera")) {
			attachCamera(Camera::createInstance(node["camera"]));
		}
		if(node.has_key("lights")) {
			auto& lights = node["lights"];
			if(lights.is_map()) {
				for(auto& pair : lights.as_map()) {
					ASSERT_LOG(pair.first.is_int() && pair.second.is_map(), 
						"'lights' map should be int:light_map pairs. " 
						<< pair.first.to_debug_string() << " : " << pair.second.to_debug_string());
					size_t ref = pair.first.as_int32();
					auto light_obj = std::make_shared<Light>(pair.second);
					attachLight(ref, light_obj);
				}
			} else if(lights.is_list()) {
				size_t ref = 0;
				for(auto& light : lights.as_list()) {
					auto light_obj = std::make_shared<Light>(light);
					attachLight(ref, light_obj);
					++ref;
				}
			} else {
				ASSERT_LOG(false, "Attribute 'lights' should be a list or map, found: " << lights.to_debug_string());
			}
		}
		if(node.has_key("render_target")) {
			auto rt = RenderTarget::create(node["render_target"]);
			attachRenderTarget(rt);
		}
		if(node.has_key("position")) {
			position_ = variant_to_vec3(node["position"]);
		}
		if(node.has_key("translation")) {
			position_ = variant_to_vec3(node["translation"]);
		}
		// rotation.
		if(node.has_key("rotation")) {
			// single rotation value. Assume about z-axis.
			if(node["rotation"].is_numeric()) {
				rotation_ = glm::angleAxis(glm::radians(node["rotation"].as_float()), z_axis);
			} else if(node["rotation"].is_list()) {
				if(node["rotation"].num_elements() == 3 && node["rotation"][0].is_float()) {
					// Attempt to specify rotation as Euler angles.
					rotation_ = glm::quat(variant_to_vec3(node["rotation"]));
				} else if(node["rotation"].num_elements() == 4 && node["rotation"][0].is_float()) {
					// Attempt to specify rotation as quaternion
					rotation_ = variant_to_quat(node["rotation"]);
				} else {
					// list of maps format. [{angle: 10, axis:[0,1,0]},{angle:20, axis:[0,0,1]}]
					for(int n = 0; n != node["rotation"].num_elements(); ++n) {
						const variant& aa = node["rotation"][n];
						ASSERT_LOG(aa.is_map(), "Expected the 'rotation' attribute to be a list of maps. " << aa.to_debug_string());
						ASSERT_LOG(aa.has_key("angle") && aa.has_key("axis"), "'rotation' attribute should me a list of maps containing 'angle' and 'axis'. " << aa.to_debug_string());
						rotation_ = rotation_ * glm::angleAxis(glm::radians(aa["angle"].as_float()), variant_to_vec3(aa["axis"]));
					}
				}
			} else {
				ASSERT_LOG(false, "Unrecognised format for 'rotation' attribute. " << node["rotation"].to_debug_string());
			}
		}
		if(node.has_key("scale")) {
			if(node["scale"].is_numeric()) {
				scale_ = glm::vec3(node["scale"].as_float());
			} else {
				scale_ = variant_to_vec3(node["scale"]);
			}
		}
	}

	SceneNode::~SceneNode()
	{
	}

	void SceneNode::attachNode(const SceneNodePtr& node)
	{
		getParentGraph()->attachNode(shared_from_this(), node);
	}

	void SceneNode::removeNode(const SceneNodePtr& node)
	{
		getParentGraph()->removeNode(shared_from_this(), node);
	}

	void SceneNode::attachObject(const SceneObjectPtr& obj)
	{
		objects_.emplace(obj);
	}

	void SceneNode::removeObject(const SceneObjectPtr& obj)
	{
		auto it = objects_.find(obj);
		ASSERT_LOG(it != objects_.end(), "Object is not in list: " << obj);
		objects_.erase(it);
	}

	void SceneNode::attachLight(size_t ref, const LightPtr& obj)
	{
		auto it = lights_.find(ref);
		if(it != lights_.end()) {
			lights_.erase(it);
		}
		lights_.emplace(ref,obj);
	}

	void SceneNode::attachCamera(const CameraPtr& obj)
	{
		camera_ = obj;
	}

	void SceneNode::attachRenderTarget(const RenderTargetPtr& obj)
	{
		render_target_ = obj;
	}

	void SceneNode::renderNode(const RenderManagerPtr& renderer, SceneNodeParams* rp)
	{
		if(camera_) {
			rp->camera = camera_;
		}
		for(auto l : lights_) {
			rp->lights[l.first] = l.second;
		}
		if(render_target_) {
			rp->render_target = render_target_;
			render_target_->clear();
		}
		
		for(auto o : objects_) {
			o->setDerivedModel(getPosition(), getRotation(), getScale());
			o->setCamera(rp->camera);
			o->setLights(rp->lights);
			o->setRenderTarget(rp->render_target);
			renderer->addRenderableToQueue(o->getQueue(), o->getOrder(), o);
		}
	}

	void SceneNode::setPosition(const glm::vec3& position) 
	{
		position_ = position;
	}

	void SceneNode::setPosition(float x, float y, float z) 
	{
		position_ = glm::vec3(x, y, z);
	}

	void SceneNode::setPosition(int x, int y, int z) 
	{
		position_ = glm::vec3(float(x), float(y), float(z));
	}

	void SceneNode::setRotation(float angle, const glm::vec3& axis) 
	{
		rotation_ = glm::angleAxis(glm::radians(angle), axis);
	}

	void SceneNode::setRotation(const glm::quat& rot) 
	{
		rotation_ = rot;
	}

	void SceneNode::setScale(float xs, float ys, float zs) 
	{
		scale_ = glm::vec3(xs, ys, zs);
	}

	void SceneNode::setScale(const glm::vec3& scale) 
	{
		scale_ = scale;
	}

	glm::mat4 SceneNode::getModelMatrix() const 
	{
		return glm::translate(glm::mat4(1.0f), position_) * glm::toMat4(rotation_) * glm::scale(glm::mat4(1.0f), scale_);
	}

	void SceneNode::notifyNodeAttached(std::weak_ptr<SceneNode> parent)
	{
		parent_ = parent;
	}

	void SceneNode::process(float)
	{
		// nothing need be done as default
	}

	std::shared_ptr<SceneGraph> SceneNode::getParentGraph() 
	{ 
		auto sg = scene_graph_.lock(); 
		ASSERT_LOG(sg != nullptr, "Parent scene graph has been deleted.");
		return sg;
	}

	std::shared_ptr<SceneNode> SceneNode::getParent()
	{
		auto parent = parent_.lock();
		ASSERT_LOG(parent != nullptr, "Parent scene node has been deleted.");
		return parent;
	}

	void SceneNode::registerObjectType(const std::string& type, ObjectTypeFunction fn)
	{
		auto it = get_object_factory().find(type);
		ASSERT_LOG(it != get_object_factory().end(), "Type(" << type << ") already registered");
		get_object_factory()[type] = fn;
	}


	std::ostream& operator<<(std::ostream& os, const SceneNode& node)
	{
		os  << "NODE(" 
			<< node.getNodeName() << " : "
			<< (node.camera_ ? "1 camera, " : "") 
			<< node.lights_.size() << " light" << (node.lights_.size() != 1 ? "s" : "") << ", "
			<< node.objects_.size() << " object" << (node.objects_.size() != 1 ? "s (" : " (");
		for(auto o : node.objects_) {
			os << ", \"" << o->objectName() << "\"";
		}
		os << "))";
		return os;
	}
}
