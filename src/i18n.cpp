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

#include <string>
#include <iostream>
#include <cstdint>

#include <unordered_map>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "i18n.hpp"
#include "logger.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "unit_test.hpp"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef TARGET_OS_HARMATTAN
#include <gconf/gconf-client.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef TARGET_BLACKBERRY
#include "bps/bps.h"
#include "bps/locale.h"
#include <sstream>
#endif

namespace
{
	////////////////////
	// IMPLEMENTATION //
	////////////////////

	//hashmap to map original to translated strings
	typedef std::unordered_map<std::string, std::string> map;
	map hashmap;

	std::string locale;

	void store_message(const std::string & msgid, const std::string & msgstr) {
		auto p = hashmap.insert(make_pair(msgid, msgstr));
		if (!p.second && msgstr != p.first->second) {
			LOG_DEBUG("i18n: Overwriting a translation of string \"" << msgid << "\":");
			LOG_DEBUG("i18n: Changing \"" << p.first->second << "\" to \"" << msgstr << "\"");
			p.first->second = msgstr;
		}
	}

	///////////////
	// MO PARSER //
	///////////////

	//header structure of the MO file format, as described on
	//http://www.gnu.org/software/hello/manual/gettext/MO-Files.html
	struct mo_header {
		uint32_t magic;
		uint32_t version;
		uint32_t number;   // number of strings
		uint32_t o_offset; // offset of original string table
		uint32_t t_offset; // offset of translated string table
	};

	//the original and translated string table consists of
	//a number of string length / file offset pairs:
	struct mo_entry {
		uint32_t length;
		uint32_t offset;
	};

	//handle the contents of an mo file
	void process_mo_contents(const std::string & content) {
		size_t size = content.size();
		if (size < sizeof(mo_header))
			return;
		mo_header* header = (mo_header*) content.c_str();
		if (header->magic != 0x950412de ||
			header->version != 0 ||
			header->o_offset + 8*header->number > size ||
			header->t_offset + 8*header->number > size)
			return;
		mo_entry* original = (mo_entry*) (content.c_str() + header->o_offset);
		mo_entry* translated = (mo_entry*) (content.c_str() + header->t_offset);

		for (unsigned i = 0; i < header->number; ++i) {
			if (original[i].offset + original[i].length > size ||
				translated[i].offset + translated[i].length > size)
				return;
			const std::string msgid = content.substr(original[i].offset, original[i].length);
			const std::string msgstr = content.substr(translated[i].offset, translated[i].length);

			store_message(msgid, msgstr);
		}
	}

	///////////////
	// PO PARSER //
	///////////////

	bool is_po_whitespace(char c) {
		static const std::string po_whitespace = " \t\n\r";
		return (po_whitespace.find(c) != std::string::npos);
	}

	// On input a string free of newline, expected to be wrapped in quotes,
	// with possible leading or trailing whitespace and escaped characters,
	// push to the stream the contents that were quoted. assumes utf-8.
	typedef std::string::const_iterator str_it;
	void parse_quoted_string(std::string & ss, const str_it & begin, const str_it & end) {

		bool pre_string = true;
		bool post_string = false;

		for (str_it it = begin; it != end; it++) {
			if (pre_string || post_string) {
				if (*it == '\"') {
					ASSERT_LOG(!post_string, "i18n: Only one quoted string is allowed on a line of po file: \n<<" << std::string(begin, end) << ">>");
					pre_string = false;
				} else {
					ASSERT_LOG(is_po_whitespace(*it), "i18n: Unexpected characters in po file where only whitespace is expected: \'" << *it << "\':\n<<" << std::string(begin, end) << ">>");
				}
			} else {
				if (*it == '\"') {
					post_string = true;
				} else if (*it == '\\') {
					it++;
					ASSERT_LOG(it != end, "i18n: po string terminated unexpectedly after escape character: \n<<" << std::string(begin, end) << ">>");
					char c = *it;
					switch (c) {
						case 'n': {
							ss += '\n';
							break;
						}
						case 't': {
							ss += '\t';
							break;
						}
						case '0': {
							ss += '\0';
							return; // can just return after null character is pushed, we are going to truncate here anyways.
						}
						case '\'':
						case '\"':
						case '\\': {
							ss += c;
							break;
						}
						default: {
							ASSERT_LOG(false, "i18n: po string contained unrecognized escape sequence: \"\\" << c << "\": \n<<" << std::string(begin, end) << ">>");
						}
					}
				} else {
					ss += *it;
				}
			}
		}
		ASSERT_LOG(pre_string || post_string, "i18n: unterminated quoted string in po file:\n<<" << std::string(begin, end) << ">>");
	}

