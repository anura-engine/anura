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
#include "SceneGraph.hpp"
#include "SceneObject.hpp"

namespace KRE
{
	SceneObject::SceneObject(const std::string& name)
		: name_(name), 
		queue_(0)
	{
	}

	SceneObject::SceneObject(const variant& node)
		: Renderable(node),
		queue_(0)
	{
		if(node.has_key("name")) {
			name_ = node["name"].as_string();
		}
		if(node.has_key("queue_value")) {
			queue_ = static_cast<size_t>(node["queue_value"].as_int());
		}
		if(node.has_key("shader")) {
			shader_name_ = node["shader"].as_string();
		}
	}

	SceneObject::SceneObject(const SceneObject& op)
		: name_(op.name_),
		queue_(op.queue_)
	{
	}

	SceneObject::~SceneObject()
	{
	}

	void SceneObject::setShaderName(const std::string& shader)
	{
		// XXX hmm hmm there is no way to update hints currently -- this is a must fix item.
		// since we can't dynamically change shaders otherwise.
		shader_name_ = shader;
	}

	DisplayDeviceDef SceneObject::attach(const DisplayDevicePtr& dd)
	{
		DisplayDeviceDef def(getAttributeSet()/*, getUniformSet()*/);
		if(!shader_name_.empty()) {
			def.setHint("shader", shader_name_);
		}
		doAttach(dd, &def);
		return def;
	}
}
