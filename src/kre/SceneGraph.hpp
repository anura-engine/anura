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

#include "RenderFwd.hpp"
#include "SceneFwd.hpp"
#include "WindowManager.hpp"
#include "treetree/tree.hpp"

namespace KRE
{
	class SceneGraph : public std::enable_shared_from_this<SceneGraph>
	{
	public:
		SceneGraph(const std::string& name);
		~SceneGraph();
		void removeNode(std::weak_ptr<SceneNode> parent, SceneNodePtr node);
		void attachNode(std::weak_ptr<SceneNode> parent, SceneNodePtr node);
		static SceneGraphPtr create(const std::string& name);
		SceneNodePtr createNode(const std::string& node_type=std::string(), const variant& node=variant());
		SceneNodePtr getRootNode();
		void renderScene(const RenderManagerPtr& renderer);
		void renderSceneHelper(const RenderManagerPtr& renderer, the::tree<SceneNodePtr>::pre_iterator& it, SceneNodeParams* snp);
	
		void process(float);

		static void registerFactoryFunction(const std::string& type, std::function<SceneNodePtr(std::weak_ptr<SceneGraph>,const variant&)>);
	private:
		std::string name_;
		the::tree<SceneNodePtr> graph_;
		SceneGraph(const SceneGraph&);

		friend std::ostream& operator<<(std::ostream& s, const SceneGraph& sg);
	};

	std::ostream& operator<<(std::ostream& s, const SceneGraph& sg);

	template<class T>
	struct SceneNodeRegistrar
	{
		SceneNodeRegistrar(const std::string& type)
		{
			// register the class factory function 
			SceneGraph::registerFactoryFunction(type, [](std::weak_ptr<SceneGraph> sg, const variant& node) -> SceneNodePtr { return T::create(sg, node); });
		}
	};
}
