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

#include <memory>
#include <string>
#include <unordered_map>

#include "glm/glm.hpp"

#include "RenderFwd.hpp"
#include "SceneUtil.hpp"

namespace KRE
{
	class Light;
	typedef std::shared_ptr<Light> LightPtr;
	typedef std::unordered_map<size_t, LightPtr> LightPtrList;
	class Camera;
	typedef std::shared_ptr<Camera> CameraPtr;
	class Parameter;
	typedef std::shared_ptr<Parameter> ParameterPtr;
	class SceneObject;
	typedef std::shared_ptr<SceneObject> SceneObjectPtr;
	class SceneNode;
	typedef std::shared_ptr<SceneNode> SceneNodePtr;
	class SceneGraph;
	typedef std::shared_ptr<SceneGraph> SceneGraphPtr;

	struct SceneNodeParams
	{
		CameraPtr camera;
		LightPtrList lights;
		RenderTargetPtr render_target;
	};

	class Blittable;
}
