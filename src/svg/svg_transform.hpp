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
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cairo.h>

#include "svg_fwd.hpp"
#include "svg_render.hpp"

namespace KRE
{
	namespace SVG 
	{
		enum class TransformType {
			ERROR,
			MATRIX,
			TRANSLATE,
			SCALE,
			ROTATE,
			SKEW_X,
			SKEW_Y,
		};

		class transform
		{
		public:
			virtual ~transform();
			virtual std::string as_string() const = 0;
			static std::vector<transform_ptr> factory(const std::string& s);
			static transform_ptr factory(TransformType, const std::vector<double>& params);
			void apply(render_context& ctx);
			void apply_matrix(cairo_matrix_t* mtx) const;
		protected:
			transform(TransformType tt);
		private:
			virtual void handle_apply(render_context& ctx) = 0;
			virtual void handle_apply_matrix(cairo_matrix_t* mtx) const = 0;

			TransformType type_;
		};
	}
}
