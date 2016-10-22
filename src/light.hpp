/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include "geometry.hpp"
#include "SceneObjectCallable.hpp"
#include "SceneUtil.hpp"

class CustomObject;
class Light;

typedef ffl::IntrusivePtr<Light> LightPtr;
typedef ffl::IntrusivePtr<const Light> ConstLightPtr;

class Light : public graphics::SceneObjectCallable
{
public:
	static LightPtr createLight(const CustomObject& obj, variant node);

	virtual variant write() const = 0;

	explicit Light(const CustomObject& obj, variant node);
	virtual ~Light();
	virtual void process() = 0;
	virtual bool onScreen(const rect& screen_area) const = 0;
protected:
	const CustomObject& object() const { return obj_; }
private:
	DECLARE_CALLABLE(Light);
	const CustomObject& obj_;
};

class CircleLight : public Light
{
public:
	CircleLight(const CustomObject& obj, variant node);
	CircleLight(const CustomObject& obj, int radius);
	variant write() const override;
	void process() override;
	bool onScreen(const rect& screen_area) const override;
	void preRender(const KRE::WindowPtr& wnd) override;
private:
	DECLARE_CALLABLE(CircleLight);
	void init();
	void updateVertices();

	std::shared_ptr<KRE::Attribute<glm::vec2>> fan_;
	std::shared_ptr<KRE::Attribute<KRE::vertex_color>> sq_;

	point center_;
	int radius_;

	KRE::Color last_color_;
};

class LightFadeLengthSetter
{
	int old_value_;
public:
	explicit LightFadeLengthSetter(int value);
	~LightFadeLengthSetter();
};
