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

#define _(String) i18n::tr(String)

namespace i18n
{
	void init();

	const std::string& tr(const std::string& msgid);
	const std::string& get_locale();
	void use_system_locale();
	void setLocale(const std::string& l);
	void load_translations();
	bool load_extra_po(const std::string& module_dir); 	// search in given module dir for a file named '%locale.po'
								// where %locale is the current locale,
								// read it and add it to current dictionary.
								// returns true if it succeeds in finding a file.
	bool is_locale_cjk();
}
