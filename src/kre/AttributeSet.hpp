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

#include <algorithm>
#include <iterator>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include "../asserts.hpp"
#include "Blend.hpp"
#include "../Color.hpp"
#include "DisplayDeviceFwd.hpp"
#include "Util.hpp"

namespace KRE
{
	class AttributeBase;
	typedef std::shared_ptr<AttributeBase> AttributeBasePtr;

	// abstract base class for Hardware-buffered attributes.
	class HardwareAttribute
	{
	public:
		HardwareAttribute(AttributeBase* parent) : parent_(parent) {}
		virtual ~HardwareAttribute() {}
		virtual void Update(const void* value, ptrdiff_t offset, size_t size) = 0;
		virtual void Bind() {}
		virtual void Unbind() {}
		virtual intptr_t Value() = 0;		
	private:
		AttributeBase* parent_;
	};
	typedef std::shared_ptr<HardwareAttribute> HardwareAttributePtr;

	class HardwareAttributeImpl : public HardwareAttribute
	{
	public:
		HardwareAttributeImpl(AttributeBase* parent) : HardwareAttribute(parent) {}
		virtual ~HardwareAttributeImpl() {}
		void Update(const void* value, ptrdiff_t offset, size_t size) {
			if(offset == 0) {
				value_ = reinterpret_cast<intptr_t>(value);
			}
		}
		void Bind() {}
		void Unbind() {}
		intptr_t Value() override { return value_; }
	private:
		intptr_t value_;
	};

	class AttributeDesc
	{
	public:
		enum class Type {
			UNKOWN,
			POSITION,
			COLOR, 
			TEXTURE,
			NORMAL,
		};
		enum class VariableType {
			BOOL,
			HALF_FLOAT,
			FLOAT,
			DOUBLE,
			FIXED,
			SHORT,
			UNSIGNED_SHORT,
			BYTE,
			UNSIGNED_BYTE,
			INT,
			UNSIGNED_INT,
			INT_2_10_10_10_REV,
			UNSIGNED_INT_2_10_10_10_REV,
			UNSIGNED_INT_10F_11F_11F_REV,
		};
		explicit AttributeDesc(Type type, 
			unsigned num_elements,
			VariableType var_type,
			bool normalise=false,
			ptrdiff_t stride=0,
			ptrdiff_t offset=0,
			size_t divisor=1);
		explicit AttributeDesc(const std::string& type_name, 
			unsigned num_elements,
			VariableType var_type,
			bool normalise=false,
			ptrdiff_t stride=0,
			ptrdiff_t offset=0,
			size_t divisor=1);
		Type AttrType() const { return type_; }
		const std::string& AttrName() const { return type_name_; }
		VariableType VarType() const { return var_type_; }
		unsigned NumElements() const { return num_elements_; }
		bool Normalise() const { return normalise_; }
		ptrdiff_t Stride() const { return stride_; }
		ptrdiff_t Offset() const { return offset_; }
		size_t Divisor() const { return divisor_; }
		void SetDisplayData(const DisplayDeviceDataPtr& ddp) { display_data_ = ddp; }
		const DisplayDeviceDataPtr& GetDisplayData() const { return display_data_; }
	private:
		Type type_;
		std::string type_name_;
		VariableType var_type_;
		unsigned num_elements_;
		bool normalise_;
		ptrdiff_t stride_;
		ptrdiff_t offset_;
		size_t divisor_;
		DisplayDeviceDataPtr display_data_;
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

	class AttributeBase
	{
	public:
		AttributeBase(AccessFreqHint freq, AccessTypeHint type)
			: access_freq_(freq),
			access_type_(type),
			offs_(0) {
		}
		virtual ~AttributeBase() {}
		void AddAttributeDescription(const AttributeDesc& attrdesc) {
			desc_.emplace_back(attrdesc);
		}
		std::vector<AttributeDesc>& GetAttrDesc() { return desc_; }
		void SetOffset(ptrdiff_t offs) {
			offs_ = offs;
		}
		ptrdiff_t GetOffset() const { return offs_; } 
		AccessFreqHint AccessFrequency() const { return access_freq_; }
		AccessTypeHint AccessType() const { return access_type_; }
		HardwareAttributePtr GetDeviceBufferData() { return hardware_; }
		void SetDeviceBufferData(const HardwareAttributePtr& hardware) { 
			hardware_ = hardware; 
			HandleAttachHardwareBuffer();
		}
	private:
		virtual void HandleAttachHardwareBuffer() = 0;
		AccessFreqHint access_freq_;
		AccessTypeHint access_type_;
		ptrdiff_t offs_;
		std::vector<AttributeDesc> desc_;
		HardwareAttributePtr hardware_;
		bool hardware_buffer_;
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
		virtual ~Attribute() {}
		
