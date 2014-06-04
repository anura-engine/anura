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

#include <algorithm>
#include <map>
#include <string>

#include "../asserts.hpp"
#include "color.hpp"

namespace KRE
{
	namespace 
	{
		template<typename T>
		T clamp(T value, T minval, T maxval)
		{
			return std::min<T>(maxval, std::max(value, minval));
		}

		typedef std::map<std::string, color> color_table_type;
		void create_color_table(color_table_type& color_table)
		{
			color_table["aliceblue"] = color(240, 248, 255);
			color_table["antiquewhite"] = color(250, 235, 215);
			color_table["aqua"] = color(0, 255, 255);
			color_table["aquamarine"] = color(127, 255, 212);
			color_table["azure"] = color(240, 255, 255);
			color_table["beige"] = color(245, 245, 220);
			color_table["bisque"] = color(255, 228, 196);
			color_table["black"] = color(0, 0, 0);
			color_table["blanchedalmond"] = color(255, 235, 205);
			color_table["blue"] = color(0, 0, 255);
			color_table["blueviolet"] = color(138, 43, 226);
			color_table["brown"] = color(165, 42, 42);
			color_table["burlywood"] = color(222, 184, 135);
			color_table["cadetblue"] = color(95, 158, 160);
			color_table["chartreuse"] = color(127, 255, 0);
			color_table["chocolate"] = color(210, 105, 30);
			color_table["coral"] = color(255, 127, 80);
			color_table["cornflowerblue"] = color(100, 149, 237);
			color_table["cornsilk"] = color(255, 248, 220);
			color_table["crimson"] = color(220, 20, 60);
			color_table["cyan"] = color(0, 255, 255);
			color_table["darkblue"] = color(0, 0, 139);
			color_table["darkcyan"] = color(0, 139, 139);
			color_table["darkgoldenrod"] = color(184, 134, 11);
			color_table["darkgray"] = color(169, 169, 169);
			color_table["darkgreen"] = color(0, 100, 0);
			color_table["darkgrey"] = color(169, 169, 169);
			color_table["darkkhaki"] = color(189, 183, 107);
			color_table["darkmagenta"] = color(139, 0, 139);
			color_table["darkolivegreen"] = color(85, 107, 47);
			color_table["darkorange"] = color(255, 140, 0);
			color_table["darkorchid"] = color(153, 50, 204);
			color_table["darkred"] = color(139, 0, 0);
			color_table["darksalmon"] = color(233, 150, 122);
			color_table["darkseagreen"] = color(143, 188, 143);
			color_table["darkslateblue"] = color(72, 61, 139);
			color_table["darkslategray"] = color(47, 79, 79);
			color_table["darkslategrey"] = color(47, 79, 79);
			color_table["darkturquoise"] = color(0, 206, 209);
			color_table["darkviolet"] = color(148, 0, 211);
			color_table["deeppink"] = color(255, 20, 147);
			color_table["deepskyblue"] = color(0, 191, 255);
			color_table["dimgray"] = color(105, 105, 105);
			color_table["dimgrey"] = color(105, 105, 105);
			color_table["dodgerblue"] = color(30, 144, 255);
			color_table["firebrick"] = color(178, 34, 34);
			color_table["floralwhite"] = color(255, 250, 240);
			color_table["forestgreen"] = color(34, 139, 34);
			color_table["fuchsia"] = color(255, 0, 255);
			color_table["gainsboro"] = color(220, 220, 220);
			color_table["ghostwhite"] = color(248, 248, 255);
			color_table["gold"] = color(255, 215, 0);
			color_table["goldenrod"] = color(218, 165, 32);
			color_table["gray"] = color(128, 128, 128);
			color_table["grey"] = color(128, 128, 128);
			color_table["green"] = color(0, 128, 0);
			color_table["greenyellow"] = color(173, 255, 47);
			color_table["honeydew"] = color(240, 255, 240);
			color_table["hotpink"] = color(255, 105, 180);
			color_table["indianred"] = color(205, 92, 92);
			color_table["indigo"] = color(75, 0, 130);
			color_table["ivory"] = color(255, 255, 240);
			color_table["khaki"] = color(240, 230, 140);
			color_table["lavender"] = color(230, 230, 250);
			color_table["lavenderblush"] = color(255, 240, 245);
			color_table["lawngreen"] = color(124, 252, 0);
			color_table["lemonchiffon"] = color(255, 250, 205);
			color_table["lightblue"] = color(173, 216, 230);
			color_table["lightcoral"] = color(240, 128, 128);
			color_table["lightcyan"] = color(224, 255, 255);
			color_table["lightgoldenrodyellow"] = color(250, 250, 210);
			color_table["lightgray"] = color(211, 211, 211);
			color_table["lightgreen"] = color(144, 238, 144);
			color_table["lightgrey"] = color(211, 211, 211);
			color_table["lightpink"] = color(255, 182, 193);
			color_table["lightsalmon"] = color(255, 160, 122);
			color_table["lightseagreen"] = color(32, 178, 170);
			color_table["lightskyblue"] = color(135, 206, 250);
			color_table["lightslategray"] = color(119, 136, 153);
			color_table["lightslategrey"] = color(119, 136, 153);
			color_table["lightsteelblue"] = color(176, 196, 222);
			color_table["lightyellow"] = color(255, 255, 224);
			color_table["lime"] = color(0, 255, 0);
			color_table["limegreen"] = color(50, 205, 50);
			color_table["linen"] = color(250, 240, 230);
			color_table["magenta"] = color(255, 0, 255);
			color_table["maroon"] = color(128, 0, 0);
			color_table["mediumaquamarine"] = color(102, 205, 170);
			color_table["mediumblue"] = color(0, 0, 205);
			color_table["mediumorchid"] = color(186, 85, 211);
			color_table["mediumpurple"] = color(147, 112, 219);
			color_table["mediumseagreen"] = color(60, 179, 113);
			color_table["mediumslateblue"] = color(123, 104, 238);
			color_table["mediumspringgreen"] = color(0, 250, 154);
			color_table["mediumturquoise"] = color(72, 209, 204);
			color_table["mediumvioletred"] = color(199, 21, 133);
			color_table["midnightblue"] = color(25, 25, 112);
			color_table["mintcream"] = color(245, 255, 250);
			color_table["mistyrose"] = color(255, 228, 225);
			color_table["moccasin"] = color(255, 228, 181);
			color_table["navajowhite"] = color(255, 222, 173);
			color_table["navy"] = color(0, 0, 128);
			color_table["oldlace"] = color(253, 245, 230);
			color_table["olive"] = color(128, 128, 0);
			color_table["olivedrab"] = color(107, 142, 35);
			color_table["orange"] = color(255, 165, 0);
			color_table["orangered"] = color(255, 69, 0);
			color_table["orchid"] = color(218, 112, 214);
			color_table["palegoldenrod"] = color(238, 232, 170);
			color_table["palegreen"] = color(152, 251, 152);
			color_table["paleturquoise"] = color(175, 238, 238);
			color_table["palevioletred"] = color(219, 112, 147);
			color_table["papayawhip"] = color(255, 239, 213);
			color_table["peachpuff"] = color(255, 218, 185);
			color_table["peru"] = color(205, 133, 63);
			color_table["pink"] = color(255, 192, 203);
			color_table["plum"] = color(221, 160, 221);
			color_table["powderblue"] = color(176, 224, 230);
			color_table["purple"] = color(128, 0, 128);
			color_table["red"] = color(255, 0, 0);
			color_table["rosybrown"] = color(188, 143, 143);
			color_table["royalblue"] = color(65, 105, 225);
			color_table["saddlebrown"] = color(139, 69, 19);
			color_table["salmon"] = color(250, 128, 114);
			color_table["sandybrown"] = color(244, 164, 96);
			color_table["seagreen"] = color(46, 139, 87);
			color_table["seashell"] = color(255, 245, 238);
			color_table["sienna"] = color(160, 82, 45);
			color_table["silver"] = color(192, 192, 192);
			color_table["skyblue"] = color(135, 206, 235);
			color_table["slateblue"] = color(106, 90, 205);
			color_table["slategray"] = color(112, 128, 144);
			color_table["slategrey"] = color(112, 128, 144);
			color_table["snow"] = color(255, 250, 250);
			color_table["springgreen"] = color(0, 255, 127);
			color_table["steelblue"] = color(70, 130, 180);
			color_table["tan"] = color(210, 180, 140);
			color_table["teal"] = color(0, 128, 128);
			color_table["thistle"] = color(216, 191, 216);
			color_table["tomato"] = color(255, 99, 71);
			color_table["turquoise"] = color(64, 224, 208);
			color_table["violet"] = color(238, 130, 238);
			color_table["wheat"] = color(245, 222, 179);
			color_table["white"] = color(255, 255, 255);
			color_table["whitesmoke"] = color(245, 245, 245);
			color_table["yellow"] = color(255, 255, 0);
			color_table["yellowgreen"] = color(154, 205, 50);		
		}

