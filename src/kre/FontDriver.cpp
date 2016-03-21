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

#include "DisplayDevice.hpp"
#include "FontDriver.hpp"
#include "FontImpl.hpp"
#include "Shaders.hpp"

namespace KRE
{
	namespace 
	{
		font_path_cache& get_font_path_cache()
		{
			static font_path_cache res;
			return res;
		}

		font_path_cache& generic_font_lookup()
		{
			static font_path_cache res;
			if(res.empty()) {
				// XXX ideally we read this from a config file.
				res["serif"] = "FreeSerif.ttf";
				res["sans-serif"] = "FreeSans.ttf";
				res["cursive"] = "Allura-Regular.ttf";
				res["fantasy"] = "TradeWinds-Regular.ttf";
				res["monospace"] = "SourceCodePro-Regular.ttf";
			}
			return res;
		}

		struct CacheKey
		{
			CacheKey(const std::string& fn, float sz) : font_name(fn), size(sz) {}
			std::string font_name;
			float size;
			bool operator<(const CacheKey& other) const {
				return font_name == other.font_name ? size < other.size : font_name < other.font_name;
			}
		};

		typedef std::map<CacheKey, FontHandlePtr> font_cache;
		font_cache& get_font_cache()
		{
			static font_cache res;
			return res;
		}

		// Returns a list of what we consider 'common' codepoints
		// these generally consist of the 7-bit ASCII characters.
		// and the unicode replacement character 0xfffd
		std::vector<char32_t>& get_common_glyphs()
		{
			static std::vector<char32_t> res;
			if(res.empty()) {
				// replace character.
				res.emplace_back(0xfffd);
				for(char32_t n = 0x21; n < 0x200; ++n) {
					res.emplace_back(n);
				}
			}
			return res;
		}

		
		std::map<std::string, font_impl_creation_fn>& get_font_providers()
		{
			static std::map<std::string, font_impl_creation_fn> res;
			return res;
		}

		font_impl_creation_fn& defaul_font_provider()
		{
			static font_impl_creation_fn res;
			return res;
		}
	}

	FontDriver::FontDriver()
	{
	}

	void FontDriver::setFontProvider(const std::string& name)
	{
		auto it = get_font_providers().find(name);
		if(it != get_font_providers().end()) {
			defaul_font_provider() = it->second;
		} else {
			LOG_ERROR("No font provider found for '" << name << "' retaining current default.");
		}
	}

	void FontDriver::registerFontProvider(const std::string& name, font_impl_creation_fn create_fn)
	{
		if(get_font_providers().empty()) {
			defaul_font_provider() = create_fn;
		}
		get_font_providers()[name] = create_fn;
	}

	void FontDriver::setAvailableFonts(const font_path_cache& font_map)
	{
		get_font_path_cache() = font_map;
	}

