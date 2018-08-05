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

#include <iostream>

#include "asserts.hpp"
#include "formatter.hpp"
#include "formula_tokenizer.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"

#pragma GCC diagnostic ignored "-Wchar-subscripts"

namespace formula_tokenizer
{
	namespace 
	{
		const FFL_TOKEN_TYPE* create_single_char_tokens() {
			static FFL_TOKEN_TYPE chars[256];
			std::fill(chars, chars+256, FFL_TOKEN_TYPE::INVALID);

			chars['('] = FFL_TOKEN_TYPE::LPARENS;
			chars[')'] = FFL_TOKEN_TYPE::RPARENS;
			chars['['] = FFL_TOKEN_TYPE::LSQUARE;
			chars[']'] = FFL_TOKEN_TYPE::RSQUARE;
			chars['{'] = FFL_TOKEN_TYPE::LBRACKET;
			chars['}'] = FFL_TOKEN_TYPE::RBRACKET;
			chars[','] = FFL_TOKEN_TYPE::COMMA;
			chars[';'] = FFL_TOKEN_TYPE::SEMICOLON;
			//   A dot is not necessarily a single char token. Can too
			// be the beginning of a decimal number with a zero valued
			// integer part implicitly omitted (such as in '.9' instead
			// of '0.9'. But very likely is a dot (chain command)
			// operator, or part of an ellipsis.
			chars['.'] = FFL_TOKEN_TYPE::OPERATOR;
			chars['+'] = FFL_TOKEN_TYPE::OPERATOR;
			chars['*'] = FFL_TOKEN_TYPE::OPERATOR;
			chars['/'] = FFL_TOKEN_TYPE::OPERATOR;
			chars['='] = FFL_TOKEN_TYPE::OPERATOR;
			chars['%'] = FFL_TOKEN_TYPE::OPERATOR;
			chars['^'] = FFL_TOKEN_TYPE::OPERATOR;
			chars['|'] = FFL_TOKEN_TYPE::PIPE;
			return chars;
		}

		const FFL_TOKEN_TYPE* single_char_tokens = create_single_char_tokens();
	}

	const std::string UNRECOGNIZED_TOKEN_STR = "Unrecognized token";

