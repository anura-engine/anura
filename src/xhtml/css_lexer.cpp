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

#include <sstream>

#include <boost/lexical_cast.hpp>

#include "asserts.hpp"
#include "css_lexer.hpp"
#include "formatter.hpp"
#include "utf8_to_codepoint.hpp"
#include "variant_utils.hpp"

namespace css
{
	namespace 
	{
		const char32_t NULL_CP = 0x0000;
		const char32_t CR = 0x000d;
		const char32_t LF = 0x000a;
		const char32_t FF = 0x000c;
		const char32_t TAB = 0x0009;
		const char32_t REPLACEMENT_CHAR = 0xfffdUL;
		const char32_t SPACE = 0x0020;
		const char32_t MAX_CODEPOINT = 0x10ffff;

		bool between(char32_t num, char32_t first, char32_t last) { return num >= first && num <= last; }
		bool digit(char32_t code) { return between(code, 0x30, 0x39); }
		bool hexdigit(char32_t code) { return digit(code) || between(code, 0x41, 0x46) || between(code, 0x61, 0x66); }
		bool newline(char32_t code) { return code == LF; }
		bool whitespace(char32_t code) { return newline(code) || code == TAB || code == SPACE; }
		bool uppercaseletter(char32_t code) { return between(code, 0x41,0x5a); }
		bool lowercaseletter(char32_t code) { return between(code, 0x61,0x7a); }
		bool letter(char32_t code) { return uppercaseletter(code) || lowercaseletter(code); }
		bool nonascii(char32_t code) { return code >= 0x80; }
		bool namestartchar(char32_t code) { return letter(code) || nonascii(code) || code == 0x5f; }
		bool namechar(char32_t code) { return namestartchar(code) || digit(code) || code == 0x2d; }
		bool nonprintable(char32_t code) { return between(code, 0,8) || code == 0xb || between(code, 0xe,0x1f) || code == 0x7f; }

		bool is_valid_escape(char32_t cp1, char32_t cp2) {
			if(cp1 != '\\') {
				return false;
			}
			return newline(cp2) ? false : true;
		}

		bool woud_start_an_identifier(char32_t cp1, char32_t cp2, char32_t cp3) {
			if(cp1 == '-') {
				return namestartchar(cp2) || cp2 == '-' || is_valid_escape(cp2, cp3);
			} else if(namestartchar(cp1)) {
				return true;
			} else if(cp1 == '\\') {
				return is_valid_escape(cp1, cp2);
			}
			return false;
		}

		bool would_start_a_number(char32_t cp1, char32_t cp2, char32_t cp3) {
			if(cp1 == '+' || cp1 == '-') {
				return digit(cp2) || (cp2 == '.' && digit(cp3));
			} else if(cp1 == '.') {
				return digit(cp2);
			} 
			return digit(cp1);
		}
		
		class StringToken : public Token
		{
		public:
			explicit StringToken(const std::string& str) : Token(TokenId::STRING), str_(str) {}
			std::string toString() const override {
				return formatter() << "StringToken(" << str_ << ")";
			}
			variant value() override { return variant(str_); }
			std::string getStringValue() const override { return str_; }
		private:
			std::string str_;
		};

		class AtToken : public Token
		{
		public:
			explicit AtToken(const std::string& ident) : Token(TokenId::AT), ident_(ident) {}
			std::string toString() const override {
				return formatter() << "AtToken(" << ident_ << ")";
			}
			variant value() override { return variant(ident_); }
			std::string getStringValue() const override { return ident_; }
		private:
			std::string ident_;
		};

		class NumberToken : public Token
		{
		public:
			explicit NumberToken(double value) : Token(TokenId::NUMBER), value_(value) {}
			std::string toString() const override {
				return formatter() << "NumberToken(" << value_ << ")";
			}
			variant value() override { return variant(value_); }
			double getNumericValue() const override { return value_; }
		private:
			double value_;
		};

