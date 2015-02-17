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

#include <cstdint>
#include <memory>
#include <string>

#include "Color.hpp"
#include "svg_render.hpp"
#include "uri.hpp"

namespace KRE
{
	namespace SVG
	{
		class element;

		class paint;
		typedef std::shared_ptr<paint> paint_ptr;

		enum class ColorAttrib {
			INHERIT,
			NONE,
			CURRENT_COLOR,
			VALUE,
			FUNC_IRI,
			ICC_COLOR,
		};

		class paint
		{
		public:
			paint();
			explicit paint(int r, int g, int b, int a=255);

			virtual ~paint();

			void set_opacity(double o) { opacity_ = o; }

			bool apply(const element* parent, render_context& ctx) const;

			static paint_ptr from_string(const std::string& s);
		private:
			explicit paint(const std::string& s);

			ColorAttrib color_attrib_;
			Color color_value_;
			uri::uri color_ref_;

			std::string icc_color_name_;
			std::vector<double> icc_color_values_;

			ColorAttrib backup_color_attrib_;
			Color backup_color_value_;

			double opacity_;
		};
	}
}
