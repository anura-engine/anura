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

#include "xhtml_fwd.hpp"

#include "geometry.hpp"

#include "display_list.hpp"
#include "xhtml.hpp"
#include "xhtml_background_info.hpp"
#include "xhtml_border_info.hpp"
#include "xhtml_style_tree.hpp"
#include "xhtml_render_ctx.hpp"

namespace xhtml
{
	struct EdgeSize
	{
		EdgeSize() : left(0), top(0), right(0), bottom(0) {}
		EdgeSize(FixedPoint l, FixedPoint t, FixedPoint r, FixedPoint b) : left(l), top(t), right(r), bottom(b) {}
		FixedPoint left;
		FixedPoint top;
		FixedPoint right;
		FixedPoint bottom;
	};

	struct Dimensions
	{
		Rect content_;
		EdgeSize padding_;
		EdgeSize border_;
		EdgeSize margin_;
	};

	enum class BoxId {
		BLOCK,
		LINE,
		TEXT,
		INLINE_BLOCK,
		INLINE_ELEMENT,
		ABSOLUTE,
		FIXED,
		LIST_ITEM,
		TABLE,
	};

	struct FloatList
	{
		FloatList() : left_(), right_() {}
		std::vector<BoxPtr> left_;
		std::vector<BoxPtr> right_;
	};

	class Box : public std::enable_shared_from_this<Box>
	{
	public:
		Box(BoxId id, BoxPtr parent, StyleNodePtr node);
		virtual ~Box() {}
		BoxId id() const { return id_; }
		const Dimensions& getDimensions() const { return dimensions_; }
		const std::vector<BoxPtr>& getChildren() const { return boxes_; }
		bool isBlockBox() const { return id_ == BoxId::BLOCK || id_ == BoxId::LIST_ITEM || id_ == BoxId::TABLE; }

		bool hasChildBlockBox() const;

		StyleNodePtr getStyleNode() const { return node_; }
		NodePtr getNode() const { return node_ != nullptr ? node_->getNode() : nullptr; }
		BoxPtr getParent() const { return parent_.lock(); }

		void addChild(BoxPtr box) { boxes_.emplace_back(box); }
		void addChildren(const std::vector<BoxPtr>& children) { boxes_.insert(boxes_.end(), children.begin(), children.end()); }
		void addAnonymousBoxes();

		void setContentRect(const Rect& r) { dimensions_.content_ = r; }
		void setContentX(FixedPoint x) { dimensions_.content_.x = x; }
		void setContentY(FixedPoint y) { dimensions_.content_.y = y; }
		void setContentWidth(FixedPoint w) { dimensions_.content_.width = w; }
		void setContentHeight(FixedPoint h) { dimensions_.content_.height = h; }

		void setPadding(const EdgeSize& e) { dimensions_.padding_ = e; }
		void setBorder(const EdgeSize& e) { dimensions_.border_ = e; }
		void setMargin(const EdgeSize& e) { dimensions_.margin_ = e; }

		void setBorderLeft(FixedPoint fp) { dimensions_.border_.left = fp; }
		void setBorderTop(FixedPoint fp) { dimensions_.border_.top = fp; }
		void setBorderRight(FixedPoint fp) { dimensions_.border_.right = fp; }
		void setBorderBottom(FixedPoint fp) { dimensions_.border_.bottom = fp; }

		void setPaddingLeft(FixedPoint fp) { dimensions_.padding_.left = fp; }
		void setPaddingTop(FixedPoint fp) { dimensions_.padding_.top = fp; }
		void setPaddingRight(FixedPoint fp) { dimensions_.padding_.right = fp; }
		void setPaddingBottom(FixedPoint fp) { dimensions_.padding_.bottom = fp; }

		void setMarginLeft(FixedPoint fp) { dimensions_.margin_.left = fp; }
		void setMarginTop(FixedPoint fp) { dimensions_.margin_.top = fp; }
		void setMarginRight(FixedPoint fp) { dimensions_.margin_.right = fp; }
		void setMarginBottom(FixedPoint fp) { dimensions_.margin_.bottom = fp; }

		void calculateVertMPB(FixedPoint containing_height);
		void calculateHorzMPB(FixedPoint containing_width);

		// These all refer to the content parameters
		FixedPoint getLeft() const { return dimensions_.content_.x; }
		FixedPoint getTop() const { return dimensions_.content_.y; }
		FixedPoint getWidth() const { return dimensions_.content_.width; }
		FixedPoint getHeight() const { return dimensions_.content_.height; }