	Token get_token(iterator& i1, iterator i2) {
		Token t;
		t.begin = i1;

		if(*i1 == '/' && i1+1 != i2) {
			if(*(i1+1) == '/') {
				//special case for matching a // comment.
				t.type = FFL_TOKEN_TYPE::COMMENT;
				i1 = std::find(i1, i2, '\n');
				t.end = i1;
				return t;
			} else if(*(i1+1) == '*') {
				//special case for matching a /* comment.
				t.type = FFL_TOKEN_TYPE::COMMENT;

				std::string::const_iterator itor = i1;

				itor += 2;

				int nesting = 1;
				while(itor != i2) {
					if(itor+1 != i2) {
						if(*itor == '/' && *(itor+1) == '*') {
							++nesting;
						} else if(*itor == '*' && *(itor+1) == '/') {
							if(--nesting == 0) {
								++itor;
								break;
							}
						}
					}

					++itor;
				}

				if(itor == i2) {
					throw TokenError("Unterminated comment");
				}

				i1 = t.end = itor + 1;
				return t;
			}
		} else if (*i1 == '.' && i1 + 1 != i2) {
			//   Can be ellipsis, but can elsewise be a decimal number
			// with a zero valued implicit omitted integer part. That is
			// consistent with the current tokenization of keywords,
			// word-form operators ('not', 'and', 'or', 'where', 'in',
			// 'asserting' and 'is'), identifiers and constant
			// identifiers enforcing that the first character is an
			// alpha character (not alphanumeric) allowing alphanumeric
			// only after the first position.
			if (util::c_isdigit(*(i1 + 1))) {
				//   Indeed decimal number with a zero valued implicit
				// omitted integer part.
				//   Can be '.2', can be '.70711' (which is the square
				// root of two divided by 2 taken with five decimal
				// digits).
				t.type = FFL_TOKEN_TYPE::DECIMAL;
				//   Finish the parsing of the decimal number.
				++i1;
				while (util::c_isdigit(*i1)) {
					++i1;
				}
				t.end = i1;
				return t;
			}
		}

		if(*i1 == '.' && i1+1 != i2 && *(i1+1) == '.') {
			i1 += 2;
			t.end = i1;
			t.type = FFL_TOKEN_TYPE::ELLIPSIS;
			return t;
		}

		t.type = single_char_tokens[*i1];
		if(t.type != FFL_TOKEN_TYPE::INVALID) {
			t.end = ++i1;
			return t;
		}

		switch(*i1) {
		case '"':
		case '\'':
		case '~':
		case '#': {
			t.type = *i1 == '#' ? FFL_TOKEN_TYPE::COMMENT : FFL_TOKEN_TYPE::STRING_LITERAL;
			std::string::const_iterator end = std::find(i1+1, i2, *i1);
			if(end == i2) {
				throw TokenError("Unterminated string or comment");
			}
			i1 = t.end = end+1;
			return t;
		}
		case 'q':
			if(i1 + 1 != i2 && strchr("~#^({[", *(i1+1))) {
				char end = *(i1+1);
				if(strchr("({[", end)) {
					char open = end;
					char close = ')';
					if(end == '{') close = '}';
					if(end == '[') close = ']';

					int nbracket = 1;

					i1 += 2;
					while(i1 != i2 && nbracket) {
						if(*i1 == open) {
							++nbracket;
						} else if(*i1 == close) {
							--nbracket;
						}

						++i1;
					}

					if(nbracket == 0) {
						t.type = FFL_TOKEN_TYPE::STRING_LITERAL;
						t.end = i1;
						return t;
					}
				} else {
					i1 = std::find(i1+2, i2, end);
					if(i1 != i2) {
						t.type = FFL_TOKEN_TYPE::STRING_LITERAL;
						t.end = ++i1;
						return t;
					}
				}

				throw TokenError("Unterminated q string");
			}
			break;
		case '<':
			if(i1+1 != i2 && *(i1+1) == '-') {
				t.type = FFL_TOKEN_TYPE::LEFT_POINTER;
				i1 += 2;
				t.end = i1;
				return t;
			}

		case '>':
			if(i1+1 != i2 && *(i1+1) == *i1) {
				t.type = *i1 == '<' ? FFL_TOKEN_TYPE::LDUBANGLE : FFL_TOKEN_TYPE::RDUBANGLE;
				i1 += 2;
				t.end = i1;
				return t;
			}

		case '!':
			t.type = FFL_TOKEN_TYPE::OPERATOR;
			++i1;
			if(i1 != i2 && *i1 == '=') {
				++i1;
			} else if(*(i1-1) == '!') {
				throw TokenError("Unexpected character in formula: '!'");
			}

			t.end = i1;
			return t;
		case '-':
			++i1;
			if(i1 != i2 && *i1 == '>') {
				t.type = FFL_TOKEN_TYPE::POINTER;
				++i1;
			} else {
				//   Consider allowing negative numbers?
				//   TODO
				if (false) {  //   TODO Negative number detection?
					//   TODO Negative number parsing?
					//   TODO Returning negative number?
				}
				//   Not currently allowing negative numbers, so the
				// token must unequivocally be a substraction operator.
				t.type = FFL_TOKEN_TYPE::OPERATOR;
			}

			t.end = i1;
			return t;
		case ':':
			++i1;
			if(i1 != i2 && *i1 == ':') {
				t.type = FFL_TOKEN_TYPE::OPERATOR;
				++i1;
			} else {
				t.type = FFL_TOKEN_TYPE::COLON;
			}

			t.end = i1;
			return t;
		case '0':
			if(i1 + 1 != i2 && *(i1+1) == 'x') {
				t.type = FFL_TOKEN_TYPE::INTEGER;
				i1 += 2;
				while(i1 != i2 && util::c_isxdigit(*i1)) {
					++i1;
				}

				t.end = i1;

				return t;
			}

			break;
		case 'd':
			if(i1 + 1 != i2 && !util::c_isalpha(*(i1+1)) && *(i1+1) != '_') {
				//die operator as in 1d6.
				t.type = FFL_TOKEN_TYPE::OPERATOR;
				t.end = ++i1;
				return t;
			}
			break;
		}

		if(util::c_isspace(*i1)) {
			t.type = FFL_TOKEN_TYPE::WHITESPACE;
			while(i1 != i2 && util::c_isspace(*i1)) {
				++i1;
			}

			t.end = i1;
			return t;
		}

		//   Integer numbers and decimal numbers with explicit integer
		// part and NOT headed by a dash glyph marking a negative value.
		if(util::c_isdigit(*i1)) {
			t.type = FFL_TOKEN_TYPE::INTEGER;
			while(i1 != i2 && util::c_isdigit(*i1)) {
				++i1;
			}

			if(i1 != i2 && *i1 == '.' && (i1+1 == i2 || *(i1+1) != '.')) {
				t.type = FFL_TOKEN_TYPE::DECIMAL;

				++i1;
				while(i1 != i2 && util::c_isdigit(*i1)) {
					++i1;
				}
			}

			t.end = i1;
			return t;
		}

		if(util::c_isalpha(*i1) || *i1 == '_') {
			++i1;
			while(i1 != i2 && (util::c_isalnum(*i1) || *i1 == '_')) {
				++i1;
			}

			t.end = i1;

			static const std::string Keywords[] = { "functions", "def", "let", "null", "true", "false", "base", "recursive", "enum" };
			for(const std::string& str : Keywords) {
				if(str.size() == (t.end - t.begin) && std::equal(str.begin(), str.end(), t.begin)) {
					t.type = FFL_TOKEN_TYPE::KEYWORD;
					return t;
				}
			}

			static const std::string Operators[] = { "not", "and", "or", "where", "in", "asserting", "is" };
			for(const std::string& str : Operators) {
				if(str.size() == (t.end - t.begin) && std::equal(str.begin(), str.end(), t.begin)) {
					t.type = FFL_TOKEN_TYPE::OPERATOR;
					return t;
				}
			}

			for(std::string::const_iterator i = t.begin; i != t.end; ++i) {
				if(util::c_islower(*i)) {
					t.type = FFL_TOKEN_TYPE::IDENTIFIER;
					return t;
				}
			}

			t.type = FFL_TOKEN_TYPE::CONST_IDENTIFIER;
			return t;
		}

		throw TokenError(formatter() << UNRECOGNIZED_TOKEN_STR << ": '"
				<< std::string(i1,i2) << "'");
	}

