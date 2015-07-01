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

#include <memory>
#include <string>
#include <vector>

#include "formatter.hpp"
#include "variant.hpp"

namespace css
{
	struct TokenizerError : public std::runtime_error 
	{
		TokenizerError(const char* str) : std::runtime_error(str) {}
	};
	
	enum class TokenId {
		IDENT,
		FUNCTION,
		AT,
		HASH,
		STRING,
		BAD_STRING,
		URL,
		BAD_URL,
		DELIM,
		NUMBER,
		PERCENT,
		DIMENSION,
		INCLUDE_MATCH,
		DASH_MATCH,
		PREFIX_MATCH,
		SUFFIX_MATCH,
		SUBSTRING_MATCH,
		COLUMN,
		WHITESPACE,
		CDO,
		CDC,
		COLON,
		SEMICOLON,
		COMMA,
		LBRACKET,
		RBRACKET,
		LPAREN,
		RPAREN,
		LBRACE,
		RBRACE,
		EOF_TOKEN,

		// These are special tokens
		BLOCK_TOKEN,
		AT_RULE_TOKEN,
		RULE_TOKEN,
		SELECTOR_TOKEN,
	};

	enum class TokenFlags {
		UNRESTRICTED = 1,
		ID = 2,
	};

	class Token;
	typedef std::shared_ptr<Token> TokenPtr;

	class Token 
	{
	public:
		explicit Token(TokenId id) : id_(id) {}
		virtual ~Token() {}
		TokenId id() const { return id_; }
		virtual std::string toString() const;
		virtual variant value() { return variant(toString()); }
		virtual std::string getStringValue() const { return ""; }
		virtual double getNumericValue() const { return 0; }

		void addParameters(std::vector<TokenPtr> tok) { params_.insert(params_.end(), tok.begin(), tok.end()); }
		void addParameter(TokenPtr tok) { params_.emplace_back(tok); }
		const std::vector<TokenPtr>& getParameters() const { return params_; }
		
		void setValue(TokenPtr tok) { value_ = tok; }
		TokenPtr getValue() const { return value_; }
		
		static std::string tokenIdToString(TokenId id);
	private:
		TokenId id_;
		std::vector<TokenPtr> params_;
		TokenPtr value_;
	};

	// The tokenizer class expects an input of unicode codepoints.
	class Tokenizer
	{
	public:
		typedef std::vector<TokenPtr>::const_iterator const_iterator;
		typedef std::vector<TokenPtr>::iterator iterator;

		explicit Tokenizer(const std::string& inp);
		const std::vector<TokenPtr>& getTokens() const { return tokens_; }
	private:
		void advance(int n=1);
		char32_t next(int n=1);
		bool eof(char32_t cp);
		std::string consumeEscape();
		void consumeWhitespace();
		void consumeComments();
		TokenPtr consumeString(char32_t end_codepoint);
		TokenPtr consumeNumericToken();
		TokenPtr consumeIdentlikeToken();
		double consumeNumber();
		std::string consumeName();
		TokenPtr consumeURLToken();
		void consumeBadURL();

		std::vector<char32_t> cp_string_;
		std::vector<char32_t>::size_type it_;

		// Look ahead+0
		char32_t la0_;

		std::vector<TokenPtr> tokens_;
	};
}
