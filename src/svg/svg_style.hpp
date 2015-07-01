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
#include <vector>

#include "FontFreetype.hpp"
#include "svg_fwd.hpp"
#include "svg_length.hpp"
#include "svg_paint.hpp"
#include "svg_render.hpp"
#include "uri.hpp"
#include "utils.hpp"

namespace KRE
{
	namespace SVG
	{
		class base_attrib
		{
		public:
			base_attrib() {}
			virtual ~base_attrib() {}
			virtual void apply(render_context& ctx) const = 0;
			virtual void clear(render_context& ctx) const = 0;
			virtual void resolve(const element* doc) = 0;
		private:
			DISALLOW_COPY_AND_ASSIGN(base_attrib);
		};

		class attribute_manager
		{
		public:
			attribute_manager(const base_attrib* attrib, render_context& ctx) 
				: attrib_(attrib), ctx_(ctx) {
				attrib_->apply(ctx_);
			}
			~attribute_manager() {
				attrib_->clear(ctx_);
			}
		private:
			const base_attrib* attrib_;
			render_context& ctx_;
		};

		enum class FontStyle {
			UNSET,
			INHERIT,
			NORMAL,
			ITALIC,
			OBLIQUE,
		};
		
		enum class FontVariant {
			UNSET,
			INHERIT,
			NORMAL,
			SMALL_CAPS,
		};

		enum class FontStretch {
			UNSET,
			INHERIT,
			WIDER,
			NARROWER,
			ULTRA_CONDENSED,
			EXTRA_CONDENSED,
			CONDENSED,
			SEMI_CONDENSED,
			NORMAL,
			SEMI_EXPANDED,
			EXPANDED,
			EXTRA_EXPANDED,
			ULTRA_EXPANDED,
		};

		enum class FontWeight {
			UNSET,
			INHERIT,
			BOLDER,
			LIGHTER,
			WEIGHT_100,
			WEIGHT_200,
			WEIGHT_300,
			WEIGHT_400,
			WEIGHT_500,
			WEIGHT_600,
			WEIGHT_700,
			WEIGHT_800,
			WEIGHT_900,
		};

		enum class FontSize {
			UNSET,
			INHERIT,
			XX_SMALL,
			X_SMALL,
			SMALL,
			MEDIUM,
			LARGE,
			X_LARGE,
			XX_LARGE,
			LARGER,
			SMALLER,
			VALUE,
		};

		enum class FontSizeAdjust {
			UNSET,
			INHERIT,
			NONE,
			VALUE,
		};

		class font_attribs : public base_attrib
		{
		public:
			font_attribs();
			explicit font_attribs(const boost::property_tree::ptree& pt);
			virtual ~font_attribs();
			virtual void apply(render_context& ctx) const override;
			virtual void clear(render_context& ctx) const override;
			virtual void resolve(const element* doc) override;
		private:
			std::vector<std::string> family_;
			FontStyle style_;
			FontVariant variant_;
			FontWeight weight_;
			FontStretch stretch_;
			FontSize size_;
			svg_length size_value_;
			FontSizeAdjust size_adjust_;
			svg_length size_adjust_value_;
		};

		enum class TextDirection {
			UNSET,
			INHERIT,
			LTR,
			RTL,
		};

		enum class UnicodeBidi {
			UNSET,
			INHERIT,
			NORMAL,
			EMBED,
			BIDI_OVERRIDE,
		};

		enum class TextSpacing {
			UNSET,
			INHERIT,
			NORMAL,
			VALUE,
		};

		enum class TextDecoration {
			UNSET,
			INHERIT,
			NONE,
			UNDERLINE,
			OVERLINE,
			LINE_THROUGH,
			BLINK,
		};

		enum class TextAlignmentBaseline {
			UNSET,
            INHERIT,
            AUTO,
            BASELINE,
            BEFORE_EDGE,
            TEXT_BEFORE_EDGE,
            MIDDLE,
            CENTRAL,
            AFTER_EDGE,
            TEXT_AFTER_EDGE,
            IDEOGRAPHIC,
            ALPHABETIC,
            HANGING,
            MATHEMATICAL,
		};

		enum class TextBaselineShift {
			UNSET,
            INHERIT,
            BASELINE,
            SUB,
            SUPER,
            VALUE,
		};

