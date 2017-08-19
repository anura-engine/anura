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

#include <iostream>
#include <memory>
#include <string>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "AlignedAllocator.hpp"
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

	enum class DecodingHint
	{
		INTEGER,
		DECIMAL,
	};

	class Color : public AlignedAllocator16
	{
	public:
		Color();
		~Color();
		explicit Color(const float r, const float g, const float b, const float a=1.0f);
		explicit Color(const int r, const int g, const int b, const int a=255);
		explicit Color(const std::string& s);
		explicit Color(const variant& node, DecodingHint hint=DecodingHint::INTEGER);
		explicit Color(const glm::vec4& value);
		explicit Color(const glm::u8vec4& value);
		explicit Color(unsigned long n, ColorByteOrder order=ColorByteOrder::RGBA);

		float r() const { return color_[0]; }
		float g() const { return color_[1]; }
		float b() const { return color_[2]; }
		float a() const { return color_[3]; }

		float red() const { return color_[0]; }
		float green() const { return color_[1]; }
		float blue() const { return color_[2]; }
		float alpha() const { return color_[3]; }

		int ri() const { return icolor_.r; }
		int gi() const { return icolor_.g; }
		int bi() const { return icolor_.b; }
		int ai() const { return icolor_.a; }

		int r_int() const { return icolor_.r; }
		int g_int() const { return icolor_.g; }
		int b_int() const { return icolor_.b; }
		int a_int() const { return icolor_.a; }

		void setRed(int a);
		void setRed(float a);

		void setGreen(int a);
		void setGreen(float a);

		void setBlue(int a);
		void setBlue(float a);

		void setAlpha(int a);
		void setAlpha(float a);

		uint32_t asARGB() const {
			return (static_cast<uint32_t>(icolor_.a) << 24)
				| (static_cast<uint32_t>(icolor_.r) << 16)
				| (static_cast<uint32_t>(icolor_.g) << 8)
				| (static_cast<uint32_t>(icolor_.b));
		}

		uint32_t asRGBA() const {
			return (static_cast<uint32_t>(icolor_.r) << 24)
				| (static_cast<uint32_t>(icolor_.g) << 16)
				| (static_cast<uint32_t>(icolor_.b) << 8)
				| (static_cast<uint32_t>(icolor_.a));
		}

		bool operator==(const Color& rhs) const {
			return asRGBA() == rhs.asRGBA();
		}

		std::size_t operator()(const Color& color) const {
			return asRGBA();
		}

		glm::u8vec4 as_u8vec4(ColorByteOrder order=ColorByteOrder::RGBA) const {
			switch(order) {
			case ColorByteOrder::BGRA: return glm::u8vec4(b_int(), g_int(), r_int(), a_int());
			case ColorByteOrder::ARGB: return glm::u8vec4(a_int(), r_int(), g_int(), b_int());
			case ColorByteOrder::ABGR: return glm::u8vec4(a_int(), b_int(), g_int(), r_int());
			default: break;
			}
			return icolor_;
		}

		const float* asFloatVector(ColorByteOrder order=ColorByteOrder::RGBA) const {
			switch(order) {
			case ColorByteOrder::BGRA: return glm::value_ptr(glm::vec4(color_[2], color_[1], color_[0], color_[3]));
			case ColorByteOrder::ARGB: return glm::value_ptr(glm::vec4(color_[3], color_[0], color_[1], color_[2]));
			case ColorByteOrder::ABGR: return glm::value_ptr(glm::vec4(color_[3], color_[2], color_[1], color_[0]));
			default: break;
			}
			return glm::value_ptr(color_);
		}

		glm::u8vec4 to_hsv() const;
		glm::vec4 to_hsv_vec4() const;
		static Color from_hsv(int h, int s, int v, int a=255);
		static Color from_hsv(float h, float s, float v, float a=1.0f);

		variant write() const;

		void preMultiply();
		void preMultiply(int alpha);
		void preMultiply(float alpha);

		static ColorPtr factory(const std::string& name);

		static const Color& colorAliceblue() { static Color res(240, 248, 255); return res; }
		static const Color& colorAntiquewhite() { static Color res(250, 235, 215); return res; }
		static const Color& colorAqua() { static Color res(0, 255, 255); return res; }
		static const Color& colorAquamarine() { static Color res(127, 255, 212); return res; }
		static const Color& colorAzure() { static Color res(240, 255, 255); return res; }
		static const Color& colorBeige() { static Color res(245, 245, 220); return res; }
		static const Color& colorBisque() { static Color res(255, 228, 196); return res; }
		static const Color& colorBlack() { static Color res(0, 0, 0); return res; }
		static const Color& colorBlanchedalmond() { static Color res(255, 235, 205); return res; }
		static const Color& colorBlue() { static Color res(0, 0, 255); return res; }
		static const Color& colorBlueviolet() { static Color res(138, 43, 226); return res; }
		static const Color& colorBrown() { static Color res(165, 42, 42); return res; }
		static const Color& colorBurlywood() { static Color res(222, 184, 135); return res; }
		static const Color& colorCadetblue() { static Color res(95, 158, 160); return res; }
		static const Color& colorChartreuse() { static Color res(127, 255, 0); return res; }
		static const Color& colorChocolate() { static Color res(210, 105, 30); return res; }
		static const Color& colorCoral() { static Color res(255, 127, 80); return res; }
		static const Color& colorCornflowerblue() { static Color res(100, 149, 237); return res; }
		static const Color& colorCornsilk() { static Color res(255, 248, 220); return res; }
		static const Color& colorCrimson() { static Color res(220, 20, 60); return res; }
		static const Color& colorCyan() { static Color res(0, 255, 255); return res; }
		static const Color& colorDarkblue() { static Color res(0, 0, 139); return res; }
		static const Color& colorDarkcyan() { static Color res(0, 139, 139); return res; }
		static const Color& colorDarkgoldenrod() { static Color res(184, 134, 11); return res; }
		static const Color& colorDarkgray() { static Color res(169, 169, 169); return res; }
		static const Color& colorDarkgreen() { static Color res(0, 100, 0); return res; }
		static const Color& colorDarkgrey() { static Color res(169, 169, 169); return res; }
		static const Color& colorDarkkhaki() { static Color res(189, 183, 107); return res; }
		static const Color& colorDarkmagenta() { static Color res(139, 0, 139); return res; }
		static const Color& colorDarkolivegreen() { static Color res(85, 107, 47); return res; }
		static const Color& colorDarkorange() { static Color res(255, 140, 0); return res; }
		static const Color& colorDarkorchid() { static Color res(153, 50, 204); return res; }
		static const Color& colorDarkred() { static Color res(139, 0, 0); return res; }
		static const Color& colorDarksalmon() { static Color res(233, 150, 122); return res; }
		static const Color& colorDarkseagreen() { static Color res(143, 188, 143); return res; }
		static const Color& colorDarkslateblue() { static Color res(72, 61, 139); return res; }
		static const Color& colorDarkslategray() { static Color res(47, 79, 79); return res; }
		static const Color& colorDarkslategrey() { static Color res(47, 79, 79); return res; }
		static const Color& colorDarkturquoise() { static Color res(0, 206, 209); return res; }
		static const Color& colorDarkviolet() { static Color res(148, 0, 211); return res; }
		static const Color& colorDeeppink() { static Color res(255, 20, 147); return res; }
		static const Color& colorDeepskyblue() { static Color res(0, 191, 255); return res; }
		static const Color& colorDimgray() { static Color res(105, 105, 105); return res; }
		static const Color& colorDimgrey() { static Color res(105, 105, 105); return res; }
		static const Color& colorDodgerblue() { static Color res(30, 144, 255); return res; }
		static const Color& colorFirebrick() { static Color res(178, 34, 34); return res; }
		static const Color& colorFloralwhite() { static Color res(255, 250, 240); return res; }
		static const Color& colorForestgreen() { static Color res(34, 139, 34); return res; }
		static const Color& colorFuchsia() { static Color res(255, 0, 255); return res; }
		static const Color& colorGainsboro() { static Color res(220, 220, 220); return res; }
		static const Color& colorGhostwhite() { static Color res(248, 248, 255); return res; }
		static const Color& colorGold() { static Color res(255, 215, 0); return res; }
		static const Color& colorGoldenrod() { static Color res(218, 165, 32); return res; }
		static const Color& colorGray() { static Color res(128, 128, 128); return res; }
		static const Color& colorGrey() { static Color res(128, 128, 128); return res; }
		static const Color& colorGreen() { static Color res(0, 128, 0); return res; }
		static const Color& colorGreenyellow() { static Color res(173, 255, 47); return res; }
		static const Color& colorHoneydew() { static Color res(240, 255, 240); return res; }
		static const Color& colorHotpink() { static Color res(255, 105, 180); return res; }
		static const Color& colorIndianred() { static Color res(205, 92, 92); return res; }
		static const Color& colorIndigo() { static Color res(75, 0, 130); return res; }
		static const Color& colorIvory() { static Color res(255, 255, 240); return res; }
		static const Color& colorKhaki() { static Color res(240, 230, 140); return res; }
		static const Color& colorLavender() { static Color res(230, 230, 250); return res; }
		static const Color& colorLavenderblush() { static Color res(255, 240, 245); return res; }
		static const Color& colorLawngreen() { static Color res(124, 252, 0); return res; }
		static const Color& colorLemonchiffon() { static Color res(255, 250, 205); return res; }
		static const Color& colorLightblue() { static Color res(173, 216, 230); return res; }
		static const Color& colorLightcoral() { static Color res(240, 128, 128); return res; }
		static const Color& colorLightcyan() { static Color res(224, 255, 255); return res; }
		static const Color& colorLightgoldenrodyellow() { static Color res(250, 250, 210); return res; }
		static const Color& colorLightgray() { static Color res(211, 211, 211); return res; }
		static const Color& colorLightgreen() { static Color res(144, 238, 144); return res; }
		static const Color& colorLightgrey() { static Color res(211, 211, 211); return res; }
		static const Color& colorLightpink() { static Color res(255, 182, 193); return res; }
		static const Color& colorLightsalmon() { static Color res(255, 160, 122); return res; }
		static const Color& colorLightseagreen() { static Color res(32, 178, 170); return res; }
		static const Color& colorLightskyblue() { static Color res(135, 206, 250); return res; }
		static const Color& colorLightslategray() { static Color res(119, 136, 153); return res; }
		static const Color& colorLightslategrey() { static Color res(119, 136, 153); return res; }
		static const Color& colorLightsteelblue() { static Color res(176, 196, 222); return res; }
		static const Color& colorLightyellow() { static Color res(255, 255, 224); return res; }
		static const Color& colorLime() { static Color res(0, 255, 0); return res; }
		static const Color& colorLimegreen() { static Color res(50, 205, 50); return res; }
		static const Color& colorLinen() { static Color res(250, 240, 230); return res; }
		static const Color& colorMagenta() { static Color res(255, 0, 255); return res; }
		static const Color& colorMaroon() { static Color res(128, 0, 0); return res; }
		static const Color& colorMediumaquamarine() { static Color res(102, 205, 170); return res; }
		static const Color& colorMediumblue() { static Color res(0, 0, 205); return res; }
		static const Color& colorMediumorchid() { static Color res(186, 85, 211); return res; }
		static const Color& colorMediumpurple() { static Color res(147, 112, 219); return res; }
		static const Color& colorMediumseagreen() { static Color res(60, 179, 113); return res; }
		static const Color& colorMediumslateblue() { static Color res(123, 104, 238); return res; }
		static const Color& colorMediumspringgreen() { static Color res(0, 250, 154); return res; }
		static const Color& colorMediumturquoise() { static Color res(72, 209, 204); return res; }
		static const Color& colorMediumvioletred() { static Color res(199, 21, 133); return res; }
		static const Color& colorMidnightblue() { static Color res(25, 25, 112); return res; }
		static const Color& colorMintcream() { static Color res(245, 255, 250); return res; }
		static const Color& colorMistyrose() { static Color res(255, 228, 225); return res; }
		static const Color& colorMoccasin() { static Color res(255, 228, 181); return res; }
		static const Color& colorNavajowhite() { static Color res(255, 222, 173); return res; }
		static const Color& colorNavy() { static Color res(0, 0, 128); return res; }
		static const Color& colorOldlace() { static Color res(253, 245, 230); return res; }
		static const Color& colorOlive() { static Color res(128, 128, 0); return res; }
		static const Color& colorOlivedrab() { static Color res(107, 142, 35); return res; }
		static const Color& colorOrange() { static Color res(255, 165, 0); return res; }
		static const Color& colorOrangered() { static Color res(255, 69, 0); return res; }
		static const Color& colorOrchid() { static Color res(218, 112, 214); return res; }
		static const Color& colorPalegoldenrod() { static Color res(238, 232, 170); return res; }
		static const Color& colorPalegreen() { static Color res(152, 251, 152); return res; }
		static const Color& colorPaleturquoise() { static Color res(175, 238, 238); return res; }
		static const Color& colorPalevioletred() { static Color res(219, 112, 147); return res; }
		static const Color& colorPapayawhip() { static Color res(255, 239, 213); return res; }
		static const Color& colorPeachpuff() { static Color res(255, 218, 185); return res; }
		static const Color& colorPeru() { static Color res(205, 133, 63); return res; }
		static const Color& colorPink() { static Color res(255, 192, 203); return res; }
		static const Color& colorPlum() { static Color res(221, 160, 221); return res; }
		static const Color& colorPowderblue() { static Color res(176, 224, 230); return res; }
		static const Color& colorPurple() { static Color res(128, 0, 128); return res; }
		static const Color& colorRed() { static Color res(255, 0, 0); return res; }
		static const Color& colorRosybrown() { static Color res(188, 143, 143); return res; }
		static const Color& colorRoyalblue() { static Color res(65, 105, 225); return res; }
		static const Color& colorSaddlebrown() { static Color res(139, 69, 19); return res; }
		static const Color& colorSalmon() { static Color res(250, 128, 114); return res; }
		static const Color& colorSandybrown() { static Color res(244, 164, 96); return res; }
		static const Color& colorSeagreen() { static Color res(46, 139, 87); return res; }
		static const Color& colorSeashell() { static Color res(255, 245, 238); return res; }
		static const Color& colorSienna() { static Color res(160, 82, 45); return res; }
		static const Color& colorSilver() { static Color res(192, 192, 192); return res; }
		static const Color& colorSkyblue() { static Color res(135, 206, 235); return res; }
		static const Color& colorSlateblue() { static Color res(106, 90, 205); return res; }
		static const Color& colorSlategray() { static Color res(112, 128, 144); return res; }
		static const Color& colorSlategrey() { static Color res(112, 128, 144); return res; }
		static const Color& colorSnow() { static Color res(255, 250, 250); return res; }
		static const Color& colorSpringgreen() { static Color res(0, 255, 127); return res; }
		static const Color& colorSteelblue() { static Color res(70, 130, 180); return res; }
		static const Color& colorTan() { static Color res(210, 180, 140); return res; }
		static const Color& colorTeal() { static Color res(0, 128, 128); return res; }
		static const Color& colorThistle() { static Color res(216, 191, 216); return res; }
		static const Color& colorTomato() { static Color res(255, 99, 71); return res; }
		static const Color& colorTurquoise() { static Color res(64, 224, 208); return res; }
		static const Color& colorViolet() { static Color res(238, 130, 238); return res; }
		static const Color& colorWheat() { static Color res(245, 222, 179); return res; }
		static const Color& colorWhite() { static Color res(255, 255, 255); return res; }
		static const Color& colorWhitesmoke() { static Color res(245, 245, 245); return res; }
		static const Color& colorYellow() { static Color res(255, 255, 0); return res; }
		static const Color& colorYellowgreen() { static Color res(154, 205, 50); return res; }		

		// XXX We should have a ColorCallable, in a seperate file, then move these two into the ColorCallable.
		static std::string getSetFieldType() { return "string"
			"|[decimal,decimal,decimal,decimal]"
			"|[decimal,decimal,decimal]"
			"|[int,int,int,int]"
			"|[int,int,int]"
			"|{red:int|decimal,green:int|decimal,blue:int|decimal,alpha:int|decimal|null}"
			"|{r:int|decimal,g:int|decimal,b:int|decimal,a:int|decimal|null}"; }
		static std::string getDefineFieldType() { return "[int,int,int,int]"; }
	private:
		void convert_to_icolor();
		void convert_to_color();
		glm::u8vec4 icolor_;
		glm::vec4 color_;
	};

	std::ostream& operator<<(std::ostream& os, const Color& c);

	inline bool operator<(const Color& lhs, const Color& rhs)
	{
		return lhs.asARGB() < rhs.asARGB();
	}

	inline bool operator!=(const Color& lhs, const Color& rhs)
	{
		return !(lhs == rhs);
	}

	Color operator*(const Color& lhs, const Color& rhs);

	typedef std::shared_ptr<Color> ColorPtr;
}
