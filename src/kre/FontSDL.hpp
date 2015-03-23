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

#include "SDL_ttf.h"
#include "Font.hpp"

namespace KRE
{
	class FontSDL : public Font
	{
	public:
		FontSDL();
		virtual ~FontSDL();
	private:
		DISALLOW_COPY_AND_ASSIGN(FontSDL);
		TexturePtr doRenderText(const std::string& text, const Color& color, int size, const std::string& font_name) const override;
		void calcTextSize(const std::string& text, int size, const std::string& font_name, int* width, int* height) const override;
		TTF_Font* getFont(int size, const std::string& font_name) const;
		int getCharWidth(int size, const std::string& fn) override;
		int getCharHeight(int size, const std::string& fn) override;
	};
}
