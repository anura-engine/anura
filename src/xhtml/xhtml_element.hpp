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

#include <string>
#include <vector>

#include "RenderFwd.hpp"

#include "xhtml_fwd.hpp"
#include "xhtml_node.hpp"
#include "css_styles.hpp"

namespace xhtml
{
	// XXX should cache class, id, xml:id, lang, dir in the class structure.
	class Element : public Node
	{
	public:
		virtual ~Element();
		static ElementPtr create(const std::string& name, WeakDocumentPtr owner=WeakDocumentPtr());
		std::string toString() const override;
		ElementId getElementId() const { return tag_; }
		const std::string& getTag() const override { return name_; }
		const std::string& getName() const { return name_; }
		bool hasTag(const std::string& tag) const { return tag == name_; }
		bool hasTag(ElementId tag) const { return tag == tag_; }
	protected:
		explicit Element(ElementId id, const std::string& name, WeakDocumentPtr owner);
		std::string name_;
		ElementId tag_;
	};

	void add_custom_element(const std::string& e);
}
