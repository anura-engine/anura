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

#include "xhtml_layout_engine.hpp"
#include "xhtml_absolute_box.hpp"
#include "xhtml_block_box.hpp"
#include "xhtml_inline_block_box.hpp"
#include "xhtml_inline_element_box.hpp"
#include "xhtml_listitem_box.hpp"
#include "xhtml_line_box.hpp"
#include "xhtml_root_box.hpp"
#include "xhtml_text_box.hpp"
#include "xhtml_text_node.hpp"

namespace xhtml
{
	// This is to ensure our fixed-point type will have enough precision.
	static_assert(sizeof(FixedPoint) < 32, "An int must be greater than or equal to 32 bits");

	using namespace css;

	namespace 
	{
		std::string display_string(Display disp) {
			switch(disp) {
				case Display::BLOCK:				return "block";
				case Display::INLINE:				return "inline";
				case Display::INLINE_BLOCK:			return "inline-block";
				case Display::LIST_ITEM:			return "list-item";
				case Display::TABLE:				return "table";
				case Display::INLINE_TABLE:			return "inline-table";
				case Display::TABLE_ROW_GROUP:		return "table-row-group";
				case Display::TABLE_HEADER_GROUP:	return "table-header-group";
				case Display::TABLE_FOOTER_GROUP:	return "table-footer-group";
				case Display::TABLE_ROW:			return "table-row";
				case Display::TABLE_COLUMN_GROUP:	return "table-column-group";
				case Display::TABLE_COLUMN:			return "table-column";
				case Display::TABLE_CELL:			return "table-cell";
				case Display::TABLE_CAPTION:		return "table-caption";
				case Display::NONE:					return "none";
				default: 
					ASSERT_LOG(false, "illegal display value: " << static_cast<int>(disp));
					break;
			}
			return "none";
		}

		template<typename T>
		struct StackManager
		{
			StackManager(std::stack<T>& counter, const T& defa=T()) : counter_(counter) { counter_.emplace(defa); }
			~StackManager() { counter_.pop(); }
			std::stack<T>& counter_;
		};
	}
	
	// XXX We're not handling text alignment or justification yet.
	LayoutEngine::LayoutEngine() 
		: root_(nullptr), 
		  dims_(), 
		  ctx_(RenderContext::get()), 
		  list_item_counter_(),
		  offset_(),
		  float_list_()
	{
		list_item_counter_.emplace(0);
		offset_.emplace(point());
		float_list_.emplace(FloatList());
	}

	void LayoutEngine::layoutRoot(StyleNodePtr node, BoxPtr parent, const point& container) 
	{
		if(root_ == nullptr) {
			root_ = std::make_shared<RootBox>(nullptr, node);
			dims_.content_ = Rect(0, 0, container.x, container.y);

			Dimensions root_dims;
			root_dims.content_.width = container.x;

			root_->layout(*this, root_dims);
			return;
		}
	}
	