	TokenError::TokenError(const std::string& m) : msg(m)
	{
	}

	TokenMatcher::TokenMatcher()
	{
	}

	TokenMatcher::TokenMatcher(FFL_TOKEN_TYPE type)
	{
		add(type);
	}

	TokenMatcher& TokenMatcher::add(FFL_TOKEN_TYPE type)
	{
		types_.push_back(type);
		return *this;
	}

	TokenMatcher& TokenMatcher::add(const std::string& str)
	{
		str_.push_back(str);
		return *this;
	}

	bool TokenMatcher::match(const Token& t) const
	{
		if(types_.empty() == false && std::find(types_.begin(), types_.end(), t.type) == types_.end()) {
			return false;
		}

		if(str_.empty() == false && std::find(str_.begin(), str_.end(), std::string(t.begin, t.end)) == str_.end()) {
			return false;
		}

		return true;
	}

	bool TokenMatcher::find_match(const Token*& i1, const Token* i2) const
	{
		int nbrackets = 0;
		while(i1 != i2 && (nbrackets > 0 || !match(*i1))) {
			switch(i1->type) {
			case FFL_TOKEN_TYPE::LPARENS:
			case FFL_TOKEN_TYPE::LSQUARE:
			case FFL_TOKEN_TYPE::LBRACKET:
				++nbrackets;
				break;

			case FFL_TOKEN_TYPE::RPARENS:
			case FFL_TOKEN_TYPE::RSQUARE:
			case FFL_TOKEN_TYPE::RBRACKET:
				--nbrackets;
				if(nbrackets < 0) {
					break;
				}
				break;
			default:
				break;
			}

			++i1;
		}

		return i1 != i2 && nbrackets == 0 && match(*i1);
	}
}

