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

#include <algorithm>

#include "asserts.hpp"
#include "code_editor_dialog.hpp"
#include "checksum.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula_callable.hpp"
#include "formula_constants.hpp"
#include "formula_function.hpp"
#include "formula_object.hpp"
#include "json_parser.hpp"
#include "json_tokenizer.hpp"
#include "md5.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "preprocessor.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"
#include "wml_formula_callable.hpp"

namespace game_logic 
{
	void remove_formula_function_cached_doc(const std::string& name);
}

namespace json 
{
	namespace 
	{
		std::map<std::string, std::string> pseudo_file_contents;
	}

	void set_file_contents(const std::string& path, const std::string& contents)
	{
		game_logic::remove_formula_function_cached_doc(contents);
		pseudo_file_contents[path] = contents;
	}

	std::string get_file_contents(const std::string& path)
	{
		std::map<std::string, std::string>::const_iterator i = pseudo_file_contents.find(path);
		if(i != pseudo_file_contents.end()) {
			return i->second;
		} else {
			return sys::read_file(module::map_file(path));
		}
	}

	ParseError::ParseError(const std::string& msg)
	  : message(msg), 
	    line(-1), 
	    col(-1)
	{
		//std::cerr << errorMessage() << "\n";
	}

	ParseError::ParseError(const std::string& msg, const std::string& filename, std::ptrdiff_t line, std::ptrdiff_t col)
	  : message(msg), 
	    fname(filename), 
	    line(line), 
	    col(col)
	{
		//std::cerr << errorMessage() << "\n";
	}

	std::string ParseError::errorMessage() const
	{
		if(line != -1) {
			return formatter() << "PARSE ERROR: " << fname << ": line " << line << " col " << col << ": " << message;
		} else {
			return formatter() << "PARSE ERROR: " << fname << ": " << message;
		}
	}

	namespace 
	{
		std::ptrdiff_t get_line_num(const std::string& doc, std::ptrdiff_t pos) 
		{
			return 1 + std::count(doc.begin(), doc.begin() + pos, '\n');
		}

		std::ptrdiff_t get_col_number(const std::string& doc, std::ptrdiff_t pos) 
		{
			const auto init = pos;
			while(pos >= 0 && doc[pos] != '\n' && doc[pos] != '\r') {
				--pos;
			}

			return 1 + init - pos;
		}

		void escape_string(std::string& s) 
		{
			for(size_t n = 0; n < s.size(); ++n) {
				if(s[n] == '\\') {
					s.erase(s.begin() + n);
					if(n < s.size() && s[n] == 'n') {
						s[n] = '\n';
					}
				}
			}
		}
	}

#define CHECK_PARSE(cond, msg, pos)						\
		do { if(!(cond)) {								\
			auto ln = get_line_num(doc, (pos));			\
			auto col = get_col_number(doc, (pos));		\
			throw ParseError((msg), fname, ln, col);	\
		} } while(0)

	namespace 
	{
		class JsonMacro;
		typedef std::shared_ptr<JsonMacro> JsonMacroPtr;

		class JsonMacro
		{
			std::string code_;
			std::map<std::string, JsonMacroPtr> macros_;
		public:
			JsonMacro(const std::string& code, std::map<std::string, JsonMacroPtr> macros);
			variant call(variant arg) const;
		};

		JsonMacro::JsonMacro(const std::string& code, std::map<std::string, JsonMacroPtr> macros) 
			:code_(code), 
			macros_(macros)
		{
		}

		enum class VAL_TYPE { NONE, OBJ, ARRAY };
		struct JsonObject 
		{
			JsonObject(variant::debug_info debug_info, bool preprocess) 
				: type(VAL_TYPE::NONE), 
				  is_base(false), 
				  is_call(false), 
				  is_deriving(false), 
				  is_merging(false), 
				  require_comma(false), 
				  require_colon(false), 
				  flatten(false), 
				  info(debug_info), 
				  begin_macro(nullptr), 
				  use_preprocessor(preprocess) 
			{}
			std::map<variant, variant> obj;
			std::vector<variant> array;
			std::set<variant> obj_already_seen;
			VAL_TYPE type;
			variant name;

			variant base;
			bool is_base;
			bool is_call;
			bool is_deriving;
			bool is_merging;

			bool require_comma;
			bool require_colon;

			bool flatten;

			variant::debug_info info;

			const char* begin_macro;

			bool use_preprocessor;

