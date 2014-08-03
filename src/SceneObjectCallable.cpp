/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "SceneObjectCallable.hpp"

namespace graphics
{
	SceneObjectCallable::SceneObjectCallable(const variant& node)
		: SceneObject(node)
	{
	}

	SceneObjectCallable::~SceneObjectCallable()
	{
	}

	SceneObjectCallable::SceneObjectCallable()
		: SceneObject("SceneObjectCallable")
	{
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(SceneObjectCallable)
		DEFINE_FIELD(name, "string")
			return variant(obj.objectName());
		DEFINE_SET_FIELD
			obj.setObjectName(value.as_string());

		DEFINE_FIELD(shader, "string")
			return variant(obj.getShaderName());
		DEFINE_SET_FIELD
			obj.setShaderName(value.as_string());

		DEFINE_FIELD(position, "[decimal,decimal]|[decimal,decimal,decimal]")
			auto p = obj.getPosition();
			std::vector<variant> v;
			v.emplace_back(p.x);
			v.emplace_back(p.y);
			if(p.z != 0) {
				v.emplace_back(p.z);
			}
			return variant(&v);
		DEFINE_SET_FIELD
			glm::vec3 p(0.0f);
			p.x = value[0].as_float();
			p.y = value[1].as_float();
			if(value.num_elements() > 2) {
				p.z = value[2].as_float();
			}
			obj.setPosition(p);
		// Need to add rotation/scale/order -- time and sanity permitting.

		DEFINE_FIELD(rotation, "decimal|[decimal,decimal]|[decimal,decimal,decimal]")
			auto ea = glm::eulerAngles(obj.getRotation());
			std::vector<variant> v;
			v.emplace_back(ea.x);
			v.emplace_back(ea.y);
			if(ea.z == 0) {
				if(ea.y == 0) {
					return variant(ea.x);
				} 
				return variant(&v);
			}
			v.emplace_back(ea.z);
			return variant(&v);
		//DEFINE_SET_FIELD

		DEFINE_FIELD(blend, "string|[string,string]")
			return obj.getBlendMode().write();
		DEFINE_SET_FIELD
			obj.setBlendMode(KRE::BlendMode(value));

		DEFINE_FIELD(blend_equation, "string|[string,string]")
			return obj.getBlendEquation().write();
		DEFINE_SET_FIELD
			obj.setBlendEquation(KRE::BlendEquation(value));

		DEFINE_FIELD(order, "int")
			return variant(obj.getOrder());
		DEFINE_SET_FIELD
			obj.setOrder(value.as_int());

	END_DEFINE_CALLABLE(SceneObjectCallable)
}
