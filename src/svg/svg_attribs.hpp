/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <boost/property_tree/ptree.hpp>
#include <string>

namespace KRE
{
	namespace SVG
	{
		class core_attribs
		{
		public:
			core_attribs(const boost::property_tree::ptree& pt);
			virtual ~core_attribs();

			const std::string& id() const { return id_; };
			const std::string& base() const { return xml_base_; };
			const std::string& lang() const { return xml_lang_; };
			const std::string& space() const { return xml_space_; };
		private:
			std::string id_;
			std::string xml_base_;
			std::string xml_lang_;
			std::string xml_space_;
		};
	}
}
