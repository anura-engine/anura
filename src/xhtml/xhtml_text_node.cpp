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

#include <boost/locale.hpp>
#include <boost/thread.hpp>

#include "asserts.hpp"
#include "profile_timer.hpp"
#include "xhtml_style_tree.hpp"
#include "xhtml_text_node.hpp"
#include "utf8_to_codepoint.hpp"
#include "unit_test.hpp"

#include "WindowManager.hpp"

#pragma comment(lib, "icudt.lib")
#pragma comment(lib, "icuin.lib")
#pragma comment(lib, "icuuc.lib")

namespace xhtml
{
	namespace
	{
		const int fixed_point_scale = 65536;

		struct TextImpl : public Text
		{
			TextImpl(const std::string& txt, WeakDocumentPtr owner) : Text(txt, owner) {}
		};

		bool is_white_space(char32_t cp) {  return cp == '\r' || cp == '\t' || cp == ' ' || cp == '\n'; }

		void tokenize_text(const std::string& text, bool collapse_ws, bool break_at_newline, Line& res) 
		{
			bool in_ws = false;
			for(auto cp : utils::utf8_to_codepoint(text)) {
				if(cp == '\n' && break_at_newline) {
					if(res.line.empty() || !res.line.back().word.empty()) {
						res.line.emplace_back(std::string(1, '\n'));
						res.line.emplace_back(std::string());
					} else {
						res.line.back().word = "\n";
						res.line.emplace_back(std::string());
					}
					continue;
				}

				if(is_white_space(cp) && collapse_ws) {
					in_ws = true;
				} else {
					if(in_ws) {
						in_ws = false;
						if(!res.line.empty() && !res.line.back().word.empty()) {
							res.line.emplace_back(std::string());
						}
					}					
					if(res.line.empty()) {
						res.line.emplace_back(std::string());
					}
					res.line.back().word += utils::codepoint_to_utf8(cp);
				}
			}
		}

	}

	Text::Text(const std::string& txt, WeakDocumentPtr owner)
		: Node(NodeId::TEXT, owner),
		  transformed_(false),
		  text_(txt),
		  break_at_line_(false)
	{
	}

	TextPtr Text::create(const std::string& txt, WeakDocumentPtr owner)
	{
		return std::make_shared<TextImpl>(txt, owner);
	}

	std::string Text::toString() const 
	{
		std::ostringstream ss;
		ss << "Text('" << text_ << "' " << nodeToString() << ")";
		return ss.str();
	}

	void Text::transformText(const StyleNodePtr& style_node, bool non_zero_width)
	{
		if(transformed_) {
			return;
		}

		boost::locale::generator gen;
		std::locale::global(gen(""));

		// Apply transform text_ based on "text-transform" property		
		
		css::TextTransform text_transform = style_node->getTextTransform();
		std::string transformed_text = text_;
		switch(text_transform) {
			case css::TextTransform::CAPITALIZE: {
				bool first_letter = true;
				transformed_text.clear();
				for(auto cp : utils::utf8_to_codepoint(text_)) {
					if(is_white_space(cp)) {
						first_letter = true;
						transformed_text += utils::codepoint_to_utf8(cp);
					} else {
						if(first_letter) {
							first_letter = false;
							transformed_text += boost::locale::to_upper(utils::codepoint_to_utf8(cp));
						} else {
							transformed_text += utils::codepoint_to_utf8(cp);
						}
					}
				}
				break;
			}
			case css::TextTransform::UPPERCASE:
				transformed_text = boost::locale::to_upper(text_);
				break;
			case css::TextTransform::LOWERCASE:
				transformed_text = boost::locale::to_lower(text_);
				break;
			case css::TextTransform::NONE:
			default: break;
		}

		css::Whitespace ws = style_node->getWhitespace();

		// indicates whitespace should be collapsed together.
		bool collapse_whitespace = ws == css::Whitespace::NORMAL || ws == css::Whitespace::NOWRAP || ws == css::Whitespace::PRE_LINE;
		// indicates we should break at the boxes line width
		break_at_line_ = non_zero_width &&
			(ws == css::Whitespace::NORMAL || ws == css::Whitespace::PRE_LINE || ws == css::Whitespace::PRE_WRAP);
		// indicates we should break on newline characters.
		bool break_at_newline = ws == css::Whitespace::PRE || ws == css::Whitespace::PRE_LINE || ws == css::Whitespace::PRE_WRAP;

		// Apply letter-spacing and word-spacing here.
		xhtml::tokenize_text(transformed_text, collapse_whitespace, break_at_newline, line_);

		transformed_ = true;
	}