UNIT_TEST(tokenizer_test)
{
	using namespace formula_tokenizer;
	std::string test = "q(def)+(abc + 0x4 * (5+3))*2in[4,5,2147483647,3.3,.23,1.0,0-1,0-0.1]";
	std::string::const_iterator i1 = test.begin();
	std::string::const_iterator i2 = test.end();
	FFL_TOKEN_TYPE types[] = {FFL_TOKEN_TYPE::STRING_LITERAL, FFL_TOKEN_TYPE::OPERATOR,
	                      FFL_TOKEN_TYPE::LPARENS, FFL_TOKEN_TYPE::IDENTIFIER,
	                      FFL_TOKEN_TYPE::WHITESPACE, FFL_TOKEN_TYPE::OPERATOR,
						  FFL_TOKEN_TYPE::WHITESPACE, FFL_TOKEN_TYPE::INTEGER,
						  FFL_TOKEN_TYPE::WHITESPACE, FFL_TOKEN_TYPE::OPERATOR,
						  FFL_TOKEN_TYPE::WHITESPACE, FFL_TOKEN_TYPE::LPARENS,
						  FFL_TOKEN_TYPE::INTEGER, FFL_TOKEN_TYPE::OPERATOR,
						  FFL_TOKEN_TYPE::INTEGER, FFL_TOKEN_TYPE::RPARENS,
			FFL_TOKEN_TYPE::RPARENS,  //   ')'
			FFL_TOKEN_TYPE::OPERATOR,  //   '*'
			FFL_TOKEN_TYPE::INTEGER,  //   '2'
			FFL_TOKEN_TYPE::OPERATOR,  //   'in'
			FFL_TOKEN_TYPE::LSQUARE,  //   '['
			FFL_TOKEN_TYPE::INTEGER,  //   '4'
			FFL_TOKEN_TYPE::COMMA,  //   ','
			FFL_TOKEN_TYPE::INTEGER,  //   '5'
			FFL_TOKEN_TYPE::COMMA,  //   ','
			FFL_TOKEN_TYPE::INTEGER,  //   '2147483647'
			FFL_TOKEN_TYPE::COMMA,  //   ','
			FFL_TOKEN_TYPE::DECIMAL,  //   '3.3'
			FFL_TOKEN_TYPE::COMMA,  //   ','
			FFL_TOKEN_TYPE::DECIMAL,  //   '.23'
			FFL_TOKEN_TYPE::COMMA,  //   ','
			FFL_TOKEN_TYPE::DECIMAL,  //   '1.0'
			FFL_TOKEN_TYPE::COMMA,  //   ','
			//   Will be eventually possible remove the heading '0' for
			// a negative number instead of two numbers joined by
			// operator?
			FFL_TOKEN_TYPE::INTEGER,  //   '0'
			FFL_TOKEN_TYPE::OPERATOR,  //   '-'
			FFL_TOKEN_TYPE::INTEGER,  //   '1'
			FFL_TOKEN_TYPE::COMMA,  //   ','
			//   Will be eventually possible remove the heading '0' for
			// a negative number instead of two numbers joined by
			// operator?
			FFL_TOKEN_TYPE::INTEGER,  //   '0'
			FFL_TOKEN_TYPE::OPERATOR,  //   '-'
			FFL_TOKEN_TYPE::DECIMAL,  //   '0.1'
			FFL_TOKEN_TYPE::RSQUARE  //   ']'
	};
	std::string tokens[] = {"q(def)", "+", "(", "abc", " ", "+", " ", "0x4", " ",
	                        "*", " ", "(", "5", "+", "3", ")", ")", "*", "2",
			"in", "[", "4", ",", "5", ",", "2147483647", ",", "3.3", ",",
			".23", ",", "1.0", ",", "0", "-", "1", ",", "0", "-", "0.1", "]"
	};
	for(int n = 0; n != sizeof(types)/sizeof(*types); ++n) {
		Token t = get_token(i1,i2);
		CHECK_EQ(std::string(t.begin,t.end), tokens[n]);
		CHECK_EQ(static_cast<int>(t.type), static_cast<int>(types[n]));

	}
}

