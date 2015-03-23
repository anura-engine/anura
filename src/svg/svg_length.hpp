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

#include "asserts.hpp"

namespace KRE
{
	namespace SVG
	{
		class svg_length
		{
		public:
			enum LengthUnit {
				SVG_LENGTHTYPE_UNKNOWN,
				SVG_LENGTHTYPE_NUMBER,
				SVG_LENGTHTYPE_PERCENTAGE,
				SVG_LENGTHTYPE_EMS,			// the 'font-size' of the relevant font 
				SVG_LENGTHTYPE_EXS,			// the 'x-height' of the relevant font 
				SVG_LENGTHTYPE_PX,			// pixels, relative to the viewing device
				SVG_LENGTHTYPE_CM,			// centimetres
				SVG_LENGTHTYPE_MM,			// millimetres
				SVG_LENGTHTYPE_IN,			// inches
				SVG_LENGTHTYPE_PT,			// points -- equal to 1/72th of an inch
				SVG_LENGTHTYPE_PC,			// picas -- 1 pica is equal to 12 points
			};
			svg_length() : value_(100.0f), units_(SVG_LENGTHTYPE_PERCENTAGE) {			
			}
			explicit svg_length(float value, LengthUnit unit) : value_(value), units_(unit) {
			}
			explicit svg_length(const std::string& length) {
				from_string(length);
			};
			void from_string(const std::string& length) {
				if(length.empty()) {
					units_ = SVG_LENGTHTYPE_UNKNOWN;
					value_ = 0.0f;
					return;
				}
				std::stringstream ss(length);
				std::string unit;
				ss >> value_;
				ss >> unit;
				if(unit.empty()) {
					units_ = SVG_LENGTHTYPE_NUMBER;
				} else if(unit == "em") {
					units_ = SVG_LENGTHTYPE_EMS;
				} else if(unit == "ex") {
					units_ = SVG_LENGTHTYPE_EXS;
				} else if(unit == "px") {
					units_ = SVG_LENGTHTYPE_PX;
				} else if(unit == "cm") {
					units_ = SVG_LENGTHTYPE_CM;
				} else if(unit == "mm") {
					units_ = SVG_LENGTHTYPE_MM;
				} else if(unit == "in") {
					units_ = SVG_LENGTHTYPE_IN;
				} else if(unit == "pt") {
					units_ = SVG_LENGTHTYPE_PT;
				} else if(unit == "pc") {
					units_ = SVG_LENGTHTYPE_PC;
				} else if(unit == "%") {
					units_ = SVG_LENGTHTYPE_PERCENTAGE;
				} else {
					ASSERT_LOG(false, "Unrecognised length unit: " << unit);
				}
			}
			float value_in_specified_units(LengthUnit units) const {
				switch(units_) {
					case SVG_LENGTHTYPE_UNKNOWN:
						ASSERT_LOG(false, "Unrecognished type SVG_LENGTHTYPE_UNKNOWN");
					case SVG_LENGTHTYPE_NUMBER:		return convert_number(units);
					case SVG_LENGTHTYPE_PERCENTAGE:	return convert_percentage(units);
					case SVG_LENGTHTYPE_EMS:		return convert_ems(units);
					case SVG_LENGTHTYPE_EXS:		return convert_exs(units);
					case SVG_LENGTHTYPE_PX:			return convert_px(units);
					case SVG_LENGTHTYPE_CM:			return convert_cm(units);
					case SVG_LENGTHTYPE_MM:			return convert_mm(units);
					case SVG_LENGTHTYPE_IN:			return convert_in(units);
					case SVG_LENGTHTYPE_PT:			return convert_pt(units);
					case SVG_LENGTHTYPE_PC:			return convert_pc(units);
				}
				return 0.0f;
			}
		private:
			float convert_number(LengthUnit units) const {
				switch(units) {
					case SVG_LENGTHTYPE_UNKNOWN:
						ASSERT_LOG(false, "Unhandled units value: SVG_LENGTHTYPE_UNKNOWN");
					case SVG_LENGTHTYPE_NUMBER:		return value_;
					case SVG_LENGTHTYPE_PERCENTAGE:	return 0;
					case SVG_LENGTHTYPE_EMS:		return 0;
					case SVG_LENGTHTYPE_EXS:		return 0;
					case SVG_LENGTHTYPE_PX:			return 0;
					case SVG_LENGTHTYPE_CM:			return 0;
					case SVG_LENGTHTYPE_MM:			return 0;
					case SVG_LENGTHTYPE_IN:			return 0;
					case SVG_LENGTHTYPE_PT:			return 0;
					case SVG_LENGTHTYPE_PC:			return 0;
					default:
						ASSERT_LOG(false, "Unrecognished units value: " << units);
				}
				return 0;
			}
			float convert_percentage(LengthUnit units) const {
				return 0;
			}
			float convert_ems(LengthUnit units) const {
				return 0;
			}
			float convert_exs(LengthUnit units) const {
				return 0;
			}
			float convert_px(LengthUnit units) const {
				return 0;
			}
			float convert_cm(LengthUnit units) const {
				return 0;
			}
			float convert_mm(LengthUnit units) const {
				return 0;
			}
			float convert_in(LengthUnit units) const {
				return 0;
			}
			float convert_pt(LengthUnit units) const {
				return 0;
			}
			float convert_pc(LengthUnit units) const {
				return 0;
			}

			float value_;
			LengthUnit units_;
		};
	}
}
