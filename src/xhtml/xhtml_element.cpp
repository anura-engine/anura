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

#include <map>

#include <boost/lexical_cast.hpp>

#include "asserts.hpp"
#include "css_parser.hpp"
#include "easy_svg.hpp"
#include "xhtml_element.hpp"
#include "xhtml_script_interface.hpp"

#include "Blittable.hpp"
#include "Font.hpp"
#include "Texture.hpp"

namespace xhtml
{
	namespace 
	{
		// Used to track what custom element value we should next allocate.
		int custom_element_counter = -1;

		typedef std::function<ElementPtr(ElementId, const std::string&, WeakDocumentPtr owner)> ElementFactoryFnType;

		struct ElementFunctionAndId
		{
			ElementFunctionAndId() : id(ElementId::ANY), fn() {}
			ElementFunctionAndId(ElementId i, ElementFactoryFnType f) : id(i), fn(f) {}
			ElementId id;
			ElementFactoryFnType fn;
		};

		typedef std::map<std::string, ElementFunctionAndId> ElementRegistry;
		ElementRegistry& get_element_registry()
		{
			static ElementRegistry res;
			return res;
		}
		
		std::map<ElementId, std::string>& get_id_registry()
		{
			static std::map<ElementId, std::string> res;
			if(res.empty()) {
				res[ElementId::ANY] = "*";
			}
			return res;
		}
		
		void register_factory_function(ElementId id, const std::string& type, ElementFactoryFnType fn)
		{
			get_element_registry()[type] = ElementFunctionAndId(id, fn);
			get_id_registry()[id] = type;
		}

		template<typename T>
		struct ElementRegistrar
		{
			ElementRegistrar(ElementId id, const std::string& type)
			{
				// register the class factory function 
				register_factory_function(id, type, [](ElementId id, const std::string& name, WeakDocumentPtr owner) -> ElementPtr { return std::make_shared<T>(id, name, owner); });
			}
		};

		template<typename T>
		struct ElementRegistrarInt
		{
			ElementRegistrarInt(ElementId id, const std::string& type, int level)
			{
				// register the class factory function 
				register_factory_function(id, type, [level](ElementId id, const std::string& name, WeakDocumentPtr owner) -> ElementPtr { return std::make_shared<T>(id, name, owner, level); });
			}
		};

		struct CustomElement : public Element
		{
			explicit CustomElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};

		// Start of elements.
		struct HtmlElement : public Element
		{
			explicit HtmlElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<HtmlElement> html_element(ElementId::HTML, "html");

		struct HeadElement : public Element
		{
			explicit HeadElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<HeadElement> head_element(ElementId::HEAD, "head");

		struct BodyElement : public Element
		{
			explicit BodyElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<BodyElement> body_element(ElementId::BODY, "body");

		struct ScriptElement : public Element
		{
			explicit ScriptElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
			bool ignoreForLayout() const override { return true; }
			void init() override {
				auto src = getAttribute("src");
				auto type = getAttribute("type");
				if(type == nullptr) {
					LOG_ERROR("No 'type' attribute specified on 'script' element. This is a required attribute.");
					return;
				}
				auto handler = Document::findScriptHandler(type->getValue());
				if(handler != nullptr) {
					if(src != nullptr && !src->getValue().empty()) {
						// load script from file and process here.
						handler->runScriptFile(src->getValue());
					}
					for(auto& child : getChildren()) {
						if(child->id() == NodeId::TEXT) {
							// Parse script nodes here.
							handler->runScript(src->getValue());
						}
					}
				}
			}
		};
		ElementRegistrar<ScriptElement> script_element(ElementId::SCRIPT, "script");

		struct ParagraphElement : public Element
		{
			explicit ParagraphElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<ParagraphElement> paragraph_element(ElementId::P, "p");

		struct AbbreviationElement : public Element
		{
			explicit AbbreviationElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<AbbreviationElement> abbreviation_element(ElementId::ABBR, "abbr");

		struct EmphasisElement : public Element
		{
			explicit EmphasisElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<EmphasisElement> emphasis_element(ElementId::EM, "em");

		struct BreakElement : public Element
		{
			explicit BreakElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<BreakElement> break_element(ElementId::BR, "br");

