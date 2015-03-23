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

#include <exception>

#include "Texture.hpp"
#include "Util.hpp"

namespace KRE
{
	class Font;
	typedef std::shared_ptr<Font> FontPtr;

	struct FontError : public std::runtime_error
	{
		FontError(const char* errstr) : std::runtime_error(errstr) {}
	};

	typedef std::map<std::string, std::string> font_path_cache;

	class Font
	{
	public:
		virtual ~Font();
		TexturePtr renderText(const std::string& text, const Color& color, int size, bool cache=true, const std::string& font_name="") const;
		static void setDefaultFont(const std::string& font_name);
		static const std::string& getDefaultFont();
		void getTextSize(const std::string& text, int* width, int* height, int size, const std::string& font_name="") const;
		static void registerFactoryFunction(const std::string& type, std::function<FontPtr()>);
		static FontPtr getInstance(const std::string& hint="");
		static std::string get_default_monospace_font() { return "FreeMono"; }
		static void setAvailableFonts(const std::map<std::string, std::string>& paths);
		static std::string findFontPath(const std::string& fontname);
		static std::vector<std::string> getAvailableFonts();
		static int charWidth(int size, const std::string& fn="");
		static int charHeight(int size, const std::string& fn="");
	protected:
		Font();
	private:
		DISALLOW_COPY_AND_ASSIGN(Font);
		virtual TexturePtr doRenderText(const std::string& text, const Color& color, int size, const std::string& font_name) const = 0;
		virtual void calcTextSize(const std::string& text, int size, const std::string& font_name, int* width, int* height) const = 0;
		virtual int getCharWidth(int size, const std::string& fn) = 0;
		virtual int getCharHeight(int size, const std::string& fn) = 0;
	};

	template<class T>
	struct FontRegistrar
	{
		FontRegistrar(const std::string& type)
		{
			// register the class factory function 
			Font::registerFactoryFunction(type, []() -> FontPtr { return FontPtr(new T());});
		}
	};

}
