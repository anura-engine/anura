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

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

#include <cairo-ft.h>

#include "logger.hpp"
#include "svg_element.hpp"
#include "svg_style.hpp"

namespace KRE
{
	namespace SVG
	{
		namespace
		{
			FuncIriValue parse_func_iri_value(const std::string& value, uri::uri& iri)
			{
				FuncIriValue ret = FuncIriValue::NONE;
				if(value == "inherit") {
					ret = FuncIriValue::INHERIT;
				} else if(value == "none") {
					// use default
				} else {
					if(value.substr(0,4) == "url(" && value.back() == ')') {
						iri = uri::uri::parse(value.substr(4, value.size()-5));
					} else {
						LOG_WARN("No url found when parsing FuncIRI value: " << value);
					}
					ret = FuncIriValue::FUNC_IRI;
				}
				return ret;
			}
		}

		using namespace boost::property_tree;

		font_attribs::font_attribs(const ptree& pt)
			: style_(FontStyle::NORMAL),
			variant_(FontVariant::NORMAL),
			stretch_(FontStretch::NORMAL),
			weight_(FontWeight::WEIGHT_400),
			size_(FontSize::MEDIUM),
			size_adjust_(FontSizeAdjust::NONE)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto font_weight = attributes->get_child_optional("font-weight");
				if(font_weight) {
					const std::string& fw = font_weight->data();
					if(fw == "inherit") {
						weight_ = FontWeight::INHERIT;
					} else if(fw == "bold") {
						weight_ = FontWeight::WEIGHT_700;
					} else if(fw == "normal") {
						weight_ = FontWeight::WEIGHT_400;
					} else if(fw == "bolder") {
						weight_ = FontWeight::BOLDER;
					} else if(fw == "lighter") {
						weight_ = FontWeight::LIGHTER;
					} else if(fw == "100") {
						weight_ = FontWeight::WEIGHT_100;
					} else if(fw == "200") {
						weight_ = FontWeight::WEIGHT_200;
					} else if(fw == "300") {
						weight_ = FontWeight::WEIGHT_300;
					} else if(fw == "400") {
						weight_ = FontWeight::WEIGHT_400;
					} else if(fw == "500") {
						weight_ = FontWeight::WEIGHT_500;
					} else if(fw == "600") {
						weight_ = FontWeight::WEIGHT_600;
					} else if(fw == "700") {
						weight_ = FontWeight::WEIGHT_700;
					} else if(fw == "800") {
						weight_ = FontWeight::WEIGHT_800;
					} else if(fw == "900") {
						weight_ = FontWeight::WEIGHT_900;
					}
				}

				auto font_variant = attributes->get_child_optional("font-variant");
				if(font_variant) {
					const std::string& fv = font_variant->data();
					if(fv == "inherit") {
						variant_ = FontVariant::INHERIT;
					} else if(fv == "normal") {
						variant_ = FontVariant::NORMAL;
					} else if(fv == "small-caps") {
						variant_ = FontVariant::SMALL_CAPS;
					}
				}

				auto font_style = attributes->get_child_optional("font-style");
				if(font_style) {
					const std::string& fs = font_style->data();
					if(fs == "inherit") {
						style_ = FontStyle::INHERIT;
					} else if(fs == "normal") {
						style_ = FontStyle::NORMAL;
					} else if(fs == "italic") {
						style_ = FontStyle::ITALIC;
					} else if(fs == "oblique") {
						style_ = FontStyle::OBLIQUE;
					}
				}

				auto font_stretch = attributes->get_child_optional("font-stretch");
				if(font_variant) {
					const std::string& fs = font_stretch->data();
					if(fs == "inherit") {
						stretch_ = FontStretch::INHERIT;
					} else if(fs == "normal") {
						stretch_ = FontStretch::NORMAL;
					} else if(fs == "wider") {
						stretch_ = FontStretch::WIDER;
					} else if(fs == "narrower") {
						stretch_ = FontStretch::NARROWER;
					} else if(fs == "ultra-condensed") {
						stretch_ = FontStretch::ULTRA_CONDENSED;
					} else if(fs == "extra-condensed") {
						stretch_ = FontStretch::EXTRA_CONDENSED;
					} else if(fs == "condensed") {
						stretch_ = FontStretch::CONDENSED;
					} else if(fs == "semi-condensed") {
						stretch_ = FontStretch::SEMI_CONDENSED;
					} else if(fs == "semi-expanded") {
						stretch_ = FontStretch::SEMI_EXPANDED;
					} else if(fs == "expanded") {
						stretch_ = FontStretch::EXPANDED;
					} else if(fs == "extra-expanded") {
						stretch_ = FontStretch::EXTRA_EXPANDED;
					} else if(fs == "ultra-expanded") {
						stretch_ = FontStretch::ULTRA_EXPANDED;
					}
				}

				auto font_size = attributes->get_child_optional("font-size");
				if(font_size) {
					const std::string& fs = font_size->data();
					if(fs == "inherit") {
						size_ = FontSize::INHERIT;
					} else if(fs == "xx-small") {
						size_ = FontSize::XX_SMALL;
					} else if(fs == "x-small") {
						size_ = FontSize::X_SMALL;
					} else if(fs == "small") {
						size_ = FontSize::SMALL;
					} else if(fs == "medium") {
						size_ = FontSize::MEDIUM;
					} else if(fs == "large") {
						size_ = FontSize::LARGE;
					} else if(fs == "x-large") {
						size_ = FontSize::X_LARGE;
					} else if(fs == "xx-large") {
						size_ = FontSize::XX_LARGE;
					} else if(fs == "larger") {
						size_ = FontSize::LARGER;
					} else if(fs == "smaller") {
						size_ = FontSize::SMALLER;
					} else {
						size_ = FontSize::VALUE;
						size_value_ = svg_length(fs);
					}
				}

				auto font_family = attributes->get_child_optional("font-family");
				if(font_family) {
					boost::char_separator<char> seperators("\n\t\r ,");
					boost::tokenizer<boost::char_separator<char>> tok(font_family->data(), seperators);
					for(auto& t : tok) {
						family_.push_back(t);
						boost::replace_all(family_.back(), "'", "");
					}
					std::cerr << "font-family: " << family_.back() << "\n";
				}

				auto font_size_adjust = attributes->get_child_optional("font-size-adjust");
				if(font_size_adjust) {
					const std::string& fsa = font_size_adjust->data();
					if(fsa == "inherit") {
						size_adjust_ = FontSizeAdjust::INHERIT;
					} else if(fsa == "none") {
						size_adjust_ = FontSizeAdjust::NONE;
					} else {
						size_adjust_ = FontSizeAdjust::VALUE;
						size_adjust_value_ = svg_length(fsa);
					}
				}