		struct ImageElement : public Element
		{
			explicit ImageElement(ElementId id, const std::string& name, WeakDocumentPtr owner) 
				: Element(id, name, owner), 
				  dims_set_(false), 
				  tex_() 
			{
			}
			void init() override {
				if(!dims_set_) {
					auto attr_w = getAttribute("width");
					auto attr_h = getAttribute("height");
					auto attr_src = getAttribute("src");
					auto attr_alt = getAttribute("alt");
					rect r;
					if(attr_src != nullptr && !attr_src->getValue().empty()) {
						tex_ = KRE::Texture::createTexture(attr_src->getValue());
						r = rect(0, 0, tex_->width(), tex_->height());
					} else if(attr_alt != nullptr && !attr_alt->getValue().empty()) {
						// Render the alt text. This could be improved. 16 below represents a 12pt font.
						tex_ = KRE::Font::getInstance()->renderText(attr_alt->getValue(), KRE::Color::colorWhite(), 16, true, "FreeSerif.ttf");
						r = rect(0, 0, tex_->width(), tex_->height());
					}
					if(attr_w != nullptr) {
						try {
							r.set_w(boost::lexical_cast<int>(attr_w->getValue()));
						} catch(boost::bad_lexical_cast&) {
							LOG_ERROR("Unable to convert 'img' tag 'width' attribute to number: " << attr_w->getValue());
						}
					}
					if(attr_h != nullptr) {
						try {
							r.set_h(boost::lexical_cast<int>(attr_h->getValue()));
						} catch(boost::bad_lexical_cast&) {
							LOG_ERROR("Unable to convert 'img' tag 'height' attribute to number: " << attr_h->getValue());
						}
					}
					dims_set_ = true;
					setDimensions(r);
				}
			}
			bool isReplaced() const override { return true; }
			KRE::SceneObjectPtr getRenderable()
			{
				if(tex_ != nullptr) {
					auto b = std::make_shared<KRE::Blittable>(tex_);
					b->setDrawRect(getDimensions());
					return b;
				}
				return nullptr;
			}
			bool dims_set_;
			KRE::TexturePtr tex_;
		};
		ElementRegistrar<ImageElement> img_element(ElementId::IMG, "img");

		struct ObjectElement : public Element
		{
			explicit ObjectElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
			bool isReplaced() const override { return true; }
		};
		ElementRegistrar<ObjectElement> object_element(ElementId::OBJECT, "object");

		struct StyleElement : public Element
		{
			explicit StyleElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
			bool ignoreForLayout() const override { return true; }
		};
		ElementRegistrar<StyleElement> style_element(ElementId::STYLE, "style");

		// Start of elements.
		struct TitleElement : public Element
		{
			explicit TitleElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		private:
			std::string title_;
		};
		ElementRegistrar<TitleElement> title_element(ElementId::TITLE, "title");

		struct LinkElement : public Element
		{
			explicit LinkElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<LinkElement> link_element(ElementId::LINK, "link");

		struct MetaElement : public Element
		{
			explicit MetaElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<MetaElement> meta_element(ElementId::META, "meta");

		struct BaseElement : public Element
		{
			explicit BaseElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<BaseElement> base_element(ElementId::BASE, "base");

		struct FormElement : public Element
		{
			explicit FormElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<FormElement> form_element(ElementId::FORM, "form");

		struct SelectElement : public Element
		{
			explicit SelectElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
			bool isReplaced() const override { return true; }
		};
		ElementRegistrar<SelectElement> select_element(ElementId::SELECT, "select");

		struct OptionGroupElement : public Element
		{
			explicit OptionGroupElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<OptionGroupElement> option_group_element(ElementId::OPTGROUP, "optgroup");

		struct OptionElement : public Element
		{
			explicit OptionElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<OptionElement> option_element(ElementId::OPTION, "option");

