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

#include <algorithm>
#include <iterator>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include "asserts.hpp"
#include "AlignedAllocator.hpp"
#include "DisplayDeviceFwd.hpp"
#include "ScopeableValue.hpp"
#include "Util.hpp"

namespace KRE
{
	// abstract base class for Hardware-buffered attributes.
	class HardwareAttribute
	{
	public:
		HardwareAttribute(AttributeBase* parent) : parent_(parent) {}
		virtual ~HardwareAttribute() {}
		virtual void update(const void* value, ptrdiff_t offset, size_t size) = 0;
		virtual void bind() {}
		virtual void unbind() {}
		virtual intptr_t value() = 0;
		virtual HardwareAttributePtr create(AttributeBase* parent) = 0;
	private:
		AttributeBase* parent_;
	};
	typedef std::shared_ptr<HardwareAttribute> HardwareAttributePtr;

	class HardwareAttributeImpl : public HardwareAttribute
	{
	public:
		HardwareAttributeImpl(AttributeBase* parent) : HardwareAttribute(parent), value_(0) {}
		virtual ~HardwareAttributeImpl() {}
		void update(const void* value, ptrdiff_t offset, size_t size) override {
			if(offset == 0) {
				value_ = reinterpret_cast<intptr_t>(value);
			}
		}
		void bind() override {}
		void unbind() override {}
		intptr_t value() override { return value_; }
		HardwareAttributePtr create(AttributeBase* parent) override {
			return std::make_shared<HardwareAttributeImpl>(parent);
		}
	private:
		intptr_t value_;
	};

	class AttributeDesc
	{
	public:
		explicit AttributeDesc(AttrType type, 
			unsigned num_elements,
			AttrFormat var_type,
			bool normalise=false,
			ptrdiff_t stride=0,
			ptrdiff_t offset=0,
			size_t divisor=1);
		explicit AttributeDesc(const std::string& type_name, 
			unsigned num_elements,
			AttrFormat var_type,
			bool normalise=false,
			ptrdiff_t stride=0,
			ptrdiff_t offset=0,
			size_t divisor=1);
		AttrType getAttrType() const { return type_; }
		const std::string& getAttrName() const { return type_name_; }
		AttrFormat getVarType() const { return var_type_; }
		unsigned getNumElements() const { return num_elements_; }
		bool normalise() const { return normalise_; }
		ptrdiff_t getStride() const { return stride_; }
		ptrdiff_t getOffset() const { return offset_; }
		size_t getDivisor() const { return divisor_; }
		void setLocation(unsigned location) { location_ = location; }
		unsigned getLocation() const { return location_; }
	private:
		AttrType type_;
		std::string type_name_;
		AttrFormat var_type_;
		unsigned num_elements_;
		bool normalise_;
		ptrdiff_t stride_;
		ptrdiff_t offset_;
		size_t divisor_;
		unsigned location_;
	};

	enum class AccessFreqHint {
		//! Data store modified once and used in-frequently
		STREAM,
		//! Data store modified once and used many times
		STATIC,
		//! Data store modified repeatedly and used many times.
		DYNAMIC,
	};
	enum class AccessTypeHint {
		//! Modified by application, used by display device for drawing.
		DRAW,
		//! Modified by display device, returned to application.
		READ,
		//! Data is modified by display device and used by display device for copying.
		COPY,
	};

	class AttributeBase : public AlignedAllocator16
	{
	public:
		AttributeBase(AccessFreqHint freq, AccessTypeHint type)
			: access_freq_(freq),
			  access_type_(type),
			  offs_(0),
              hardware_buffer_(false),
			  enabled_(true) {
		}
		AttributeBase(const AttributeBase& a);
		virtual ~AttributeBase() {}
		void addAttributeDesc(const AttributeDesc& attrdesc) {
			desc_.emplace_back(attrdesc);
		}
		std::vector<AttributeDesc>& getAttrDesc() { return desc_; }
		void setOffset(ptrdiff_t offs) {
			offs_ = offs;
		}
		ptrdiff_t getOffset() const { return offs_; } 
		AccessFreqHint getAccessFrequency() const { return access_freq_; }
		AccessTypeHint getAccessType() const { return access_type_; }
		HardwareAttributePtr getDeviceBufferData() { return hardware_; }
		void setDeviceBufferData(const HardwareAttributePtr& hardware) { 
			hardware_ = hardware; 
			handleAttachHardwareBuffer();
		}
		void enable(bool e=true) { enabled_ = e; }
		void disable() { enabled_ = false; }
		bool isEnabled() const { return enabled_; }

		virtual AttributeBasePtr clone() = 0;
		void setParent(std::weak_ptr<AttributeSet> attrset) { parent_ = attrset; }
		AttributeSetPtr getParent() const;
	private:
		virtual void handleAttachHardwareBuffer() = 0;
		AccessFreqHint access_freq_;
		AccessTypeHint access_type_;
		ptrdiff_t offs_;
		std::vector<AttributeDesc> desc_;
		HardwareAttributePtr hardware_;
		bool hardware_buffer_;
		bool enabled_;
		std::weak_ptr<AttributeSet> parent_;
	};

