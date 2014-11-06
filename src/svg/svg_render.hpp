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

#pragma once

#include <cairo.h>
#include <stack>

#include "../asserts.hpp"
#include "../ft_iface.hpp"
#include "color.hpp"

namespace KRE
{
	namespace SVG
	{
		class paint;
		typedef std::shared_ptr<paint> paint_ptr;

		// Basically the concrete values that are set and stacked.
		class font_attribs_set 
		{
		public:
			font_attribs_set() {}
			~font_attribs_set() {}

			void push_font_size(double size) { size_.push(size); }
			void pop_font_size() { size_.pop(); }
			double top_font_size() { return size_.top(); }

			void push_font_face(FT_Face face) { face_.push(face); }
			void pop_font_face() { face_.pop(); }
			FT_Face top_font_face() { return face_.top(); }
		private:
			std::stack<double> size_;
			std::stack<FT_Face> face_;
		};

		class render_context
		{
		public:
			// cairo is the context, width/height are the physical size, in pixels, of
			// the drawing canvas.
			render_context(cairo_t* cairo, unsigned width, unsigned height)
				: cairo_(cairo),
				width_(width),
				height_(height)
			{}
			~render_context() {
				ASSERT_LOG(fill_color_stack_.empty(), "Fill color stack in rendering context not empty at exit");
				ASSERT_LOG(stroke_color_stack_.empty(), "Stroke color stack in rendering context not empty at exit");
			}

			cairo_t* cairo() { return cairo_; }
			
			void fill_color_push(const paint_ptr& p) {
				fill_color_stack_.emplace(p);
			}
			paint_ptr fill_color_pop() {
				auto p = fill_color_stack_.top();
				fill_color_stack_.pop();
				return p;
			}
			paint_ptr fill_color_top() const {
				return fill_color_stack_.top();
			}

			void stroke_color_push(const paint_ptr& p) {
				stroke_color_stack_.emplace(p);
			}
			paint_ptr stroke_color_pop() {
				auto p = stroke_color_stack_.top();
				stroke_color_stack_.pop();
				return p;
			}
			paint_ptr stroke_color_top() const {
				return stroke_color_stack_.top();
			}

			void opacity_push(double alpha) {
				opacity_stack_.push(alpha);
			}
			double opacity_pop() {
				double alpha = opacity_stack_.top();
				opacity_stack_.pop();
				return alpha;
			}
			double opacity_top() const {
				return opacity_stack_.top();
			}
			color_ptr get_current_color() const { return current_color_; }
			void set_current_color(color_ptr cc) { current_color_ = cc; }
			unsigned width() const { return width_; }
			unsigned height() const { return height_; }

			double letter_spacing_top() const { return letter_spacing_.top(); }
			void letter_spacing_push(double spacing) { letter_spacing_.emplace(spacing); }
			double letter_spacing_pop() {
				double spacing = letter_spacing_.top();
				letter_spacing_.pop();
				return spacing;
			}

			font_attribs_set& fa() { return font_attributes_; }
		private:
			cairo_t* cairo_;
			color_ptr current_color_;
			std::stack<paint_ptr> fill_color_stack_;
			std::stack<paint_ptr> stroke_color_stack_;
			std::stack<double> opacity_stack_;
			font_attribs_set font_attributes_;
			unsigned width_;
			unsigned height_;
			/* All the text context to store -- XXX fixme come up with a more elegant solution
			TextDirection direction_;
			UnicodeBidi bidi_;
			TextSpacing letter_spacing_;
			svg_length letter_spacing_value_;
			TextSpacing word_spacing_;
			svg_length word_spacing_value_;
			TextDecoration decoration_;
			TextAlignmentBaseline baseline_alignment_;
			TextBaselineShift baseline_shift_;
			TextDominantBaseline dominant_baseline_;
			GlyphOrientation glyph_orientation_vertical_;
			double glyph_orientation_vertical_value_;
			GlyphOrientation glyph_orientation_horizontal_;
			double glyph_orientation_horizontal_value_;
			WritingMode writing_mode_;
			Kerning kerning_;
			svg_length kerning_value_;
			*/
			std::stack<double> letter_spacing_;
		};

	}
}