			void setup_base(variant v) {
				if(v.is_null()) {
					return;
				}
				for(const variant_pair& value : v.as_map()) {
					if(value.first.is_string() && !value.first.as_string().empty() && value.first.as_string()[0] == '@') {
						continue;
					}

					obj[value.first] = value.second;
				}
			}

			void add(variant name, variant v) {
				if(use_preprocessor && name.is_string() && name.as_string() == "@base") {
					return;
				}

				if(type == VAL_TYPE::OBJ) {
					if(is_deriving) {
						setup_base(v);
						is_deriving = false;
					} else {
						if(is_merging && obj.count(name)) {
							smart_merge_variants(&obj[name], v);
						} else {
							obj[name] = v;
						}
					}
				} else {
					if(flatten && v.is_list()) {
						for(int n = 0; n != v.num_elements(); ++n) {
							add(name, v[n]);
						}
						return;
					}

					if(base.is_null() == false && v.is_map()) {
						std::map<variant, variant> items = base.as_map();
						std::map<variant, variant> override = v.as_map();
						for(std::map<variant, variant>::const_iterator i = override.begin(); i != override.end(); ++i) {
							items[i->first] = i->second;
						}

						variant new_v(&items);
						if(v.get_debug_info()) {
							new_v.setDebugInfo(*v.get_debug_info());
						}

						array.push_back(new_v);
					} else {
						array.push_back(v);
					}
				}
			}

			variant as_variant() {
				if(type == VAL_TYPE::OBJ) {
					variant v(&obj);
					v.setDebugInfo(info);
					return v;
				} else {
					variant v(&array);
					v.setDebugInfo(info);
					return v;
				}
			}
		};

		std::set<std::string> filename_registry;