	LinePtr Text::reflowText(iterator& start, FixedPoint remaining_line_width, const StyleNodePtr& style_node)
	{
		auto parent = getParent();
		ASSERT_LOG(parent != nullptr, "Text::reflowText() parent was null.");
		ASSERT_LOG(transformed_ == true, "Text must be transformed before reflowing.");
		auto ctx = RenderContext::get();

		line_.space_advance = style_node->getFont()->calculateCharAdvance(' ');
		FixedPoint word_spacing = style_node->getWordSpacing()->compute();
		line_.space_advance += word_spacing;
		FixedPoint letter_spacing = style_node->getLetterSpacing()->compute();
		line_.space_advance += letter_spacing;
		css::Direction dir = style_node->getDirection();

		// XXX padding-left is applied to the start of the first word
		// and padding-right is applied to the end of the last word.
		// padding-top/padding-bottom effect the way the background is drawn but
		// do not effect the line-height.
		// margins have no effect.
		// border-left only applies to the start of the line
		// border-top/border-bottom are drawn, but don't effect line height
		// border-right effects the end of the last line.

		LinePtr current_line = std::make_shared<Line>();
		current_line->space_advance = line_.space_advance;

		// accumulator for current line lenth
		FixedPoint length_acc = 0;

		for(; start != end(); ++start) {
			auto& word = *start;
			// "\n" by itself in the word stream indicates a forced line break.
			if(word.word == "\n") {
				if(length_acc != 0) {
					current_line->is_end_line = true;
					return current_line;
				}
				continue;
			}
			word.advance.clear();
			word.advance = style_node->getFont()->getGlyphPath(word.word);
			if(letter_spacing != 0) {
				long ls_acc = 0;
				for(auto& pt : word.advance) {
					pt.x += ls_acc;
					ls_acc += letter_spacing;
				}
			}
			if(break_at_line_ && length_acc + word.advance.back().x + line_.space_advance > remaining_line_width) {
				// Enforce a minimum of one-word per line even if it overflows.
				if(current_line->line.empty() && !word.word.empty()) {
					current_line->line.emplace_back(word);
					++start;
				}

				current_line->is_end_line = true;
				return current_line;
			} else {
				length_acc += word.advance.back().x + line_.space_advance;
				current_line->line.emplace_back(word);
			}
		}

		// XXX Do we need to add a catch here so that if the last line width + space_advance > maximum_line_width
		// then we set is_end_line=true?

		return current_line;
	}
}

std::ostream& operator<<(std::ostream& os, const xhtml::Line& line) 
{
	for(auto& word : line.line) {
		os << word.word << " ";
	}
	return os;
}

bool operator==(const xhtml::Line& lhs, const xhtml::Line& rhs)
{
	if(lhs.line.size() != rhs.line.size()) {
		return false;
	}
	for(int n = 0; n != lhs.line.size(); ++n) {
		if(lhs.line[n].word != rhs.line[n].word) {
			return false;
		}
	}
	return true;
}

UNIT_TEST(text_tokenize)
{
	/*auto res = xhtml::tokenize_text("This \t\nis \t a \ntest \t", true, false);
	CHECK(res == xhtml::Line({xhtml::Word("This"), xhtml::Word("is"), xhtml::Word("a"), xhtml::Word("test")}), "collapse white-space test failed.");
	
	res = xhtml::tokenize_text("This \t\nis \t a \ntest \t", true, true);
	CHECK(res == xhtml::Line({xhtml::Word("This"), xhtml::Word("\n"), xhtml::Word("is"), xhtml::Word("a"), xhtml::Word("\n"), xhtml::Word("test")}), "collapse white-space+break at newline test failed.");
	
	res = xhtml::tokenize_text("This \t\nis \t a \ntest", false, false);	
	CHECK(res == xhtml::Line({xhtml::Word("This \t\nis \t a \ntest")}), "no collapse, no break at newline test failed.");
	
	res = xhtml::tokenize_text("This \t\nis \t a \ntest \t", false, true);
	CHECK(res == xhtml::Line({xhtml::Word("This \t"), xhtml::Word("\n"), xhtml::Word("is \t a "), xhtml::Word("\n"), xhtml::Word("test \t")}), "no collapse, break at newline test failed.");

	res = xhtml::tokenize_text("Lorem \n\t\n\tipsum", true, true);
	CHECK(res == xhtml::Line({xhtml::Word("Lorem"), xhtml::Word("\n"), xhtml::Word("\n"), xhtml::Word("ipsum")}), "collapse white-space test failed.");*/
}

