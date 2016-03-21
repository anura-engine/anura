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
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "geometry.hpp"
#include "AttributeSet.hpp"
#include "Color.hpp"
#include "RenderFwd.hpp"
#include "SceneObject.hpp"
#include "Texture.hpp"

namespace KRE
{
	typedef std::map<std::string, std::string> font_path_cache;	

	struct FontError2 : public std::runtime_error
	{
		FontError2(const std::string& str) : std::runtime_error(str) {}
	};

	struct font_coord
	{
		font_coord(const glm::vec2& v, const glm::vec2& t) : vtx(v), tc(t) {}
		glm::vec2 vtx;
		glm::vec2 tc;
	};

	class FontRenderable : public SceneObject
	{
	public:
		FontRenderable();
		void clear();
		void update(std::vector<font_coord>* queue);
		int getWidth() const { return width_; }
		int getHeight() const { return height_; }
		void setWidth(int width) { width_ = width; }
		void setHeight(int height) { height_ = height; }
		void setColorPointer(const ColorPtr& color);
		void preRender(const WindowPtr& wnd);
	private:
		std::shared_ptr<Attribute<font_coord>> attribs_;
		// intrinsic width and height when rendered, in pixels.
		int width_;
		int height_;
		ColorPtr color_;
	};
	typedef std::shared_ptr<FontRenderable> FontRenderablePtr;

	class ColoredFontRenderable : public SceneObject
	{
	public:
		ColoredFontRenderable();
		void clear();
		void update(std::vector<font_coord>* queue);
		int getWidth() const { return width_; }
		int getHeight() const { return height_; }
		void setWidth(int width) { width_ = width; }
		void setHeight(int height) { height_ = height; }
		void setColorPointer(const ColorPtr& color);
		void preRender(const WindowPtr& wnd);
		void updateColors(const std::vector<Color>& colors);
		void setVerticesPerColor(int n) { vertices_per_color_ = n; }
	private:
		std::shared_ptr<Attribute<font_coord>> attribs_;
		std::shared_ptr<Attribute<glm::u8vec4>> color_attrib_;
		// intrinsic width and height when rendered, in pixels.
		int width_;
		int height_;
		ColorPtr color_;
		int vertices_per_color_;
	};
	typedef std::shared_ptr<ColoredFontRenderable> ColoredFontRenderablePtr;

	class FontHandle
	{
	public:
		class Impl;
		FontHandle(std::unique_ptr<Impl>&& impl, const std::string& fnt_name, const std::string& fnt_path, float size, const Color& color, bool init_texture);
		~FontHandle();
		float getFontSize();
		float getFontXHeight();
		const std::string& getFontName();
		const std::string& getFontPath();
		const std::string& getFontFamily();
		void renderText();
		void getFontMetrics();
		int getDescender();
		rect getBoundingBox(const std::string& text);
		FontRenderablePtr createRenderableFromPath(FontRenderablePtr r, const std::string& text, const std::vector<point>& path);
		ColoredFontRenderablePtr createColoredRenderableFromPath(ColoredFontRenderablePtr r, const std::string& text, const std::vector<point>& path, const std::vector<KRE::Color>& colors);
		const std::vector<point>& getGlyphPath(const std::string& text);
		int calculateCharAdvance(char32_t cp);
		int getScaleFactor() const { return 65536; }
		std::vector<unsigned> getGlyphs(const std::string& text);
		void* getRawFontHandle();
		float getLineGap() const;
	private:
		std::unique_ptr<Impl> impl_;
	};
	typedef std::shared_ptr<FontHandle> FontHandlePtr;

	typedef std::function<std::unique_ptr<FontHandle::Impl>(const std::string&, const std::string&, float , const Color&, bool)> font_impl_creation_fn;

	class FontDriver
	{
	public:
		static void setFontProvider(const std::string& name);
		static void registerFontProvider(const std::string& name, font_impl_creation_fn create_fn);
		static FontHandlePtr getFontHandle(const std::vector<std::string>& font_list, float size, const Color& color=Color::colorWhite(), bool init_texture=true, const std::string& driver=std::string());
		static void setAvailableFonts(const font_path_cache& font_map);
		//static TexturePtr renderText(const std::string& text, ...);
		static const std::vector<char32_t>& getCommonGlyphs();
	private:
		FontDriver();
	};

	struct FontDriverRegistrar
	{
		explicit FontDriverRegistrar(const std::string& name, font_impl_creation_fn create_fn) 
		{
			FontDriver::registerFontProvider(name, create_fn);
		}
	};
}
