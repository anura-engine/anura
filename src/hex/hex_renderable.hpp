/*
	Copyright (C) 2014-2015 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "AttributeSet.hpp"
#include "SceneNode.hpp"
#include "SceneObject.hpp"

namespace hex
{
	class MapNode;
	typedef std::shared_ptr<MapNode> MapNodePtr;
	class MapLayer;
	typedef std::shared_ptr<MapLayer> MapLayerPtr;

	class MapNode : public KRE::SceneNode
	{
	public:
		explicit MapNode(std::weak_ptr<KRE::SceneGraph> sg, const variant& node);
		static MapNodePtr create(std::weak_ptr<KRE::SceneGraph> sg, const variant& node);
	private:
		std::vector<MapLayerPtr> layers_;
		void notifyNodeAttached(std::weak_ptr<SceneNode> parent);
		MapNode() = delete;
		MapNode(const MapNode&) = delete;
		void operator=(const MapNode&) = delete;
	};
	
	class MapLayer : public KRE::SceneObject
	{
	public:
		MapLayer(MapNodePtr parent);
		void preRender(const KRE::WindowPtr& wnd);
	private:
		std::shared_ptr<KRE::Attribute<KRE::vertex_texcoord>> attr_;
	};
}
