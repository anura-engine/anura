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

#pragma comment(lib, "SDL2_ttf")

#include <boost/filesystem.hpp>

#include "module.hpp"
#include "DisplayDevice.hpp"
#include "FontSDL.hpp"
#include "SurfaceSDL.hpp"

namespace KRE
{
	namespace
	{
		static FontRegistrar<FontSDL> font_sdl_register("SDL");

		typedef std::map<std::pair<std::string, int>, TTF_Font*> FontMap;
		FontMap& get_font_table()
		{
			static FontMap res;
			return res;
		}

		std::map<std::string,std::string>& get_font_list()
		{
			static sys::file_path_map res;
			if(res.empty()) {
				//sys::get_unique_files("data/fonts/", res);
				module::get_unique_filenames_under_dir("data/fonts/", &res);
			}
			return res;
		}

		// XXX I don't really like this here since it breaks a nice seperation
		// between application and library.
		bool get_font_path(const std::string& name, std::string& fontn)
		{
			std::map<std::string,std::string>& res = get_font_list();
			std::map<std::string, std::string>::const_iterator itor = res.find(name);
			if(itor == res.end()) {
				return false;
			}
			fontn = itor->second;
			return true;
		}

		SDL_Color to_SDL_Color(const Color& c)
		{
			SDL_Color color = {c.r_int(), c.g_int(), c.b_int(), c.a_int()};
			return color;
		}
	}

	FontSDL::FontSDL()
	{
		int res = TTF_Init();
		ASSERT_LOG(res != -1, "SDL_ttf initialisation failed: " << TTF_GetError());

		SDL_version compile_version;
		const SDL_version *link_version = TTF_Linked_Version();
		SDL_TTF_VERSION(&compile_version);
		LOG_INFO("Compiled with SDL_ttf version: " << compile_version.major << "." << compile_version.minor << "." << compile_version.patch);
		LOG_INFO("Linked   with SDL_ttf version: " << link_version->major << "." << link_version->minor << "." << link_version->patch);
	}

	FontSDL::~FontSDL()
	{
		TTF_Quit();
	}

	TTF_Font* FontSDL::getFont(int size, const std::string& font_name) const
	{
		std::string fn;
		std::string fontn = font_name.empty() ? getDefaultFont() : font_name;
		if(!get_font_path(fontn + ".ttf", fn)) {
			if(!get_font_path(fontn + ".otf", fn)) {
				ASSERT_LOG(false, "Unable to find font file for '" << font_name << "'");
			}
		}

		TTF_Font* font = NULL;
		auto font_pair = std::make_pair(fn, size);
		auto it = get_font_table().find(font_pair);
		if(it == get_font_table().end()) {
			font = TTF_OpenFont(fn.c_str(), size);
			ASSERT_LOG(font != NULL, "Failed to open font file '" << fn << "': " << TTF_GetError());
			get_font_table()[font_pair] = font;
		} else {
			font = it->second;
		}
		return font;	
	}

	TexturePtr FontSDL::doRenderText(const std::string& text, const Color& color, int size, const std::string& font_name) const
	{
		TTF_Font* font = getFont(size, font_name);
		SurfaceSDL* surf;
		if(std::find(text.begin(), text.end(), '\n') == text.end()) {
			surf = new SurfaceSDL(TTF_RenderUTF8_Blended(font, text.c_str(), to_SDL_Color(color)));
		} else {
			std::vector<SDL_Surface*> parts;
			std::vector<std::string> lines = util::split(text, "\n");
			int height = 0, width = 0;
			for(auto& line : lines) {
				parts.emplace_back(TTF_RenderUTF8_Blended(font, line.c_str(), to_SDL_Color(color)));
				if(parts.back() == NULL) {
					LOG_ERROR("Failed to render string: '" << line << "'\n");
					throw FontError();
				}

				if(parts.back()->w > width) {
					width = parts.back()->w;
				}

				height += parts.back()->h;
			}

			const SDL_PixelFormat* f = parts.front()->format;
			surf = new SurfaceSDL(SDL_CreateRGBSurface(0, width, height, f->BitsPerPixel, f->Rmask, f->Gmask, f->Bmask, f->Amask));
			int ypos = 0;
			for(auto& part : parts) {
				SDL_Rect rect = {0, ypos, part->w, part->h};
				SDL_SetSurfaceBlendMode(part, SDL_BLENDMODE_NONE);
				SDL_BlitSurface(part, NULL, surf->get(), &rect);
				ypos += part->h;
				SDL_FreeSurface(part);
			}
		}
		return DisplayDevice::getCurrent()->createTexture(SurfacePtr(surf), false);
	}

	void FontSDL::calcTextSize(const std::string& text, int size, const std::string& font_name, int* width, int* height) const
	{
		TTF_Font* font = getFont(size, font_name);
		int res = TTF_SizeUTF8(font, text.c_str(), width, height);
		ASSERT_LOG(res == 0, "Error calculating size of string: " << TTF_GetError());
	}

	void FontSDL::doReloadFontPaths() 
	{
		get_font_list().clear();
	}

	std::vector<std::string> FontSDL::handleGetAvailableFonts() 
	{
		using namespace boost::filesystem;
		auto& res = get_font_list();
		std::vector<std::string> v;
		for(auto it : res) {
			path p(it.second);
			if(p.extension() == ".ttf" || p.extension() == ".otf") {
				v.push_back(p.stem().generic_string());
			}
		}
		return v;
	}

	int FontSDL::getCharWidth(int size, const std::string& fn) 
	{
		static std::map<std::string, std::map<int, int>> size_cache;
		int& width = size_cache[fn][size];
		if(width) {
			return width;
		}
		int w, h;
		calcTextSize("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", size, fn, &w, &h);
		width = w/36;
		return width;
	}

	int FontSDL::getCharHeight(int size, const std::string& fn) 
	{
		static std::map<std::string, std::map<int, int>> size_cache;
		int& height = size_cache[fn][size];
		if(height) {
			return height;
		}
		int w, h;
		calcTextSize("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", size, fn, &w, &h);
		height = h;
		return height;
	}
}