	// A helper which stores a message for the po parser
	// Skips empty strings -- as a compatibility issue these should not be stored in the catalog, and left untranslated.
	// (this is a compatability issue for the other gettext tools, and the pot generator which marks all string initially `msgstr ""`)
	// Stops the message string at embedded null character -- this allows translator to mark empty string "" as the translation,
	// using `msgstr "\0"`
	// We don't want embedded nulls in the translation dictionary anyways.
	void store_message_helper_po(const std::string & msgid, const std::string & msgstr)
	{
		if (msgstr.size()) {
			size_t embedded_null = msgstr.find_first_of((char) 0);
			if (embedded_null != std::string::npos) {
				store_message(msgid, msgstr.substr(0, embedded_null));
			} else {
				store_message(msgid, msgstr);
			}
		}
	}

	enum po_item { PO_NONE, PO_MSGID, PO_MSGSTR };

	void process_po_contents(const std::string & content) {
		static const std::string MSGID = "msgid ";
		static const std::string MSGSTR = "msgstr ";

		std::string msgid;
		std::string msgstr;

		po_item current_item = PO_NONE;

		str_it line_end = content.begin();
		for (str_it line_start = content.begin(); line_start != content.end(); line_start = (line_end == content.end() ? line_end : ++line_end)) {
			size_t line_size = 0;
			while ((line_end != content.end()) && (*line_end != '\n')) {
				++line_end;
				++line_size;
			}
			// line_start, line_end, line_size should be const for the rest of the loop
			// (the above should be equivalent to using std::getline)
			if (line_size > 0 && *line_start != '#') {
				if (line_size >= MSGID.size() && std::equal(line_start, line_start + MSGID.size(), MSGID.begin())) {
					switch(current_item) {
						case PO_MSGID: {
							LOG_DEBUG("i18n: ignoring a MSGID which had no MSGSTR: \n<<" << msgid << ">>");
							break;
						}
						case PO_MSGSTR: { // This is the start of a new item, so store the previous item
							store_message_helper_po(msgid, msgstr);
							break;
						}
						case PO_NONE:
							break;
					}
					msgid = "";
					msgstr = "";
					msgid.reserve(line_size);
					parse_quoted_string(msgid, line_start + MSGID.size(), line_end);
					current_item = PO_MSGID;
				} else if (line_size >= MSGSTR.size() && std::equal(line_start, line_start + MSGSTR.size(), MSGSTR.begin())) {
					ASSERT_LOG(current_item == PO_MSGID, "i18n: in po file, found a msgstr with no earlier msgid:\n<<" << std::string(line_start, line_end) << ">>");

					msgstr.reserve(line_size);
					parse_quoted_string(msgstr, line_start + MSGSTR.size(), line_end);
					current_item = PO_MSGSTR;
				} else {
					switch (current_item) {
						case PO_MSGID: {
							parse_quoted_string(msgid, line_start, line_end);
							break;
						}
						case PO_MSGSTR: {
							parse_quoted_string(msgstr, line_start, line_end);
							break;
						}
						case PO_NONE: {
							for (str_it it = line_start; it != line_end; ++it) {
								ASSERT_LOG(is_po_whitespace(*it), "i18n: in po file, the first non-whitespace non-comment line should begin 'msgid ': \n<<" << std::string(line_start, line_end) << ">>");
							}
							break;
						}
					}
				}
			}
		}

		// Make sure to store the very last message also
		switch(current_item) {
			case PO_MSGSTR: {
				store_message_helper_po(msgid, msgstr);
				break;
			}
			case PO_MSGID: {
				LOG_DEBUG("i18n: ignoring a MSGID which had no MSGSTR: \n<<" << msgid << ">>");
				break;
			}
			case PO_NONE: {
				LOG_WARN("i18n: parsed a po file which had no content");
				break;
			}
		}
	}
}

	///////////////
	// INTERFACE //
	///////////////