UNIT_TEST(tokenization_error_test_0)
{
	const std::string test = "../*..*//* /* /* /**/ */ */ */ :: FOO /*..";
	std::string::const_iterator i1 = test.begin();
	std::string::const_iterator i2 = test.end();
	const formula_tokenizer::FFL_TOKEN_TYPE types[] = {
			formula_tokenizer::FFL_TOKEN_TYPE::ELLIPSIS,
			formula_tokenizer::FFL_TOKEN_TYPE::COMMENT,
			formula_tokenizer::FFL_TOKEN_TYPE::COMMENT,
			formula_tokenizer::FFL_TOKEN_TYPE::WHITESPACE,
			formula_tokenizer::FFL_TOKEN_TYPE::OPERATOR,
			formula_tokenizer::FFL_TOKEN_TYPE::WHITESPACE,
			formula_tokenizer::FFL_TOKEN_TYPE::CONST_IDENTIFIER,
			formula_tokenizer::FFL_TOKEN_TYPE::WHITESPACE };
	const std::string tokens[] = {
			"..", "/*..*/", "/* /* /* /**/ */ */ */", " ", "::",
			" ", "FOO", " " };
	for (int i = 0; i < sizeof(tokens) / sizeof(*tokens); i++) {
		const formula_tokenizer::Token t =
				formula_tokenizer::get_token(i1, i2);
		const std::string t_as_string(t.begin, t.end);
		LOG_INFO(t_as_string);
		CHECK_EQ(t_as_string, tokens[i]);
		CHECK_EQ(static_cast<int>(t.type), static_cast<int>(types[i]));
	}
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			const formula_tokenizer::Token t1 =
					formula_tokenizer::get_token(i1, i2);
		} catch (const formula_tokenizer::TokenError te) {
			excepted = true;
		}
	}
	ASSERT_LOG(excepted, "failed to throw a tokenizer error on being presented an unterminated C-style comment");
}

UNIT_TEST(tokenization_error_test_1)
{
	const std::string test = "blah#blahblah";
	std::string::const_iterator i1 = test.begin();
	std::string::const_iterator i2 = test.end();
	const formula_tokenizer::FFL_TOKEN_TYPE types[] = {
			formula_tokenizer::FFL_TOKEN_TYPE::IDENTIFIER };
	const std::string tokens[] = { "blah" };
		const formula_tokenizer::Token t0 =
		formula_tokenizer::get_token(i1, i2);
	CHECK_EQ(std::string(t0.begin, t0.end), tokens[0]);
	CHECK_EQ(static_cast<int>(t0.type), static_cast<int>(types[0]));
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			const formula_tokenizer::Token t1 =
					formula_tokenizer::get_token(i1, i2);
		} catch (const formula_tokenizer::TokenError te) {
			excepted = true;
		}
	}
	ASSERT_LOG(excepted, "failed to throw a tokenizer error on being presented an unterminated shell-style comment");
}