		variant parse_internal(const std::string& doc, const std::string& fname,
							   JSON_PARSE_OPTIONS options,
							   std::map<std::string, JsonMacroPtr>* macros,
							   const game_logic::FormulaCallable* callable)
		{
			std::map<std::string, JsonMacroPtr> macros_buf;
			if(!macros) {
				macros = &macros_buf;
			}

			bool use_preprocessor = options == JSON_PARSE_OPTIONS::USE_PREPROCESSOR;

			std::set<std::string>::const_iterator filename_itor = filename_registry.insert(fname).first;

			variant::debug_info debug_info;
			debug_info.filename = &*filename_itor;
			debug_info.line = 1;
			debug_info.column = 1;

			const char* debug_pos = doc.c_str();

			const char* i1 = doc.c_str();
			const char* i2 = i1 + doc.size();
			try {
				std::vector<JsonObject> stack;
				stack.push_back(JsonObject(debug_info, use_preprocessor));
				stack.push_back(JsonObject(debug_info, use_preprocessor));
				stack[0].type = VAL_TYPE::ARRAY;

				for(Token t = get_token(i1, i2); t.type != Token::TYPE::NUM_TYPES; t = get_token(i1, i2)) {
					while(debug_pos != t.begin) {
						if(*debug_pos == '\n') {
							++debug_info.line;
							debug_info.column = 0;
						} else {
							++debug_info.column;
						}

						++debug_pos;
					}

					CHECK_PARSE(stack.size() > 1, "Unexpected characters at end of input", t.begin - doc.c_str());

					CHECK_PARSE(!stack.back().require_colon || t.type == Token::TYPE::COLON, "Unexpected characters, when expecting a ':'", t.begin - doc.c_str());
					CHECK_PARSE(!stack.back().require_comma || t.type == Token::TYPE::COMMA || t.type == Token::TYPE::RCURLY || t.type == Token::TYPE::RSQUARE, "Unexpected characters, when expecting a ','", t.begin - doc.c_str());

					switch(t.type) {
					case Token::TYPE::COLON: {
						CHECK_PARSE(stack.back().require_colon, "Unexpected :", t.begin - doc.c_str());
						stack.back().require_colon = false;
						if(stack.back().begin_macro) {
							stack.back().begin_macro = t.end;

							//we'll turn off the preprocessor while we parse the macro.
							use_preprocessor = false;
						}
						break;
					}

					case Token::TYPE::COMMA: {
						CHECK_PARSE(stack.back().require_comma, "Unexpected ,", t.begin - doc.c_str());
						stack.back().require_comma = false;
						break;
					}

					case Token::TYPE::LCURLY: {
						if(stack.back().type == VAL_TYPE::ARRAY) {
							stack.push_back(JsonObject(debug_info, use_preprocessor));
							stack.back().setup_base(stack[stack.size()-2].base);
						}

						CHECK_PARSE(stack.back().type == VAL_TYPE::NONE, "Unexpected {", t.begin - doc.c_str());
						stack.back().type = VAL_TYPE::OBJ;
						break;
					}
					case Token::TYPE::RCURLY: {
						CHECK_PARSE(stack.back().type == VAL_TYPE::OBJ, "Unexpected }", t.begin - doc.c_str());

						stack.back().info.end_line = debug_info.line;
						stack.back().info.end_column = debug_info.column;

						const char* begin_macro = stack.back().begin_macro;
						const bool is_base = stack.back().is_base;
						const bool is_call = stack.back().is_call;
						variant name = stack.back().name;
						variant v = stack.back().as_variant();
						stack.pop_back();

						if(is_base) {
							stack.back().base = v;
						} else if(is_call) {
							std::string call_macro = v["@call"].as_string();
							std::map<std::string, JsonMacroPtr>::const_iterator itor = macros->find(call_macro);
							CHECK_PARSE(itor != macros->end(), "Could not find macro", t.begin - doc.c_str());

							stack.back().add(name, itor->second->call(v));
						} else if(begin_macro) {
							(*macros)[name.as_string()].reset(new JsonMacro(std::string(begin_macro, t.end), *macros));
							use_preprocessor = true;
						} else if(use_preprocessor && v.is_map() && game_logic::WmlSerializableFormulaCallable::deserializeObj(v, &v)) {
							stack.back().add(name, v);
						} else {
							stack.back().add(name, v);
						}
						stack.back().require_comma = true;
						break;
					}

					case Token::TYPE::LSQUARE: {
						if(stack.back().type == VAL_TYPE::ARRAY) {
							stack.push_back(JsonObject(debug_info, use_preprocessor));
						}

						CHECK_PARSE(stack.back().type == VAL_TYPE::NONE, "Unexpected [", t.begin - doc.c_str());
						stack.back().type = VAL_TYPE::ARRAY;
						break;
					}

					case Token::TYPE::RSQUARE: {
						CHECK_PARSE(stack.back().type == VAL_TYPE::ARRAY, "Unexpected ]", t.begin - doc.c_str());

						stack.back().info.end_line = debug_info.line;
						stack.back().info.end_column = debug_info.column;

						const char* begin_macro = stack.back().begin_macro;
						variant name = stack.back().name;
						variant v = stack.back().as_variant();
						stack.pop_back();

						if(begin_macro) {
							(*macros)[name.as_string()].reset(new JsonMacro(std::string(begin_macro, t.end), *macros));
							use_preprocessor = true;
						} else {
							stack.back().add(name, v);
						}
						stack.back().require_comma = true;
						break;
					}

					case Token::TYPE::IDENTIFIER:
						CHECK_PARSE(stack.back().type == VAL_TYPE::OBJ, "Unexpected identifier: " + std::string(t.begin, t.end), t.begin - doc.c_str());
					case Token::TYPE::STRING: {
						std::string s(t.begin, t.end);
						variant::debug_info str_debug_info = debug_info;
						str_debug_info.end_line = str_debug_info.line;
						str_debug_info.end_column = str_debug_info.column;
						for(std::string::const_iterator i = s.begin(); i != s.end(); ++i) {
							if(*i == '\n') {
								str_debug_info.end_line++;
								str_debug_info.end_column = 0;
							} else {
								str_debug_info.end_column++;
							}
						}

						if(t.type == Token::TYPE::STRING) {
							escape_string(s);
						}

						variant v;
				
						bool is_macro = false;
						bool is_flatten = false;
						if(use_preprocessor) {
							const std::string Macro = "@macro ";
							if(stack.back().type == VAL_TYPE::OBJ && s.size() > Macro.size() && std::equal(Macro.begin(), Macro.end(), s.begin())) {
								s.erase(s.begin(), s.begin() + Macro.size());
								is_macro = true;
							}

							try {
								v = preprocess_string_value(s, callable);

								if(v.get_debug_info()) {
									str_debug_info = *v.get_debug_info();
								}
							} catch(preprocessor_error&) {
								CHECK_PARSE(false, "Preprocessor error: " + s, t.begin - doc.c_str());
							}

							const std::string CallStr = "@call";
							if(stack.back().type == VAL_TYPE::OBJ && s == "@call") {
								stack.back().is_call = true;
							} else if(stack.back().type == VAL_TYPE::OBJ && stack[stack.size()-2].type == VAL_TYPE::ARRAY && s == "@base") {
								stack.back().is_base = true;
							}

							if(s == "@flatten") {
								is_flatten = true;
							}

							if(stack.back().type == VAL_TYPE::OBJ && s == "@derive") {
								stack.back().is_deriving = true;
							}

							if(stack.back().type == VAL_TYPE::OBJ && s == "@merge") {
								stack.back().is_deriving = true;
								stack.back().is_merging = true;
							}

						} else {
							v = variant(s);
						}

						if(t.translate && v.is_string()) {
							v = variant::create_translated_string(v.as_string());
						}

						if(stack.back().type == VAL_TYPE::OBJ) {
							if(!stack.back().obj_already_seen.insert(v).second) {
								CHECK_PARSE(false, "Repeated attribute: " + v.write_json(), t.begin - doc.c_str());
							}

							stack.push_back(JsonObject(str_debug_info, use_preprocessor));
							v.setDebugInfo(str_debug_info);
							stack.back().name = v;
							stack.back().require_colon = true;

							if(is_macro) {
								stack.back().begin_macro = i1;
							}
						} else if(stack.back().type == VAL_TYPE::ARRAY) {
							if(is_flatten) {
								stack.back().flatten = true;
							} else {
								stack.back().add(variant(""), v);
							}

							stack.back().require_comma = true;
						} else {
							const char* begin_macro = stack.back().begin_macro;
							variant name = stack.back().name;
							v.setDebugInfo(str_debug_info);
							stack.pop_back();

							if(begin_macro) {
								(*macros)[name.as_string()].reset(new JsonMacro(std::string(begin_macro, t.end), *macros));
								use_preprocessor = true;
							} else {
								stack.back().add(name, v);
							}
							stack.back().require_comma = true;
						}

						break;
					}

					case Token::TYPE::NUMBER:
					case Token::TYPE::TRUE_VALUE:
					case Token::TYPE::FALSE_VALUE:
					case Token::TYPE::NULL_VALUE: {
						variant v;
						if(t.type == Token::TYPE::NUMBER) {
							std::string s(t.begin, t.end);
							if(std::count(s.begin(), s.end(), '.')) {
									v = variant(decimal::from_string(s));
							} else {
								int val = atoi(s.c_str());
								v = variant(val);
							}
						} else if(t.type == Token::TYPE::TRUE_VALUE) {
							v = variant::from_bool(true);
						} else if(t.type == Token::TYPE::FALSE_VALUE) {
							v = variant::from_bool(false);
						}

						CHECK_PARSE(stack.back().type != VAL_TYPE::OBJ, "Unexpected value in object", t.begin - doc.c_str());
						if(stack.back().type == VAL_TYPE::ARRAY) {
							stack.back().add(variant(""), v);
							stack.back().require_comma = true;
						} else {
							variant name = stack.back().name;
							stack.pop_back();
							stack.back().add(name, v);
							stack.back().require_comma = true;
						}

						break;
					}
				
					default: assert(false);
					}
				}


				CHECK_PARSE(stack.size() == 1 && stack.back().array.size() == 1, "Unexpected end of input", i1 - doc.c_str());
				return stack.back().array.front();
			} catch(TokenizerError& e) {
				CHECK_PARSE(false, e.msg, e.loc - doc.c_str());
			}
		}