	/* Templated attribute buffer. Is sub-optimal in that we double buffer attributes
		if there is a real hardware buffer attached. But mitigating that it is easy
		for us to generate a new hardware buffer from existing data in the case of
		a context tear down.
	*/
	template<typename T, 
		template<typename E, 
		         typename = std::allocator<E>> 
		class Container = std::vector>
	class Attribute : public AttributeBase
	{
	public:
		typedef typename Container<T>::reference reference;
		typedef typename Container<T>::const_reference const_reference;
		typedef typename Container<T>::iterator iterator;
		typedef typename Container<T>::const_iterator const_iterator;
		typedef typename Container<T>::size_type size_type;
		typedef T value_type;

		Attribute(AccessFreqHint freq, AccessTypeHint type=AccessTypeHint::DRAW) 
			:  AttributeBase(freq, type) {
		}
		void clear() {
			elements_.clear();
			getParent()->setCount(0);
			getParent()->clearMultiDrawData();
		}
		void update(const Container<T>& values) {
			elements_ = values;
			if(getDeviceBufferData() && elements_.size() > 0) {
				getDeviceBufferData()->update(&elements_[0], 0, elements_.size() * sizeof(T));
				getParent()->setCount(elements_.size());
			}
		}
		void update(const Container<T>& src, iterator dst) {
			std::ptrdiff_t dst1 = std::distance(elements_.begin(), dst);
			std::ptrdiff_t dst2 = std::distance(src.begin(), src.end());
			std::copy(src.begin(), src.end(), std::inserter(elements_, dst));
			if(getDeviceBufferData() && dst2 > 0) {
				getDeviceBufferData()->update(&elements_[0], dst1, dst2 * sizeof(T));
				getParent()->setCount(dst1 + dst2);
			}
		}
		void update(Container<T>* src, iterator dst) {
			std::ptrdiff_t dst1 = std::distance(elements_.begin(), dst);
			std::ptrdiff_t dst2 = std::distance(src->begin(), src->end());
			std::move(src->begin(), src->end(), std::inserter(elements_, dst));
			if(getDeviceBufferData() && dst2 > 0) {
				getDeviceBufferData()->update(&elements_[0], dst1, dst2 * sizeof(T));
				getParent()->setCount(dst1 + dst2);
			}
		}
		void update(Container<T>* values) {
			elements_.swap(*values);
			if(getDeviceBufferData() && elements_.size() > 0) {
				getDeviceBufferData()->update(&elements_[0], 0, elements_.size() * sizeof(T));
				getParent()->setCount(elements_.size());
			}
		}
		void addMultiDraw(Container<T>* src) {
			ASSERT_LOG(getParent() != nullptr && getParent()->isMultiDrawEnabled(), "Parent attribute set not enabled for multi-draw. Call enableMultiDraw() on parent.");
			std::ptrdiff_t dst1 = elements_.size();
			std::ptrdiff_t dst2 = std::distance(src->begin(), src->end());
			std::move(src->begin(), src->end(), std::inserter(elements_, elements_.end()));
			if(getDeviceBufferData() && dst2 > 0) {
				getDeviceBufferData()->update(&elements_[0], dst1, dst2 * sizeof(T));
			}
			getParent()->addMultiDrawData(dst1, dst2);
		}
		size_t size() const { 
			return elements_.size();
		}
		void bind() {
			ASSERT_LOG(getDeviceBufferData() != nullptr, "Bind call on null hardware attribute buffer.");
			getDeviceBufferData()->bind();
		}		
		void unbind() {
			ASSERT_LOG(getDeviceBufferData() != nullptr, "Bind call on null hardware attribute buffer.");
			getDeviceBufferData()->unbind();
		}
		const_iterator begin() const {
			return elements_.begin();
		}
		const_iterator end() const {
			return elements_.end();
		}
		const_iterator cbegin() const {
			return elements_.cbegin();
		}
		const_iterator cend() const {
			return elements_.cend();
		}
		iterator begin() {
			return elements_.begin();
		}
		iterator end() {
			return elements_.end();
		}
		void setOffset(const_iterator& it) {
			setOffset(std::distance(elements_.begin(), it));
		}
		AttributeBasePtr clone() override {
			return std::make_shared<Attribute<T, Container>>(*this);
		}
	private:
		void handleAttachHardwareBuffer() override {
			// This just makes sure that if we add any elements
			// before an attach then they are all updated correctly.
			if(elements_.size() > 0) {
				getDeviceBufferData()->update(&elements_[0], 0, elements_.size() * sizeof(T));
				getParent()->setCount(elements_.size());
			}
		}
		Container<T> elements_;
	};