	std::vector<BoxPtr> LayoutEngine::layoutChildren(const std::vector<StyleNodePtr>& children, BoxPtr parent, LineBoxPtr& open_box)
	{
		StackManager<point> offset_manager(offset_, point(parent->getLeft(), parent->getTop()) + offset_.top());

		std::vector<BoxPtr> res;
		for(auto it = children.begin(); it != children.end(); ++it) {
			auto child = *it;
			auto node = child->getNode();
			ASSERT_LOG(node != nullptr, "Something went wrong, there was a StyleNode without an associated DOM node.");
			if(node->id() == NodeId::ELEMENT) {
				if(node->ignoreForLayout()) {
					continue;
				}
				// Adjust counters for list items as needed
				std::unique_ptr<StackManager<int>> li_manager;
				if(node->hasTag(ElementId::UL) || node->hasTag(ElementId::OL)) {
					li_manager.reset(new StackManager<int>(list_item_counter_, 0));
				}
				if(node->hasTag(ElementId::LI) ) {
					auto &top = list_item_counter_.top();
					++top;
				}

				const Display display = child->getDisplay();
				const Float cfloat = child->getFloat();
				const Position position = child->getPosition();

				if(display == Display::NONE) {
					// Do not create a box for this or it's children
					// early return
					continue;
				}

				if(position == Position::ABSOLUTE_POS) {
					// absolute positioned elements are taken out of the normal document flow
					parent->addAbsoluteElement(*this, parent->getDimensions(), std::make_shared<AbsoluteBox>(parent, child));
				} else if(position == Position::FIXED) {
					// fixed positioned elements are taken out of the normal document flow
					root_->addFixed(std::make_shared<BlockBox>(parent, child));
				} else {
					if(cfloat != Float::NONE) {
						// XXX need to add an offset to position for the float box based on body margin.
						// N.B. if the current display is one of the CssDisplay::TABLE* styles then this should be
						// a table box rather than a block box. Inline boxes are going to get wrapped in a BlockBox
						
						if(display == Display::BLOCK) {
							res.emplace_back(std::make_shared<BlockBox>(parent, child));
						} else if(display == Display::LIST_ITEM) {
							res.emplace_back(std::make_shared<ListItemBox>(parent, child, list_item_counter_.top()));
						} else if(display == Display::TABLE) {
							//root_->addFloatBox(*this, std::make_shared<TableBox>(parent, child), cfloat, offset_.top().x, offset_.top().y + (open_box != nullptr ? open_box->getCursor().y : 0));
							ASSERT_LOG(false, "Implement Table display");
						} else {
							// default to using a block box to wrap content.
							res.emplace_back(std::make_shared<BlockBox>(parent, child));
						}
						continue;
					}
					switch(display) {
						case Display::NONE:
							// Do not create a box for this or it's children
							break;
						case Display::INLINE: {
							if(node->isReplaced()) {
								// replaced elements should generate a box.
								// XXX should these go into open_box?
								res.emplace_back(std::make_shared<InlineElementBox>(parent, child));
							} else {
								// non-replaced elements we just generate children and add them.
								for(auto& inline_child : child->getChildren()) {
									NodePtr inline_node = inline_child->getNode();
									if(inline_node != nullptr && inline_node->id() == NodeId::TEXT) {
										inline_child->inheritProperties(child);
									}
								}
								std::vector<BoxPtr> new_children = layoutChildren(child->getChildren(), parent, open_box);
								res.insert(res.end(), new_children.begin(), new_children.end());
							}
							break;
						}
						case Display::BLOCK: {
							if(open_box) {
								if(!open_box->getChildren().empty()) {
									res.emplace_back(open_box);
								}
								open_box.reset();
							}
							res.emplace_back(std::make_shared<BlockBox>(parent, child));
							break;
						}
						case Display::INLINE_BLOCK: {
							if(open_box == nullptr) {
								open_box = std::make_shared<LineBox>(parent);
							}
							auto ibb = std::make_shared<InlineBlockBox>(parent, child);
							ibb->layout(*this, parent->getDimensions());
							open_box->addChild(ibb);
							break;
						}
						case Display::LIST_ITEM: {
							if(open_box) {
								if(!open_box->getChildren().empty()) {
									res.emplace_back(open_box);
								}
								open_box.reset();
							}
							res.emplace_back(std::make_shared<ListItemBox>(parent, child, list_item_counter_.top()));
							break;
						}
						case Display::TABLE:
						case Display::INLINE_TABLE:
						case Display::TABLE_ROW_GROUP:
						case Display::TABLE_HEADER_GROUP:
						case Display::TABLE_FOOTER_GROUP:
						case Display::TABLE_ROW:
						case Display::TABLE_COLUMN_GROUP:
						case Display::TABLE_COLUMN:
						case Display::TABLE_CELL:
						case Display::TABLE_CAPTION:
							ASSERT_LOG(false, "FIXME: LayoutEngine::formatNode(): " << display_string(display));
							break;
						default:
							ASSERT_LOG(false, "illegal display value: " << static_cast<int>(display));
							break;
					}
				}
			} else if(node->id() == NodeId::TEXT) {
				TextPtr tnode = std::dynamic_pointer_cast<Text>(node);
				ASSERT_LOG(tnode != nullptr, "Logic error, couldn't up-cast node to Text.");

				tnode->transformText(true);
				if(open_box == nullptr) {
					open_box = std::make_shared<LineBox>(parent);
				}
				auto txt = std::make_shared<TextBox>(open_box, child);
				open_box->addChild(txt);
			} else {
				ASSERT_LOG(false, "Unhandled node id, only elements and text can be used in layout: " << static_cast<int>(node->id()));
			}
		}
		return res;
	}

	FixedPoint LayoutEngine::getDescent() const 
	{
		return ctx_.getFontHandle()->getDescender();
	}

	void LayoutEngine::addFloat(BoxPtr float_box)
	{
		ASSERT_LOG(!float_list_.empty(), "Empty float list.");
		if(float_box->getStyleNode()->getFloat() == Float::LEFT) {
			float_list_.top().left_.emplace_back(float_box);
		} else {
			float_list_.top().right_.emplace_back(float_box);
		}
	}