namespace i18n
{
	const std::string& tr(const std::string& msgid)
	{
		//do not attempt to translate empty strings ("") since that returns metadata
		if (msgid.empty())
			return msgid;
		map::iterator it = hashmap.find (msgid);
		if (it != hashmap.end())
			return it->second;
		//if no translated string was found, return the original
		return msgid;
	}

	const std::string& get_locale()
	{
		return locale;
	}

	// Feels like a hack
	bool is_locale_cjk()
	{
		if(locale.empty()) {
			return false;
		}
		if(locale.size() == 1 && (locale == "C" || locale == "c")) {
			return false;
		}
		ASSERT_LOG(locale.size() >= 2, "Length of local string too short: " << locale);
		return (locale[0] == 'z'  && locale[1] == 'h')
			|| (locale[0] == 'j'  && locale[1] == 'a')
			|| (locale[0] == 'k'  && locale[1] == 'o');
	}

	namespace {
		std::string mo_dir(const std::string & locale_str) {
			return "./locale/" + locale_str + "/LC_MESSAGES/";
		}

		//strip the charset part of the country and language code,
		//leave the script code if there is one
		//e.g. "pt_BR.UTF8" --> "pt_BR"
		//     "sr_RS.UTF-8@latin" --> "sr_RS@latin"
		std::string trim_locale_charset(const std::string & locale) {
			size_t found = locale.find(".");
			if (found != std::string::npos) {
				size_t found2 = locale.substr(found).find('@');
				if (found2 != std::string::npos) {
					return locale.substr(0, found) + locale.substr(found).substr(found2);
				}
				return locale.substr(0, found);
			}
			return locale;
		}

		// Try to adjust the locale for cases when we failed to find a match
		std::string tweak_locale(const std::string & locale) {
			size_t found = locale.find("@");
			if (found != std::string::npos) {
				return locale.substr(0, found);
			}

			found = locale.find("_");
			if (found != std::string::npos) {
				return locale.substr(0, found);
			}
			return "";
		}
	}

	void load_translations()
	{
		hashmap.clear();

		std::vector<std::string> files;
		std::string dirname;

		for (std::string loc = trim_locale_charset(locale); loc.size() >= 2; loc = tweak_locale(loc)) {
			dirname = mo_dir(loc);
			module::get_files_in_dir(dirname, &files);

			if (files.size()) {
				bool loaded_something = false;
				for(auto & file : files) {
					std::string extension;
					try {
						extension = file.substr(file.find_last_of('.'));
					} catch (std::out_of_range &) {}

					std::string path = dirname + file;
					ASSERT_LOG(sys::file_exists(module::map_file(path)), "confused... file does not exist which was found earlier: " << path);
					if (extension == ".mo") {
						LOG_DEBUG("loading translations from mo file: " << path);
						process_mo_contents(sys::read_file(module::map_file(path)));
						loaded_something = true;
					} else if (extension == ".po") {
						LOG_DEBUG("loading translations from po file: " << path);
						process_po_contents(sys::read_file(module::map_file(path)));
						loaded_something = true;
					} else {
						LOG_DEBUG("skipping translations file: " << path);
					}
				}
				if (loaded_something) {
					return ;
				} else {
					LOG_DEBUG("did not find any mo or po files in dir " << dirname);
				}
			}
		}

		LOG_WARN("did not find any translation files. locale = " << locale << " , dirname = " << dirname);
	}

	bool load_extra_po(const std::string & module_dir) {
		for (std::string loc = trim_locale_charset(locale); loc.size() >= 2; loc = tweak_locale(loc)) {
			std::string path = module_dir + loc + ".po";
			if (sys::file_exists(module::map_file(path))) {
				LOG_DEBUG("loading translations from po file: " << path);
				process_po_contents(sys::read_file(module::map_file(path)));
				return true;
			}
		}
		LOG_DEBUG("could not find translations in " << module_dir << " associated to locale " << locale);

		return false;
	}

	void setLocale(const std::string& l)
	{
		locale = l;
		i18n::load_translations();
	}

