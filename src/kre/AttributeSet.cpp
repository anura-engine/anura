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

#include "AttributeSet.hpp"
#include "DisplayDevice.hpp"

namespace KRE
{
	AttributeSet::AttributeSet(bool indexed, bool instanced)
		: draw_mode_(DrawMode::TRIANGLES),
		indexed_draw_(indexed),
		instanced_draw_(instanced),
		index_type_(IndexType::INDEX_NONE),
		instance_count_(0),
		offset_(0),
		count_(0)
	{
	}

	AttributeSet::AttributeSet(const AttributeSet& as)
		: draw_mode_(as.draw_mode_),
		  indexed_draw_(as.indexed_draw_),
		  instanced_draw_(as.instanced_draw_),
		  index_type_(as.index_type_),
		  instance_count_(as.instance_count_),
		  index8_(as.index8_),
		  index16_(as.index16_),
		  index32_(as.index32_),
		  attributes_(),
		  count_(as.count_),
		  offset_(as.offset_)
	{
		for(auto& attr : as.attributes_) {
			attributes_.emplace_back(attr->clone());
		}
	}

	AttributeSet::~AttributeSet()
	{
	}

	AttributeSetPtr AttributeSet::clone()
	{
		return std::make_shared<AttributeSet>(*this);
	}

	void AttributeSet::setDrawMode(DrawMode dm)
	{
		draw_mode_ = dm;
	}

	void AttributeSet::updateIndicies(const std::vector<uint8_t>& value) 
	{
		index_type_ = IndexType::INDEX_UCHAR;
		index8_ = value;
		count_ = index8_.size();
		handleIndexUpdate();
	}

	void AttributeSet::updateIndicies(const std::vector<uint16_t>& value) 
	{
		index_type_ = IndexType::INDEX_USHORT;
		index16_ = value;
		count_ = index16_.size();
		handleIndexUpdate();
	}

	void AttributeSet::updateIndicies(const std::vector<uint32_t>& value) 
	{
		index_type_ = IndexType::INDEX_ULONG;
		index32_ = value;
		count_ = index32_.size();
		handleIndexUpdate();
	}

	void AttributeSet::updateIndicies(std::vector<uint8_t>* value)
	{
		index_type_ = IndexType::INDEX_UCHAR;
		index8_.swap(*value);
		handleIndexUpdate();
	}

	void AttributeSet::updateIndicies(std::vector<uint16_t>* value)
	{
		index_type_ = IndexType::INDEX_USHORT;
		index16_.swap(*value);
		handleIndexUpdate();
	}

	void AttributeSet::updateIndicies(std::vector<uint32_t>* value)
	{
		index_type_ = IndexType::INDEX_ULONG;
		index32_.swap(*value);
		handleIndexUpdate();
	}

	void AttributeSet::addAttribute(const AttributeBasePtr& attrib) 
	{
		attributes_.emplace_back(attrib);
		auto hwbuffer = DisplayDevice::createAttributeBuffer(isHardwareBacked(), attrib.get());
		attrib->setDeviceBufferData(hwbuffer);
	}


	AttributeDesc::AttributeDesc(AttrType type, 
		unsigned num_elements,
		AttrFormat var_type,
		bool normalise,
		ptrdiff_t stride,
		ptrdiff_t offset,
		size_t divisor)
		: type_(type),
		  num_elements_(num_elements),
		  var_type_(var_type),
		  normalise_(normalise),
		  stride_(stride),
		  offset_(offset),
		  divisor_(divisor)
	{
		switch(type_) {
		case AttrType::POSITION:	type_name_ = "position"; break;
		case AttrType::COLOR:		type_name_ = "color"; break;
		case AttrType::TEXTURE:		type_name_ = "texcoord"; break;
		case AttrType::NORMAL:		type_name_ = "normal"; break;
		default:
			ASSERT_LOG(false, "Unknown type used.");
		}
	}

	AttributeDesc::AttributeDesc(const std::string& type_name, 
		unsigned num_elements,
		AttrFormat var_type,
		bool normalise,
		ptrdiff_t stride,
		ptrdiff_t offset,
		size_t divisor)
		: type_(AttrType::UNKOWN),
		type_name_(type_name),
		num_elements_(num_elements),
		var_type_(var_type),
		normalise_(normalise),
		stride_(stride),
		offset_(offset),
		divisor_(divisor)
	{
	}

	AttributeBase::AttributeBase(const AttributeBase& a)  
		: access_freq_(a.access_freq_),
		  access_type_(a.access_type_),
		  offs_(a.offs_),
		  desc_(a.desc_),
		  hardware_(),
		  hardware_buffer_(a.hardware_buffer_),
		  enabled_(a.enabled_)
	{
		hardware_ = DisplayDevice::createAttributeBuffer(hardware_, this);
	}
}
