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
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <stdio.h>

#include "asserts.hpp"
#include "checksum.hpp"
#include "filesystem.hpp"
#include "md5.hpp"
#include "module.hpp"
#include "json_parser.hpp"
#include "unit_test.hpp"
#include "variant.hpp"

namespace checksum 
{

	namespace 
	{
		bool verified = false;
		std::map<std::string, std::string> hashes;
		std::string whole_game_signature;
		std::string	g_build_description;
	}

	manager::manager() 
	{
		try {
			whole_game_signature = md5::sum(sys::read_file("./signature.cfg"));
			variant v = json::parse_from_file("./signature.cfg");

			if(!v.is_map()) {
				verified = false;
				return;
			}

			if(v[variant("description")].is_string()) {
				g_build_description = v[variant("description")].as_string();
			}

			v = v[variant("signatures")];

			if(!v.is_map()) {
				verified = false;
				return;
			}

			std::vector<std::string> keys = v.getKeys().as_list_string();
			std::vector<std::string> values = v.getValues().as_list_string();
			ASSERT_EQ(keys.size(), values.size());
			for(int n = 0; n != keys.size(); ++n) {
				hashes[keys[n]] = values[n];
			}

			verified = true;
		} catch(...) {
			verified = false;
		}
	}

	manager::~manager() 
	{
		std::cerr << "EXITING WITH " << (verified ? "VERIFIED" : "UNVERIFIED") << " SESSION\n";
	}

	const std::string& build_description()
	{
		return g_build_description;
	}

	const std::string& game_signature()
	{
		return whole_game_signature;
	}

	bool is_verified()
	{
		return verified;
	}

	namespace 
	{
		bool both_slashes(char a, char b) {
			return a == '/' && b == '/';
		}
	}

	void verify_file(const std::string& fname_input, const std::string& contents)
	{
		if(!verified) {
			return;
		}

		std::string fname = fname_input;
		fname.erase(std::unique(fname.begin(), fname.end(), both_slashes), fname.end());

		if(fname.size() < 5 || std::string(fname.begin(), fname.begin()+5) != "data/") {
			return;
		}

		const std::map<std::string,std::string>::const_iterator itor = hashes.find(fname);
		if(itor == hashes.end()) {
			if(!contents.empty()) {
				std::cerr << "UNVERIFIED NEW FILE: " << fname << "\n";
				verified = false;
			}
			return;
		}

		verified = md5::sum(contents) == itor->second;
		if(!verified) {
			std::cerr << "UNVERIFIED FILE: " << fname << " (((" << contents << ")))\n";
		}
	}

	}

	namespace {
	void get_signatures(const std::string& dir, std::map<std::string, std::string>* results)
	{
		std::vector<std::string> files, dirs;
		module::get_files_in_dir(dir, &files, &dirs);
		for(const std::string& d : dirs) {
			get_signatures(dir + "/" + d, results);
		}

		for(const std::string& fname : files) {
			const std::string path = dir + "/" + fname;
			const std::string contents = sys::read_file(module::map_file(path));
			ASSERT_LOG(contents != "", "COULD NOT READ " << path);
			const std::string md5sum = md5::sum(contents);
			(*results)[path] = md5sum;
		}
	}
}

COMMAND_LINE_UTILITY(sign_game_data)
{
	if(args.size() != 1) {
		fprintf(stderr, "ERROR: PLEASE PROVIDE A UNIQUE TEXT DESCRIPTION OF THE BUILD YOU ARE SIGNING AS AN ARGUMENT\n");
		return;
	}

	std::map<std::string,std::string> signatures;
	get_signatures("data", &signatures);

	std::map<variant,variant> output;
	for(std::map<std::string,std::string>::const_iterator i = signatures.begin(); i != signatures.end(); ++i) {
		output[variant(i->first)] = variant(i->second);
	}

	std::map<variant,variant> info;
	info[variant("signatures")] = variant(&output);
	info[variant("description")] = variant(args.front());

	sys::write_file("signature.cfg", variant(&info).write_json());
}
