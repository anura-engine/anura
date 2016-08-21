/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

namespace formula_tokenizer
{
	typedef std::string::const_iterator iterator;

	enum class FFL_TOKEN_TYPE { 
		OPERATOR, 
		STRING_LITERAL,
		CONST_IDENTIFIER,
		IDENTIFIER, 
		INTEGER, 
		DECIMAL,
		LPARENS, 
		RPARENS,
		LSQUARE, 
		RSQUARE, 
		LBRACKET, 
		RBRACKET,
		LDUBANGLE, 
		RDUBANGLE,
		COMMA, 
		SEMICOLON, 
		COLON, 
		WHITESPACE, 
		KEYWORD,
		COMMENT, 
		POINTER, 
		LEFT_POINTER,
		PIPE, 
		ELLIPSIS,
		INVALID
	};

	inline FFL_TOKEN_TYPE operator-(FFL_TOKEN_TYPE type, int n) {
		return static_cast<FFL_TOKEN_TYPE>(static_cast<int>(type)-n);
	}

	struct Token 
	{
		FFL_TOKEN_TYPE type;
		iterator begin, end;
		std::string str() const { return std::string(begin,end); }

		bool equals(const char* s) const { return end - begin == strlen(s) && std::equal(begin, end, s); }
	};

	Token get_token(iterator& i1, iterator i2);

	struct TokenError 
	{
		TokenError(const std::string& m);
		std::string msg;
	};

	//A special interface for searching for and matching tokens.
	class TokenMatcher 
	{
	public:
		TokenMatcher();
		explicit TokenMatcher(FFL_TOKEN_TYPE type);
		TokenMatcher& add(FFL_TOKEN_TYPE type);
		TokenMatcher& add(const std::string& str);

		bool match(const Token& t) const;

		//Find the first matching token within the given range and return it.
		//Does not return tokens that are inside any kinds of brackets.
		bool find_match(const Token*& i1, const Token* i2) const;
	private:
		std::vector<FFL_TOKEN_TYPE> types_;
		std::vector<std::string> str_;
	};
}
