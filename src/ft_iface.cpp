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

#include <map>

#include "asserts.hpp"
#include "ft_iface.hpp"
#include "module.hpp"
#include "utf8_to_codepoint.hpp"

#pragma comment(lib, "libfreetype-6")

namespace KRE
{
	namespace FT
	{
		namespace 
		{
			const char* fallback_font_name = "FreeSans.ttf";
			const char* font_path = "data/fonts/";

			FT_Library& get_freetype_library()
			{
				static FT_Library library = nullptr;
				if(library == nullptr) {
					FT_Error error = FT_Init_FreeType(&library);
					ASSERT_LOG(error == 0, "Error initialising freetype library: " << error);
				}
				return library;
			}
		}

		FT_Face get_font_face(const std::string& font_file, int index)
		{
			FT_Library& library = get_freetype_library();
			static std::map<std::string,FT_Face> font_map;
			auto it = font_map.find(font_file);
			if(it == font_map.end()) {
				FT_Face face;
				FT_Error error = FT_New_Face(library, module::map_file(font_path + font_file).c_str(), index, &face);
				if(error != 0) {
					error = FT_New_Face(library, module::map_file(font_path + font_file + ".otf").c_str(), index, &face);
					if(error != 0) {
						error = FT_New_Face(library, module::map_file(font_path + font_file + ".ttf").c_str(), index, &face);
						if(error != 0) {
							// Try our default fallback font. If we can't find that we have
							// serious issues.
							error = FT_New_Face(library, module::map_file(std::string(font_path) + fallback_font_name).c_str(), index, &face);
						}
					}
				}
				if(error != 0) {
					return nullptr;
				}
				//ASSERT_LOG(error == 0, "Could not load font face: " << font_file << " error: " << error);
				font_map[font_file] = face;
				return face;
			} 
			return it->second;
		}

		std::vector<unsigned> get_glyphs_from_string(FT_Face face, const std::string& utf8)
		{
			std::vector<unsigned> res;
			for(auto cp : utils::utf8_to_codepoint(utf8)) {
				res.emplace_back(FT_Get_Char_Index(face, cp));
			}
			return res;
		}
	}
}