		class DimensionToken : public Token
		{
		public:
			explicit DimensionToken(double value, const std::string& units) : Token(TokenId::DIMENSION), value_(value), units_(units) {}
			std::string toString() const override {
				return formatter() << "DimensionToken(" << value_ << " " << units_ << ")";
			}
			variant value() override { 
				variant_builder res;
				res.add("value", value_);
				res.add("units", units_);
				return res.build();
			}
			double getNumericValue() const override { return value_; }
			std::string getStringValue() const override { return units_; }
		private:
			double value_;
			std::string units_;
		};

		class IdentToken : public Token
		{
		public:
			explicit IdentToken(const std::string& ident) : Token(TokenId::IDENT), ident_(ident) {}
			std::string toString() const override {
				return formatter() << "IdentToken(" << ident_ << ")";
			}
			variant value() override { return variant(ident_); }
			std::string getStringValue() const override { return ident_; }
		private:
			std::string ident_;
		};

		class UrlToken : public Token
		{
		public:
			UrlToken() : Token(TokenId::URL), url_() {}
			explicit UrlToken(const std::string& url) : Token(TokenId::URL), url_(url) {}
			std::string toString() const override {
				return formatter() << "UrlToken(" << url_<< ")";
			}
			variant value() override { return variant(url_); }
			std::string getStringValue() const override { return url_; }
		private:
			std::string url_;
		};

		class FunctionToken : public Token
		{
		public:
			explicit FunctionToken(const std::string& fn) : Token(TokenId::FUNCTION), fn_(fn) {}
			std::string toString() const override {
				return formatter() << "FunctionToken(" << fn_ << ")";
			}
			variant value() override { return variant(fn_); }
			std::string getStringValue() const override { return fn_; }
		private:
			std::string fn_;
		};

		class PercentToken : public Token
		{
		public:
			explicit PercentToken(double value) : Token(TokenId::PERCENT), value_(value) {}
			std::string toString() const override {
				return formatter() << "PercentToken(" << value_ << "%%)";
			}
			variant value() override { return variant(value_); }
			double getNumericValue() const override { return value_; }
		private:
			double value_;
		};
		
		class DelimiterToken : public Token
		{
		public:
			explicit DelimiterToken(const std::string& delim) : Token(TokenId::DELIM), delim_(delim) {}
			std::string toString() const override {
				return formatter() << "DelimiterToken(" << delim_ << ")";
			}
			variant value() override { return variant(delim_); }
			std::string getStringValue() const override { return delim_; }
		private:
			std::string delim_;
		};

		class HashToken : public Token
		{
		public:
			explicit HashToken(bool restricted, const std::string& name) : Token(TokenId::HASH), name_(name), unrestricted_(!restricted) {}
			std::string toString() const override {
				return formatter() << "HashToken(" << (unrestricted_ ? "unrestricted " : "id ") << name_ << ")";
			}
			variant value() override { 
				variant_builder res;
				res.add("name", name_);
				res.add("unrestricted", unrestricted_);
				return res.build();
			}
			std::string getStringValue() const override { return name_; }
		private:
			std::string name_;
			bool unrestricted_;
		};
	}