		enum class InputElementType {
			TEXT,
			PASSWORD,
			CHECKBOX,
			RADIO,
			SUBMIT,
			IMAGE,
			RESET,
			BUTTON,
			HIDDEN,
			FILE,
		};
		struct InputElement : public Element
		{
			explicit InputElement(ElementId id, const std::string& name, WeakDocumentPtr owner) 
				: Element(id, name, owner),
				  is_checked_(false),
				  width_(16),
				  height_(16),
				  radio_tex_(KRE::svg_texture_from_file("radiobutton.svg", width_, height_)),
				  radio_checked_tex_(KRE::svg_texture_from_file("radiobutton-checked.svg", width_, height_)),
				  checkbox_tex_(KRE::svg_texture_from_file("checkbox.svg", width_, height_)),
				  checkbox_checked_tex_(KRE::svg_texture_from_file("checkbox-checked.svg", width_, height_))
			{
			}
			void handleSetDimensions(const rect& r) override {
				if(width_ != r.w() || height_ != r.h()) {
					width_ = r.w();
					height_ = r.h();
					switch(type_) {
						case InputElementType::CHECKBOX:
							checkbox_tex_ = KRE::svg_texture_from_file("checkbox.svg", width_, height_);
							checkbox_checked_tex_ = KRE::svg_texture_from_file("checkbox-checked.svg", width_, height_);
							break;
						case InputElementType::RADIO:
							radio_tex_ = KRE::svg_texture_from_file("radiobutton.svg", width_, height_);
							radio_checked_tex_ = KRE::svg_texture_from_file("radiobutton-checked.svg", width_, height_);
							break;
						case InputElementType::TEXT:
						case InputElementType::PASSWORD:
						case InputElementType::SUBMIT:
						case InputElementType::IMAGE:
						case InputElementType::RESET:
						case InputElementType::BUTTON:
						case InputElementType::HIDDEN:
						case InputElementType::FILE:
						default: 
							ASSERT_LOG(false, "Need to add getRenderable() for InputElement of type: " << static_cast<int>(type_));
							break;
					}			
				}
			}
			void init() override {
				auto attr_checked = getAttribute("checked");
				if(attr_checked) {
					is_checked_ = true;
				}
				auto type = getAttribute("type");
				if(type) {
					const std::string& value = type->getValue();
					if(value == "text") {
						type_ = InputElementType::TEXT;
					} else if(value == "password") {
						type_ = InputElementType::PASSWORD;
					} else if(value == "checkbox") {
						type_ = InputElementType::CHECKBOX;
						setDimensions(rect(0, 0, width_, height_));
					} else if(value == "radio") {
						type_ = InputElementType::RADIO;
						setDimensions(rect(0, 0, width_, height_));
					} else if(value == "submit") {
						type_ = InputElementType::SUBMIT;
					} else if(value == "image") {
						type_ = InputElementType::IMAGE;
					} else if(value == "reset") {
						type_ = InputElementType::RESET;
					} else if(value == "button") {
						type_ = InputElementType::BUTTON;
					} else if(value == "hidden") {
						type_ = InputElementType::HIDDEN;
					} else if(value == "file") {
						type_ = InputElementType::FILE;
					}
				} else {
					ASSERT_LOG(false, "'input' element had no type. asserting rather than using a default.");
				}
			}
			bool handleMouseButtonUpInt(bool* trigger, const point& p) override
			{ 
				if(geometry::pointInRect(p, getActiveRect())) {
					LOG_DEBUG("test1");
					is_checked_ = !is_checked_;
				}
				return true; 
			}
			bool handleMouseButtonDownInt(bool* trigger, const point& p) override 
			{ 
				if(geometry::pointInRect(p, getActiveRect())) {
					LOG_DEBUG("test2");
				}
				return true; 
			}
			bool isReplaced() const override { return true; }
			KRE::SceneObjectPtr getRenderable()
			{
				switch(type_) {
					case InputElementType::CHECKBOX: {
						// XXX this should be improved. some sort of custom
						// SceneObject that shares state with this input element
						// Then we can process mouse events here and dynamically
						// have the renderable update.
						if(is_checked_) {
							auto b = std::make_shared<KRE::Blittable>(checkbox_checked_tex_);
							return b;
						} else {
							auto b = std::make_shared<KRE::Blittable>(checkbox_tex_);
							return b;
						}
					}
					case InputElementType::RADIO: {
						if(is_checked_) {
							auto b = std::make_shared<KRE::Blittable>(radio_checked_tex_);
							return b;
						} else {
							auto b = std::make_shared<KRE::Blittable>(radio_tex_);
							return b;
						}
					}
					case InputElementType::TEXT:
					case InputElementType::PASSWORD:
					case InputElementType::SUBMIT:
					case InputElementType::IMAGE:
					case InputElementType::RESET:
					case InputElementType::BUTTON:
					case InputElementType::HIDDEN:
					case InputElementType::FILE:
					default: 
						ASSERT_LOG(false, "Need to add getRenderable() for InputElement of type: " << static_cast<int>(type_));
						break;
				}
				return nullptr;
			}
		
			InputElementType type_;
			int width_;
			int height_;
			bool is_checked_;
			// probably better to fold these into one texture
			KRE::TexturePtr radio_tex_;
			KRE::TexturePtr radio_checked_tex_;
			KRE::TexturePtr checkbox_tex_;
			KRE::TexturePtr checkbox_checked_tex_;
		};
		ElementRegistrar<InputElement> input_element(ElementId::INPUT, "input");

