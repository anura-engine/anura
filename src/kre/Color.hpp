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

#include <memory>
#include <string>
#include <glm/gtc/type_precision.hpp>

#include "variant.hpp"

namespace KRE
{
	class Color;
	typedef std::shared_ptr<Color> ColorPtr;

	enum class ColorByteOrder
	{
		RGBA,
		ARGB,
		BGRA,
		ABGR,
	};

	class Color
	{
	public:
		Color();
		~Color();
		explicit Color(const double r, const double g, const double b, const double a=1.0);
		explicit Color(const int r, const int g, const int b, const int a=255);
		explicit Color(const std::string& s);
		explicit Color(const variant& node);
		explicit Color(unsigned long n, ColorByteOrder order=ColorByteOrder::RGBA);

		double r() const { return color_[0]; }
		double g() const { return color_[1]; }
		double b() const { return color_[2]; }
		double a() const { return color_[3]; }

		double red() const { return color_[0]; }
		double green() const { return color_[1]; }
		double blue() const { return color_[2]; }
		double alpha() const { return color_[3]; }

		float rf() const { return static_cast<float>(color_[0]); }
		float gf() const { return static_cast<float>(color_[1]); }
		float bf() const { return static_cast<float>(color_[2]); }
		float af() const { return static_cast<float>(color_[3]); }

		int r_int() const { return static_cast<int>(255*color_[0]); }
		int g_int() const { return static_cast<int>(255*color_[1]); }
		int b_int() const { return static_cast<int>(255*color_[2]); }
		int a_int() const { return static_cast<int>(255*color_[3]); }

		void setRed(int a);
		void setRed(double a);

		void setGreen(int a);
		void setGreen(double a);

		void setBlue(int a);
		void setBlue(double a);

		void setAlpha(int a);
		void setAlpha(double a);

		unsigned long asARGB() const {
			return (a_int() << 24) | (r_int() << 16) | (g_int() << 8) | b_int();
		}

		unsigned long asRGBA() const {
			return (r_int() << 24) | (b_int() << 16) | (b_int() << 8) | a_int();
		}

		glm::u8vec4 as_u8vec4() const {
			return glm::u8vec4(r_int(), g_int(), b_int(), a_int());
		}

		const float* asFloatVector() const {
			return color_;
		}

		variant write() const;

		static ColorPtr factory(const std::string& name);