	Tokenizer::Tokenizer(const std::string& inp)
		: cp_string_(),
		  it_(0),
		  la0_(-1),
		  tokens_()
	{
		// Replace CR, FF and CR/LF pairs with LF
		// Replace U+0000 with U+FFFD
		bool is_lf = false;
		for(auto ch : utils::utf8_to_codepoint(inp)) {
			switch(ch) {
				case NULL_CP: 
					if(is_lf) {
						is_lf = false;
						cp_string_.emplace_back(LF);
					}
					cp_string_.emplace_back(REPLACEMENT_CHAR); 
					break;
				case CR: // fallthrough
				case LF:
				case FF: is_lf = true; break;
				default: 
					if(is_lf) {
						is_lf = false;
						cp_string_.emplace_back(LF);
					}
					cp_string_.emplace_back(ch);
					break;
			}
		}

		{
			std::stringstream ss;
			for(auto ch : cp_string_) {
				ss << static_cast<char>(ch);
			}
			//LOG_DEBUG("cp_string: " << ss.str());
		}
		
		if(cp_string_.empty()) {
			return;
		}

		// tokenize string.
		it_ = 0;
		la0_ = cp_string_[0];
		while(it_ < cp_string_.size()) {
			if(la0_ == '/' && next() == '*') {
				consumeComments();
			}
			if(la0_ == LF || la0_ == TAB || la0_ == SPACE) {
				consumeWhitespace();
				tokens_.emplace_back(std::make_shared<Token>(TokenId::WHITESPACE));
			} else if(la0_ == '"') {
				tokens_.emplace_back(consumeString(la0_));
			} else if(la0_ == '#') {
				if(namechar(next()) || is_valid_escape(next(1), next(2))) {
					advance();
					tokens_.emplace_back(std::make_shared<HashToken>(woud_start_an_identifier(next(1), next(2), next(3)), consumeName()));
				} else {
					tokens_.emplace_back(std::make_shared<DelimiterToken>(utils::codepoint_to_utf8(la0_)));
					advance();
				}
			} else if(la0_ == '$') {
				if(next() == '=') {
					advance(2);
					tokens_.emplace_back(std::make_shared<Token>(TokenId::SUFFIX_MATCH));
				} else {
					tokens_.emplace_back(std::make_shared<DelimiterToken>(utils::codepoint_to_utf8(la0_)));
					advance();
				}
			} else if(la0_ == '\'') {
				tokens_.emplace_back(consumeString(la0_));
			} else if(la0_ == '(') {
				advance();
				tokens_.emplace_back(std::make_shared<Token>(TokenId::LPAREN));
			} else if(la0_ == ')') {
				advance();
				tokens_.emplace_back(std::make_shared<Token>(TokenId::RPAREN));
			} else if(la0_ == '*') {
				if(next() == '=') {
					advance(2);
					tokens_.emplace_back(std::make_shared<Token>(TokenId::SUBSTRING_MATCH));
				} else {
					tokens_.emplace_back(std::make_shared<DelimiterToken>(utils::codepoint_to_utf8(la0_)));
					advance();
				}
			} else if(la0_ == '+') {
				if(would_start_a_number(la0_, next(1), next(2))) {
					tokens_.emplace_back(consumeNumericToken());
				} else {
					tokens_.emplace_back(std::make_shared<DelimiterToken>(utils::codepoint_to_utf8(la0_)));
					advance();
				}
			} else if(la0_ == ',') {
				advance();
				tokens_.emplace_back(std::make_shared<Token>(TokenId::COMMA));
			} else if(la0_ == '-') {
				if(would_start_a_number(la0_, next(1), next(2))) {
					tokens_.emplace_back(consumeNumericToken());
				} else if(next(1) == '-' && next(2) == '>') {
					tokens_.emplace_back(std::make_shared<Token>(TokenId::CDC));
					advance(3);
				} else if(woud_start_an_identifier(la0_, next(1), next(2))) {
					tokens_.emplace_back(consumeIdentlikeToken());
				} else {
					tokens_.emplace_back(std::make_shared<DelimiterToken>(utils::codepoint_to_utf8(la0_)));
					advance();
				}
			} else if(la0_ == '.') {
				if(would_start_a_number(la0_, next(1), next(2))) {
					tokens_.emplace_back(consumeNumericToken());
				} else {
					advance();
					tokens_.emplace_back(std::make_shared<DelimiterToken>("."));
				}
			} else if(la0_ == ':') {
				advance();
				tokens_.emplace_back(std::make_shared<Token>(TokenId::COLON));
			} else if(la0_ == ';') {
				advance();
				tokens_.emplace_back(std::make_shared<Token>(TokenId::SEMICOLON));
			} else if(la0_ == '<') {
				if(next(1) == '!' && next(2) == '-' && next(3) == '-') {
					tokens_.emplace_back(std::make_shared<Token>(TokenId::CDO));
				} else {
					advance();
					tokens_.emplace_back(std::make_shared<DelimiterToken>("<"));
				}
			} else if(la0_ == '@') {
				if(woud_start_an_identifier(next(1), next(2), next(3))) {
					advance();
					tokens_.emplace_back(std::make_shared<AtToken>(consumeName()));
				} else {
					advance();
					tokens_.emplace_back(std::make_shared<DelimiterToken>("@"));
				}
			} else if(la0_ == '[') {
				advance();
				tokens_.emplace_back(std::make_shared<Token>(TokenId::LBRACKET));
			} else if(la0_ == '\\') {
				if(is_valid_escape(la0_, next(1))) {
					tokens_.emplace_back(consumeIdentlikeToken());
				} else {
					LOG_ERROR("Parse error while processing codepoint: " << utils::codepoint_to_utf8(la0_));
					tokens_.emplace_back(std::make_shared<DelimiterToken>("\\"));
				}
			} else if(la0_ == ']') {
				advance();
				tokens_.emplace_back(std::make_shared<Token>(TokenId::RBRACKET));
			} else if(la0_ == '^') {
				if(next() == '=') {
					tokens_.emplace_back(std::make_shared<Token>(TokenId::PREFIX_MATCH));
					advance(2);
				} else {
					tokens_.emplace_back(std::make_shared<DelimiterToken>("^"));
					advance();
				}
			} else if(la0_ == '{') {
				advance();
				tokens_.emplace_back(std::make_shared<Token>(TokenId::LBRACE));
			} else if(la0_ == '}') {
				advance();
				tokens_.emplace_back(std::make_shared<Token>(TokenId::RBRACE));
			} else if(digit(la0_)) {
				tokens_.emplace_back(consumeNumericToken());
			} else if(namestartchar(la0_)) {
				tokens_.emplace_back(consumeIdentlikeToken());
			} else if(la0_ == '|') {
				if(next() == '=') {
					tokens_.emplace_back(std::make_shared<Token>(TokenId::DASH_MATCH));
					advance(2);
				} else if(next() == '|') {
					tokens_.emplace_back(std::make_shared<Token>(TokenId::COLUMN));
					advance(2);
				} else {
					tokens_.emplace_back(std::make_shared<DelimiterToken>("|"));
					advance();
				}
			} else if(la0_ == '~') {
				if(next() == '=') {
					tokens_.emplace_back(std::make_shared<Token>(TokenId::INCLUDE_MATCH));
					advance(2);
				} else {
					tokens_.emplace_back(std::make_shared<DelimiterToken>("^"));
					advance();
				}
			} else if(eof(la0_)) {
				tokens_.emplace_back(std::make_shared<Token>(TokenId::EOF_TOKEN));
			} else {
				tokens_.emplace_back(std::make_shared<DelimiterToken>(utils::codepoint_to_utf8(la0_)));
				advance();
			}
		}
		// deubgging to print list of tokens.
		//LOG_DEBUG("Token list: ");
		//for(auto& tok : tokens_) {
		//	LOG_DEBUG("    " << tok->toString());
		//}
	}