UNIT_TEST(tokenization_error_test_2)
{
	const std::string test = "blah q(blahblah";
	std::string::const_iterator i1 = test.begin();
	std::string::const_iterator i2 = test.end();
	const formula_tokenizer::FFL_TOKEN_TYPE types[] = {
			formula_tokenizer::FFL_TOKEN_TYPE::IDENTIFIER,
			formula_tokenizer::FFL_TOKEN_TYPE::WHITESPACE };
	const std::string tokens[] = { "blah", " " };
	for (int i = 0; i < sizeof(tokens) / sizeof(*tokens); i++) {
		const formula_tokenizer::Token t =
				formula_tokenizer::get_token(i1, i2);
		LOG_INFO(std::string(t.begin, t.end));
		CHECK_EQ(std::string(t.begin, t.end), tokens[i]);
		CHECK_EQ(static_cast<int>(t.type), static_cast<int>(types[i]));
	}
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			const formula_tokenizer::Token t1 =
					formula_tokenizer::get_token(i1, i2);
		} catch (const formula_tokenizer::TokenError te) {
			excepted = true;
		}
	}
	ASSERT_LOG(excepted, "failed to throw a tokenizer error on being presented an unterminated quote");
}

// XXX   Why is this illegal?
UNIT_TEST(tokenization_error_test_3)
{
	const std::string test = "blah!!blah";
	std::string::const_iterator i1 = test.begin();
	std::string::const_iterator i2 = test.end();
	const formula_tokenizer::FFL_TOKEN_TYPE types[] = {
			formula_tokenizer::FFL_TOKEN_TYPE::IDENTIFIER };
	const std::string tokens[] = { "blah" };
	const formula_tokenizer::Token t0 =
			formula_tokenizer::get_token(i1, i2);
	CHECK_EQ(std::string(t0.begin, t0.end), tokens[0]);
	CHECK_EQ(static_cast<int>(t0.type), static_cast<int>(types[0]));
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			const formula_tokenizer::Token t1 =
					formula_tokenizer::get_token(i1, i2);
		} catch (const formula_tokenizer::TokenError te) {
			excepted = true;
		}
	}
	ASSERT_LOG(excepted, "failed to throw a tokenizer error on being presented a double exclamation mark");
}

UNIT_TEST(tokenization_error_test_4)
{
	const std::string test = "blah$ blah";
	std::string::const_iterator i1 = test.begin();
	std::string::const_iterator i2 = test.end();
	const formula_tokenizer::FFL_TOKEN_TYPE types[] = {
			formula_tokenizer::FFL_TOKEN_TYPE::IDENTIFIER, };
	const std::string tokens[] = { "blah" };
	const formula_tokenizer::Token t0 =
			formula_tokenizer::get_token(i1, i2);
	CHECK_EQ(std::string(t0.begin, t0.end), tokens[0]);
	CHECK_EQ(static_cast<int>(t0.type), static_cast<int>(types[0]));
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			const formula_tokenizer::Token t1 =
					formula_tokenizer::get_token(i1, i2);
		} catch (const formula_tokenizer::TokenError te) {
			ASSERT_LOG(
					te.msg.find(formula_tokenizer::UNRECOGNIZED_TOKEN_STR) !=
							std::string::npos,
					"got unexpected tokenizer error '" << te.msg << '\'');
			excepted = true;
		}
	}
	ASSERT_LOG(excepted, "failed to throw a tokenizer error on being presented an unrecognized token");
}

UNIT_TEST(tokenization_test_1)
{
	const std::string test = " << q >> ";
	std::string::const_iterator i1 = test.begin();
	std::string::const_iterator i2 = test.end();
	const formula_tokenizer::FFL_TOKEN_TYPE types[] = {
			formula_tokenizer::FFL_TOKEN_TYPE::WHITESPACE,
			formula_tokenizer::FFL_TOKEN_TYPE::LDUBANGLE,
			formula_tokenizer::FFL_TOKEN_TYPE::WHITESPACE,
			formula_tokenizer::FFL_TOKEN_TYPE::IDENTIFIER,
			formula_tokenizer::FFL_TOKEN_TYPE::WHITESPACE,
			formula_tokenizer::FFL_TOKEN_TYPE::RDUBANGLE,
			formula_tokenizer::FFL_TOKEN_TYPE::WHITESPACE };
	const std::string tokens[] = { " ", "<<", " ", "q", " ", ">>", " " };
	for (int i = 0; i < sizeof(tokens) / sizeof(*tokens); i++) {
		const formula_tokenizer::Token t =
				formula_tokenizer::get_token(i1, i2);
		LOG_INFO(std::string(t.begin, t.end));
		CHECK_EQ(std::string(t.begin, t.end), tokens[i]);
		CHECK_EQ(static_cast<int>(t.type), static_cast<int>(types[i]));
	}
}

