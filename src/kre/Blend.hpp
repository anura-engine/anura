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

#include "DisplayDeviceFwd.hpp"
#include "Util.hpp"

#include "variant.hpp"

namespace KRE
{
	enum class BlendModeConstants {
		BM_ZERO,
		BM_ONE,
		BM_SRC_COLOR,
		BM_ONE_MINUS_SRC_COLOR,
		BM_DST_COLOR,
		BM_ONE_MINUS_DST_COLOR,
		BM_SRC_ALPHA,
		BM_ONE_MINUS_SRC_ALPHA,
		BM_DST_ALPHA,
		BM_ONE_MINUS_DST_ALPHA,
		BM_CONSTANT_COLOR,
		BM_ONE_MINUS_CONSTANT_COLOR,
		BM_CONSTANT_ALPHA,
		BM_ONE_MINUS_CONSTANT_ALPHA,
	};

	enum class BlendEquationConstants {
		BE_ADD,
		BE_SUBTRACT,
		BE_REVERSE_SUBTRACT,
		BE_MIN,
		BE_MAX,
	};

	class BlendEquation
	{
	public:
		BlendEquation();
		explicit BlendEquation(BlendEquationConstants rgba_eq);
		explicit BlendEquation(BlendEquationConstants rgb_eq, BlendEquationConstants alpha_eq);
		explicit BlendEquation(const variant& node);
		void setRgbEquation(BlendEquationConstants rgb_eq);
		void setAlphaEquation(BlendEquationConstants alpha_eq);
		void setEquation(BlendEquationConstants rgba_eq);
		BlendEquationConstants getRgbEquation() const;
		BlendEquationConstants getAlphaEquation() const;
		variant write() const;
		struct Manager
		{
			Manager(const BlendEquation& be);
			~Manager();
			BlendEquationImplBasePtr impl_;
			const BlendEquation& eqn_;
		};
	private:
		// Equations to use for seperate rgb and alpha components.
		BlendEquationConstants rgb_;
		BlendEquationConstants alpha_;
	};

	inline bool operator==(const BlendEquation& lhs, const BlendEquation& rhs) {
		return lhs.getRgbEquation() == rhs.getRgbEquation() && lhs.getAlphaEquation()== rhs.getAlphaEquation();
	}
	inline bool operator!=(const BlendEquation& lhs, const BlendEquation& rhs) {
		return !operator==(lhs, rhs);
	}

	class BlendEquationImplBase
	{
	public:
		BlendEquationImplBase();
		virtual ~BlendEquationImplBase();
		virtual void apply(const BlendEquation& eqn) const = 0;
		virtual void clear(const BlendEquation& eqn) const = 0;
	private:
		DISALLOW_COPY_AND_ASSIGN(BlendEquationImplBase);
	};

	class BlendMode
	{
	public:
		BlendMode() : src_(BlendModeConstants::BM_SRC_ALPHA), dst_(BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA) {}
		explicit BlendMode(BlendModeConstants src, BlendModeConstants dst) : src_(src), dst_(dst) {}
		explicit BlendMode(const variant& node);
		BlendModeConstants source() const { return src_; }
		BlendModeConstants destination() const { return dst_; }
		BlendModeConstants src() const { return src_; }
		BlendModeConstants dst() const { return dst_; }
		variant write() const;
		void set(BlendModeConstants src, BlendModeConstants dst) {
			src_ = src;
			dst_ = dst;
		}
		void setSource(BlendModeConstants src) {
			src_ = src;
		}
		void setDestination(BlendModeConstants dst) {
			dst_ = dst;
		}
		void setSrc(BlendModeConstants src) {
			src_ = src;
		}
		void setDst(BlendModeConstants dst) {
			dst_ = dst;
		}
		void set(const variant& node);
	private:
		BlendModeConstants src_;
		BlendModeConstants dst_;
	};

	inline bool operator==(const BlendMode& lhs, const BlendMode& rhs) {
		return lhs.src() == rhs.src() && lhs.dst()== rhs.dst();
	}
	inline bool operator!=(const BlendMode& lhs, const BlendMode& rhs) {
		return !operator==(lhs, rhs);
	}
}