				if(attributes->get_child_optional("font")) {
					ASSERT_LOG(false, "'font' attribute unimplemented.");
				}
			}
		}

		font_attribs::~font_attribs()
		{
		}

		void font_attribs::apply(render_context& ctx) const
		{
			bool font_set = false;
			for(auto& family : family_) {
				FT_Face face = FT::get_font_face(family);
				if(face) {
					auto ff = cairo_ft_font_face_create_for_ft_face(face, 0);
					cairo_set_font_face(ctx.cairo(), ff);

					ctx.fa().push_font_face(face);
					font_set = true;
					break;
				}
			}
			ASSERT_LOG(font_set == true, "Couldn't set requested font.");
			double size = 0;
			switch(size_)
			{
			case FontSize::UNSET:		/* do nothing */ break;
			case FontSize::INHERIT:		/* do nothing */ break;
			case FontSize::XX_SMALL:	size = 6.9; break;
			case FontSize::X_SMALL:		size = 8.3; break;
			case FontSize::SMALL:		size = 10; break;
			case FontSize::MEDIUM:		size = 12; break;
			case FontSize::LARGE:		size = 14.4; break;
			case FontSize::X_LARGE:		size = 17.3; break;
			case FontSize::XX_LARGE:	size = 20.7; break;
			case FontSize::LARGER:		size = ctx.fa().top_font_size() * 1.2; break;
			case FontSize::SMALLER:		size = ctx.fa().top_font_size() / 1.2; break;
			case FontSize::VALUE:
				size = size_value_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER);
				break;
			default: break;
			}
			if(size > 0) {
				ctx.fa().push_font_size(size);
				cairo_set_font_size(ctx.cairo(), size);
			}
			// XXX use font_size_adjust if defined to scale the height of X in the chosen font to the given value.
			// seems rather annoyingly all-in-all.
		}

		void font_attribs::clear(render_context& ctx) const
		{
			// XXX
			if(size_ != FontSize::UNSET && size_ != FontSize::INHERIT) {
				ctx.fa().pop_font_size();
			}
			if(family_.size() > 0 && !family_[0].empty()) {
				ctx.fa().pop_font_face();
			}
		}

		void font_attribs::resolve(const element* doc)
		{
			// nothing need be done
		}

		text_attribs::text_attribs(const ptree& pt)
			: direction_(TextDirection::LTR),
			bidi_(UnicodeBidi::NORMAL),
			letter_spacing_(TextSpacing::NORMAL),
			word_spacing_(TextSpacing::NORMAL),
			decoration_(TextDecoration::NONE),
			baseline_alignment_(TextAlignmentBaseline::AUTO),
			baseline_shift_(TextBaselineShift::BASELINE),
			dominant_baseline_(TextDominantBaseline::AUTO),
			glyph_orientation_vertical_(GlyphOrientation::AUTO),
			glyph_orientation_vertical_value_(0),
			glyph_orientation_horizontal_(GlyphOrientation::AUTO),
			glyph_orientation_horizontal_value_(0),
			writing_mode_(WritingMode::LR_TB),
			kerning_(Kerning::AUTO)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto direction = attributes->get_child_optional("direction");
				if(direction) {
					const std::string& dir = direction->data();
					if(dir == "inherit") {
						direction_ = TextDirection::INHERIT;
					} else if(dir == "ltr") {
						direction_ = TextDirection::LTR;
					} else if(dir == "rtl") {
						direction_ = TextDirection::RTL;
					}
				}

				auto unicode_bidi = attributes->get_child_optional("unicode-bidi");
				if(unicode_bidi) {
					const std::string& bidi = unicode_bidi->data();
					if(bidi == "inherit") {
						bidi_ = UnicodeBidi::INHERIT;
					} else if(bidi == "normal") {
						bidi_ = UnicodeBidi::NORMAL;
					} else if(bidi == "embed") {
						bidi_ = UnicodeBidi::EMBED;
					} else if(bidi == "bidi-override") {
						bidi_ = UnicodeBidi::BIDI_OVERRIDE;
					}
				}

				auto letter_spacing = attributes->get_child_optional("letter-spacing");
				if(letter_spacing) {
					const std::string& ls = letter_spacing->data();
					if(ls == "inherit") {
						letter_spacing_ = TextSpacing::INHERIT;
					} else if(ls == "normal") {
						letter_spacing_ = TextSpacing::NORMAL;
					} else {
						letter_spacing_ = TextSpacing::VALUE;
						letter_spacing_value_ = svg_length(ls);
					}
				}

				auto word_spacing = attributes->get_child_optional("word-spacing");
				if(word_spacing) {
					const std::string& ws = word_spacing->data();
					if(ws == "inherit") {
						word_spacing_ = TextSpacing::INHERIT;
					} else if(ws == "normal") {
						word_spacing_ = TextSpacing::NORMAL;
					} else {
						word_spacing_ = TextSpacing::VALUE;
						word_spacing_value_ = svg_length(ws);
					}
				}

				auto kerning = attributes->get_child_optional("kerning");
				if(kerning) {
					const std::string& kern = kerning->data();
					if(kern == "inherit") {
						kerning_ = Kerning::INHERIT;
					} else if(kern == "auto") {
						kerning_ = Kerning::AUTO;
					} else {
						kerning_ = Kerning::VALUE;
						kerning_value_ = svg_length(kern);
					}
				}

				auto text_decoration = attributes->get_child_optional("text-decoration");
				if(text_decoration) {
					const std::string& td = text_decoration->data();
					if(td == "inherit") {
						decoration_ = TextDecoration::INHERIT;
					} else if(td == "none") {
						decoration_ = TextDecoration::NONE;
					} else if(td == "underline") {
						decoration_ = TextDecoration::UNDERLINE;
					} else if(td == "overline") {
						decoration_ = TextDecoration::OVERLINE;
					} else if(td == "blink") {
						decoration_ = TextDecoration::BLINK;
					} else if(td == "line-through") {
						decoration_ = TextDecoration::LINE_THROUGH;
					}
				}

				auto writing_mode = attributes->get_child_optional("writing-mode");
				if(writing_mode) {
					const std::string& wm = writing_mode->data();
					if(wm == "inherit") {
						writing_mode_ = WritingMode::INHERIT;
					} else if(wm == "lr-tb") {
						writing_mode_ = WritingMode::LR_TB;
					} else if(wm == "rl-tb") {
						writing_mode_ = WritingMode::RL_TB;
					} else if(wm == "tb-rl") {
						writing_mode_ = WritingMode::TB_RL;
					} else if(wm == "lr") {
						writing_mode_ = WritingMode::LR;
					} else if(wm == "rl") {
						writing_mode_ = WritingMode::RL;
					} else if(wm == "tb") {
						writing_mode_ = WritingMode::TB;
					}
				}

				auto baseline_alignment = attributes->get_child_optional("alignment-baseline");
				if(baseline_alignment) {
					const std::string& ba = baseline_alignment->data();
					if(ba == "inherit") {
						baseline_alignment_ = TextAlignmentBaseline::INHERIT;
					} else if(ba == "auto") {
						baseline_alignment_ = TextAlignmentBaseline::AUTO;
					} else if(ba == "baseline") {
						baseline_alignment_ = TextAlignmentBaseline::BASELINE;
					} else if(ba == "before-edge") {
						baseline_alignment_ = TextAlignmentBaseline::BEFORE_EDGE;
					} else if(ba == "text-before-edge") {
						baseline_alignment_ = TextAlignmentBaseline::TEXT_BEFORE_EDGE;
					} else if(ba == "middle") {
						baseline_alignment_ = TextAlignmentBaseline::MIDDLE;
					} else if(ba == "central") {
						baseline_alignment_ = TextAlignmentBaseline::CENTRAL;
					} else if(ba == "after-edge") {
						baseline_alignment_ = TextAlignmentBaseline::AFTER_EDGE;
					} else if(ba == "text-after-edge") {
						baseline_alignment_ = TextAlignmentBaseline::TEXT_AFTER_EDGE;
					} else if(ba == " ideographic") {
						baseline_alignment_ = TextAlignmentBaseline::IDEOGRAPHIC;
					} else if(ba == "alphabetic") {
						baseline_alignment_ = TextAlignmentBaseline::ALPHABETIC;
					} else if(ba == "hanging") {
						baseline_alignment_ = TextAlignmentBaseline::HANGING;
					} else if(ba == "mathematical") {
						baseline_alignment_ = TextAlignmentBaseline::MATHEMATICAL;
					}
				}

				auto baseline_shift = attributes->get_child_optional("baseline-shift");
				if(baseline_shift) {
					const std::string& bs = baseline_shift->data();
					if(bs == "inherit") {
						baseline_shift_ = TextBaselineShift::INHERIT;
					} else if(bs == "baseline") {
						baseline_shift_ = TextBaselineShift::BASELINE;
					} else if(bs == "sub") {
						baseline_shift_ = TextBaselineShift::SUB;
					} else if(bs == "super") {
						baseline_shift_ = TextBaselineShift::SUPER;
					}
				}

				auto dominant_baseline = attributes->get_child_optional("dominant-baseline");
				if(dominant_baseline) {
					const std::string& db = dominant_baseline->data();
					if(db == "inherit") {
						dominant_baseline_ = TextDominantBaseline::INHERIT;
					} else if(db == "auto") {
						dominant_baseline_ = TextDominantBaseline::AUTO;
					} else if(db == "use-script") {
						dominant_baseline_ = TextDominantBaseline::USE_SCRIPT;
					} else if(db == "no-change") {
						dominant_baseline_ = TextDominantBaseline::NO_CHANGE;
					} else if(db == "reset-size") {
						dominant_baseline_ = TextDominantBaseline::RESET_SIZE;
					} else if(db == " ideographic") {
						dominant_baseline_ = TextDominantBaseline::IDEOGRAPHIC;
					} else if(db == "alphabetic") {
						dominant_baseline_ = TextDominantBaseline::ALPHABETIC;
					} else if(db == "hanging") {
						dominant_baseline_ = TextDominantBaseline::HANGING;
					} else if(db == "mathematical") {
						dominant_baseline_ = TextDominantBaseline::MATHEMATICAL;
					} else if(db == "central") {
						dominant_baseline_ = TextDominantBaseline::CENTRAL;
					} else if(db == "middle") {
						dominant_baseline_ = TextDominantBaseline::MIDDLE;
					} else if(db == "text-after-edge") {
						dominant_baseline_ = TextDominantBaseline::TEXT_AFTER_EDGE;
					} else if(db == "text-before-edge") {
						dominant_baseline_ = TextDominantBaseline::TEXT_BEFORE_EDGE;
					}
				}

				auto glyph_orientation_vert = attributes->get_child_optional("glyph-orientation-vertical");
				if(glyph_orientation_vert) {
					const std::string& go = glyph_orientation_vert->data();
					if(go == "inherit") {
						glyph_orientation_vertical_ = GlyphOrientation::INHERIT;
					} else if(go == "auto") {
						glyph_orientation_vertical_ = GlyphOrientation::AUTO;
					} else {
						glyph_orientation_vertical_ = GlyphOrientation::VALUE;
						try {
							glyph_orientation_vertical_value_ = boost::lexical_cast<double>(go);
							// test for 0,90,180,270 degree values
						} catch(boost::bad_lexical_cast&) {
							ASSERT_LOG(false, "Bad numeric conversion of glyph-orientation-vertical value: " << go);
						}
					}
				}

				auto glyph_orientation_horz = attributes->get_child_optional("glyph-orientation-horizontal");
				if(glyph_orientation_horz) {
					const std::string& go = glyph_orientation_horz->data();
					if(go == "inherit") {
						glyph_orientation_horizontal_ = GlyphOrientation::INHERIT;
					} else if(go == "auto") {
						glyph_orientation_horizontal_ = GlyphOrientation::AUTO;
					} else {
						glyph_orientation_horizontal_ = GlyphOrientation::VALUE;
						try {
							glyph_orientation_horizontal_value_ = boost::lexical_cast<double>(go);
							// test for 0,90,180,270 degree values
						} catch(boost::bad_lexical_cast&) {
							ASSERT_LOG(false, "Bad numeric conversion of glyph-orientation-horizontal value: " << go);
						}
					}
				}
			}
		}

		text_attribs::~text_attribs()
		{
		}

		void text_attribs::apply(render_context& ctx) const
		{
			if(letter_spacing_ == TextSpacing::VALUE) {
				ctx.letter_spacing_push(letter_spacing_value_.value_in_specified_units(svg_length::SVG_LENGTHTYPE_NUMBER));
			} else if(letter_spacing_ == TextSpacing::NORMAL) {
				ctx.letter_spacing_push(0);
			}
			// XXX
		}

		void text_attribs::clear(render_context& ctx) const
		{
			if(letter_spacing_ == TextSpacing::VALUE || letter_spacing_ == TextSpacing::NORMAL) {
				ctx.letter_spacing_pop();
			}
			// XXX
		}

		void text_attribs::resolve(const element* doc)
		{
			// XXX
		}


		visual_attribs::visual_attribs(const ptree& pt)
			: overflow_(Overflow::VISIBLE),
			clip_(Clip::AUTO),
			display_(Display::INLINE),
			visibility_(Visibility::VISIBLE),
			current_color_(new paint(0,0,0)),
			cursor_(Cursor::AUTO)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto overflow = attributes->get_child_optional("overflow");
				if(overflow) {
					const std::string& ovf = overflow->data();
					if(ovf == "inherit") {
						overflow_ = Overflow::INHERIT;
					} else if(ovf == "visible") {
						overflow_ = Overflow::VISIBLE;
					} else if(ovf == "hidden") {
						overflow_ = Overflow::HIDDEN;
					} else if(ovf == "scroll") {
						overflow_ = Overflow::SCROLL;
					}
				}

				auto clip = attributes->get_child_optional("clip");
				if(clip) {
					const std::string& clp = clip->data();
					if(clp == "inherit") {
						clip_ = Clip::INHERIT;
					} else if(clp == "auto") {
						clip_ = Clip::AUTO;
					} else {
						clip_ = Clip::SHAPE;
						boost::char_separator<char> seperators(" \n\t\r,", "();");
						boost::tokenizer<boost::char_separator<char>> tok(clp, seperators);
						auto it = tok.begin();
						ASSERT_LOG(*it == "rect", "Only supported clip shape is 'rect'");
						++it;
						ASSERT_LOG(it != tok.end(), "Inufficient values given to clip 'rect' shape.");
						if(*it == "auto") {
							clip_y1_ = svg_length(0,svg_length::SVG_LENGTHTYPE_NUMBER);
						} else {
							clip_y1_ = svg_length(*it);
						}
						++it;
						ASSERT_LOG(it != tok.end(), "Inufficient values given to clip 'rect' shape.");
						if(*it == "auto") {
							clip_x2_ = svg_length(0,svg_length::SVG_LENGTHTYPE_NUMBER);
						} else {
							clip_x2_ = svg_length(*it);
						}
						++it;
						ASSERT_LOG(it != tok.end(), "Inufficient values given to clip 'rect' shape.");
						if(*it == "auto") {
							clip_y2_ = svg_length(0,svg_length::SVG_LENGTHTYPE_NUMBER);
						} else {
							clip_y2_ = svg_length(*it);
						}
						++it;
						ASSERT_LOG(it != tok.end(), "Inufficient values given to clip 'rect' shape.");
						if(*it == "auto") {
							clip_x1_ = svg_length(0,svg_length::SVG_LENGTHTYPE_NUMBER);
						} else {
							clip_x1_ = svg_length(*it);
						}
					}
				}

				auto cursor = attributes->get_child_optional("cursor");
				if(cursor) {
					const std::string& curs = cursor->data();
					if(curs == "inherit") {
						cursor_ = Cursor::INHERIT;
					} else {
						boost::char_separator<char> seperators(" \n\t\r,");
						boost::tokenizer<boost::char_separator<char>> tok(curs, seperators);
						for(auto it = tok.begin(); it != tok.end(); ++it) {
							if(*it == "auto") {
								cursor_ = Cursor::AUTO;
							} else if(*it == "crosshair") {
								cursor_ = Cursor::CROSSHAIR;
							} else if(*it == "default") {
								cursor_ = Cursor::DEFAULT;
							} else if(*it == "pointer") {
								cursor_ = Cursor::POINTER;
							} else if(*it == "move") {
								cursor_ = Cursor::MOVE;
							} else if(*it == "e-resize") {
								cursor_ = Cursor::E_RESIZE;
							} else if(*it == "ne-resize") {
								cursor_ = Cursor::NE_RESIZE;
							} else if(*it == "nw-resize") {
								cursor_ = Cursor::NW_RESIZE;
							} else if(*it == "n-resize") {
								cursor_ = Cursor::N_RESIZE;
							} else if(*it == "se-resize") {
								cursor_ = Cursor::SE_RESIZE;
							} else if(*it == "sw-resize") {
								cursor_ = Cursor::SW_RESIZE;
							} else if(*it == "s-resize") {
								cursor_ = Cursor::S_RESIZE;
							} else if(*it == "w-resize") {
								cursor_ = Cursor::W_RESIZE;
							} else if(*it == "text") {
								cursor_ = Cursor::TEXT;
							} else if(*it == "wait") {
								cursor_ = Cursor::WAIT;
							} else if(*it == "help") {
								cursor_ = Cursor::HELP;
							} else {
								if(it->substr(0,4) == "url(" && it->back() == ')') {
									cursor_funciri_.emplace_back(it->substr(4,it->size()-5));
								}
							}
						}
					}
				}

				auto display = attributes->get_child_optional("display");
				if(display) {
					const std::string& disp = display->data();
					if(disp == "inherit") {
						display_ = Display::INHERIT;
					} else if(disp == "inline") {
						display_ = Display::INLINE;
					} else if(disp == "block") {
						display_ = Display::BLOCK;
					} else if(disp == "list-item") {
						display_ = Display::LIST_ITEM;
					} else if(disp == "run-in") {
						display_ = Display::RUN_IN;
					} else if(disp == "compact") {
						display_ = Display::COMPACT;
					} else if(disp == "marker") {
						display_ = Display::MARKER;
					} else if(disp == "table") {
						display_ = Display::TABLE;
					} else if(disp == "inline-table") {
						display_ = Display::INLINE_TABLE;
					} else if(disp == "table-row-group") {
						display_ = Display::TABLE_ROW_GROUP;
					} else if(disp == "table-header-group") {
						display_ = Display::TABLE_HEADER_GROUP;
					} else if(disp == "table-footer-group") {
						display_ = Display::TABLE_FOOTER_GROUP;
					} else if(disp == "table-row") {
						display_ = Display::TABLE_ROW;
					} else if(disp == "table-column-group") {
						display_ = Display::TABLE_COLUMN_GROUP;
					} else if(disp == "table-column") {
						display_ = Display::TABLE_COLUMN;
					} else if(disp == "table-cell") {
						display_ = Display::TABLE_CELL;
					} else if(disp == "table-caption") {
						display_ = Display::TABLE_CAPTION;
					} else if(disp == "none") {
						display_ = Display::NONE;
					}
				}

				auto visibility = attributes->get_child_optional("visibility");
				if(visibility) {
					const std::string& vis = visibility->data();
					if(vis == "inherit") {
						visibility_ = Visibility::INHERIT;
					} else if(vis == "visible") {
						visibility_ = Visibility::VISIBLE;
					} else if(vis == "hidden") {
						visibility_ = Visibility::HIDDEN;
					} else if(vis == "collapse") {
						visibility_ = Visibility::COLLAPSE;
					}
				}

				auto color = attributes->get_child_optional("color");
				if(color) {
					current_color_ = paint::from_string(color->data());
				}
			}
		}

		visual_attribs::~visual_attribs()
		{
		}

		void visual_attribs::apply(render_context& ctx) const
		{
			// XXX
			cairo_push_group(ctx.cairo());
		}

		void visual_attribs::clear(render_context& ctx) const
		{
			// XXX
			auto patt = cairo_pop_group(ctx.cairo());
			if(display_ == Display::NONE) {
				cairo_pattern_destroy(patt);
			} else {
				cairo_set_source(ctx.cairo(), patt);
				cairo_pattern_destroy(patt);
				cairo_paint(ctx.cairo());
			}
		}

		void visual_attribs::resolve(const element* doc)
		{
			// XXX
		}

		clipping_attribs::clipping_attribs(const ptree& pt)
			: path_(FuncIriValue::NONE),
			rule_(ClipRule::NON_ZERO),
			mask_(FuncIriValue::NONE),
			opacity_(OpacityAttrib::VALUE),
			opacity_value_(1.0)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");
			if(attributes) {
				auto mask = attributes->get_child_optional("mask");
				if(mask) {
					const std::string& msk = mask->data();
					if(msk == "inherit") {
						mask_ = FuncIriValue::INHERIT;
					} else if(msk == "none") {
						mask_ = FuncIriValue::NONE;
					} else {
						mask_ = FuncIriValue::FUNC_IRI;
						if(msk.substr(0,4) == "url(" && msk.back() == ')') {
							mask_ref_ = msk.substr(4,msk.size()-5);
						}
					}
				}

				auto path = attributes->get_child_optional("clip-path");
				if(path) {
					const std::string& pth = path->data();
					if(pth == "inherit") {
						path_ = FuncIriValue::INHERIT;
					} else if(pth == "none") {
						path_ = FuncIriValue::NONE;
					} else {
						path_ = FuncIriValue::FUNC_IRI;
						if(pth.substr(0,4) == "url(" && pth.back() == ')') {
							path_ref_ = pth.substr(4,pth.size()-5);
						}
					}
				}

				auto rule = attributes->get_child_optional("clip-rule");
				if(rule) {
					const std::string& r = rule->data();
					if(r == "inherit") {
						rule_ = ClipRule::INHERIT;
					} else if(r == "nonzero") {
						rule_ = ClipRule::NON_ZERO;
					} else if(r == "evenodd") {
						rule_ = ClipRule::EVEN_ODD;
					}
				}

				auto opacity = attributes->get_child_optional("opacity");
				if(opacity) {
					const std::string& o = opacity->data();
					if(o == "inherit") {
						opacity_ = OpacityAttrib::INHERIT;
					} else {
						opacity_ = OpacityAttrib::VALUE;
						try {
							opacity_value_ = boost::lexical_cast<double>(o);
						} catch(boost::bad_lexical_cast&) {
							ASSERT_LOG(false, "Unable to obtain object/group opacity value: " << o);
						}
					}
				}
			}
		}

		clipping_attribs::~clipping_attribs()
		{
		}

		void clipping_attribs::apply(render_context& ctx) const
		{
			// mask_
			// rule_
			// path_
			switch(opacity_)
			{
				case OpacityAttrib::UNSET:	/* do nothing */ break;
				case OpacityAttrib::INHERIT:	/* do nothing */ break;
				case OpacityAttrib::VALUE:
					ctx.opacity_push(opacity_value_);
					break;
				default: break;
			}
			
			if(path_ == FuncIriValue::FUNC_IRI && path_resolved_ != NULL) {
				path_resolved_->clip(ctx);
			}
		}

		void clipping_attribs::clear(render_context& ctx) const
		{
			// XXX
			if(opacity_ == OpacityAttrib::VALUE) {
				ctx.opacity_pop();
			}
		}

		void clipping_attribs::resolve(const element* doc)
		{
			if(path_ == FuncIriValue::FUNC_IRI) {
				ASSERT_LOG(!path_ref_.empty(), "clip-path reference is empty");
				ASSERT_LOG(path_ref_[0] == '#', "Inter-document FuncIRI references not supported: " << path_ref_);
				auto child = doc->find_child(path_ref_.substr(1));
				if(child) {
					path_resolved_ = child;
					// XXX we should check child is of type clip_path here and Warn/unset the clip path if not.
				} else {
					LOG_WARN("Reference to clip-path child element not found:  (will ignore clip-path)" << path_ref_);
					path_ = FuncIriValue::UNSET;
				}
			}
		}

		filter_effect_attribs::filter_effect_attribs(const ptree& pt)
			: enable_background_(Background::ACCUMULATE),
			filter_(FuncIriValue::NONE),
			flood_color_(new paint(0,0,0)),
			flood_opacity_(OpacityAttrib::VALUE),
			flood_opacity_value_(1.0),
			lighting_color_(new paint(255,255,255))
		{
			auto attributes = pt.get_child_optional("<xmlattr>");

			if(attributes) {
				auto filter = attributes->get_child_optional("filter");
				if(filter) {
					const std::string& filt = filter->data();
					if(filt == "inherit") {
						filter_ = FuncIriValue::INHERIT;
					} else if(filt == "none") {
						filter_ = FuncIriValue::NONE;
					} else {
						filter_ = FuncIriValue::FUNC_IRI;
						if(filt.substr(0,4) == "url(" && filt.back() == ')') {
							filter_ref_ = uri::uri::parse(filt.substr(4, filt.size()-5));
						}
					}
				}

				auto enable_background = attributes->get_child_optional("enable-background");
				if(enable_background) {
					const std::string& bckg = enable_background->data();
					if(bckg == "inherit") {
						enable_background_ = Background::INHERIT;
					} else if(bckg == "accumulate") {
						enable_background_ = Background::ACCUMULATE;
					} else {
						boost::char_separator<char> seperators(" \n\t\r,");
						boost::tokenizer<boost::char_separator<char>> tok(bckg, seperators);
						auto it = tok.begin();
						ASSERT_LOG(*it == "new", "'enable-background' attribute expected new keyword.");
						enable_background_ = Background::NEW;
						++it;
						if(it != tok.end()) {
							x_ = svg_length(*it);
							++it;
							// technically if less than 4 values are given it should disable pocessing.
							// I feel throwing an error is better.
							ASSERT_LOG(it != tok.end(), "Expected 'enable-background' with 4 parameters for 'new' value, got 1");
							y_ = svg_length(*it);
							++it;
							ASSERT_LOG(it != tok.end(), "Expected 'enable-background' with 4 parameters for 'new' value, got 2");
							w_ = svg_length(*it);
							++it;
							ASSERT_LOG(it != tok.end(), "Expected 'enable-background' with 4 parameters for 'new' value, got 3");
							h_ = svg_length(*it);
						}
					}
				}

				auto flood_color = attributes->get_child_optional("flood-color");
				if(flood_color) {
					flood_color_ = paint::from_string(flood_color->data());
				}

				auto opacity = attributes->get_child_optional("flood-opacity");
				if(opacity) {
					const std::string& o = opacity->data();
					if(o == "inherit") {
						flood_opacity_ = OpacityAttrib::INHERIT;
					} else {
						flood_opacity_ = OpacityAttrib::VALUE;
						try {
							flood_opacity_value_ = boost::lexical_cast<double>(o);
						} catch(boost::bad_lexical_cast&) {
							ASSERT_LOG(false, "Unable to obtain flood_opacity value: " << o);
						}
					}
				}

				auto lighting_color = attributes->get_child_optional("lighting-color");
				if(lighting_color) {
					lighting_color_ = paint::from_string(lighting_color->data());
				}
			}
		}

		filter_effect_attribs::~filter_effect_attribs()
		{
		}

		void filter_effect_attribs::apply(render_context& ctx) const
		{
			// XXX
		}

		void filter_effect_attribs::clear(render_context& ctx) const
		{
			// XXX
		}

		void filter_effect_attribs::resolve(const element* doc)
		{
			// XXX
		}

		painting_properties::painting_properties(const ptree& pt)
			: stroke_(paint_ptr()),
			stroke_opacity_(OpacityAttrib::UNSET),
			stroke_opacity_value_(1.0),
			stroke_width_(StrokeWidthAttrib::UNSET),
			stroke_width_value_(1.0),
			stroke_linecap_(LineCapAttrib::UNSET),
			stroke_linejoin_(LineJoinAttrib::UNSET),
			stroke_miter_limit_(MiterLimitAttrib::UNSET),
			stroke_miter_limit_value_(4.0),
			stroke_dash_array_(DashArrayAttrib::UNSET),
			stroke_dash_offset_(DashOffsetAttrib::UNSET),
			stroke_dash_offset_value_(0, svg_length::SVG_LENGTHTYPE_NUMBER),
			fill_(paint_ptr()),
			fill_rule_(FillRuleAttrib::UNSET),
			fill_opacity_(OpacityAttrib::UNSET),
			fill_opacity_value_(1.0),
			color_interpolation_(ColorInterpolationAttrib::UNSET),
			color_interpolation_filters_(ColorInterpolationAttrib::UNSET),
			color_rendering_(RenderingAttrib::UNSET),
			shape_rendering_(ShapeRenderingAttrib::UNSET),
			text_rendering_(TextRenderingAttrib::UNSET),
			image_rendering_(RenderingAttrib::UNSET),
			color_profile_(ColorProfileAttrib::UNSET)
			/*
			stroke_(paint_ptr(new paint())),
			stroke_opacity_(OpacityAttrib::VALUE),
			stroke_opacity_value_(1.0),
			stroke_width_(StrokeWidthAttrib::VALUE),
			stroke_width_value_(1.0),
			stroke_linecap_(LineCapAttrib::BUTT),
			stroke_linejoin_(LineJoinAttrib::MITER),
			stroke_miter_limit_(MiterLimitAttrib::VALUE),
			stroke_miter_limit_value_(4.0),
			stroke_dash_array_(DashArrayAttrib::NONE),
			stroke_dash_offset_(DashOffsetAttrib::VALUE),
			stroke_dash_offset_value_(0, svg_length::SVG_LENGTHTYPE_NUMBER),
			fill_(paint_ptr(new paint(0,0,0))),
			fill_rule_(FillRuleAttrib::EVENODD),
			fill_opacity_(OpacityAttrib::VALUE),
			fill_opacity_value_(1.0),
			color_interpolation_(ColorInterpolationAttrib::sRGBA),
			color_interpolation_filters_(ColorInterpolationAttrib::linearRGBA),
			color_rendering_(RenderingAttrib::AUTO),
			shape_rendering_(ShapeRenderingAttrib::AUTO),
			text_rendering_(TextRenderingAttrib::AUTO),
			image_rendering_(RenderingAttrib::AUTO),
			color_profile_(ColorProfileAttrib::AUTO)			*/
		{
			auto attributes = pt.get_child_optional("<xmlattr>");

			if(attributes) {
				auto stroke = attributes->get_child_optional("stroke");
				if(stroke) {
					stroke_ = paint::from_string(stroke->data());
				}

				auto stroke_opacity = attributes->get_child_optional("stroke-opacity");
				if(stroke_opacity) {
					const std::string& o = stroke_opacity->data();
					if(o == "inherit") {
						stroke_opacity_ = OpacityAttrib::INHERIT;
					} else {
						stroke_opacity_ = OpacityAttrib::VALUE;
						try {
							stroke_opacity_value_ = boost::lexical_cast<double>(o);
						} catch(boost::bad_lexical_cast&) {
							ASSERT_LOG(false, "Unable to obtain stroke-opacity value: " << o);
						}
					}
				}

				auto stroke_width = attributes->get_child_optional("stroke-width");
				if(stroke_width) {
					const std::string& sw = stroke_width->data();
					if(sw == "inherit") {
						stroke_width_ = StrokeWidthAttrib::INHERIT;
					} else {
						if(sw.find("%") != std::string::npos) {
							stroke_width_ = StrokeWidthAttrib::PERCENTAGE;
							try {
								stroke_width_value_ = boost::lexical_cast<double>(sw) / 100.0;
							} catch(boost::bad_lexical_cast&) {
								ASSERT_LOG(false, "Unable to obtain 'stroke-width' value: " << sw);
							}
						} else {
							stroke_width_ = StrokeWidthAttrib::VALUE;
							try {
								stroke_width_value_ = boost::lexical_cast<double>(sw);
							} catch(boost::bad_lexical_cast&) {
								ASSERT_LOG(false, "Unable to obtain 'stroke-width' value: " << sw);
							}
						}
					}
				}

				auto stroke_linecap = attributes->get_child_optional("stroke-linecap");
				if(stroke_linecap) {
					const std::string& slc = stroke_linecap->data();
					if(slc == "inherit") {
						stroke_linecap_ = LineCapAttrib::INHERIT;
					} else if(slc == "butt") {
						stroke_linecap_ = LineCapAttrib::BUTT;
					} else if(slc == "round") {
						stroke_linecap_ = LineCapAttrib::ROUND;
					} else if(slc == "square") {
						stroke_linecap_ = LineCapAttrib::SQUARE;
					}
				}

				auto stroke_linejoin = attributes->get_child_optional("stroke-linejoin");
				if(stroke_linejoin) {
					const std::string& slj = stroke_linejoin->data();
					if(slj == "inherit") {
						stroke_linejoin_ = LineJoinAttrib::INHERIT;
					} else if(slj == "miter") {
						stroke_linejoin_ = LineJoinAttrib::MITER;
					} else if(slj == "round") {
						stroke_linejoin_ = LineJoinAttrib::ROUND;
					} else if(slj == "bevel") {
						stroke_linejoin_ = LineJoinAttrib::BEVEL;
					}
				}

				auto stroke_miterlimit = attributes->get_child_optional("stroke-miterlimit");
				if(stroke_miterlimit) {
					const std::string& sml = stroke_miterlimit->data();
					if(sml == "inherit") {
						stroke_miter_limit_ = MiterLimitAttrib::INHERIT;
					} else {
						stroke_miter_limit_ = MiterLimitAttrib::VALUE;
						try {
							stroke_miter_limit_value_ = boost::lexical_cast<double>(sml);
						} catch(boost::bad_lexical_cast&) {
							ASSERT_LOG(false, "Unable to obtain 'stroke-miterlimit' value: " << sml);
						}
						ASSERT_LOG(stroke_miter_limit_value_ >= 1.0, "'stroke-miterlimit' value must be greater than 1.0: " << stroke_miter_limit_value_);
					}
				}

				auto stroke_dasharray = attributes->get_child_optional("stroke-dasharray");
				if(stroke_dasharray) {
					const std::string& sda = stroke_dasharray->data();
					if(sda == "inherit") {
						stroke_dash_array_ = DashArrayAttrib::INHERIT;
					} else if(sda == "none") {
						stroke_dash_array_ = DashArrayAttrib::NONE;
					} else {
						stroke_dash_array_ = DashArrayAttrib::VALUE;
						//stroke_dash_array_value_;
						boost::char_separator<char> seperators(" \n\t\r,");
						boost::tokenizer<boost::char_separator<char>> tok(sda, seperators);
						for(auto it : tok) {
							stroke_dash_array_value_.emplace_back(svg_length(it));
						}
					}
				}

				auto stroke_dash_offset = attributes->get_child_optional("stroke-dashoffset");
				if(stroke_dash_offset) {
					const std::string& sdo = stroke_dash_offset->data();
					if(sdo == "inherit") {
						stroke_dash_offset_ = DashOffsetAttrib::INHERIT;
					} else {
						stroke_dash_offset_ = DashOffsetAttrib::VALUE;
						stroke_dash_offset_value_ = svg_length(sdo);
					}
				}

				auto fill = attributes->get_child_optional("fill");
				if(fill) {
					fill_ = paint::from_string(fill->data());
				}

				auto fill_opacity = attributes->get_child_optional("fill-opacity");
				if(fill_opacity) {
					const std::string& o = fill_opacity->data();
					if(o == "inherit") {
						fill_opacity_ = OpacityAttrib::INHERIT;
					} else {
						fill_opacity_ = OpacityAttrib::VALUE;
						try {
							fill_opacity_value_ = boost::lexical_cast<double>(o);
						} catch(boost::bad_lexical_cast&) {
							ASSERT_LOG(false, "Unable to obtain fill-opacity value: " << o);
						}
					}
				}

				auto fill_rule = attributes->get_child_optional("fill-rule");
				if(fill_rule) {
					const std::string& fr = fill_rule->data();
					if(fr == "inherit") {
						fill_rule_ = FillRuleAttrib::INHERIT;
					} else if(fr == "nonzero") {
						fill_rule_ = FillRuleAttrib::NONZERO;
					} else if(fr == "evenodd") {
						fill_rule_ = FillRuleAttrib::EVENODD;
					}
				}

				auto color_interpolation = attributes->get_child_optional("color-interpolation");
				if(color_interpolation) {
					const std::string& ci = color_interpolation->data();
					if(ci == "auto") {
						color_interpolation_ = ColorInterpolationAttrib::AUTO;
					} else if(ci == "sRGBA") {
						color_interpolation_ = ColorInterpolationAttrib::sRGBA;
					} else if(ci == "linearRGBA") {
						color_interpolation_ = ColorInterpolationAttrib::linearRGBA;
					} else if(ci == "inherit") {
						color_interpolation_ = ColorInterpolationAttrib::INHERIT;
					}
				}

				auto color_interpolation_filters = attributes->get_child_optional("color-interpolation-filters");
				if(color_interpolation_filters) {
					const std::string& cif = color_interpolation_filters->data();
					if(cif == "auto") {
						color_interpolation_filters_ = ColorInterpolationAttrib::AUTO;
					} else if(cif == "sRGBA") {
						color_interpolation_filters_ = ColorInterpolationAttrib::sRGBA;
					} else if(cif == "linearRGBA") {
						color_interpolation_filters_ = ColorInterpolationAttrib::linearRGBA;
					} else if(cif == "inherit") {
						color_interpolation_filters_ = ColorInterpolationAttrib::INHERIT;
					}
				}

				auto color_rendering = attributes->get_child_optional("color-rendering");
				if(color_rendering) {
					const std::string& rend = color_rendering->data();
					if(rend == "inherit") {
						color_rendering_ = RenderingAttrib::INHERIT;
					} else if(rend == "auto") {
						color_rendering_ = RenderingAttrib::AUTO;
					} else if(rend == "optimizeSpeed") {
						color_rendering_ = RenderingAttrib::OPTIMIZE_SPEED;
					} else if(rend == "optimizeQuality") {
						color_rendering_ = RenderingAttrib::OPTIMIZE_QUALITY;
					}
				}

				auto shape_rendering = attributes->get_child_optional("shape-rendering");
				if(shape_rendering) {
					const std::string& rend = shape_rendering->data();
					if(rend == "inherit") {
						shape_rendering_ = ShapeRenderingAttrib::INHERIT;
					} else if(rend == "auto") {
						shape_rendering_ = ShapeRenderingAttrib::AUTO;
					} else if(rend == "optimizeSpeed") {
						shape_rendering_ = ShapeRenderingAttrib::OPTIMIZE_SPEED;
					} else if(rend == "crispEdges") {
						shape_rendering_ = ShapeRenderingAttrib::CRISP_EDGES;
					} else if(rend == "geometricPrecision") {
						shape_rendering_ = ShapeRenderingAttrib::GEOMETRIC_PRECISION;
					}
				}

				auto text_rendering = attributes->get_child_optional("text-rendering");
				if(text_rendering) {
					const std::string& rend = text_rendering->data();
					if(rend == "inherit") {
						text_rendering_ = TextRenderingAttrib::INHERIT;
					} else if(rend == "auto") {
						text_rendering_ = TextRenderingAttrib::AUTO;
					} else if(rend == "optimizeSpeed") {
						text_rendering_ = TextRenderingAttrib::OPTIMIZE_SPEED;
					} else if(rend == "optimizeLegibility") {
						text_rendering_ = TextRenderingAttrib::OPTIMIZE_LEGIBILITY;
					} else if(rend == "geometricPrecision") {
						text_rendering_ = TextRenderingAttrib::GEOMETRIC_PRECISION;
					}
				}

				auto image_rendering = attributes->get_child_optional("image-rendering");
				if(image_rendering) {
					const std::string& rend = image_rendering->data();
					if(rend == "inherit") {
						image_rendering_ = RenderingAttrib::INHERIT;
					} else if(rend == "auto") {
						image_rendering_ = RenderingAttrib::AUTO;
					} else if(rend == "optimizeSpeed") {
						image_rendering_ = RenderingAttrib::OPTIMIZE_SPEED;
					} else if(rend == "optimizeQuality") {
						image_rendering_ = RenderingAttrib::OPTIMIZE_QUALITY;
					}
				}

				auto color_profile = attributes->get_child_optional("color-profile");
				if(color_profile) {
					const std::string& cp = color_profile->data();
					if(cp == "inherit") {
						color_profile_ = ColorProfileAttrib::INHERIT;
					} else if(cp == "auto") {
						color_profile_ = ColorProfileAttrib::AUTO;
					} else if(cp == "sRGBA") {
						color_profile_ = ColorProfileAttrib::sRGB;
					} else {
						std::cerr << "XXX: unhandled 'color-profile' attribute value: " << cp << std::endl;
					}
				}
			}
		}

		painting_properties::~painting_properties()
		{
		}

		void painting_properties::apply(render_context& ctx) const
		{
			cairo_save(ctx.cairo());

			if(stroke_) {
				ctx.stroke_color_push(stroke_);
			}
			if(fill_) {
				ctx.fill_color_push(fill_);
			}

			switch(stroke_opacity_)
			{
				case OpacityAttrib::UNSET:		/* do nothing */ break;
				case OpacityAttrib::INHERIT:	/* do nothing */ break;
				case OpacityAttrib::VALUE:
					// XXX This might not be right, if "stroke" is not set but
					// "stroke_opacity" is. Since then we're setting the opacity
					// on the current parent.
					ctx.stroke_color_top()->set_opacity(stroke_opacity_value_);
					break;
				default: break;
			}
			switch(fill_opacity_)
			{
				case OpacityAttrib::UNSET:		/* do nothing */ break;
				case OpacityAttrib::INHERIT:	/* do nothing */ break;
				case OpacityAttrib::VALUE:
					// XXX This might not be right, if "stroke" is not set but
					// "stroke_opacity" is. Since then we're setting the opacity
					// on the current parent.
					ctx.fill_color_top()->set_opacity(fill_opacity_value_);
					break;
				default: break;
			}

			switch(stroke_width_) {
				case StrokeWidthAttrib::UNSET:		/* do nothing */ break;
				case StrokeWidthAttrib::INHERIT:	/* do nothing */ break;
				case StrokeWidthAttrib::PERCENTAGE:
					ASSERT_LOG(false, "fixme: StrokeWidthAttrib::PERCENTAGE");
					break;
				case StrokeWidthAttrib::VALUE:
					cairo_set_line_width(ctx.cairo(), stroke_width_value_);
					break;
				default: break;
			}
			switch(stroke_linecap_)
			{
				case LineCapAttrib::UNSET:		/* do nothing */ break;
				case LineCapAttrib::INHERIT:	/* do nothing */ break;
				case LineCapAttrib::BUTT:	
					cairo_set_line_cap(ctx.cairo(), CAIRO_LINE_CAP_BUTT);
					break;
				case LineCapAttrib::ROUND:
					cairo_set_line_cap(ctx.cairo(), CAIRO_LINE_CAP_ROUND);
					break;
				case LineCapAttrib::SQUARE:
					cairo_set_line_cap(ctx.cairo(), CAIRO_LINE_CAP_SQUARE);
					break;
				default: break;
			}
			switch(stroke_linejoin_)
			{
				case LineJoinAttrib::UNSET:		/* do nothing */ break;
				case LineJoinAttrib::INHERIT:	/* do nothing */ break;
				case LineJoinAttrib::MITER:
					cairo_set_line_join(ctx.cairo(), CAIRO_LINE_JOIN_MITER);
					break;
				case LineJoinAttrib::ROUND:
					cairo_set_line_join(ctx.cairo(), CAIRO_LINE_JOIN_ROUND);
					break;
				case LineJoinAttrib::BEVEL:
					cairo_set_line_join(ctx.cairo(), CAIRO_LINE_JOIN_BEVEL);
					break;
				default: break;
			}

			switch (stroke_miter_limit_)
			{
				case MiterLimitAttrib::UNSET:	/* do nothing */ break;
				case MiterLimitAttrib::INHERIT:	/* do nothing */ break;
				case MiterLimitAttrib::VALUE:
					cairo_set_miter_limit(ctx.cairo(), stroke_miter_limit_value_);
					break;
				default: break;
			}
			// XXX stroke_dash_array_
			// XXX stroke_dash_offset_
			switch(fill_rule_)
			{
				case FillRuleAttrib::UNSET:		/* do nothing */ break;
				case FillRuleAttrib::INHERIT:	/* do nothing */ break;
				case FillRuleAttrib::NONZERO:
					cairo_set_fill_rule(ctx.cairo(), CAIRO_FILL_RULE_WINDING);
					break;
				case FillRuleAttrib::EVENODD:
					cairo_set_fill_rule(ctx.cairo(), CAIRO_FILL_RULE_EVEN_ODD);
					break;
				default: break;
			}
			// XXX color_interpolation_
			// XXX color_rendering_
			// XXX shape_rendering_
			// XXX text_rendering_
			// XXX image_rendering_
			// XXX color_profile_
		}

		void painting_properties::clear(render_context& ctx) const
		{
			if(fill_) {
				ctx.fill_color_pop();
			}
			if(stroke_) {
				ctx.stroke_color_pop();
			}
			cairo_restore(ctx.cairo());
		}

		void painting_properties::resolve(const element* doc)
		{
			// XXX
		}

		marker_attribs::marker_attribs(const ptree& pt)
		{
			auto attributes = pt.get_child_optional("<xmlattr>");

			if(attributes) {
				// using the marker attribute set's all three (start,mid,end) to the same value.
				auto marker = attributes->get_child_optional("marker");
				if(marker) {
					start_ = parse_func_iri_value(marker->data(), start_iri_);
					end_ = mid_ = start_;
					end_iri_ = mid_iri_ = start_iri_;
				}
				auto marker_start = attributes->get_child_optional("marker-start");
				if(marker_start) {
					start_ = parse_func_iri_value(marker_start->data(), start_iri_);
				}
				auto marker_mid = attributes->get_child_optional("marker-mid");
				if(marker_mid) {
					mid_ = parse_func_iri_value(marker_mid->data(), mid_iri_);
				}
				auto marker_end = attributes->get_child_optional("marker-end");
				if(marker_end) {
					end_ = parse_func_iri_value(marker_end->data(), end_iri_);
				}
			}
		}

		marker_attribs::~marker_attribs()
		{
		}

		void marker_attribs::apply(render_context& ctx) const
		{
			// XXX
		}

		void marker_attribs::clear(render_context& ctx) const
		{
			// XXX
		}

		void marker_attribs::resolve(const element* doc)
		{
			// XXX
		}
	}
}