	void Tokenizer::advance(int n)
	{
		it_ += n;
		if(it_ >= cp_string_.size()) {
			//throw TokenizerError("EOF error");
			la0_ = -1;
		} else {
			la0_ = cp_string_[it_];
		}
	}

	bool Tokenizer::eof(char32_t cp)
	{
		return cp == -1;
	}

	char32_t Tokenizer::next(int n)
	{
		if(n > 3) {
			throw TokenizerError("Out of spec error, no more than three codepoints of lookahead");
		}
		if(it_ + n >= cp_string_.size()) {
			return -1;
		}
		return cp_string_[it_ + n];
	}

	void Tokenizer::consumeWhitespace()
	{
		// consume LF/TAB/SPACE
		while(la0_ == LF || la0_ == TAB || la0_ == SPACE) {
			advance();
		}
	}

	void Tokenizer::consumeComments()
	{
		advance(2);
		while(it_ < cp_string_.size()-2) {
			if(la0_ == '*' && next() == '/') {
				return;
			}
			advance();
		}
		throw TokenizerError("EOF in comments");
	}

	TokenPtr Tokenizer::consumeString(char32_t end_codepoint)
	{
		std::string res;
		advance();
		while(la0_ != end_codepoint) {
			if(la0_ == LF) {
				return std::make_shared<Token>(TokenId::BAD_STRING);
			} else if(eof(la0_)) {
				return std::make_shared<StringToken>(res);
			} else if(la0_ == '\\') {
				if(eof(next())) {
					// does nothing.
				} else if(next() == LF) {
					advance();
					continue;
				} else {
					res += consumeEscape();
				}
			}
			res += utils::codepoint_to_utf8(la0_);
			advance();
		}
		advance();
		return std::make_shared<StringToken>(res);
	}