		variant JsonMacro::call(variant arg) const
		{
			std::map<std::string, JsonMacroPtr> m = macros_;
			game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
			for(const variant_pair& p : arg.as_map()) {
				callable->add(p.first.as_string(), p.second);
			}

			variant holder(callable);

			return parse_internal(code_, "", JSON_PARSE_OPTIONS::USE_PREPROCESSOR, &m, callable);
		}
	}

	variant parse(const std::string& doc, JSON_PARSE_OPTIONS options)
	{
		return parse_internal(doc, "", options, nullptr, nullptr);
	}

	variant parse_from_file(const std::string& fname, JSON_PARSE_OPTIONS options)
	{
		try {
			std::string data = get_file_contents(fname);

			typedef std::pair<std::string, JSON_PARSE_OPTIONS> CacheKey;
			static std::map<CacheKey, variant> cache;

			CacheKey key(md5::sum(data), options);
			std::map<CacheKey, variant>::iterator cache_itor = cache.find(key);
			if(cache_itor != cache.end()) {
				return cache_itor->second;
			}

			checksum::verify_file(fname, data);

			if(data.empty()) {
				throw ParseError(formatter() << "Could not find file " << fname);
			}

			variant result;
		
			try {
				result = parse_internal(data, fname, options, nullptr, nullptr);
			} catch(ParseError& e) {
				if(!preferences::edit_and_continue()) {
					throw e;
				}

				static bool in_edit_and_continue = false;
				if(in_edit_and_continue) {
					throw e;
				}

				in_edit_and_continue = true;
				edit_and_continue_fn(module::map_file(fname), 
					formatter() << "At " << module::map_file(fname) << " " << e.line << ": " << e.message, 
					[=](){ parse_from_file(fname, options); });
				in_edit_and_continue = false;
				return parse_from_file(fname, options);
			}

			for(std::map<CacheKey, variant>::iterator i = cache.begin(); i != cache.end(); ) {
				if(i->second.refcount() == 1) {
					cache.erase(i++);
				} else {
					++i;
				}
			}

			cache[key] = result;
			return result;
		} catch(ParseError& e) {
			// Removed the completely asinine practice of emitting they parser error message.
			// If you want to output it, then you should be catching the exception elsewhere
			// and outputting it there.
			e.fname = fname;
			throw(e);
		}
	}

