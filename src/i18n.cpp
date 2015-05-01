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

			auto p = hashmap.insert(make_pair(msgid, msgstr));
			if (!p.second && msgstr != p.first->second) {
				LOG_DEBUG("i18n: Overwriting a translation of string \"" << msgid << "\":");
				LOG_DEBUG("i18n: Changing \"" << p.first->second << "\" to \"" << msgstr << "\"");
				p.first->second = msgstr;
			}
		}
	}

	// On input a string free of newline, expected to be wrapped in quotes,
	// with possible leading or trailing whitespace, and escaped characters (?),
	// this function should push to the stream the contents that were quoted,
	// and ideally flag errors... and handle utf-8 appropriately...
	void parse_quoted_string(std::stringstream & ss, const std::string & str) {
		static std::string whitespace = " \t\n\r";

		bool pre_string = true;
		bool post_string = false;

		for (std::string::const_iterator it = str.begin(); it != str.end(); it++) {
			if (pre_string || post_string) {
				if (*it == '\"') {
					if (post_string) {
						LOG_ERROR("i18n: Only one quoted string is allowed on a line of po file: \n<<" << str << ">>");
						return;
					}
					pre_string = false;
				} else if (whitespace.find(*it) == std::string::npos) {
					LOG_ERROR("i18n: Unexpected characters in po file where only whitespace is expected: \'" << *it << "\':\n<<" << str << ">>");
				}
			} else {
				if (*it == '\"') {
					post_string = true;
				} else if (*it == '\\') {
					it++;
					char c = *it;
					switch (c) {
						case 'n': {
							ss << '\n';
							break;
						}
						default:
							ss << c;
							break;
					}
				} else {
					ss << *it;
				}
			}
		}
		if (!pre_string && !post_string) {
			LOG_ERROR("i18n: unterminated quoted string in po file:\n<<" << str << ">>");
		}
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
				if (line.size() > 6 && line.substr(0,6) == "msgid ") {
					// This is the start of a new item, so store the previous item (if there was a previous item)
					if (current_item != PO_NONE) {
						std::string id = msgid.str(), str = msgstr.str();
						auto p = hashmap.insert(make_pair(id, str));
						if (str != p.first->second) {
							LOG_DEBUG("i18n: Overwriting a translation of string \"" << id << "\":");
							LOG_DEBUG("i18n: Changing \"" << p.first->second << "\" to \"" << str << "\"");
							p.first->second = str;
						}
						msgid.str("");
						msgstr.str("");
					}
					parse_quoted_string(msgid, line.substr(6));
					current_item = PO_MSGID;
				} else if (line.size() > 7 && line.substr(0,7) == "msgstr ") {
					if (current_item == PO_NONE) {
						LOG_ERROR("i18n: in po file, found a msgstr with no earlier msgid:\n<<" << line << ">>");
					}

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
						default:
							break;
					}
				}
			}
		}
	}
}

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

	void load_translations()
	{
		hashmap.clear();
		//strip the charset part of the country and language code,
		//e.g. "pt_BR.UTF8" --> "pt_BR"
		size_t found = locale.find(".");
		if (found != std::string::npos) {
			locale = locale.substr(0, found);
		}
		if (locale.size() < 2)
			return;

		std::vector<std::string> files;
		std::string dirname = "./locale/" + locale + "/LC_MESSAGES/";
		found = locale.find("@");

		module::get_files_in_dir(dirname, &files);
		if (!files.size() && found != std::string::npos) {
			locale = locale.substr(0, found);
			dirname = "./locale/" + locale + "/LC_MESSAGES/";
			module::get_files_in_dir(dirname, &files);
		}
		//strip the country code, e.g. "de_DE" --> "de"
		found = locale.find("_");
		if (!files.size() && found != std::string::npos) {
			locale = locale.substr(0, found);
			dirname = "./locale/" + locale + "/LC_MESSAGES/";
			module::get_files_in_dir(dirname, &files);
		}

		bool loaded_something = false;

		for(auto & file : files) {
			try {
				std::string extension = file.substr(file.find_last_of('.'));
				std::string path = dirname + file;
				ASSERT_LOG(sys::file_exists(module::map_file(path)), "confused... file does not exist which was found earlier: " << file);
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
			} catch (std::out_of_range &) {
				ASSERT_LOG(false, "bad file: " + file);
			}
		}

		if (!loaded_something) {
			LOG_WARN("did not find any translation files. \n locale = " << locale << "\n dirname = " << dirname);
		}
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
}
