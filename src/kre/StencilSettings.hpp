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

namespace KRE
{
	enum class StencilFace
	{
		FRONT,
		BACK,
		FRONT_AND_BACK,
	};

	enum class StencilFunc
	{
		NEVER,
		LESS,
		LESS_THAN_OR_EQUAL,
		GREATER,
		GREATER_THAN_OR_EQUAL,
		EQUAL,
		NOT_EQUAL,
		ALWAYS,
	};

	enum class StencilOperation
	{
		KEEP,
		ZERO,
		REPLACE,
		INCREMENT,
		INCREMENT_WRAP,
		DECREMENT,
		DECREMENT_WRAP,
		INVERT,
	};

	class StencilSettings
	{
	public:
		StencilSettings(bool en, 
			StencilFace face, 
			StencilFunc func, 
			unsigned ref_mask, 
			int ref, 
			unsigned mask,
			StencilOperation sfail,
			StencilOperation dpfail,
			StencilOperation dppass) 
			: enabled_(en),
			face_(face),
			func_(func),
			mask_(mask),
			ref_(ref),
			ref_mask_(ref_mask),
			sfail_(sfail),
			dpfail_(dpfail),
			dppass_(dppass)
		{
		}
		bool enabled() const { return enabled_; }
		StencilFace face() const { return face_; }
		StencilFunc func() const { return func_; }
		void setMask(unsigned mask) { mask_ = mask; }
		unsigned mask() const { return mask_; }
		int ref() const { return ref_; }
		unsigned ref_mask() const { return ref_mask_; } 
		StencilOperation sfail() const { return sfail_; }
		StencilOperation dpfail() const { return dpfail_; }
		StencilOperation dppass() const { return dppass_; }
	private:
		StencilSettings();
		bool enabled_;
		StencilFace face_;
		StencilFunc func_;
		unsigned mask_;
		int ref_;
		unsigned ref_mask_;
		StencilOperation sfail_;
		StencilOperation dpfail_;
		StencilOperation dppass_;
	};

	const StencilSettings& get_stencil_mask_settings();
	const StencilSettings& get_stencil_keep_settings();
}