	variant parse_from_file_or_die(const std::string& fname, JSON_PARSE_OPTIONS options)
	{
		try {
			return parse_from_file(fname, options);
		} catch(json::ParseError& e) {
			ASSERT_LOG(false, e.errorMessage());
		}
	}

	bool file_exists_and_is_valid(const std::string& fname)
	{
		try {
			parse_from_file(fname, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
			return true;
		} catch(ParseError&) {
			return false;
		}
	}

	UNIT_TEST(json_base)
	{
		std::string doc = "[{\"@base\": true, x: 5, y: 4}, {}, {a: 9, y: 2}, \"@eval {}\"]";
		variant v = parse(doc);
		CHECK_EQ(v.num_elements(), 3);
		CHECK_EQ(v[0]["x"], variant(5));
		CHECK_EQ(v[1]["x"], variant(5));
		CHECK_EQ(v[0]["y"], variant(4));
		CHECK_EQ(v[1]["y"], variant(2));
		CHECK_EQ(v[1]["a"], variant(9));

		CHECK_EQ(v[2]["x"], variant(5));
		CHECK_EQ(v[2]["y"], variant(4));

		CHECK_EQ(v[0]["@base"].is_null(), true);
	}

	UNIT_TEST(json_flatten)
	{
		std::string doc = "[\"@flatten\", [0,1,2], [3,4,5]]";
		variant v = parse(doc);
		CHECK_EQ(v.num_elements(), 6);
		for(int n = 0; n != v.num_elements(); ++n) {
			CHECK_EQ(v[n], variant(n));
		}
	}

	UNIT_TEST(json_derive)
	{
		std::string doc = "{\"@derive\": {x: 4, y:3, m: {a: 5, y:2}}, y: 2, a: 7, m: {a: 2}}";
		variant v = parse(doc);
		CHECK_EQ(v["x"], variant(4));
		CHECK_EQ(v["y"], variant(2));
		CHECK_EQ(v["a"], variant(7));
		CHECK_EQ(v["m"].is_map(), true);
		CHECK_EQ(v["m"].num_elements(), 1);
		CHECK_EQ(v["m"]["a"], variant(2));
	}

	UNIT_TEST(json_merge)
	{
		std::string doc = "{\"@merge\": {x: 4, y:3, m: {a: 5, y:2}}, y: 2, a: 7, m: {a: 2}}";
		variant v = parse(doc);
		CHECK_EQ(v["x"], variant(4));
		CHECK_EQ(v["y"], variant(2));
		CHECK_EQ(v["a"], variant(7));
		CHECK_EQ(v["m"].is_map(), true);
		CHECK_EQ(v["m"].num_elements(), 2);
		CHECK_EQ(v["m"]["a"], variant(2));
		CHECK_EQ(v["m"]["y"], variant(2));
	}

	UNIT_TEST(json_macro)
	{
		std::string doc = "{\"@macro f\": {a: \"@eval 4 + x\", b: \"@eval y\"},"
						  "value: {\"@call\": \"f\", x: 2, y: {a: 4, z: 5}}}";
		variant v = parse(doc);
		LOG_DEBUG(v.write_json());
		CHECK_EQ(v.getKeys().num_elements(), 1);

		v = v["value"];
		CHECK_EQ(v["a"], variant(6));
		CHECK_EQ(v["b"]["a"], variant(4));
		CHECK_EQ(v["b"]["z"], variant(5));
	}
}