	std::string Tokenizer::consumeEscape()
	{
		std::string res;
		advance();
		if(hexdigit(la0_)) {
			std::string digits;
			digits.push_back(static_cast<char>(la0_));
			for(int n = 1; n < 6 && hexdigit(next()); ++n) {
				advance();
				digits.push_back(static_cast<char>(la0_));
			}
			if(whitespace(next())) {
				advance();
			}
			int value;
			std::stringstream ss;
			ss << std::hex << digits;
			ss >> value;
			if(value >= MAX_CODEPOINT) {
				value = REPLACEMENT_CHAR;
			}
			res = utils::codepoint_to_utf8(value);
		} else if(eof(la0_)) {
			res = utils::codepoint_to_utf8(REPLACEMENT_CHAR);
		} else {
			res = utils::codepoint_to_utf8(la0_);
		}
		return res;
	}

	std::string Tokenizer::consumeName()
	{
		std::string res;
		while(true) {
			if (eof(la0_)) {
				return res;
			} else if(namechar(la0_)) {
				res += utils::codepoint_to_utf8(la0_);
				advance();
			} else if (is_valid_escape(la0_, next())) {
				res += consumeEscape();
				advance();
			} else {
				return res;
			}
		}
		return res;
	}

	TokenPtr Tokenizer::consumeNumericToken()
	{
		std::string res;
		auto num = consumeNumber();
		if(woud_start_an_identifier(la0_, next(1), next(2))) {
			return std::make_shared<DimensionToken>(num, consumeName());
		} else if(la0_ == '%') {
			advance();
			return std::make_shared<PercentToken>(num);
		}
		return std::make_shared<NumberToken>(num);
	}

	double Tokenizer::consumeNumber()
	{
		std::string res;
		if(la0_ == '-' || la0_ == '+') {
			res += utils::codepoint_to_utf8(la0_);
			advance();
		}
		while(digit(la0_)) {
			res += utils::codepoint_to_utf8(la0_);
			advance();
		}
		if(la0_ == '.' && digit(next(1))) {
			res += utils::codepoint_to_utf8(la0_);
			advance();
			while(digit(la0_)) {
				res += utils::codepoint_to_utf8(la0_);
				advance();
			}
		}
		if((la0_ == 'e' || la0_ == 'E') && digit(next(1))) {
			res += utils::codepoint_to_utf8(la0_);
			advance();
			res += utils::codepoint_to_utf8(la0_);
			advance();
			while(digit(la0_)) {
				res += utils::codepoint_to_utf8(la0_);
				advance();
			}
		} else if((la0_ == 'e' || la0_ == 'E') && (next(1) == '-' || next(1) == '+') && digit(next(2))) {
			res += utils::codepoint_to_utf8(la0_);
			advance();
			res += utils::codepoint_to_utf8(la0_);
			advance();
			res += utils::codepoint_to_utf8(la0_);
			advance();
			while(digit(la0_)) {
				res += utils::codepoint_to_utf8(la0_);
				advance();
			}
		}

		double num = 0;
		try {
			num = boost::lexical_cast<double>(res);
		} catch (boost::bad_lexical_cast& e) {
			LOG_ERROR("Parse error converting '" << res << "' to numeric value: " << e.what());
		}
		return num;
	}

