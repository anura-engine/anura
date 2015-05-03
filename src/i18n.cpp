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
	void parse_quoted_string(std::stringstream & ss, const std::string & str) {

		bool pre_string = true;
		bool post_string = false;

		for (std::string::const_iterator it = str.begin(); it != str.end(); it++) {
			if (pre_string || post_string) {
				if (*it == '\"') {
					ASSERT_LOG(!post_string, "i18n: Only one quoted string is allowed on a line of po file: \n<<" << str << ">>");
					pre_string = false;
				} else {
					ASSERT_LOG(is_po_whitespace(*it), "i18n: Unexpected characters in po file where only whitespace is expected: \'" << *it << "\':\n<<" << str << ">>");
				}
			} else {
				if (*it == '\"') {
					post_string = true;
				} else if (*it == '\\') {
					it++;
					ASSERT_LOG(it != str.end(), "i18n: po string terminated unexpectedly after escape character: \n<<" << str << ">>");
					char c = *it;
					switch (c) {
						case 'n': {
							ss << '\n';
							break;
						}
						case 't': {
							ss << '\t';
							break;
						}
						case '\'':
						case '\"':
						case '\\': {
							ss << c;
							break;
						}
						default: {
							ASSERT_LOG(false, "i18n: po string contained unrecognized escape sequence: \"\\" << c << "\": \n<<" << str << ">>");
						}
					}
				} else {
					ss << *it;
				}
			}
		}
		ASSERT_LOG(pre_string || post_string, "i18n: unterminated quoted string in po file:\n<<" << str << ">>");
	}

	enum po_item { PO_NONE, PO_MSGID, PO_MSGSTR };

	void process_po_contents(const std::string & content) {
		std::stringstream ss;
		ss.str(content);

		std::stringstream msgid;
		std::stringstream msgstr;

		po_item current_item = PO_NONE;

		for (std::string line; std::getline(ss, line); ) {
			if (line.size() > 0 && line[0] != '#') {
				if (line.size() >= 6 && line.substr(0,6) == "msgid ") {
					switch(current_item) {
						case PO_MSGID: {
							LOG_DEBUG("i18n: ignoring a MSGID which had no MSGSTR: " << msgid.str());
							break;
						}
						case PO_MSGSTR: { // This is the start of a new item, so store the previous item
							store_message(msgid.str(), msgstr.str());
							break;
						}
						case PO_NONE:
							break;
					}
					msgid.str("");
					msgstr.str("");
					parse_quoted_string(msgid, line.substr(6));
					current_item = PO_MSGID;
				} else if (line.size() >= 7 && line.substr(0,7) == "msgstr ") {
					ASSERT_LOG(current_item == PO_MSGID, "i18n: in po file, found a msgstr with no earlier msgid:\n<<" << line << ">>");

					parse_quoted_string(msgstr, line.substr(7));
					current_item = PO_MSGSTR;
				} else {
					switch (current_item) {
						case PO_MSGID: {
							parse_quoted_string(msgid, line);
							break;
						}
						case PO_MSGSTR: {
							parse_quoted_string(msgstr, line);
							break;
						}
						case PO_NONE: {
							for (const char c : line) {
								ASSERT_LOG(is_po_whitespace(c), "i18n: in po file, the first non-whitespace non-comment line should begin 'msgid ': \n<<" << line << ">>");
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
				store_message(msgid.str(), msgstr.str());
				break;
			}
			case PO_MSGID: {
				LOG_DEBUG("i18n: ignoring a MSGID which had no MSGSTR: " << msgid.str());
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

	UNIT_TEST(po_parse_error_reporting_1)
	{
		try {
			const assert_recover_scope scope(SilenceAsserts);
			hashmap.clear();
			process_po_contents("\
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
		} catch (validation_failure_exception &) {
			hashmap.clear();
			return;
		}
		ASSERT_LOG(false, "failure was expected");
	}

	UNIT_TEST(po_parse_error_reporting_2)
	{
		try {
			const assert_recover_scope scope(SilenceAsserts);
			hashmap.clear();
			process_po_contents("\
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
		} catch (validation_failure_exception &) {
			hashmap.clear();
			return;
		}
		ASSERT_LOG(false, "failure was expected");
	}

	UNIT_TEST(po_parse_error_reporting_3)
	{
		try {
			const assert_recover_scope scope(SilenceAsserts);
			hashmap.clear();
			process_po_contents("\
\n\
#bar\n\
#baz\n\
msgstr \"jkl;\"\n\
\n\
\n\
#foo\n\
msgid \"foo\"\n\
msgstr \"bar\"");
		} catch (validation_failure_exception &) {
			hashmap.clear();
			return;
		}
		ASSERT_LOG(false, "failure was expected");
	}

	UNIT_TEST(po_parse_error_reporting_4)
	{
		try {
			const assert_recover_scope scope(SilenceAsserts);
			hashmap.clear();
			process_po_contents("\
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
		} catch (validation_failure_exception &) {
			hashmap.clear();
			return;
		}
		ASSERT_LOG(false, "failure was expected");
	}

	UNIT_TEST(po_parse_error_reporting_5)
	{
		try {
			const assert_recover_scope scope(SilenceAsserts);
			hashmap.clear();
			process_po_contents("\
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
		} catch (validation_failure_exception &) {
			hashmap.clear();
			return;
		}
		ASSERT_LOG(false, "failure was expected");
	}

	UNIT_TEST(po_parse_error_reporting_6)
	{
		try {
			const assert_recover_scope scope(SilenceAsserts);
			hashmap.clear();
			process_po_contents("\
msgid \"asdf\"\n\
msgstr \"jkl;\"\n\
\n\
\n\
msgid \"foo\"\"bar\"\n\
msgstr \"baz\"");
		} catch (validation_failure_exception &) {
			hashmap.clear();
			return;
		}
		ASSERT_LOG(false, "failure was expected");
	}


#define TEST_LOCALE_PROCESSING \
do { \
loc = trim_locale_charset(loc); \
for (const char ** ptr = expected; *ptr != nullptr; ptr++) { \
	CHECK_EQ(loc, *ptr); \
	loc = tweak_locale(loc); \
} \
CHECK_EQ(loc, ""); \
} while(0)

	UNIT_TEST(locale_processing)
	{
		{
			std::string loc = "ar";
			const char * expected [] = { "ar", nullptr };

			TEST_LOCALE_PROCESSING;
		}
		{
			std::string loc = "be_BY";
			const char * expected [] = { "be_BY", "be", nullptr };

			TEST_LOCALE_PROCESSING;
		}
		{
			std::string loc = "sr@latin";
			const char * expected [] = { "sr@latin" , "sr", nullptr };

			TEST_LOCALE_PROCESSING;
		}
		{
			std::string loc = "sr_RS@latin";
			const char * expected [] = { "sr_RS@latin" , "sr_RS", "sr", nullptr };

			TEST_LOCALE_PROCESSING;
		}
		{
			std::string loc = "sr_RS.UTF-8@latin";
			const char * expected [] = { "sr_RS@latin" , "sr_RS", "sr", nullptr };

			TEST_LOCALE_PROCESSING;
		}
	}
}
