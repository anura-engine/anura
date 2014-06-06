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

#pragma once

#include <unordered_map>
#include <vector>

#include "RenderFwd.hpp"
#include "SceneFwd.hpp"

namespace KRE
{
	class SceneNode
	{
	public:
		explicit SceneNode(SceneGraph* sg);
		virtual ~SceneNode();
		void AttachNode(const SceneNodePtr& node);
		void AttachLight(size_t ref, const LightPtr& obj);
		void AttachCamera(const CameraPtr& obj);
		void AttachObject(const SceneObjectPtr& obj);
		void AttachRenderTarget(const RenderTargetPtr& obj);
		const CameraPtr& Camera() const { return camera_; }
		const LightPtrList& Lights() const { return lights_; }
		const RenderTargetPtr GetRenderTarget() const { return render_target_; }
		void RenderNode(const RenderManagerPtr& renderer, SceneNodeParams* rp);
		SceneGraph* ParentGraph() { return scene_graph_; }
		virtual void Process(double);
		virtual void NodeAttached();
		void SetNodeName(const std::string& s) { name_ = s; }
		const std::string& NodeName() const { return name_; }
	private:
		std::string name_;
		SceneGraph* scene_graph_;
		std::vector<SceneObjectPtr> objects_;
		LightPtrList lights_;
		CameraPtr camera_;
		RenderTargetPtr render_target_;
		SceneNode();
		SceneNode(const SceneNode&);
		SceneNode& operator=(const SceneNode&);

		friend std::ostream& operator<<(std::ostream& os, const SceneNode& node);
	};

	std::ostream& operator<<(std::ostream& os, const SceneNode& node);
}