		struct TextAreaElement : public Element
		{
			explicit TextAreaElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
			bool isReplaced() const override { return true; }
		};
		ElementRegistrar<TextAreaElement> text_area_element(ElementId::TEXTAREA, "textarea");

		struct ButtonElement : public Element
		{
			explicit ButtonElement(ElementId id, const std::string& name, WeakDocumentPtr owner) 
				: Element(id, name, owner) ,
				  width_(240),
				  height_(100),
				  img_src_("button.svg"),
				  tex_(KRE::svg_texture_from_file(img_src_, width_, height_))
			{
			}
			void init() override {
				auto attr_src = getAttribute("src");
				if(attr_src != nullptr && !attr_src->getValue().empty()) {
					img_src_ = attr_src->getValue();
					tex_ = KRE::Texture::createTexture(img_src_);
					width_ = tex_->width();
					height_ = tex_->height();
				}
				setDimensions(rect(0, 0, width_, height_));
			}
			void handleSetDimensions(const rect& r) override {
				if(width_ != r.w() || height_ != r.h()) {
					width_ = r.w();
					height_ = r.h();
					tex_ = KRE::svg_texture_from_file(img_src_, width_, height_);
				}
			}
			bool isReplaced() const override { return true; }
			KRE::SceneObjectPtr getRenderable()
			{
				if(tex_ != nullptr) {
					return std::make_shared<KRE::Blittable>(tex_);
				}
				return nullptr;
			}
			int width_;
			int height_;
			std::string img_src_;
			bool dims_set_;
			KRE::TexturePtr tex_;
		};
		ElementRegistrar<ButtonElement> button_element(ElementId::BUTTON, "button");

		struct LabelElement : public Element
		{
			explicit LabelElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<LabelElement> label_element(ElementId::LABEL, "label");

		struct FieldSetElement : public Element
		{
			explicit FieldSetElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<FieldSetElement> field_set_element(ElementId::FIELDSET, "fieldset");

		struct LegendElement : public Element
		{
			explicit LegendElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<LegendElement> legend_element(ElementId::LEGEND, "legend");

		struct UnorderedListElement : public Element
		{
			explicit UnorderedListElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<UnorderedListElement> unordered_list_element(ElementId::UL, "ul");

		struct OrderedListElement : public Element
		{
			explicit OrderedListElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<OrderedListElement> ordered_list_element(ElementId::OL, "ol");

		struct DefinitionListElement : public Element
		{
			explicit DefinitionListElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<DefinitionListElement> definition_list_element(ElementId::DL, "dl");

		struct DirectoryElement : public Element
		{
			explicit DirectoryElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<DirectoryElement> directory_element(ElementId::DIR, "dir");

		struct MenuElement : public Element
		{
			explicit MenuElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<MenuElement> menu_element(ElementId::MENU, "menu");

		struct ListItemElement : public Element
		{
			explicit ListItemElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<ListItemElement> list_item_element(ElementId::LI, "li");

		struct DivElement : public Element
		{
			explicit DivElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<DivElement> div_element(ElementId::DIV, "div");

		struct HeadingElement : public Element
		{
			explicit HeadingElement(ElementId id, const std::string& name, WeakDocumentPtr owner, int level) : Element(id, name, owner), level_(level) {}
		private:
			int level_;
		};
		ElementRegistrarInt<HeadingElement> h1_element(ElementId::H1, "h1", 1);
		ElementRegistrarInt<HeadingElement> h2_element(ElementId::H2, "h2", 2);
		ElementRegistrarInt<HeadingElement> h3_element(ElementId::H3, "h3", 3);
		ElementRegistrarInt<HeadingElement> h4_element(ElementId::H4, "h4", 4);
		ElementRegistrarInt<HeadingElement> h5_element(ElementId::H5, "h5", 5);
		ElementRegistrarInt<HeadingElement> h6_element(ElementId::H6, "h6", 6);

		struct QuoteElement : public Element
		{
			explicit QuoteElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<QuoteElement> quote_element(ElementId::Q, "q");

		struct BlockQuoteElement : public Element
		{
			explicit BlockQuoteElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<BlockQuoteElement> block_quote_element(ElementId::BLOCKQUOTE, "blockquote");

		struct PreformattedElement : public Element
		{
			explicit PreformattedElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<PreformattedElement> preformatted_element(ElementId::PRE, "pre");

