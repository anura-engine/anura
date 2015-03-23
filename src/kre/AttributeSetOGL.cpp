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

#include "AttributeSetOGL.hpp"

namespace KRE
{
	namespace
	{
		GLenum convert_access_type_and_frequency(AccessFreqHint f, AccessTypeHint t)
		{
			switch(f) {
			case AccessFreqHint::STATIC:
				switch(t) {
				case AccessTypeHint::DRAW: return GL_STATIC_DRAW;
				case AccessTypeHint::READ: return GL_STATIC_READ;
				case AccessTypeHint::COPY: return GL_STATIC_COPY;
				}
				break;
			case AccessFreqHint::STREAM:
				switch(t) {
				case AccessTypeHint::DRAW: return GL_STREAM_DRAW;
				case AccessTypeHint::READ: return GL_STREAM_READ;
				case AccessTypeHint::COPY: return GL_STREAM_COPY;
				}
				break;
			case AccessFreqHint::DYNAMIC:
				switch(t) {
				case AccessTypeHint::DRAW: return GL_DYNAMIC_DRAW;
				case AccessTypeHint::READ: return GL_DYNAMIC_READ;
				case AccessTypeHint::COPY: return GL_DYNAMIC_COPY;
				}
				break;
			}
			ASSERT_LOG(false, "Not a valid combination of Access Frequency and Access Type.");
			return GL_NONE;
		}
	}


	HardwareAttributeOGL::HardwareAttributeOGL(AttributeBase* parent)
		: HardwareAttribute(parent), 
		buffer_id_(-1),
		access_pattern_(convert_access_type_and_frequency(parent->getAccessFrequency(), parent->getAccessType())),
		size_(0)
	{
		glGenBuffers(1, &buffer_id_);
		//LOG_DEBUG("Created Hardware Attribute Buffer id: " << buffer_id_);
	}

	HardwareAttributePtr HardwareAttributeOGL::create(AttributeBase* parent)
	{
		return std::make_shared<HardwareAttributeOGL>(parent);		
	}

	HardwareAttributeOGL::~HardwareAttributeOGL()
	{
		glDeleteBuffers(1, &buffer_id_);
	}

	void HardwareAttributeOGL::update(const void* value, ptrdiff_t offset, size_t size)
	{
		glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
		if(offset == 0) {
			// this is a minor optimisation.
			glBufferData(GL_ARRAY_BUFFER, size, 0, access_pattern_);
			glBufferSubData(GL_ARRAY_BUFFER, 0, size, value);
			size_ = size;
		} else {
			if(size_ == 0) {
				glBufferData(GL_ARRAY_BUFFER, size+offset, 0, access_pattern_);
			}
			ASSERT_LOG(size+offset <= size_, 
				"When buffering data offset+size exceeds data store size: " 
				<< size+offset 
				<< " > " 
				<< size_);
			glBufferSubData(GL_ARRAY_BUFFER, offset, size, value);
			size_ = size + offset;
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void HardwareAttributeOGL::bind()
	{
		glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
	}

	void HardwareAttributeOGL::unbind()
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}


	AttributeSetOGL::AttributeSetOGL(bool indexed, bool instanced)
		: AttributeSet(indexed, instanced)
	{
		if(indexed) {
			glGenBuffers(1, &index_buffer_id_);
		}
	}

	AttributeSetOGL::AttributeSetOGL(const AttributeSetOGL& as)
		: AttributeSet(as)
	{
		if(as.isIndexed()) {
			glGenBuffers(1, &index_buffer_id_);
		}
	}

	AttributeSetOGL::~AttributeSetOGL()
	{
		if(isIndexed()) {
			glDeleteBuffers(1, &index_buffer_id_);
		}
	}

	AttributeSetPtr AttributeSetOGL::clone() 
	{
		return std::make_shared<AttributeSetOGL>(*this);
	}

	struct IndexManager
	{
		IndexManager(GLuint buffer_id) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer_id);
		}
		~IndexManager() {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
	};

	void AttributeSetOGL::bindIndex()
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id_);
	}

	void AttributeSetOGL::unbindIndex()
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	void AttributeSetOGL::handleIndexUpdate()
	{
		IndexManager im(index_buffer_id_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, getTotalArraySize(), getIndexData(), GL_STATIC_DRAW);
	}
}
