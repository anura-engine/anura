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

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

#include <vector>

namespace KRE
{
	namespace FT
	{
		// Get a font face from a file.
		FT_Face get_font_face(const std::string& font_file, int index=0);
		// convert a utf8 encoding strings into a series of glyph indicies in the font face.
		std::vector<unsigned> get_glyphs_from_string(FT_Face face, const std::string& utf8);
	}
}