		enum class TextDominantBaseline {
			UNSET,
            INHERIT,
            AUTO,
            USE_SCRIPT,
            NO_CHANGE,
            RESET_SIZE,
            IDEOGRAPHIC,
            ALPHABETIC,
            HANGING,
            MATHEMATICAL,
            CENTRAL,
            MIDDLE,
            TEXT_AFTER_EDGE,
            TEXT_BEFORE_EDGE,
		};

		enum class GlyphOrientation {
			UNSET,
			INHERIT,
			AUTO,
			VALUE,
		};

		enum class WritingMode {
			UNSET,
			INHERIT,
			LR_TB,
			RL_TB,
			TB_RL,
			LR,
			RL,
			TB,
		};

		enum class Kerning {
			UNSET,
			INHERIT,
			AUTO,
			VALUE,
		};

		class text_attribs : public base_attrib
		{
		public:
			text_attribs(const boost::property_tree::ptree& pt);
			virtual ~text_attribs();
			void apply(render_context& ctx) const override;
			void clear(render_context& ctx) const override;
			void resolve(const element* doc) override;
		private:
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
			/*
				text-anchor
			*/
		};

		enum class Overflow {
			UNSET,
			INHERIT,
			VISIBLE,
			HIDDEN,
			SCROLL,
		};

		enum class Clip {
			UNSET,
			INHERIT,
			AUTO,
			SHAPE,
		};

		enum class Cursor {
			UNSET,
			INHERIT,
			AUTO,
			CROSSHAIR,
			DEFAULT,
			POINTER,
			MOVE,
			E_RESIZE,
			NE_RESIZE,
			NW_RESIZE,
			N_RESIZE,
			SE_RESIZE,
			SW_RESIZE,
			S_RESIZE,
			W_RESIZE,
			TEXT,
			WAIT,
			HELP,
		};

		enum class Display {
			UNSET,
			INHERIT,
			NONE,
			INLINE,
			BLOCK,
			LIST_ITEM,
			RUN_IN,
			COMPACT,
			MARKER,
			TABLE,
			INLINE_TABLE,
			TABLE_ROW_GROUP,
			TABLE_HEADER_GROUP,
			TABLE_FOOTER_GROUP,
			TABLE_ROW,
			TABLE_COLUMN_GROUP,
			TABLE_COLUMN,
			TABLE_CELL,
			TABLE_CAPTION,
		};

		enum class Visibility {
			UNSET,
			INHERIT,
			VISIBLE,
			HIDDEN,
			COLLAPSE,
		};

		class visual_attribs : public base_attrib
		{
		public:
			visual_attribs(const boost::property_tree::ptree& pt);
			virtual ~visual_attribs();
			void apply(render_context& ctx) const override;
			void clear(render_context& ctx) const override;
			void resolve(const element* doc) override;
		private:
			Overflow overflow_;
			Clip clip_;
			// if clip_ == Clip::SHAPE then use the following to define
			// clip shape box.
			svg_length clip_x1_;
			svg_length clip_y1_;
			svg_length clip_x2_;
			svg_length clip_y2_;
			std::vector<std::string> cursor_funciri_;
			Cursor cursor_;
			Display display_;
			Visibility visibility_;
			paint_ptr current_color_;
		};

		enum class FuncIriValue {
			UNSET,
			INHERIT,
			NONE,
			FUNC_IRI,
		};

		enum class ClipRule {
			UNSET,
			INHERIT,
			NON_ZERO,
			EVEN_ODD,
		};

		enum class OpacityAttrib {
			UNSET,
			INHERIT,
			VALUE,
		};

		class clipping_attribs : public base_attrib
		{
		public:
			clipping_attribs(const boost::property_tree::ptree& pt);
			virtual ~clipping_attribs();
			void apply(render_context& ctx) const override;
			void clear(render_context& ctx) const override;
			void resolve(const element* doc) override;
		private:
			FuncIriValue path_;
			std::string path_ref_;
			element_ptr path_resolved_;
			ClipRule rule_;
			FuncIriValue mask_;
			std::string mask_ref_;
			OpacityAttrib opacity_;
			double opacity_value_;
		};

		enum class Background {
			UNSET,
			INHERIT,
			ACCUMULATE,
			NEW,
		};
		