	void use_system_locale()
	{
#ifdef _WIN32
		{
			char c[1024];
			GetLocaleInfoA(LOCALE_USER_DEFAULT,LOCALE_SISO639LANGNAME,c,1024);
			if(c[0]!='\0'){
				locale=c;
				GetLocaleInfoA(LOCALE_USER_DEFAULT,LOCALE_SISO3166CTRYNAME,c,1024);
				if(c[0]!='\0') locale+=std::string("_")+c;
			}
		}
#endif

#ifdef __APPLE__
		CFArrayRef localeIDs = CFLocaleCopyPreferredLanguages();
		if (localeIDs)
		{
			CFStringRef localeID = (CFStringRef)CFArrayGetValueAtIndex(localeIDs, 0);
			char tmp[16];
			if (CFStringGetCString(localeID, tmp, 16, kCFStringEncodingUTF8))
				locale = std::string(tmp);
			CFRelease(localeIDs);
		}
#endif

#if defined(TARGET_OS_HARMATTAN)
		std::cerr << "Get GConf default client\n";
		GConfClient *gconf = gconf_client_get_default();
		locale = std::string(gconf_client_get_string(gconf, "/meegotouch/i18n/region", nullptr));
#elif defined(TARGET_BLACKBERRY)
		char *language = 0;
		char *country = 0;
		if (BPS_SUCCESS == locale_get(&language, &country) && language!= nullptr && country != nullptr) {
			std::stringstream ss;
			ss << language << "_" << country;
			locale = ss.str();

			bps_free(language);
			bps_free(country);
		}
#else
		char *cstr = getenv("LANG");
		if (cstr != nullptr)
			locale = cstr;
		if (locale.size() < 2)
		{
			cstr = getenv("LC_ALL");
			if (cstr != nullptr)
				locale = cstr;
		}

		if (locale == "zh-Hans") locale = "zh_CN"; //hack to make it work on iOS
		if (locale == "zh-Hant") locale = "zh_TW";
#endif
		i18n::load_translations();
	}

	void init()
	{
		locale = preferences::locale();
		if(locale == "system" || locale == "") {
			i18n::use_system_locale();
		} else {
			i18n::setLocale(locale);
		}
	}


	////////////////
	// UNIT TESTS //
	////////////////

	namespace {
		void CHECK_CATALOG(const map & answer) {
			for (auto & v : answer) {
				CHECK_EQ (tr(v.first), v.second);
			}

			for (auto & v : hashmap) {
				auto it = answer.find(v.first);
				CHECK_EQ (it != answer.end(), true);
				CHECK_EQ (it->second, v.second);
			}

			hashmap.clear();
		}

		void CHECK_FOR_PO_PARSE_ERROR(const std::string & doc) {
			try {
				const assert_recover_scope scope(SilenceAsserts);
				hashmap.clear();

				process_po_contents(doc);
			} catch (validation_failure_exception &) {
				hashmap.clear();
				return;
			}
			ASSERT_LOG(false, "failure was expected when parsing: \n***\n" << doc << "\n***\n");
		}
	}

	UNIT_TEST(po_parse_1)
	{
		hashmap.clear();
		process_po_contents("\
#foo\n\
#bar\n\
#baz\n\
msgid \"asdf\"\n\
msgstr \"jkl;\"\n\
\n\
\n\
#foo\n\
msgid \"foo\"\n\
msgstr \"bar\"\n\
\n\
msgid \"tmnt\"\n\
msgstr \"teenage\"\n\
\"mutant\"\n\
\"ninja\"\n\
\"turtles\"\n\
msgid \"a man\\n\"\n\
\"a plan\\n\"\n\
\"a canal\"\n\
msgstr \"panama\"");

		map answer;
		answer["asdf"] = "jkl;";
		answer["foo"] = "bar";
		answer["tmnt"] = "teenagemutantninjaturtles";
		answer["a man\na plan\na canal"] = "panama";

		CHECK_CATALOG(answer);
	}

	UNIT_TEST(po_parse_2)
	{
		hashmap.clear();
		process_po_contents("\
\t\t\n\
msgid \"he said \\\"she said.\\\"\"\n\
msgstr \"by the \\\"sea shore\\\"?\"\n\
\n\
\n\
#msgid blahlbahlbah\n\
msgid \"say what?\"\n\
# msgstr noooo\n\
    \n\
msgstr \"come again?\"\n\
\n\
\n\
msgid \"ignore me!\"");

		map answer;
		answer["he said \"she said.\""] = "by the \"sea shore\"?";
		answer["say what?"] = "come again?";

		CHECK_CATALOG(answer);
	}