		color_table_type& get_color_table() 
		{
			static color_table_type res;
			if(res.empty()) {
				create_color_table(res);
			}
			return res;
		}

		/*float convert_numeric(const variant& node)
		{
			if(node.is_int()) {
				return clamp<int>(node.as_int(), 0, 255) / 255.0f;
			} else if(node.is_float()) {
				return clamp<float>(node.as_float(), 0.0f, 1.0f);
			}
			ASSERT_LOG(false, "attribute of color value was expected to be numeric type.");
			return 1.0f;
		}*/
	}



	color::color()
	{
		color_[0] = 1.0f;
		color_[1] = 1.0f;
		color_[2] = 1.0f;
		color_[3] = 1.0f;
	}

	color::~color()
	{
	}

	color::color(const double r, const double g, const double b, const double a)
	{
		color_[0] = float(r);
		color_[1] = float(g);
		color_[2] = float(b);
		color_[3] = float(a);
	}

	color::color(const int r, const int g, const int b, const int a)
	{
		color_[0] = clamp<int>(r,0,255)/255.0f;
		color_[1] = clamp<int>(g,0,255)/255.0f;
		color_[2] = clamp<int>(b,0,255)/255.0f;
		color_[3] = clamp<int>(a,0,255)/255.0f;
	}

	color color::from_name(const std::string& name)
	{
		auto it = get_color_table().find(name);
		ASSERT_LOG(it != get_color_table().end(), "Couldn't find color '" << name << "' in known color list");
		return it->second;
	}