		struct HorizontalRuleElement : public Element
		{
			explicit HorizontalRuleElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<HorizontalRuleElement> horizontal_rule_element(ElementId::HR, "hr");

		struct ModElement : public Element
		{
			explicit ModElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<ModElement> mod_element(ElementId::MOD, "mod");

		struct AnchorElement : public Element
		{
			explicit AnchorElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<AnchorElement> anchor_element(ElementId::A, "a");

		struct ParamElement : public Element
		{
			explicit ParamElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<ParamElement> param_element(ElementId::PARAM, "param");

		struct AppletElement : public Element
		{
			explicit AppletElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<AppletElement> applet_element(ElementId::APPLET, "applet");

		struct MapElement : public Element
		{
			explicit MapElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<MapElement> map_element(ElementId::MAP, "map");

		struct AreaElement : public Element
		{
			explicit AreaElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<AreaElement> area_element(ElementId::AREA, "area");

		struct TableElement : public Element
		{
			explicit TableElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<TableElement> table_element(ElementId::TABLE, "table");

		struct TableCaptionElement : public Element
		{
			explicit TableCaptionElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<TableCaptionElement> table_caption_element(ElementId::CAPTION, "caption");

		struct TableColElement : public Element
		{
			explicit TableColElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<TableColElement> table_col_element(ElementId::COL, "col");

		struct TableColGroupElement : public Element
		{
			explicit TableColGroupElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<TableColGroupElement> table_col_group_element(ElementId::COLGROUP, "colgroup");

		struct TableSectionElement : public Element
		{
			explicit TableSectionElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<TableSectionElement> table_head_element(ElementId::THEAD, "thead");
		ElementRegistrar<TableSectionElement> table_foot_element(ElementId::TFOOT, "tfoot");
		ElementRegistrar<TableSectionElement> table_body_element(ElementId::TBODY, "tbody");

		struct TableRowElement : public Element
		{
			explicit TableRowElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<TableRowElement> table_row_element(ElementId::TR, "tr");

		struct TableCellElement : public Element
		{
			explicit TableCellElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<TableCellElement> table_cell_element(ElementId::TD, "td");

		struct FrameSetElement : public Element
		{
			explicit FrameSetElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<FrameSetElement> frame_set_element(ElementId::FRAMESET, "frameset");

		struct FrameElement : public Element
		{
			explicit FrameElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<FrameElement> frame_element(ElementId::FRAME, "frame");

		struct IFrameElement : public Element
		{
			explicit IFrameElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<IFrameElement> i_frame_element(ElementId::IFRAME, "iframe");

		struct SpanElement : public Element
		{
			explicit SpanElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<SpanElement> span_element(ElementId::SPAN, "span");

		struct AcronymElement : public Element
		{
			explicit AcronymElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<AcronymElement> acronym_element(ElementId::ACRONYM, "acronym");
		
		struct AddressElement : public Element
		{
			explicit AddressElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<AddressElement> address_element(ElementId::ADDRESS, "address");
		
		struct BoldElement : public Element
		{
			explicit BoldElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<SpanElement> bold_element(ElementId::B, "b");
		
		struct BDOElement : public Element
		{
			explicit BDOElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<SpanElement> bdo_element(ElementId::BDO, "bdo");
		
		struct BigElement : public Element
		{
			explicit BigElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<BigElement> big_element(ElementId::BIG, "big");
		
		struct CitationElement : public Element
		{
			explicit CitationElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<CitationElement> cite_element(ElementId::CITE, "cite");
		
		struct CodeElement : public Element
		{
			explicit CodeElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<CodeElement> code_element(ElementId::CODE, "code");
		
		struct DefinitionDescriptionElement : public Element
		{
			explicit DefinitionDescriptionElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<DefinitionDescriptionElement> dd_element(ElementId::DD, "dd");
		
		struct InsertedContentElement : public Element
		{
			explicit InsertedContentElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<InsertedContentElement> ins_element(ElementId::INS, "ins");
		
		struct DeletedContentElement : public Element
		{
			explicit DeletedContentElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<DeletedContentElement> del_element(ElementId::DEL, "del");
		
		struct DefiningInstanceElement : public Element
		{
			explicit DefiningInstanceElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<DefiningInstanceElement> dfn_element(ElementId::DFN, "dfn");
		
		struct DefinitionTermElement : public Element
		{
			explicit DefinitionTermElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<DefinitionTermElement> dt_element(ElementId::DT, "dt");
		