BENCHMARK(tokenizer_bench)
{
	const std::string input =
"	  #function which returns true if the object is in an animation that"
"	   requires frogatto be on the ground#"	
"	  def animation_requires_standing(obj)"
"	    obj.animation in ['stand', 'stand_up_slope', 'stand_down_slope', 'run', 'walk', 'lookup', 'crouch', 'enter_crouch', 'leave_crouch', 'turn', 'roll','skid'];"
"	  def set_facing(obj, facing) if(obj.facing != facing and (not (obj.animation in ['interact', 'slide'])),"
"	           [facing(facing), if(obj.is_standing, animation('turn'))]);"

"	  def stand(obj)"
"	   if(abs(obj.velocity_x) > 240 and (not obj.animation in ['walk']), animation('skid'),"
"	     if(abs(obj.slope_standing_on) < 20, animation('stand'),"
"		   if(obj.slope_standing_on*obj.facing > 0, animation('stand_down_slope'),"
"			                                animation('stand_up_slope'))));"


"	  #make Frogatto walk. anim can be either 'walk' or 'run'. Does checking"
"	   to make sure Frogatto is in a state where he can walk or run."
"	   Will make Frogatto 'glide' if in mid air.#"
"	  def walk(obj, dir, anim)"
"	    if(obj.is_standing and (not (obj.animation in ['walk', 'run', 'jump', 'turn', 'run', 'crouch', 'enter_crouch', 'roll', 'run_attack', 'energyshot', 'attack', 'up_attack', 'interact'])), [animation(anim), if(anim = 'run', [sound('run.ogg')])],"
"	       #Frogatto is in the air, so make him glide.#"
"		   if(((not obj.is_standing) and obj.animation != 'slide'), set(obj.velocity_x, obj.velocity_x + obj.jump_glide*dir)));"

"	  #Function to attempt to make Frogatto crouch; does checking to make"
"	   sure he's in a state that allows entering a crouch.#"
"	  def crouch(obj)"
"	  	if(((not obj.animation in ['crouch', 'enter_crouch', 'roll', 'interact'] ) and obj.is_standing), animation('enter_crouch'));"
"	  def roll(obj)"
"	    if( obj.animation in ['crouch'] and obj.is_standing, animation('roll'));"
"	  def get_charge_cycles(obj)"
"	    if(obj.tmp.start_attack_cycle, obj.cycle - obj.tmp.start_attack_cycle, 0);"
	  
"	  #Function to make Frogatto attack. Does checking and chooses the"
"	   appropriate type of attack animation, if any.#"
"	  def attack(obj, charge_cycles)"
"	  [if('fat' in obj.variations,"
"				[animation('spit')],["
"					if(obj.animation in ['stand', 'stand_up_slope', 'stand_down_slope', 'walk', 'lookup','skid'], animation(if(obj.ctrl_up, 'up_', '') + if(charge_cycles >= obj.vars.charge_time, 'energyshot', 'attack'))),"
					
"					if(obj.animation in ['run'], animation('run_attack')),"
					
"					if(obj.animation in ['jump', 'fall'], animation(if(charge_cycles >= obj.vars.charge_time,'energyshot' + if(obj.ctrl_down,'_down','_jump'),  if(obj.ctrl_down, 'fall_spin_attack', 'jump_attack' )))),"
					
"					if(obj.animation in ['crouch'] and (charge_cycles > obj.vars.charge_time), animation('energyshot_crouch'))]"
				
"	    )];";

	BENCHMARK_LOOP {
		std::string::const_iterator i1 = input.begin();
		std::string::const_iterator i2 = input.end();
		while(i1 != i2) {
			formula_tokenizer::get_token(i1, i2);
		}
	}
}
