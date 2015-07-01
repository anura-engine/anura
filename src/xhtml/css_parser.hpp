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

#include "css_lexer.hpp"
#include "css_properties.hpp"
#include "css_stylesheet.hpp"

namespace css
{
	struct ParserError : public std::runtime_error
	{
		ParserError(const char* msg) : std::runtime_error(msg) {}
		ParserError(const std::string& str) : std::runtime_error(str) {}
	};

	class Parser
	{
	public:
		static void parse(StyleSheetPtr ss, const std::string& str);
		static StylePtr parseSingleDeclaration(const std::string& str);
		static PropertyList parseDeclarationList(const std::string& str);

		const StyleSheetPtr& getStyleSheet() const { return style_sheet_; }
		const std::vector<TokenPtr>& getTokens() const { return tokens_; }
	private:
		Parser(StyleSheetPtr ss, const std::vector<TokenPtr>& tokens);
		void init();
		std::vector<TokenPtr> pasrseRuleList(int level);
		TokenPtr parseAtRule();
		TokenPtr parseQualifiedRule();
		TokenPtr parseComponentValue();
		std::vector<TokenPtr> parseBraceBlock();
		std::vector<TokenPtr> parseParenBlock();
		std::vector<TokenPtr> parseBracketBlock();
		TokenPtr parseFunction();
		void parseRule(TokenPtr);

		TokenId currentTokenType();
		void advance(int n=1);

		StyleSheetPtr style_sheet_;
		std::vector<TokenPtr> tokens_;
		std::vector<TokenPtr>::const_iterator token_;
		std::vector<TokenPtr>::const_iterator end_;
	};
}