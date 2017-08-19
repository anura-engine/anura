/*
	Copyright (C) 2013-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "SDL_opengles2.h"

#include "AttributeSet.hpp"

namespace KRE
{
	class HardwareAttributeGLESv2 : public HardwareAttribute
	{
	public:
		HardwareAttributeGLESv2(AttributeBase* parent);
		virtual ~HardwareAttributeGLESv2();
		void update(const void* value, ptrdiff_t offset, size_t size) override;
		void bind() override;
		void unbind() override;
		intptr_t value() override { return 0; }
		HardwareAttributePtr create(AttributeBase* parent) override;
	private:
		GLuint buffer_id_;
		GLenum access_pattern_;
		size_t size_;
	};


	class AttributeSetGLESv2 : public AttributeSet
	{
	public:
		enum class DrawMode {
			POINTS,
			LINE_STRIP,
			LINE_LOOP,
			LINES,
			TRIANGLE_STRIP,
			TRIANGLE_FAN,
			TRIANGLES,
			QUAD_STRIP,
			QUADS,
			POLYGON,		
		};
	
		explicit AttributeSetGLESv2(bool indexed, bool instanced);
		AttributeSetGLESv2(const AttributeSetGLESv2&);
		virtual ~AttributeSetGLESv2();	
		const void* getIndexArray() const override { return nullptr; }
		void bindIndex() override;
		void unbindIndex() override;
		bool isHardwareBacked() const override { return true; }
		AttributeSetPtr clone() override;
	private:
		DISALLOW_ASSIGN_AND_DEFAULT(AttributeSetGLESv2);
		void handleIndexUpdate() override;
		GLuint index_buffer_id_;
	};
	typedef std::shared_ptr<AttributeSet> AttributeSetPtr;
}