		FixedPoint getMBPWidth() { 
			return dimensions_.margin_.left + dimensions_.margin_.right
				+ dimensions_.padding_.left + dimensions_.padding_.right
				+ dimensions_.border_.left + dimensions_.border_.right;
		}

		FixedPoint getMBPHeight() { 
			return dimensions_.margin_.top + dimensions_.margin_.bottom
				+ dimensions_.padding_.top + dimensions_.padding_.bottom
				+ dimensions_.border_.top + dimensions_.border_.bottom;
		}

		FixedPoint getMBPLeft() {
			return dimensions_.margin_.left
				+ dimensions_.padding_.left
				+ dimensions_.border_.left;
		}

		FixedPoint getMBPTop() {
			return dimensions_.margin_.top
				+ dimensions_.padding_.top
				+ dimensions_.border_.top;
		}

		FixedPoint getMBPBottom() {
			return dimensions_.margin_.bottom
				+ dimensions_.padding_.bottom
				+ dimensions_.border_.bottom;
		}

		FixedPoint getMBPRight() {
			return dimensions_.margin_.right
				+ dimensions_.padding_.right
				+ dimensions_.border_.right;
		}

		Rect getAbsBoundingBox() {
			return Rect(dimensions_.content_.x - getMBPLeft() + getOffset().x, 
				dimensions_.content_.y - getMBPTop() + getOffset().y, 
				getMBPWidth() + getWidth(),
				getMBPHeight() + getHeight());
		}

		static RootBoxPtr createLayout(StyleNodePtr node, int containing_width, int containing_height);

		void layout(LayoutEngine& eng, const Dimensions& containing);
		virtual std::string toString() const = 0;

		void addAbsoluteElement(LayoutEngine& eng, const Dimensions& containing, BoxPtr abs);

		void preOrderTraversal(std::function<void(BoxPtr, int)> fn, int nesting);
		bool ancestralTraverse(std::function<bool(const ConstBoxPtr&)> fn) const;

		const point& getOffset() const { return offset_; }

		void render(DisplayListPtr display_list, const point& offset) const;

		BorderInfo& getBorderInfo() { return border_info_; }
		const BorderInfo& getBorderInfo() const { return border_info_; }

		virtual FixedPoint getBaselineOffset() const { return dimensions_.content_.height; }
		virtual FixedPoint getBottomOffset() const { return dimensions_.content_.height; }

		FixedPoint getLineHeight() const { return line_height_; }
		bool isEOL() const { return end_of_line_; }
		void setEOL(bool eol=true) { end_of_line_ = eol; }
		virtual bool isMultiline() const { return false; }

		bool isReplaceable() const { return is_replaceable_; }

		bool isFloat() const { return node_ != nullptr && node_->getFloat() != css::Float::NONE; }

		// for text boxes
		virtual void justify(FixedPoint containing_width) {};
	protected:
		void clearChildren() { boxes_.clear(); } 
		virtual void handleRenderBackground(DisplayListPtr display_list, const point& offset) const;
		virtual void handleRenderBorder(DisplayListPtr display_list, const point& offset) const;
	private:
		virtual void handleLayout(LayoutEngine& eng, const Dimensions& containing) = 0;
		virtual void handlePreChildLayout2(LayoutEngine& eng, const Dimensions& containing) {}
		virtual void handlePreChildLayout(LayoutEngine& eng, const Dimensions& containing) {}
		virtual void handlePostChildLayout(LayoutEngine& eng, BoxPtr child) {}
		virtual void postParentLayout(LayoutEngine& eng, const Dimensions& containing) {}
		virtual void handleRender(DisplayListPtr display_list, const point& offset) const = 0;
		virtual void handleEndRender(DisplayListPtr display_list, const point& offset) const {}

		void setParent(BoxPtr parent) { parent_ = parent; }
		void init();

		BoxId id_;
		StyleNodePtr node_;
		std::weak_ptr<Box> parent_;
		Dimensions dimensions_;
		std::vector<BoxPtr> boxes_;
		std::vector<BoxPtr> absolute_boxes_;

		BackgroundInfo background_info_;
		BorderInfo border_info_;

		point offset_;
		FixedPoint line_height_;

		// Helper marker when doing LineBox layout
		bool end_of_line_;
		bool is_replaceable_;
	};

	std::ostream& operator<<(std::ostream& os, const Rect& r);
}