	TokenPtr Tokenizer::consumeIdentlikeToken()
	{
		std::string str = consumeName();
		if(str.size() >= 3 && (str[0] == 'u' || str[0] == 'U') 
			&& (str[1] == 'r' || str[1] == 'R')
			&& (str[2] == 'l' || str[2] == 'L') 
			&& la0_ == '(') {
			advance();
			while(whitespace(la0_) && whitespace(next())) {
				advance();
			}
			if(la0_ == '\'' || la0_ == '"') {
				return std::make_shared<FunctionToken>(str);
			} else if(whitespace(la0_) && (next() == '\'' || next() == '"')) {
				return std::make_shared<FunctionToken>(str);
			} else {
				return consumeURLToken();
			}
		} else if(la0_ == '(') {
			advance();
			return std::make_shared<FunctionToken>(str);
		}
		return std::make_shared<IdentToken>(str);
	}

	TokenPtr Tokenizer::consumeURLToken()
	{
		std::string res;
		while(whitespace(la0_)) {
			advance();
		}
		if(eof(la0_)) {
			return std::make_shared<UrlToken>();
		}
		while(true) {
			if(la0_ == ')' || eof(la0_)) {
				advance();
				return std::make_shared<UrlToken>(res);
			} else if(whitespace(la0_)) {
				while(whitespace(la0_)) {
					advance();
				}
				if(la0_ == ')' || eof(la0_)) {
					advance();
					return std::make_shared<UrlToken>(res);
				} else {
					consumeBadURL();
					return std::make_shared<Token>(TokenId::BAD_URL);
				}
			} else if(la0_ == '"' || la0_ == '\'' || la0_ == '(' || nonprintable(la0_)) {
				LOG_ERROR("Parse error while processing codepoint: " << utils::codepoint_to_utf8(la0_));
				consumeBadURL();
				return std::make_shared<Token>(TokenId::BAD_URL);
			} else if(la0_ == '\\') {
				if(is_valid_escape(la0_, next(1))) {
					res += consumeEscape();
				} else {
					LOG_ERROR("Parse error while processing codepoint: " << utils::codepoint_to_utf8(la0_));
					consumeBadURL();
					return std::make_shared<Token>(TokenId::BAD_URL);
				}
			} else {
				res += utils::codepoint_to_utf8(la0_);
				advance();
			}
		}
	}

	void Tokenizer::consumeBadURL()
	{
		while(true) {
			if(la0_ == '-' || eof(la0_)) {
				return;
			} else if(is_valid_escape(la0_, next(1))) {
				consumeEscape();
			} else {
				advance();
			}
		}
	}

	std::string Token::toString() const
	{
		return tokenIdToString(id_);
	}

	std::string Token::tokenIdToString(TokenId id) 
	{
		switch(id) {
			case TokenId::BAD_STRING:		return "BAD-STRING";
			case TokenId::BAD_URL:			return "BAD-URL";
			case TokenId::INCLUDE_MATCH:	return "INCLUDE-MATCH";
			case TokenId::DASH_MATCH: 		return "DASH-MATCH";
			case TokenId::PREFIX_MATCH: 	return "PREFIX-MATCH";
			case TokenId::SUFFIX_MATCH: 	return "SUFFIX-MATCH";
			case TokenId::SUBSTRING_MATCH:	return "SUBSTRING-MATCH";
			case TokenId::COLUMN:			return "COLUMN";
			case TokenId::WHITESPACE:		return "WHITESPACE";
			case TokenId::CDO:				return "CDO";
			case TokenId::CDC:				return "CRC";
			case TokenId::COLON:			return "COLON";
			case TokenId::SEMICOLON:		return "SEMI-COLON";
			case TokenId::COMMA:			return "COMMA";
			case TokenId::LBRACKET:			return "L-BRACKET";
			case TokenId::RBRACKET:			return "R-BRACKET";
			case TokenId::LPAREN:			return "L-PAREN";
			case TokenId::RPAREN:			return "R-PAREN";
			case TokenId::LBRACE:			return "L-BRACE";
			case TokenId::RBRACE:			return "R-BRACE";
			case TokenId::EOF_TOKEN:		return "EOF";
			default: break;
		}
		return "<<bad-token>>";
	}
}
