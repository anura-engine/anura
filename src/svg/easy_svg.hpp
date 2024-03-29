/*
	Copyright (C) 2013-2016 by Kristina Simpson <sweet.kristas@gmail.com>

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

#include "Texture.hpp"

namespace KRE
{
	TexturePtr svg_texture_from_file(const std::string& file, int width, int height);

	// Takes an array of SVG images and adds them to a single texture, populating a list of
	// texture coordinates if needed.
	TexturePtr svgs_to_single_texture(const std::vector<std::string>& files, const std::vector<point>& wh, std::vector<rectf>* tex_coords = nullptr);
	// Similar to the above function but assumes all the files are the same width/height
	TexturePtr svgs_to_single_texture(const std::vector<std::string>& files, int width, int height, std::vector<rectf>* tex_coords = nullptr);
}