	UNIT_TEST(po_parse_3)
	{
		hashmap.clear();
		process_po_contents("\
msgid \"veni vidi vici\"\n\
msgstr \"i came, i saw, i conquered\"\n\
msgid \"a tree falls\"\n\
msgstr \"\"\n\
msgid \"the sound of a tree falls\"\n\
msgstr \"\\0\"\n\
");

		map answer;
		answer["veni vidi vici"] = "i came, i saw, i conquered";
		answer["the sound of a tree falls"] = "";

		CHECK_CATALOG(answer);
	}

	UNIT_TEST(po_parse_error_reporting_1)
	{
		CHECK_FOR_PO_PARSE_ERROR("\
#foo\n\
#bar\n\
#baz\n\
msgid \"asdf\"\n\
msgstr \"jkl;\n\
\n\
\n\
#foo\n\
msgid \"foo\"\n\
msgstr \"bar\"");
	}

	UNIT_TEST(po_parse_error_reporting_2)
	{
		CHECK_FOR_PO_PARSE_ERROR("\
#foo\n\
#bar\n\
#baz\n\
msgi \"asdf\"\n\
msgstr \"jkl;\"\n\
\n\
\n\
#foo\n\
msgid \"foo\"\n\
msgstr \"bar\"");
	}

	UNIT_TEST(po_parse_error_reporting_3)
	{
		CHECK_FOR_PO_PARSE_ERROR("\
\n\
#bar\n\
#baz\n\
msgstr \"jkl;\"\n\
\n\
\n\
#foo\n\
msgid \"foo\"\n\
msgstr \"bar\"");
	}

	UNIT_TEST(po_parse_error_reporting_4)
	{
		CHECK_FOR_PO_PARSE_ERROR("\
   \n\
#bar\n\
#baz\n\
msgid \"asdf\"\"\n\
msgstr \"jkl;\"\n\
\n\
\n\
#foo\n\
msgid \"foo\"\n\
msgstr \"bar\"");
	}

	UNIT_TEST(po_parse_error_reporting_5)
	{
		CHECK_FOR_PO_PARSE_ERROR("\
\r\n\
#bar\n\
#baz\n\
msgid \"asdf\"\n\
msgtr \"jkl;\"\n\
\n\
\n\
#foo\n\
msgid \"foo\"\n\
msgstr \"bar\"");
	}

	UNIT_TEST(po_parse_error_reporting_6)
	{
		CHECK_FOR_PO_PARSE_ERROR("\
msgid \"asdf\"\n\
msgstr \"jkl;\"\n\
\n\
\n\
msgid \"foo\"\"bar\"\n\
msgstr \"baz\"");
	}

	namespace {
		void TEST_LOCALE_PROCESSING(std::string loc, const char ** const expected)
		{
			loc = trim_locale_charset(loc);
			for (const char ** ptr = expected; *ptr != nullptr; ptr++) {
				CHECK_EQ(loc, *ptr);
				loc = tweak_locale(loc);
			}
			CHECK_EQ(loc, "");
		}
	}

	UNIT_TEST(locale_processing)
	{
		{
			std::string loc = "ar";
			const char * expected [] = { "ar", nullptr };

			TEST_LOCALE_PROCESSING(loc, expected);
		}
		{
			std::string loc = "be_BY";
			const char * expected [] = { "be_BY", "be", nullptr };

			TEST_LOCALE_PROCESSING(loc, expected);
		}
		{
			std::string loc = "sr@latin";
			const char * expected [] = { "sr@latin" , "sr", nullptr };

			TEST_LOCALE_PROCESSING(loc, expected);
		}
		{
			std::string loc = "sr_RS@latin";
			const char * expected [] = { "sr_RS@latin" , "sr_RS", "sr", nullptr };

			TEST_LOCALE_PROCESSING(loc, expected);
		}
		{
			std::string loc = "sr_RS.UTF-8@latin";
			const char * expected [] = { "sr_RS@latin" , "sr_RS", "sr", nullptr };

			TEST_LOCALE_PROCESSING(loc, expected);
		}
	}
}