	FontHandlePtr FontDriver::getFontHandle(const std::vector<std::string>& font_list, float size, const Color& color, bool init_texture, const std::string& driver)
	{
		std::string selected_font;
		std::string font_path;
		for(auto& fnt : font_list) {
			auto it = get_font_path_cache().find(fnt);
			if(it != get_font_path_cache().end()) {
				selected_font = it->first;
				font_path = it->second;
				break;
			} else {
				it = get_font_path_cache().find(fnt + ".ttf");
				if(it != get_font_path_cache().end()) {
					selected_font = it->first;
					font_path = it->second;
					break;
				} else {
					it = get_font_path_cache().find(fnt + ".otf");
					if(it != get_font_path_cache().end()) {
						selected_font = it->first;
						font_path = it->second;
						break;
					} else {
						it = generic_font_lookup().find(fnt);
						if(it != generic_font_lookup().end()) {
							it = get_font_path_cache().find(it->second);
							if(it != get_font_path_cache().end()) {
								selected_font = it->first;
								font_path = it->second;
								break;
							}
						}
					}
				}
			}
		}

		if(font_path.empty()) {
			std::ostringstream ss;
			ss << "Unable to find a font to match in the given list:";
			for(auto& fnt : font_list) {
				ss << " " << fnt;
			}
			throw FontError2(ss.str());
		}

		auto it = get_font_cache().find(CacheKey(font_path, size));
		if(it != get_font_cache().end()) {
			return it->second;
		}
		ASSERT_LOG(!get_font_providers().empty(), "No font providers have been defined.");

		std::unique_ptr<FontHandle::Impl> fnt_impl = nullptr;
		if(!driver.empty()) {
			auto it = get_font_providers().find(driver);
			if(it != get_font_providers().end()) {
				fnt_impl = it->second(selected_font, font_path, size, color, init_texture);
			}
		}
		if(fnt_impl == nullptr) {
			ASSERT_LOG(defaul_font_provider(), "No default font provider found.");
			fnt_impl = defaul_font_provider()(selected_font, font_path, size, color, init_texture);
		}
		ASSERT_LOG(fnt_impl != nullptr, "No font implementation.");
		// N.B. After this call fnt_impl is moved into the FontHandle object and while no longer be valid.
		auto fh = std::make_shared<FontHandle>(std::move(fnt_impl), selected_font, font_path, size, color, init_texture);
		get_font_cache()[CacheKey(font_path, size)] = fh;
		return fh;
	}

	const std::vector<char32_t>& FontDriver::getCommonGlyphs()
	{
		return get_common_glyphs();
	}