	// This is mostly ugly, do not use unless you're sure you know what you're doing.
	class GenericAttribute : public AttributeBase
	{
	public:
		GenericAttribute(AccessFreqHint freq, AccessTypeHint type=AccessTypeHint::DRAW);
		AttributeBasePtr clone() override;
		void update(const void* data_ptr, int data_size, int count);
	private:
		void handleAttachHardwareBuffer() override;
	};

	class AttributeSet : public ScopeableValue, public std::enable_shared_from_this<AttributeSet>
	{
	public:
		explicit AttributeSet(bool indexed, bool instanced);
		AttributeSet(const AttributeSet& as);
		virtual ~AttributeSet();

		void setDrawMode(DrawMode dm);
		DrawMode getDrawMode() { return draw_mode_; }

		bool isIndexed() const { return indexed_draw_; }
		bool isInstanced() const { return instanced_draw_; }
		IndexType getIndexType() const { return index_type_; }
		virtual const void* getIndexArray() const { 
			switch(index_type_) {
			case IndexType::INDEX_NONE:		break;
			case IndexType::INDEX_UCHAR:	return &index8_[0];
			case IndexType::INDEX_USHORT:	return &index16_[0];
			case IndexType::INDEX_ULONG:	return &index32_[0];
			}
			ASSERT_LOG(false, "Index type not set to valid value.");
		};
		int getTotalArraySize() const {
			switch(index_type_) {
			case IndexType::INDEX_NONE:		break;
			case IndexType::INDEX_UCHAR:	return static_cast<int>(index8_.size() * sizeof(uint8_t));
			case IndexType::INDEX_USHORT:	return static_cast<int>(index16_.size() * sizeof(uint16_t));
			case IndexType::INDEX_ULONG:	return static_cast<int>(index32_.size() * sizeof(uint32_t));
			}
			ASSERT_LOG(false, "Index type not set to valid value.");
		}
		void setCount(size_t count) { count_= count; }
		size_t getCount() const { return count_; }
		void setInstanceCount(int instance_count) { instance_count_ = instance_count; }
		int getInstanceCount() const { return instance_count_; }

		void updateIndicies(const std::vector<uint8_t>& value);
		void updateIndicies(const std::vector<uint16_t>& value);
		void updateIndicies(const std::vector<uint32_t>& value);
		void updateIndicies(std::vector<uint8_t>* value);
		void updateIndicies(std::vector<uint16_t>* value);
		void updateIndicies(std::vector<uint32_t>* value);

		void addAttribute(const AttributeBasePtr& attrib);

		virtual void bindIndex() {};
		virtual void unbindIndex() {};

		void setOffset(ptrdiff_t offset) { offset_ = offset; }
		ptrdiff_t getOffset() const { return offset_; }

		virtual bool isHardwareBacked() const { return false; }

		std::vector<AttributeBasePtr>& getAttributes() { return attributes_; }

		void enable(bool e=true) { enabled_ = e; }
		void disable() { enabled_ = false; }
		bool isEnabled() const { return enabled_; }

		void enableMultiDraw(bool en=true) { multi_draw_enabled_ = en; }
		bool isMultiDrawEnabled() const { return multi_draw_enabled_; }
		int getMultiDrawCount() const { return multi_draw_instances_; }
		void clearMultiDrawData() { 
			multi_draw_count_.clear();
			multi_draw_offset_.clear();
			multi_draw_instances_ = 0;
		}
		void addMultiDrawData(std::ptrdiff_t offset, std::size_t size) { 
			multi_draw_count_.emplace_back(static_cast<int>(size)); 
			multi_draw_offset_.emplace_back(static_cast<int>(offset));
			++multi_draw_instances_;
		}
		const std::vector<int>& getMultiCountArray() const { return multi_draw_count_; }
		const std::vector<int>& getMultiOffsetArray() const { return multi_draw_offset_; }

		virtual AttributeSetPtr clone();
	protected:
		const void* getIndexData() const { 
			switch(index_type_) {
				case IndexType::INDEX_NONE:		break;
				case IndexType::INDEX_UCHAR:	return &index8_[0];
				case IndexType::INDEX_USHORT:	return &index16_[0];
				case IndexType::INDEX_ULONG:	return &index32_[0];
			}
			ASSERT_LOG(false, "Index type not set to valid value.");
		};
	private:
		virtual void handleIndexUpdate() {}
		DrawMode draw_mode_;
		bool indexed_draw_;
		bool instanced_draw_;
		IndexType index_type_;
		int instance_count_;
		std::vector<uint8_t> index8_;
		std::vector<uint16_t> index16_;
		std::vector<uint32_t> index32_;
		std::vector<AttributeBasePtr> attributes_;
		size_t count_;
		ptrdiff_t offset_;
		bool enabled_;
		bool multi_draw_enabled_;
		int multi_draw_instances_;
		std::vector<int> multi_draw_count_;
		std::vector<int> multi_draw_offset_;
		AttributeSet() =delete;
	};
}