		static Color colorAliceblue() { return Color(240, 248, 255); }
		static Color colorAntiquewhite() { return Color(250, 235, 215); }
		static Color colorAqua() { return Color(0, 255, 255); }
		static Color colorAquamarine() { return Color(127, 255, 212); }
		static Color colorAzure() { return Color(240, 255, 255); }
		static Color colorBeige() { return Color(245, 245, 220); }
		static Color colorBisque() { return Color(255, 228, 196); }
		static Color colorBlack() { return Color(0, 0, 0); }
		static Color colorBlanchedalmond() { return Color(255, 235, 205); }
		static Color colorBlue() { return Color(0, 0, 255); }
		static Color colorBlueviolet() { return Color(138, 43, 226); }
		static Color colorBrown() { return Color(165, 42, 42); }
		static Color colorBurlywood() { return Color(222, 184, 135); }
		static Color colorCadetblue() { return Color(95, 158, 160); }
		static Color colorChartreuse() { return Color(127, 255, 0); }
		static Color colorChocolate() { return Color(210, 105, 30); }
		static Color colorCoral() { return Color(255, 127, 80); }
		static Color colorCornflowerblue() { return Color(100, 149, 237); }
		static Color colorCornsilk() { return Color(255, 248, 220); }
		static Color colorCrimson() { return Color(220, 20, 60); }
		static Color colorCyan() { return Color(0, 255, 255); }
		static Color colorDarkblue() { return Color(0, 0, 139); }
		static Color colorDarkcyan() { return Color(0, 139, 139); }
		static Color colorDarkgoldenrod() { return Color(184, 134, 11); }
		static Color colorDarkgray() { return Color(169, 169, 169); }
		static Color colorDarkgreen() { return Color(0, 100, 0); }
		static Color colorDarkgrey() { return Color(169, 169, 169); }
		static Color colorDarkkhaki() { return Color(189, 183, 107); }
		static Color colorDarkmagenta() { return Color(139, 0, 139); }
		static Color colorDarkolivegreen() { return Color(85, 107, 47); }
		static Color colorDarkorange() { return Color(255, 140, 0); }
		static Color colorDarkorchid() { return Color(153, 50, 204); }
		static Color colorDarkred() { return Color(139, 0, 0); }
		static Color colorDarksalmon() { return Color(233, 150, 122); }
		static Color colorDarkseagreen() { return Color(143, 188, 143); }
		static Color colorDarkslateblue() { return Color(72, 61, 139); }
		static Color colorDarkslategray() { return Color(47, 79, 79); }
		static Color colorDarkslategrey() { return Color(47, 79, 79); }
		static Color colorDarkturquoise() { return Color(0, 206, 209); }
		static Color colorDarkviolet() { return Color(148, 0, 211); }
		static Color colorDeeppink() { return Color(255, 20, 147); }
		static Color colorDeepskyblue() { return Color(0, 191, 255); }
		static Color colorDimgray() { return Color(105, 105, 105); }
		static Color colorDimgrey() { return Color(105, 105, 105); }
		static Color colorDodgerblue() { return Color(30, 144, 255); }
		static Color colorFirebrick() { return Color(178, 34, 34); }
		static Color colorFloralwhite() { return Color(255, 250, 240); }
		static Color colorForestgreen() { return Color(34, 139, 34); }
		static Color colorFuchsia() { return Color(255, 0, 255); }
		static Color colorGainsboro() { return Color(220, 220, 220); }
		static Color colorGhostwhite() { return Color(248, 248, 255); }
		static Color colorGold() { return Color(255, 215, 0); }
		static Color colorGoldenrod() { return Color(218, 165, 32); }
		static Color colorGray() { return Color(128, 128, 128); }
		static Color colorGrey() { return Color(128, 128, 128); }
		static Color colorGreen() { return Color(0, 128, 0); }
		static Color colorGreenyellow() { return Color(173, 255, 47); }
		static Color colorHoneydew() { return Color(240, 255, 240); }
		static Color colorHotpink() { return Color(255, 105, 180); }
		static Color colorIndianred() { return Color(205, 92, 92); }
		static Color colorIndigo() { return Color(75, 0, 130); }
		static Color colorIvory() { return Color(255, 255, 240); }
		static Color colorKhaki() { return Color(240, 230, 140); }
		static Color colorLavender() { return Color(230, 230, 250); }
		static Color colorLavenderblush() { return Color(255, 240, 245); }
		static Color colorLawngreen() { return Color(124, 252, 0); }
		static Color colorLemonchiffon() { return Color(255, 250, 205); }
		static Color colorLightblue() { return Color(173, 216, 230); }
		static Color colorLightcoral() { return Color(240, 128, 128); }
		static Color colorLightcyan() { return Color(224, 255, 255); }
		static Color colorLightgoldenrodyellow() { return Color(250, 250, 210); }
		static Color colorLightgray() { return Color(211, 211, 211); }
		static Color colorLightgreen() { return Color(144, 238, 144); }
		static Color colorLightgrey() { return Color(211, 211, 211); }
		static Color colorLightpink() { return Color(255, 182, 193); }
		static Color colorLightsalmon() { return Color(255, 160, 122); }
		static Color colorLightseagreen() { return Color(32, 178, 170); }
		static Color colorLightskyblue() { return Color(135, 206, 250); }
		static Color colorLightslategray() { return Color(119, 136, 153); }
		static Color colorLightslategrey() { return Color(119, 136, 153); }
		static Color colorLightsteelblue() { return Color(176, 196, 222); }
		static Color colorLightyellow() { return Color(255, 255, 224); }
		static Color colorLime() { return Color(0, 255, 0); }
		static Color colorLimegreen() { return Color(50, 205, 50); }
		static Color colorLinen() { return Color(250, 240, 230); }
		static Color colorMagenta() { return Color(255, 0, 255); }
		static Color colorMaroon() { return Color(128, 0, 0); }
		static Color colorMediumaquamarine() { return Color(102, 205, 170); }
		static Color colorMediumblue() { return Color(0, 0, 205); }
		static Color colorMediumorchid() { return Color(186, 85, 211); }
		static Color colorMediumpurple() { return Color(147, 112, 219); }
		static Color colorMediumseagreen() { return Color(60, 179, 113); }
		static Color colorMediumslateblue() { return Color(123, 104, 238); }
		static Color colorMediumspringgreen() { return Color(0, 250, 154); }
		static Color colorMediumturquoise() { return Color(72, 209, 204); }
		static Color colorMediumvioletred() { return Color(199, 21, 133); }
		static Color colorMidnightblue() { return Color(25, 25, 112); }
		static Color colorMintcream() { return Color(245, 255, 250); }
		static Color colorMistyrose() { return Color(255, 228, 225); }
		static Color colorMoccasin() { return Color(255, 228, 181); }
		static Color colorNavajowhite() { return Color(255, 222, 173); }
		static Color colorNavy() { return Color(0, 0, 128); }
		static Color colorOldlace() { return Color(253, 245, 230); }
		static Color colorOlive() { return Color(128, 128, 0); }
		static Color colorOlivedrab() { return Color(107, 142, 35); }
		static Color colorOrange() { return Color(255, 165, 0); }
		static Color colorOrangered() { return Color(255, 69, 0); }
		static Color colorOrchid() { return Color(218, 112, 214); }
		static Color colorPalegoldenrod() { return Color(238, 232, 170); }
		static Color colorPalegreen() { return Color(152, 251, 152); }
		static Color colorPaleturquoise() { return Color(175, 238, 238); }
		static Color colorPalevioletred() { return Color(219, 112, 147); }
		static Color colorPapayawhip() { return Color(255, 239, 213); }
		static Color colorPeachpuff() { return Color(255, 218, 185); }
		static Color colorPeru() { return Color(205, 133, 63); }
		static Color colorPink() { return Color(255, 192, 203); }
		static Color colorPlum() { return Color(221, 160, 221); }
		static Color colorPowderblue() { return Color(176, 224, 230); }
		static Color colorPurple() { return Color(128, 0, 128); }
		static Color colorRed() { return Color(255, 0, 0); }
		static Color colorRosybrown() { return Color(188, 143, 143); }
		static Color colorRoyalblue() { return Color(65, 105, 225); }
		static Color colorSaddlebrown() { return Color(139, 69, 19); }
		static Color colorSalmon() { return Color(250, 128, 114); }
		static Color colorSandybrown() { return Color(244, 164, 96); }
		static Color colorSeagreen() { return Color(46, 139, 87); }
		static Color colorSeashell() { return Color(255, 245, 238); }
		static Color colorSienna() { return Color(160, 82, 45); }
		static Color colorSilver() { return Color(192, 192, 192); }
		static Color colorSkyblue() { return Color(135, 206, 235); }
		static Color colorSlateblue() { return Color(106, 90, 205); }
		static Color colorSlategray() { return Color(112, 128, 144); }
		static Color colorSlategrey() { return Color(112, 128, 144); }
		static Color colorSnow() { return Color(255, 250, 250); }
		static Color colorSpringgreen() { return Color(0, 255, 127); }
		static Color colorSteelblue() { return Color(70, 130, 180); }
		static Color colorTan() { return Color(210, 180, 140); }
		static Color colorTeal() { return Color(0, 128, 128); }
		static Color colorThistle() { return Color(216, 191, 216); }
		static Color colorTomato() { return Color(255, 99, 71); }
		static Color colorTurquoise() { return Color(64, 224, 208); }
		static Color colorViolet() { return Color(238, 130, 238); }
		static Color colorWheat() { return Color(245, 222, 179); }
		static Color colorWhite() { return Color(255, 255, 255); }
		static Color colorWhitesmoke() { return Color(245, 245, 245); }
		static Color colorYellow() { return Color(255, 255, 0); }
		static Color colorYellowgreen() { return Color(154, 205, 50); }		

		// XXX We should have a ColorCallable, in a seperate file, then move these two into the ColorCallable.
		static std::string getSetFieldType() { return "string"
			"|[int,int,int,int]"
			"|[int,int,int]"
			"|{red:int|decimal,green:int|decimal,blue:int|decimal,alpha:int|decimal|null}"
			"|{r:int|decimal,g:int|decimal,b:int|decimal,a:int|decimal|null}"; }
		static std::string getDefineFieldType() { return "[int,int,int,int]"; }
	private:
		float color_[4];
	};

	inline bool operator<(const Color& lhs, const Color& rhs)
	{
		return lhs.asARGB() < rhs.asARGB();
	}

	inline bool operator==(const Color& lhs, const Color& rhs)
	{
		return lhs.asARGB() == rhs.asARGB();
	}

	inline bool operator!=(const Color& lhs, const Color& rhs)
	{
		return !operator==(lhs, rhs);
	}

	typedef std::shared_ptr<Color> ColorPtr;
}