	FixedPoint LayoutEngine::getXAtPosition(FixedPoint y1, FixedPoint y2) const 
	{
		FixedPoint x = 0;
		// since we expect only a small number of floats per element
		// a linear search through them seems fine at this point.
		for(auto& lf : getFloatList().left_) {
			auto& dims = lf->getDimensions();
			auto bb = lf->getAbsBoundingBox();
			if((y1 >= bb.y && y1 <= (bb.y + bb.height)) || (y2 >= bb.y && y2 <= (bb.y + bb.height))) {
				x = std::max(x, lf->getMBPWidth() + lf->getDimensions().content_.x + dims.content_.width);
			}
		}
		return x;
	}

	FixedPoint LayoutEngine::getX2AtPosition(FixedPoint y1, FixedPoint y2) const 
	{
		FixedPoint x2 = dims_.content_.width;
		// since we expect only a small number of floats per element
		// a linear search through them seems fine at this point.
		for(auto& rf : getFloatList().right_) {
			auto& dims = rf->getDimensions();
			auto bb = rf->getAbsBoundingBox();
			if((y1 >= bb.y && y1 <= (bb.y + bb.height)) || (y2 >= bb.y && y2 <= (bb.y + bb.height))) {
				x2 = std::min(x2, rf->getMBPWidth() + dims.content_.width);
			}
		}
		return x2;
	}

	FixedPoint LayoutEngine::getWidthAtPosition(FixedPoint y1, FixedPoint y2, FixedPoint width) const 
	{
		// since we expect only a small number of floats per element
		// a linear search through them seems fine at this point.
		for(auto& lf : getFloatList().left_) {
			auto bb = lf->getAbsBoundingBox();
			if((y1 >= bb.y && y1 <= (bb.y + bb.height)) || (y2 >= bb.y && y2 <= (bb.y + bb.height))) {
				width -= lf->getMBPWidth() + lf->getDimensions().content_.width;
			}
		}
		for(auto& rf : getFloatList().right_) {
			auto& dims = rf->getDimensions();
			auto bb = rf->getAbsBoundingBox();
			if((y1 >= bb.y && y1 <= (bb.y + bb.height)) || (y2 >= bb.y && y2 <= (bb.y + bb.height))) {
				width -= rf->getMBPWidth() + dims.content_.width;
			}
		}
		return width < 0 ? 0 : width;
	}

	const point& LayoutEngine::getOffset()
	{
		ASSERT_LOG(!offset_.empty(), "There was no item on the offset stack -- programmer logic bug.");
		return offset_.top();
	}

	void LayoutEngine::moveCursorToClearFloats(Clear float_clear, point& cursor)
	{
		FixedPoint new_y = cursor.y;
		if(float_clear == Clear::LEFT || float_clear == Clear::BOTH) {
			for(auto& lf : getFloatList().left_) {
				new_y = std::max(new_y, lf->getMBPHeight() + lf->getOffset().y + lf->getDimensions().content_.y + lf->getDimensions().content_.height);
			}
		}
		if(float_clear == Clear::RIGHT || float_clear == Clear::BOTH) {
			for(auto& rf : getFloatList().right_) {
				new_y = std::max(new_y, rf->getMBPHeight() + rf->getOffset().y + rf->getDimensions().content_.y + rf->getDimensions().content_.height);
			}
		}
		if(new_y != cursor.y) {
			const FixedPoint y1 = new_y + offset_.top().y;
			cursor = point(getXAtPosition(y1, y1), new_y);
		}
	}

	bool LayoutEngine::hasFloatsAtPosition(FixedPoint y1, FixedPoint y2) const
	{
		for(auto& lf : getFloatList().left_) {
			auto& dims = lf->getDimensions();
			auto bb = lf->getAbsBoundingBox();
			if((y1 >= bb.y && y1 <= (bb.y + bb.height)) || (y2 >= bb.y && y2 <= (bb.y + bb.height))) {
				return true;
			}
		}
		for(auto& rf : getFloatList().right_) {
			auto& dims = rf->getDimensions();
			auto bb = rf->getAbsBoundingBox();
			if((y1 >= bb.y && y1 <= (bb.y + bb.height)) || (y2 >= bb.y && y2 <= (bb.y + bb.height))) {
				return true;
			}
		}
		return false;
	}

	const FloatList& LayoutEngine::getFloatList() const 
	{ 
		ASSERT_LOG(!float_list_.empty(), "Float list was empty!");
		return float_list_.top(); 
	}
}