		struct ItalicsElement : public Element
		{
			explicit ItalicsElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<ItalicsElement> italics_element(ElementId::I, "i");
		
		struct KbdElement : public Element
		{
			explicit KbdElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<KbdElement> kbd_element(ElementId::KBD, "kbd");
		
		struct NoScriptElement : public Element
		{
			explicit NoScriptElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<NoScriptElement> noscript_element(ElementId::NOSCRIPT, "noscript");
		
		struct RubyBaseTextElement : public Element
		{
			explicit RubyBaseTextElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<RubyBaseTextElement> rb_element(ElementId::RB, "rb");
		
		struct RubyBaseContainerElement : public Element
		{
			explicit RubyBaseContainerElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<RubyBaseContainerElement> rbc_element(ElementId::RBC, "rbc");
		
		struct RubyAnnotationTextElement : public Element
		{
			explicit RubyAnnotationTextElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<RubyAnnotationTextElement> rt_element(ElementId::RT, "rt");
		
		struct RubyTextContainerElement : public Element
		{
			explicit RubyTextContainerElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<RubyTextContainerElement> rtc_element(ElementId::RTC, "rtc");
		
		struct RubyAnnotaionElement : public Element
		{
			explicit RubyAnnotaionElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<RubyAnnotaionElement> ruby_element(ElementId::RUBY, "ruby");
		
		struct SampleOutputElement : public Element
		{
			explicit SampleOutputElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<SpanElement> samp_element(ElementId::SAMP, "samp");
		
		struct SmallElement : public Element
		{
			explicit SmallElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<SmallElement> small_element(ElementId::SMALL, "small");
		
		struct StrongElement : public Element
		{
			explicit StrongElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<StrongElement> strong_element(ElementId::STRONG, "strong");
		
		struct SubscriptElement : public Element
		{
			explicit SubscriptElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<SubscriptElement> subscript_element(ElementId::SUB, "sub");
		
		struct SuperscriptElement : public Element
		{
			explicit SuperscriptElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<SuperscriptElement> superscript_element(ElementId::SUP, "sup");
		
		struct TeleTypeElement : public Element
		{
			explicit TeleTypeElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<TeleTypeElement> teletype_element(ElementId::TT, "tt");
		
		struct VariableElement : public Element
		{
			explicit VariableElement(ElementId id, const std::string& name, WeakDocumentPtr owner) : Element(id, name, owner) {}
		};
		ElementRegistrar<VariableElement> var_element(ElementId::VAR, "var");

	}

	Element::Element(ElementId id, const std::string& name, WeakDocumentPtr owner)
		: Node(NodeId::ELEMENT, owner),
		  name_(name),
		  tag_(id)
	{
	}

	Element::~Element()
	{
	}

	ElementPtr Element::create(const std::string& name, WeakDocumentPtr owner)
	{
		auto it = get_element_registry().find(name);
		if(it == get_element_registry().end()) {
			add_custom_element(name);
			it = get_element_registry().find(name);
			ASSERT_LOG(it != get_element_registry().end(), "Couldn't find factory function for '" << name << "' though one was recently added.");
		}
		return it->second.fn(it->second.id, name, owner);	
	}

	std::string Element::toString() const 
	{
		std::ostringstream ss;
		ss << "Element('" << name_ << "' " << nodeToString();
		for(auto& p : getProperties()) {
			ss << " " << css::get_property_name(p.first);// << ":" << p.second;
		}
		ss << ")";
		return ss.str();
	}

	const std::string& element_id_to_string(ElementId id)
	{
		auto it = get_id_registry().find(id);
		ASSERT_LOG(it != get_id_registry().end(), "Couldn't find an element with id of: " << static_cast<int>(id));
		return it->second;
	}

	void add_custom_element(const std::string& e)
	{
		LOG_INFO("Creating custom element '" << e << "' with id: " << custom_element_counter);

		register_factory_function(static_cast<ElementId>(custom_element_counter), 
			e, 
			[](ElementId id, const std::string& name, WeakDocumentPtr owner) -> ElementPtr { return std::make_shared<CustomElement>(id, name, owner); });

		--custom_element_counter;
	}


	ElementId string_to_element_id(const std::string& e)
	{
		auto it = get_element_registry().find(e);
		if(it == get_element_registry().end()) {
			add_custom_element(e);
			it = get_element_registry().find(e);
		}
		ASSERT_LOG(it != get_element_registry().end(), "No element with type '" << e << "' was found.");
		return it->second.id;
	}

}