		void Update(const Container<T>& values) {
			elements_ = values;
			if(GetDeviceBufferData()) {
				GetDeviceBufferData()->Update(&elements_[0], 0, elements_.size() * sizeof(T));
			}
		}
		void Update(const Container<T>& src, iterator& dst) {
			std::copy(src.begin(), src.end(), dst);
			if(GetDeviceBufferData()) {
				GetDeviceBufferData()->Update(&elements_[0], 
					std::distance(elements_.begin(), dst), 
					std::distance(src.begin(), src.end()) * sizeof(T));
			}
		}
		void Update(Container<T>* values) {
			elements_.swap(*values);
			if(GetDeviceBufferData()) {
				GetDeviceBufferData()->Update(&elements_[0], 0, elements_.size() * sizeof(T));
			}
		}
		size_t size() const { 
			return elements_.size();
		}
		void Bind() {
			ASSERT_LOG(GetDeviceBufferData() != NULL, "Bind call on null hardware attribute buffer.");
			GetDeviceBufferData()->Bind();
		}		
		void Unbind() {
			ASSERT_LOG(GetDeviceBufferData() != NULL, "Bind call on null hardware attribute buffer.");
			GetDeviceBufferData()->Unbind();
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
		void SetOffset(const_iterator& it) {
			SetOffset(std::distance(elements_.begin(), it));
		}
	private:
		DISALLOW_COPY_ASSIGN_AND_DEFAULT(Attribute);
		void HandleAttachHardwareBuffer() override {
			// This just makes sure that if we add any elements
			// before an attach then they are all updated correctly.
			if(elements_.size() > 0) {
				GetDeviceBufferData()->Update(&elements_[0], 0, elements_.size() * sizeof(T));
			}
		}
		Container<T> elements_;
	};

	class AttributeSet
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
		enum class IndexType {
			INDEX_NONE,
			INDEX_UCHAR,
			INDEX_USHORT,
			INDEX_ULONG,
		};
		explicit AttributeSet(bool indexed, bool instanced);
		virtual ~AttributeSet();

		void SetDrawMode(DrawMode dm);
		DrawMode GetDrawMode() { return draw_mode_; }

		bool IsIndexed() const { return indexed_draw_; }
		bool IsInstanced() const { return instanced_draw_; }
		IndexType GetIndexType() const { return index_type_; }
		virtual const void* GetIndexArray() const { 
			switch(index_type_) {
			case IndexType::INDEX_NONE:		break;
			case IndexType::INDEX_UCHAR:	return &index8_[0];
			case IndexType::INDEX_USHORT:	return &index16_[0];
			case IndexType::INDEX_ULONG:	return &index32_[0];
			}
			ASSERT_LOG(false, "Index type not set to valid value.");
		};
		size_t GetTotalArraySize() const {
			switch(index_type_) {
			case IndexType::INDEX_NONE:		break;
			case IndexType::INDEX_UCHAR:	return index8_.size() * sizeof(uint8_t);
			case IndexType::INDEX_USHORT:	return index16_.size() * sizeof(uint16_t);
			case IndexType::INDEX_ULONG:	return index32_.size() * sizeof(uint32_t);
			}
			ASSERT_LOG(false, "Index type not set to valid value.");
		}
		void SetCount(size_t count) { count_= count; }
		size_t GetCount() const { return count_; }
		void SetInstanceCount(size_t instance_count) { instance_count_ = instance_count; }
		size_t GetInstanceCount() const { return instance_count_; }

		void UpdateIndicies(const std::vector<uint8_t>& value);
		void UpdateIndicies(const std::vector<uint16_t>& value);
		void UpdateIndicies(const std::vector<uint32_t>& value);
		void UpdateIndicies(std::vector<uint8_t>* value);
		void UpdateIndicies(std::vector<uint16_t>* value);
		void UpdateIndicies(std::vector<uint32_t>* value);

		void AddAttribute(const AttributeBasePtr& attrib);

		virtual void BindIndex() {};
		virtual void UnbindIndex() {};

		void SetOffset(ptrdiff_t offset) { offset_ = offset; }
		ptrdiff_t GetOffset() const { return offset_; }

		virtual bool IsHardwareBacked() const { return false; }

		std::vector<AttributeBasePtr>& GetAttributes() { return attributes_; }

		const BlendEquation& getBlendEquation() const { return blend_eqn_; }
		void setBlendEquation(const BlendEquation& eqn) { blend_eqn_ = eqn; }

		const BlendMode& getBlendMode() const { return blend_mode_; }
		void setBlendMode(const BlendMode& bm) { blend_mode_ = bm; }
		void setBlendMode(BlendModeConstants src, BlendModeConstants dst) { blend_mode_.Set(src, dst); }

		ColorPtr getColor() const { return color_; }
		void setColor(const Color& color) { color_.reset(new Color(color)); }
	protected:
		const void* GetIndexData() const { 
			switch(index_type_) {
			case IndexType::INDEX_NONE:		break;
			case IndexType::INDEX_UCHAR:	return &index8_[0];
			case IndexType::INDEX_USHORT:	return &index16_[0];
			case IndexType::INDEX_ULONG:	return &index32_[0];
			}
			ASSERT_LOG(false, "Index type not set to valid value.");
		};
	private:
		DISALLOW_COPY_ASSIGN_AND_DEFAULT(AttributeSet);
		virtual void HandleIndexUpdate() {}
		DrawMode draw_mode_;
		bool indexed_draw_;
		bool instanced_draw_;
		IndexType index_type_;
		size_t instance_count_;
		std::vector<uint8_t> index8_;
		std::vector<uint16_t> index16_;
		std::vector<uint32_t> index32_;
		std::vector<AttributeBasePtr> attributes_;
		size_t count_;
		ptrdiff_t offset_;
		BlendEquation blend_eqn_;
		BlendMode blend_mode_;
		KRE::ColorPtr color_;
	};
	typedef std::shared_ptr<AttributeSet> AttributeSetPtr;
}
