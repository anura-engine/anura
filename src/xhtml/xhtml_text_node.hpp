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

#include "xhtml_node.hpp"

namespace KRE
{
	class FontHandle;
	typedef std::shared_ptr<FontHandle> FontHandlePtr;
};

namespace xhtml
{
	class Text : public Node
	{
	public:
		typedef std::vector<Word>::iterator iterator;
		static TextPtr create(const std::string& txt, WeakDocumentPtr owner=WeakDocumentPtr());
		void addText(const std::string& txt) { text_ += txt; }
		iterator begin() { return line_.line.begin(); }
		iterator end() { return line_.line.end(); }
		LinePtr reflowText(iterator& start, FixedPoint maximum_line_width, KRE::FontHandlePtr fh);
		void transformText(bool non_zero_width);
	protected:
		explicit Text(const std::string& txt, WeakDocumentPtr owner);
		std::string toString() const override;
		const std::string& getValue() const override { return text_; }
	private:
		bool transformed_;
		std::string text_;
		Line line_;
		bool break_at_line_;
	};
}