		class filter_effect_attribs : public base_attrib
		{
		public:
			filter_effect_attribs(const boost::property_tree::ptree& pt);
			virtual ~filter_effect_attribs();
			void apply(render_context& ctx) const override;
			void clear(render_context& ctx) const override;
			void resolve(const element* doc) override;
		private:
			Background enable_background_;
			// if enable_background_==NEW these contain the co-ordinates specified.
			svg_length x_;
			svg_length y_;
			svg_length w_;
			svg_length h_;
			FuncIriValue filter_;
			uri::uri filter_ref_;
			paint_ptr flood_color_;
			OpacityAttrib flood_opacity_;
			double flood_opacity_value_;
			paint_ptr lighting_color_;
		};

		enum class FillRuleAttrib {
			UNSET,
			INHERIT,
			NONZERO,
			EVENODD,
		};
		enum class LineJoinAttrib {
			UNSET,
			INHERIT,
			MITER,
			ROUND,
			BEVEL,
		};
		enum class LineCapAttrib {
			UNSET,
			INHERIT,
			BUTT,
			ROUND,
			SQUARE,
		};

		enum class StrokeWidthAttrib {
			UNSET,
			INHERIT,
			PERCENTAGE,
			VALUE,
		};

		enum class MiterLimitAttrib {
			UNSET,
			INHERIT,
			VALUE,
		};

		enum class DashArrayAttrib {
			UNSET,
			INHERIT,
			NONE,
			VALUE,
		};

		enum class DashOffsetAttrib {
			UNSET,
			INHERIT,
			VALUE,
		};

		enum class ColorInterpolationAttrib {
			UNSET,
			INHERIT,
			AUTO,
			sRGBA,
			linearRGBA,
		};
		
		enum class RenderingAttrib {
			UNSET,
			INHERIT,
			AUTO,
			OPTIMIZE_SPEED,
			OPTIMIZE_QUALITY,
		};

		enum class ShapeRenderingAttrib {
			UNSET,
			INHERIT,
			AUTO,
			OPTIMIZE_SPEED,
			CRISP_EDGES,
			GEOMETRIC_PRECISION,
		};

		enum class TextRenderingAttrib {
			UNSET,
			INHERIT,
			AUTO,
			OPTIMIZE_SPEED,
			OPTIMIZE_LEGIBILITY,
			GEOMETRIC_PRECISION,
		};

		enum class ColorProfileAttrib {
			UNSET,
			INHERIT,
			AUTO,
			sRGB,
			NAME,
			IRI,
		};

		class painting_properties : public base_attrib
		{
		public:
			painting_properties(const boost::property_tree::ptree& pt);
			virtual ~painting_properties();
			void apply(render_context& ctx) const override;
			void clear(render_context& ctx) const override;
			void resolve(const element* doc) override;
		private:
			// default none
			paint_ptr stroke_;
			OpacityAttrib stroke_opacity_;
			double stroke_opacity_value_;
			// default 1
			StrokeWidthAttrib stroke_width_;
			double stroke_width_value_;
			// default butt	
			LineCapAttrib stroke_linecap_;
			// default miter
			LineJoinAttrib stroke_linejoin_;
			// default 4.0
			MiterLimitAttrib stroke_miter_limit_;
			double stroke_miter_limit_value_;
			// default none
			DashArrayAttrib stroke_dash_array_;
			std::vector<svg_length> stroke_dash_array_value_;
			// default 0
			DashOffsetAttrib stroke_dash_offset_;
			svg_length stroke_dash_offset_value_;
			// default black
			paint_ptr fill_;
			// defualt even-odd
			FillRuleAttrib fill_rule_;
			// default 1.0
			OpacityAttrib fill_opacity_;
			double fill_opacity_value_;
			// default sRGB
			ColorInterpolationAttrib color_interpolation_;
			// default linearRGB
			ColorInterpolationAttrib color_interpolation_filters_;
			// default auto
			RenderingAttrib color_rendering_;
			// default auto
			ShapeRenderingAttrib shape_rendering_;
			// default auto
			TextRenderingAttrib text_rendering_;
			// default auto
			RenderingAttrib image_rendering_;
			// default auto
			ColorProfileAttrib color_profile_;
			std::string color_profile_value_;
		};

		// Apply to path, line, polyline and polygon elements.
		class marker_attribs : public base_attrib
		{
		public:
			marker_attribs(const boost::property_tree::ptree& pt);
			virtual ~marker_attribs();
			void apply(render_context& ctx) const override;
			void clear(render_context& ctx) const override;
			void resolve(const element* doc) override;
		private:
			FuncIriValue start_;
			uri::uri start_iri_;
			FuncIriValue mid_;
			uri::uri mid_iri_;
			FuncIriValue end_;
			uri::uri end_iri_;
		};
	}
}
