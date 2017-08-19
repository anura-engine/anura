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

#include <memory>

namespace xhtml
{
	typedef int FixedPoint;

	class RenderContext;

	class Node;
	typedef std::shared_ptr<Node> NodePtr;
	typedef std::weak_ptr<Node> WeakNodePtr;

	class Document;
	typedef std::shared_ptr<Document> DocumentPtr;
	typedef std::weak_ptr<Document> WeakDocumentPtr;

	class DocumentFragment;
	typedef std::shared_ptr<DocumentFragment> DocumentFragmentPtr;

	class Element;
	typedef std::shared_ptr<Element> ElementPtr;

	class Text;
	typedef std::shared_ptr<Text> TextPtr;

	class Attribute;
	typedef std::shared_ptr<Attribute> AttributePtr;

	struct Rect {
		Rect() : x(0), y(0), width(0), height(0) {}
		Rect(FixedPoint xx, FixedPoint yy, FixedPoint ww, FixedPoint hh) : x(xx), y(yy), width(ww), height(hh) {}
		FixedPoint x;
		FixedPoint y;
		FixedPoint width;
		FixedPoint height;
	};

	class LayoutEngine;
	class Box;
	class RootBox;
	class LineBox;
	class TextBox;
	typedef std::shared_ptr<Box> BoxPtr;
	typedef std::shared_ptr<const Box> ConstBoxPtr;
	typedef std::shared_ptr<RootBox> RootBoxPtr;
	typedef std::shared_ptr<LineBox> LineBoxPtr;
	typedef std::shared_ptr<TextBox> TextBoxPtr;

	struct Dimensions;

	class StyleNode;
	typedef std::shared_ptr<StyleNode> StyleNodePtr;
	typedef std::weak_ptr<StyleNode> WeakStyleNodePtr;

	class Script;
	typedef std::shared_ptr<Script> ScriptPtr;
}