	/*color::color(const variant& node)
	{
		color_[0] = color_[1] = color_[2] = 0.0f;
		color_[3] = 1.0f;

		if(node.is_string()) {
			const std::string& colstr = node.as_string();
			auto it = get_color_table().find(colstr);
			ASSERT_LOG(it != get_color_table().end(), "Couldn't find color '" << colstr << "' in known color list");
			*this = it->second;
		} else if(node.is_list()) {
			ASSERT_LOG(node.num_elements() == 3 || node.num_elements() == 4,
				"color nodes must be lists of 3 or 4 numbers.");
			for(size_t n = 0; n != node.num_elements(); ++n) {
				color_[n] = convert_numeric(node[n]);
			}
		} else if(node.is_map()) {
			if(node.has_key("red")) {
				color_[0] = convert_numeric(node["red"]);
			} else if(node.has_key("r")) {
				color_[0] = convert_numeric(node["r"]);
			}
			if(node.has_key("green")) {
				color_[1] = convert_numeric(node["green"]);
			} else if(node.has_key("g")) {
				color_[1] = convert_numeric(node["g"]);
			}
			if(node.has_key("blue")) {
				color_[2] = convert_numeric(node["blue"]);
			} else if(node.has_key("b")) {
				color_[2] = convert_numeric(node["b"]);
			}
			if(node.has_key("alpha")) {
				color_[3] = convert_numeric(node["alpha"]);
			} else if(node.has_key("a")) {
				color_[3] = convert_numeric(node["a"]);
			}
		} else {
			ASSERT_LOG(false, "Unrecognised color value: " << node.type_as_string());
		}
	}*/
}