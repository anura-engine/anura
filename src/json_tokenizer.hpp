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

namespace json 
{
	struct TokenizerError 
	{
		const char* msg;
		const char* loc;
	};

	struct Token 
	{
		Token() : type(TYPE::NUM_TYPES), begin(0), end(0), translate(false) {}
		enum class TYPE { 
			NUMBER, 
			STRING, 
			LCURLY, 
			RCURLY,
			LSQUARE, 
			RSQUARE, 
			COMMA, 
			COLON,
			TRUE_VALUE, 
			FALSE_VALUE, 
			NULL_VALUE,
			IDENTIFIER,
			NUM_TYPES 
		};
		TYPE type;
		const char* begin, *end;
		bool translate;
	};

	Token get_token(const char*& i1, const char* i2);

	//Gets the full token, unlike get_token which will e.g. return the
	//characters inside the string.
	Token get_token_full(const char*& i1, const char* i2);
}
