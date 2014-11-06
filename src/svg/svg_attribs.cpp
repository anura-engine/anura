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
     in a product, an acknowledgement in the product documentation would be
     appreciated but is not required.

     2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.

     3. This notice may not be removed or altered from any source
     distribution.
*/

#include "svg_attribs.hpp"

namespace KRE
{
	namespace SVG
	{
		using namespace boost::property_tree;

		core_attribs::core_attribs(const ptree& pt)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");

			if(attributes) {
				auto id = attributes->get_child_optional("id");
				if(id) {
					id_ = id->data();
				}
				auto xml_base = attributes->get_child_optional("xml:base");
				if(xml_base) {
					xml_base_ = xml_base->data();
				}
				auto xml_lang = attributes->get_child_optional("xml:lang");
				if(xml_lang) {
					xml_lang_ = xml_lang->data();
				}
				auto xml_space = attributes->get_child_optional("xml:space");
				if(xml_space) {
					xml_space_ = xml_space->data();
				}
			}
		}

		core_attribs::~core_attribs()
		{
		}
	}
}
