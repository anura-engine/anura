/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#ifndef FORMULA_TOKENIZER_HPP_INCLUDED
#define FORMULA_TOKENIZER_HPP_INCLUDED

#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

namespace formula_tokenizer
{

typedef std::string::const_iterator iterator;

enum FFL_TOKEN_TYPE { TOKEN_OPERATOR, TOKEN_STRING_LITERAL,
                  TOKEN_CONST_IDENTIFIER,
		          TOKEN_IDENTIFIER, TOKEN_INTEGER, TOKEN_DECIMAL,
                  TOKEN_LPARENS, TOKEN_RPARENS,
				  TOKEN_LSQUARE, TOKEN_RSQUARE, 
				  TOKEN_LBRACKET, TOKEN_RBRACKET,
				  TOKEN_LDUBANGLE, TOKEN_RDUBANGLE,
				  TOKEN_COMMA, TOKEN_SEMICOLON, TOKEN_COLON, 
				  TOKEN_WHITESPACE, TOKEN_KEYWORD,
				  TOKEN_COMMENT, TOKEN_POINTER, TOKEN_LEFT_POINTER,
				  TOKEN_PIPE, TOKEN_ELLIPSIS,
				  TOKEN_INVALID  };

struct token {
	FFL_TOKEN_TYPE type;
	iterator begin, end;

	bool equals(const char* s) const { return end - begin == strlen(s) && std::equal(begin, end, s); }
};

token get_token(iterator& i1, iterator i2);

struct token_error {
	token_error(const std::string& m);
	std::string msg;
};

//A special interface for searching for and matching tokens.
class token_matcher {
public:
	token_matcher();
	explicit token_matcher(FFL_TOKEN_TYPE type);
	token_matcher& add(FFL_TOKEN_TYPE type);
	token_matcher& add(const std::string& str);

	bool match(const token& t) const;

	//Find the first matching token within the given range and return it.
	//Does not return tokens that are inside any kinds of brackets.
	bool find_match(const token*& i1, const token* i2) const;
private:
	std::vector<FFL_TOKEN_TYPE> types_;
	std::vector<std::string> str_;
};

}

#endif
