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

#include <cairo.h>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "asserts.hpp"

#ifdef _MSC_VER
#   define _GLIBCXX_USE_NOEXCEPT
#endif

namespace KRE
{
	namespace SVG
	{
		enum class PathInstruction {
			MOVETO,
			LINETO,
			LINETO_H,
			LINETO_V,
			CLOSEPATH,
			CUBIC_BEZIER,
			QUADRATIC_BEZIER,
			ARC,
		};

		class path_cmd_context
		{
		public:
			path_cmd_context(cairo_t* cairo) 
				: cairo_(cairo), 
				cp1x_(0), 
				cp1y_(0),
				control_point_set_(false) {
			}
			~path_cmd_context() {}
			cairo_t* cairo_context() { return cairo_; }
			void set_control_points(double x, double y) {
				cp1x_ = x;
				cp1y_ = y;
				control_point_set_ = true;
			}
			void clear_control_points() {
				control_point_set_ = false;
			}
			void get_control_points(double* x, double* y) {
				ASSERT_LOG(x != nullptr, "x is null. no place for result");
				ASSERT_LOG(y != nullptr, "y is null. no place for result");
				if(control_point_set_) {
					*x = cp1x_;
					*y = cp1y_;
				} else {
					cairo_get_current_point(cairo_, x, y);
				}
			}
		private:
			cairo_t* cairo_;
			bool control_point_set_;
			double cp1x_;
			double cp1y_;
		};

		class path_command
		{
		public:
			virtual ~path_command();

			void cairo_render(path_cmd_context& ctx);

			bool is_absolute() const { return absolute_; }
			bool is_relative() const { return !absolute_; }
		protected:
			path_command(PathInstruction ins, bool absolute);
		private:
			virtual void handle_cairo_render(path_cmd_context& ctx) = 0;
			PathInstruction ins_;
			bool absolute_;
		};
		typedef std::shared_ptr<path_command> path_commandPtr;

		class parsing_exception
		{
		public:
			parsing_exception(const std::string& ss) : s_(ss) {}
            const char* what() const {
				return s_.c_str();
			}
		private:
			std::string s_;
		};

		std::vector<path_commandPtr> parse_path(const std::string& s);
	}
}