	FontRenderable::FontRenderable() 
		: SceneObject("font-renderable"),
		  attribs_(nullptr),
		  width_(0),
		  height_(0),
		  color_(nullptr)
	{
		ShaderProgramPtr shader = ShaderProgram::getProgram("font_shader")->clone();
		setShader(shader);
		auto as = DisplayDevice::createAttributeSet();
		attribs_.reset(new Attribute<font_coord>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(font_coord), offsetof(font_coord, vtx)));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE,  2, AttrFormat::FLOAT, false, sizeof(font_coord), offsetof(font_coord, tc)));
		as->addAttribute(AttributeBasePtr(attribs_));
		as->setDrawMode(DrawMode::TRIANGLES);
		as->clearblendState();
		as->clearBlendMode();

		addAttributeSet(as);

		int u_ignore_alpha = shader->getUniform("ignore_alpha");
		int a_color_attr = shader->getAttribute("a_color");
		shader->setUniformDrawFunction([u_ignore_alpha, a_color_attr](ShaderProgramPtr shader) {
			shader->setUniformValue(u_ignore_alpha, 0);
			glm::vec4 attr_color(1.0f);
			shader->setAttributeValue(a_color_attr, glm::value_ptr(attr_color));
		});				
	}

	void FontRenderable::preRender(const WindowPtr& wnd)
	{
		//ASSERT_LOG(color_ != nullptr, "Color pointer was null.");
		if(color_ != nullptr) {
			setColor(*color_);
		}
	}

	void FontRenderable::setColorPointer(const ColorPtr& color) 
	{
		ASSERT_LOG(color != nullptr, "Font color was null.");
		color_ = color;
	}

	void FontRenderable::update(std::vector<font_coord>* queue)
	{
		attribs_->update(queue, attribs_->end());
	}

	void FontRenderable::clear()
	{
		attribs_->clear();
	}

	ColoredFontRenderable::ColoredFontRenderable() 
		: SceneObject("colored-font-renderable"),
		  attribs_(nullptr),
		  color_attrib_(nullptr),
		  width_(0),
		  height_(0),
		  color_(nullptr),
		  vertices_per_color_(6)
	{
		ShaderProgramPtr shader = ShaderProgram::getProgram("font_shader");
		setShader(shader);
		auto as = DisplayDevice::createAttributeSet();
		attribs_.reset(new Attribute<font_coord>(AccessFreqHint::STATIC, AccessTypeHint::DRAW));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(font_coord), offsetof(font_coord, vtx)));
		attribs_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE,  2, AttrFormat::FLOAT, false, sizeof(font_coord), offsetof(font_coord, tc)));
		as->addAttribute(attribs_);

		color_attrib_.reset(new Attribute<glm::u8vec4>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
		color_attrib_->addAttributeDesc(AttributeDesc(AttrType::COLOR,  4, AttrFormat::UNSIGNED_BYTE, true));
		as->addAttribute(color_attrib_);

		as->setDrawMode(DrawMode::TRIANGLES);
		as->clearblendState();
		as->clearBlendMode();

		addAttributeSet(as);

		int u_ignore_alpha = shader->getUniform("ignore_alpha");
		shader->setUniformDrawFunction([u_ignore_alpha](ShaderProgramPtr shader) {
			shader->setUniformValue(u_ignore_alpha, 0);
		});				
	}

	void ColoredFontRenderable::preRender(const WindowPtr& wnd)
	{
		//ASSERT_LOG(color_ != nullptr, "Color pointer was null.");
		if(color_ != nullptr) {
			setColor(*color_);
		}
	}

	void ColoredFontRenderable::setColorPointer(const ColorPtr& color) 
	{
		ASSERT_LOG(color != nullptr, "Font color was null.");
		color_ = color;
	}

	void ColoredFontRenderable::update(std::vector<font_coord>* queue)
	{
		attribs_->update(queue, attribs_->end());
	}

	void ColoredFontRenderable::updateColors(const std::vector<Color>& colors)
	{
		std::vector<glm::u8vec4> col;
		col.reserve(vertices_per_color_ * colors.size());
		for(auto& color : colors) {
			auto c = color.as_u8vec4();
			for(int n = 0; n != vertices_per_color_; ++n) {
				col.emplace_back(c);
			}
		}
		color_attrib_->update(&col);
	}

	void ColoredFontRenderable::clear()
	{
		attribs_->clear();
	}

	FontHandle::FontHandle(std::unique_ptr<Impl>&& impl, const std::string& fnt_name, const std::string& fnt_path, float size, const Color& color, bool init_texture)
		: impl_(std::move(impl))
	{
	}

	FontHandle::~FontHandle()
	{
	}

	float FontHandle::getFontSize()
	{
		return impl_->size_;
	}

	float FontHandle::getFontXHeight()
	{
		// XXX
		return impl_->x_height_;
	}

	const std::string& FontHandle::getFontName()
	{
		return impl_->fnt_;
	}

	const std::string& FontHandle::getFontPath()
	{
		return impl_->fnt_path_;
	}

	const std::string& FontHandle::getFontFamily()
	{
		// XXX
		return impl_->fnt_;
	}

	void FontHandle::renderText()
	{
	}

	void FontHandle::getFontMetrics()
	{
	}

	int FontHandle::getDescender() 
	{
		return impl_->getDescender();
	}

	const std::vector<point>& FontHandle::getGlyphPath(const std::string& text)
	{
		return impl_->getGlyphPath(text);
	}

	rect FontHandle::getBoundingBox(const std::string& text)
	{
		return rect();
	}

	FontRenderablePtr FontHandle::createRenderableFromPath(FontRenderablePtr r, const std::string& text, const std::vector<point>& path)
	{
		return impl_->createRenderableFromPath(r, text, path);
	}

	ColoredFontRenderablePtr FontHandle::createColoredRenderableFromPath(ColoredFontRenderablePtr r, const std::string& text, const std::vector<point>& path, const std::vector<KRE::Color>& colors)
	{
		return impl_->createColoredRenderableFromPath(r, text, path, colors);
	}

	int FontHandle::calculateCharAdvance(char32_t cp)
	{
		return impl_->calculateCharAdvance(cp);
	}
	
	std::vector<unsigned> FontHandle::getGlyphs(const std::string& text)
	{
		return impl_->getGlyphs(text);
	}

	void* FontHandle::getRawFontHandle()
	{
		return impl_->getRawFontHandle();
	}

	float FontHandle::getLineGap() const
	{
		return impl_->getLineGap();
	}
}
