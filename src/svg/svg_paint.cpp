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

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <map>

#include "asserts.hpp"
#include "svg_paint.hpp"

namespace KRE
{
	namespace SVG
	{
		namespace 
		{
			uint8_t convert_hex_digit(char d) {
				uint8_t value = 0;
				if(d >= 'A' && d <= 'F') {
					value = d - 'A' + 10;
				} else if(d >= 'a' && d <= 'f') {
					value = d - 'a' + 10;
				} else if(d >= '0' && d <= '9') {
					value = d - '0';
				} else {
					ASSERT_LOG(false, "Unrecognised hex digit: " << d);
				}
				return value;
			}
		}

		paint::paint()
			: color_attrib_(ColorAttrib::NONE),
			backup_color_attrib_(ColorAttrib::NONE),
			opacity_(1.0)
		{
		}

		paint::paint(int r, int g, int b, int a)
			: color_attrib_(ColorAttrib::VALUE),
			backup_color_attrib_(ColorAttrib::NONE),
			opacity_(1.0),
			color_value_(r, g, b, a)
		{
		}

		paint::paint(const std::string& s)
			: color_attrib_(ColorAttrib::VALUE),
			backup_color_attrib_(ColorAttrib::NONE),
			opacity_(1.0)
		{
			if(s == "none") {
				color_attrib_ = ColorAttrib::NONE;
			} else if(s == "currentColor") {
				color_attrib_ = ColorAttrib::CURRENT_COLOR;
			} else if(s.length() > 1 && s[0] == '#') {
				color_value_ = Color(s);
			} else if(s.length() > 3 && s.substr(0,3) == "rgb") {
				boost::char_separator<char> seperators(" \n\t\r,()");
				boost::tokenizer<boost::char_separator<char>> tok(s.substr(3), seperators);
				int count = 0;
				int cv[3];
				for(auto it = tok.begin(); it != tok.end(); ++it) {
					char* end = nullptr;
					long value = strtol(it->c_str(), &end, 10);
					uint8_t col_val = 0;
					if(value == 0 && end == it->c_str()) {
						ASSERT_LOG(false, "Unable to parse string as an integer: " << *it);
					}
					if(end != nullptr && *end == '%') {
						ASSERT_LOG(value >= 0 && value <= 100, "Percentage values range from 0-100: " << value);
						col_val = uint8_t(value / 100.0 * 255);
					} else {
						ASSERT_LOG(value >= 0 && value <= 255, "Percentage values range from 0-255: " << value);
						col_val = uint8_t(value);
					}
					ASSERT_LOG(count < 3, "Too many numbers in color value");
					cv[count] = col_val;
					count++;
				}
				ASSERT_LOG(count == 3, "Too few numbers in rgb color value");
				color_value_ = Color(cv[0],cv[1],cv[2]);
			} else if(s.length() > 4 && s.substr(0, 4) == "url(") {
				auto st_it = std::find(s.begin(), s.end(), '(');
				auto ed_it = std::find(s.begin(), s.end(), '0');
				color_ref_ = uri::uri::parse(std::string(st_it, ed_it));
				color_attrib_ = ColorAttrib::FUNC_IRI;

				std::string backup(ed_it, s.end());
				if(!backup.empty()) {
					// XXX parse stuff
				}
			} else if(s.length() > 9 && s.substr(0,9) == "icc-color") {
				boost::char_separator<char> seperators(" \n\t\r,()");
				boost::tokenizer<boost::char_separator<char>> tok(s.substr(9), seperators);
				auto it = tok.begin();
				icc_color_name_ = *it;
				for(; it != tok.end(); ++it) {
					try {
						double value = boost::lexical_cast<double>(*it);
						icc_color_values_.emplace_back(value);
					} catch(boost::bad_lexical_cast&) {
						ASSERT_LOG(false, "Unable to convert icc-color value from string to numeric: " << *it << " : " << s);
					}
				}
				color_attrib_ = ColorAttrib::ICC_COLOR;
			} else {
				color_value_ = Color(s);
			}
		}

		paint::~paint()
		{
		}

		bool paint::apply(const element* parent, render_context& ctx) const
		{
			switch(color_attrib_) {
			case ColorAttrib::NONE:
				// Nothing to do if there is no color.
				return false;
			case ColorAttrib::CURRENT_COLOR: {
				auto cc = ctx.get_current_color();
				ASSERT_LOG(cc != nullptr, "Current color specified as color source, but there is no current color value.");
				cairo_set_source_rgb(ctx.cairo(), cc->r(), cc->g(), cc->b());
				return true;
			}
			case ColorAttrib::FUNC_IRI:
				ASSERT_LOG(false, "XXX: todo: lookup FUNC_IRI to get color value");
				return true;
			case ColorAttrib::ICC_COLOR:
				ASSERT_LOG(false, "XXX: todo: ICC_COLOR to get color value");
				return true;
			case ColorAttrib::INHERIT:
				// XXX: Nothing to do?
				return true;
			case ColorAttrib::VALUE:
				double opacity = ctx.opacity_top();
				cairo_set_source_rgba(ctx.cairo(), color_value_.r(), color_value_.g(), color_value_.b(), opacity);
				return true;
			}
			return false;
		}

		paint_ptr paint::from_string(const std::string& s)
		{
			return paint_ptr(new paint(s));
		}
	}
}
