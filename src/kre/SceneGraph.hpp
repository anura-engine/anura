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

#include "RenderFwd.hpp"
#include "SceneFwd.hpp"
#include "WindowManager.hpp"
#include "treetree/tree.hpp"

namespace KRE
{
	typedef std::function<SceneObjectPtr(const std::string&)> ObjectTypeFunction;

	class SceneGraph
	{
	public:
		SceneGraph(const std::string& name);
		~SceneGraph();
		void AttachNode(SceneNode* parent, SceneNodePtr node);
		static SceneGraphPtr Create(const std::string& name);
		SceneNodePtr CreateNode(const std::string& node_type=std::string(), const variant& node=variant());
		static void RegisterObjectType(const std::string& type, ObjectTypeFunction fn);
		SceneNodePtr RootNode();
		void RenderScene(const RenderManagerPtr& renderer);
		void RenderSceneHelper(const RenderManagerPtr& renderer, the::tree<SceneNodePtr>::pre_iterator& it, SceneNodeParams* snp);
	
		void Process(double);

		static void RegisterFactoryFunction(const std::string& type, std::function<SceneNodePtr(SceneGraph*,const variant&)>);
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
			SceneGraph::RegisterFactoryFunction(type, [](SceneGraph* sg, const variant& node) -> SceneNodePtr { return SceneNodePtr(new T(sg, node));});
		}
	};
}
