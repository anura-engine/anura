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

#include "AttributeSet.hpp"
#include "SceneObject.hpp"
#include "SceneTree.hpp"
#include "DisplayDevice.hpp"

#include "solid_renderable.hpp"
#include "xhtml_border_info.hpp"
#include "xhtml_box.hpp"
#include "xhtml_layout_engine.hpp"
#include "xhtml_render_ctx.hpp"

namespace xhtml
{
	using namespace css;

	namespace 
	{
		class TextureRenderable : public KRE::SceneObject
		{
		public:
			explicit TextureRenderable(KRE::TexturePtr tex) 
				: KRE::SceneObject("TextureRenderable"), 
				  attribs_() 
			{
				setTexture(tex);

				using namespace KRE;
				auto as = DisplayDevice::createAttributeSet(true);
				attribs_.reset(new KRE::Attribute<vertex_texcoord>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
				attribs_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, vtx)));
				attribs_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE,  2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, tc)));
				as->addAttribute(AttributeBasePtr(attribs_));
				as->setDrawMode(DrawMode::TRIANGLES);
		
				addAttributeSet(as);
			}
			void update(std::vector<KRE::vertex_texcoord>* coords)
			{
				attribs_->update(coords);
			}
		private:
			std::shared_ptr<KRE::Attribute<KRE::vertex_texcoord>> attribs_;
		};

		void generate_solid_left_side(std::vector<KRE::vertex_color>* vc, float x, float w, float y, float yw, float y2, float y2w, const glm::u8vec4& color)
		{
			vc->emplace_back(glm::vec2(x, y), color);
			vc->emplace_back(glm::vec2(x, y+yw), color);
			vc->emplace_back(glm::vec2(x+w, y+yw), color);

			vc->emplace_back(glm::vec2(x+w, y+yw), color);
			vc->emplace_back(glm::vec2(x, y+yw), color);
			vc->emplace_back(glm::vec2(x, y2), color);

			vc->emplace_back(glm::vec2(x, y2), color);
			vc->emplace_back(glm::vec2(x+w, y+yw), color);
			vc->emplace_back(glm::vec2(x+w, y2), color);

			vc->emplace_back(glm::vec2(x+w, y2), color);
			vc->emplace_back(glm::vec2(x, y2), color);
			vc->emplace_back(glm::vec2(x, y2+y2w), color);
		}

		void generate_solid_right_side(std::vector<KRE::vertex_color>* vc, float x, float w, float y, float yw, float y2, float y2w, const glm::u8vec4& color)
		{
			vc->emplace_back(glm::vec2(x+w, y), color);
			vc->emplace_back(glm::vec2(x, y+yw), color);
			vc->emplace_back(glm::vec2(x+w, y+yw), color);

			vc->emplace_back(glm::vec2(x+w, y+yw), color);
			vc->emplace_back(glm::vec2(x, y+yw), color);
			vc->emplace_back(glm::vec2(x, y2), color);

			vc->emplace_back(glm::vec2(x, y2), color);
			vc->emplace_back(glm::vec2(x+w, y+yw), color);
			vc->emplace_back(glm::vec2(x+w, y2), color);

			vc->emplace_back(glm::vec2(x+w, y2), color);
			vc->emplace_back(glm::vec2(x, y2), color);
			vc->emplace_back(glm::vec2(x+w, y2+y2w), color);
		}
		
		void generate_solid_top_side(std::vector<KRE::vertex_color>* vc, float x, float xw, float x2, float x2w, float y, float yw, const glm::u8vec4& color)
		{
			vc->emplace_back(glm::vec2(x, y), color);
			vc->emplace_back(glm::vec2(x+xw, y+yw), color);
			vc->emplace_back(glm::vec2(x+xw, y), color);

			vc->emplace_back(glm::vec2(x+xw, y+yw), color);
			vc->emplace_back(glm::vec2(x+xw, y), color);
			vc->emplace_back(glm::vec2(x2, y+yw), color);

			vc->emplace_back(glm::vec2(x+xw, y), color);
			vc->emplace_back(glm::vec2(x2, y+yw), color);
			vc->emplace_back(glm::vec2(x2, y), color);

			vc->emplace_back(glm::vec2(x2, y+yw), color);
			vc->emplace_back(glm::vec2(x2, y), color);
			vc->emplace_back(glm::vec2(x2+x2w, y), color);
		}

		void generate_solid_bottom_side(std::vector<KRE::vertex_color>* vc, float x, float xw, float x2, float x2w, float y, float yw, const glm::u8vec4& color)
		{
			vc->emplace_back(glm::vec2(x, y+yw), color);
			vc->emplace_back(glm::vec2(x+xw, y), color);
			vc->emplace_back(glm::vec2(x+xw, y+yw), color);

			vc->emplace_back(glm::vec2(x+xw, y+yw), color);
			vc->emplace_back(glm::vec2(x+xw, y), color);
			vc->emplace_back(glm::vec2(x2, y+yw), color);

			vc->emplace_back(glm::vec2(x+xw, y), color);
			vc->emplace_back(glm::vec2(x2, y+yw), color);
			vc->emplace_back(glm::vec2(x2, y), color);

			vc->emplace_back(glm::vec2(x2, y+yw), color);
			vc->emplace_back(glm::vec2(x2, y), color);
			vc->emplace_back(glm::vec2(x2+x2w, y+yw), color);
		}

		std::vector<glm::vec2> generate_coords(float offs, int count, float size, float spacer = 0)
		{
			std::vector<glm::vec2> res;
			for(int n = 0; n < count; ++n) {
				const float c1 = offs + n * size + (n+1) * spacer;
				const float c2 = offs + (n+1) * size + (n+1) * spacer;
				res.emplace_back(c1, c2);
			}
			return res;
		}

		void render_repeat_vert(std::vector<KRE::vertex_texcoord>* coords, float l, float t, float r, float b, float xsize, float ysize, float u1, float v1, float u2, float v2)
		{
			// figure out the vertical centre.
			const float centre_y = (b - t) / 2.0f;
			// first tile is placed so that it is positioned in the middle of this.
			const float first_tile_y1 = centre_y - ysize / 2.0f;
			const float first_tile_y2 = centre_y + ysize / 2.0f;

			const float whole_tiles_above = std::floor((first_tile_y1 - t) / ysize);
			const float whole_tiles_below = std::floor((b - first_tile_y2) / ysize);
			int total_whole_tiles = static_cast<int>(whole_tiles_above + whole_tiles_below) + 1;

			const float start_y = first_tile_y1 - whole_tiles_above * ysize;
			const float end_y   = first_tile_y2 + whole_tiles_below * ysize;

			for(auto& c : generate_coords(start_y, total_whole_tiles, ysize)) {
				coords->emplace_back(glm::vec2(l, c.x), glm::vec2(u1, v1));
				coords->emplace_back(glm::vec2(l, c.y), glm::vec2(u1, v2));
				coords->emplace_back(glm::vec2(r, c.y), glm::vec2(u2, v2));
				coords->emplace_back(glm::vec2(r, c.y), glm::vec2(u2, v2));
				coords->emplace_back(glm::vec2(l, c.x), glm::vec2(u1, v1));
				coords->emplace_back(glm::vec2(r, c.x), glm::vec2(u2, v1));
			}
			
			// Next we need to add the two fractional tiles.
			// top cap
			const float trimmed_v1 = v1 + (v2 - v1) * (1.0f - (start_y - t) / ysize); // XXX
			coords->emplace_back(glm::vec2(l, t), glm::vec2(u1, trimmed_v1));
			coords->emplace_back(glm::vec2(l, start_y), glm::vec2(u1, v2));
			coords->emplace_back(glm::vec2(r, start_y), glm::vec2(u2, v2));
			coords->emplace_back(glm::vec2(r, start_y), glm::vec2(u2, v2));
			coords->emplace_back(glm::vec2(l, t), glm::vec2(u1, trimmed_v1));
			coords->emplace_back(glm::vec2(r, t), glm::vec2(u2, trimmed_v1));
			// bottom cap
			const float trimmed_v2 = v2 - (v2 - v1) * (1.0f - (start_y - t) / ysize); // XXX
			coords->emplace_back(glm::vec2(l, end_y), glm::vec2(u1, v1));
			coords->emplace_back(glm::vec2(l, b), glm::vec2(u1, trimmed_v2));
			coords->emplace_back(glm::vec2(r, b), glm::vec2(u2, trimmed_v2));
			coords->emplace_back(glm::vec2(r, b), glm::vec2(u2, trimmed_v2));
			coords->emplace_back(glm::vec2(l, end_y), glm::vec2(u1, v1));
			coords->emplace_back(glm::vec2(r, end_y), glm::vec2(u2, v1));
		}

		void render_repeat_horiz(std::vector<KRE::vertex_texcoord>* coords, float l, float t, float r, float b, float xsize, float ysize, float u1, float v1, float u2, float v2)
		{
			// figure out the vertical centre.
			const float centre_x = (r - l) / 2.0f;
			// first tile is placed so that it is positioned in the middle of this.
			const float first_tile_x1 = centre_x - xsize / 2.0f;
			const float first_tile_x2 = centre_x + xsize / 2.0f;

			const float whole_tiles_left = std::floor((first_tile_x1 - l) / xsize);
			const float whole_tiles_right = std::floor((r - first_tile_x2) / xsize);
			int total_whole_tiles = static_cast<int>(whole_tiles_left + whole_tiles_right) + 1;

			const float start_x = first_tile_x1 - whole_tiles_left * xsize;
			const float end_x   = first_tile_x2 + whole_tiles_right * xsize;

			for(auto& c : generate_coords(start_x, total_whole_tiles, xsize)) {
				coords->emplace_back(glm::vec2(c.x, t), glm::vec2(u1, v1));
				coords->emplace_back(glm::vec2(c.y, t), glm::vec2(u1, v2));
				coords->emplace_back(glm::vec2(c.y, b), glm::vec2(u2, v2));
				coords->emplace_back(glm::vec2(c.y, b), glm::vec2(u2, v2));
				coords->emplace_back(glm::vec2(c.x, t), glm::vec2(u1, v1));
				coords->emplace_back(glm::vec2(c.x, b), glm::vec2(u2, v1));
			}
			
			// Next we need to add the two fractional tiles.
			// left cap
			const float trimmed_u1 = u1 + (u2 - u1) * (1.0f - (start_x - l) / xsize); // XXX
			coords->emplace_back(glm::vec2(l, t), glm::vec2(trimmed_u1, v1));
			coords->emplace_back(glm::vec2(l, b), glm::vec2(trimmed_u1, v2));
			coords->emplace_back(glm::vec2(start_x, b), glm::vec2(u2, v2));
			coords->emplace_back(glm::vec2(start_x, b), glm::vec2(u2, v2));
			coords->emplace_back(glm::vec2(l, t), glm::vec2(trimmed_u1, v1));
			coords->emplace_back(glm::vec2(start_x, t), glm::vec2(u2, v1));
			// right cap
			const float trimmed_u2 = u2 - (u2 - u1) * (1.0f - (start_x - l) / xsize); // XXX
			coords->emplace_back(glm::vec2(end_x, t), glm::vec2(u1, v1));
			coords->emplace_back(glm::vec2(end_x, b), glm::vec2(u1, v2));
			coords->emplace_back(glm::vec2(r, b), glm::vec2(trimmed_u2, v2));
			coords->emplace_back(glm::vec2(r, b), glm::vec2(trimmed_u2, v2));
			coords->emplace_back(glm::vec2(end_x, t), glm::vec2(u1, v1));
			coords->emplace_back(glm::vec2(r, t), glm::vec2(trimmed_u2, v1));
		}

		void render_round_vert(std::vector<KRE::vertex_texcoord>* coords, float l, float t, float r, float b, float xsize, float ysize, float u1, float v1, float u2, float v2, bool use_space=false)
		{
			// total height of space
			const float height = b - t;
			// total number of whole tiles that can fit in space
			const float whole_tiles = std::floor(height / ysize);
			if(whole_tiles <= 0) {
				return;
			}
			float y_spacer = 0.0f;
			if(!use_space) {
				ysize = height / whole_tiles;
			} else {
				y_spacer = (height - whole_tiles * ysize) / (whole_tiles + 1.0f);
			}
			const int total_whole_tiles = static_cast<int>(whole_tiles);

			for(auto& c : generate_coords(t, total_whole_tiles, ysize, y_spacer)) {
				coords->emplace_back(glm::vec2(l, c.x), glm::vec2(u1, v1));
				coords->emplace_back(glm::vec2(l, c.y), glm::vec2(u1, v2));
				coords->emplace_back(glm::vec2(r, c.y), glm::vec2(u2, v2));
				coords->emplace_back(glm::vec2(r, c.y), glm::vec2(u2, v2));
				coords->emplace_back(glm::vec2(l, c.x), glm::vec2(u1, v1));
				coords->emplace_back(glm::vec2(r, c.x), glm::vec2(u2, v1));
			}
		}

		void render_round_horiz(std::vector<KRE::vertex_texcoord>* coords, float l, float t, float r, float b, float xsize, float ysize, float u1, float v1, float u2, float v2, bool use_space=false)
		{
			// total height of space
			const float width = r - l;
			// total number of whole tiles that can fit in space
			const float whole_tiles = std::floor(width / xsize);
			if(whole_tiles <= 0) {
				return;
			}
			float x_spacer = 0.0f;
			if(!use_space) {
				xsize = width / whole_tiles;
			} else {
				x_spacer = (width - whole_tiles * xsize) / (whole_tiles + 1.0f);
			}
			const int total_whole_tiles = static_cast<int>(whole_tiles);
			for(auto& c : generate_coords(l, total_whole_tiles, xsize, x_spacer)) {
				coords->emplace_back(glm::vec2(c.x, t), glm::vec2(u1, v1));
				coords->emplace_back(glm::vec2(c.y, t), glm::vec2(u1, v2));
				coords->emplace_back(glm::vec2(c.y, b), glm::vec2(u2, v2));
				coords->emplace_back(glm::vec2(c.y, b), glm::vec2(u2, v2));
				coords->emplace_back(glm::vec2(c.x, t), glm::vec2(u1, v1));
				coords->emplace_back(glm::vec2(c.x, b), glm::vec2(u2, v1));
			}
		}
	}

	BorderInfo::BorderInfo(const StyleNodePtr& styles)
		: styles_(styles),
		  image_(),
		  slice_{},
		  outset_{},
		  widths_{}
	{
	}

	bool BorderInfo::isValid(css::Side side) const 
	{ 
		return (styles_ != nullptr && styles_->getBorderStyle()[static_cast<int>(side)] != css::BorderStyle::HIDDEN 
			&& styles_->getBorderStyle()[static_cast<int>(side)] != css::BorderStyle::NONE) || (styles_ != nullptr && styles_->getBorderImage() != nullptr); 
	}

	void BorderInfo::init(const Dimensions& dims)
	{
		if(styles_ == nullptr) {
			return;
		}
		std::array<FixedPoint, 4> border = { dims.border_.top, dims.border_.left, dims.border_.bottom, dims.border_.right };

		auto& outset = styles_->getBorderImageOutset();
		for(auto side = 0; side != 4; ++side) {
			if(outset[side].getLength().isNumber()) {
				// If the outset is a plain number, it is take as the multiple of border_widths
				outset_[side] = static_cast<float>(outset[side].getLength().compute()) / LayoutEngine::getFixedPointScale()
					* static_cast<float>(border[side]) / LayoutEngine::getFixedPointScaleFloat();
			} else {
				// is a dimensioned length.
				outset_[side] = static_cast<float>(outset[side].getLength().compute()) / LayoutEngine::getFixedPointScaleFloat();
			}
		}

		// We adjust the border image area by the outset values.
		const FixedPoint border_image_width = dims.content_.width 
			+ dims.padding_.left 
			+ dims.padding_.right 
			+ dims.border_.left 
			+ dims.border_.right
			+ static_cast<FixedPoint>((outset_[1] + outset_[3]) * LayoutEngine::getFixedPointScale());
		const FixedPoint border_image_height = dims.content_.height 
			+ dims.padding_.top 
			+ dims.padding_.bottom 
			+ dims.border_.left 
			+ dims.border_.right 
			+ static_cast<FixedPoint>((outset_[0] + outset_[2]) * LayoutEngine::getFixedPointScale());

		auto& slices = styles_->getBorderImageSlice();
		for(auto side = 0; side != 4; ++side) {
			const Length& slice_length = slices[side].getLength();
			if(slice_length.isNumber()) {
				// is number in pixels
				slice_[side] = static_cast<float>(slice_length.compute()) / LayoutEngine::getFixedPointScaleFloat();
			} else if(slice_length.isPercent()) {
				// is percentage, referring to size of border image
				FixedPoint image_wh = ((side & 1) ? image_->surfaceWidth() : image_->surfaceHeight()) * LayoutEngine::getFixedPointScale();
				slice_[side] = static_cast<float>(slice_length.compute(image_wh)) / LayoutEngine::getFixedPointScaleFloat();
				// need to cap at 100%
			} else {
				ASSERT_LOG(false, "Received a length that wasn't a number or percent for slice value: ");
			}
			ASSERT_LOG(slice_[side] >= 0, "Negative values for slices are illegal");
		}

		auto& widths = styles_->getBorderImageWidth();
		for(auto side = 0; side != 4; ++side) {
			if(widths[side].isAuto()) {
				// intrinsic width of corrsponding slice.
				widths_[side] = slice_[side];
			} else if(widths[side].getLength().isNumber()) {
				// is multiple of border width
				widths_[side] = static_cast<float>(widths[side].getLength().compute()) / LayoutEngine::getFixedPointScaleFloat() 
					* static_cast<float>(border[side]) / LayoutEngine::getFixedPointScaleFloat();
			} else if(widths[side].getLength().isPercent()) {
				// is percentage
				const FixedPoint bia = (side & 1) ? border_image_width : border_image_height;
				widths_[side] = static_cast<float>(widths[side].getLength().compute(bia)) / LayoutEngine::getFixedPointScaleFloat();
			} else {
				// is dimensioned value
				widths_[side] = static_cast<float>(widths[side].getLength().compute()) / LayoutEngine::getFixedPointScaleFloat();
			}
			ASSERT_LOG(widths_[side] >= 0, "Negative values for width are illegal");
		}

		// Proportionally reduce width values if there are pairs that would overlap.
		const float l_width = static_cast<float>(border_image_width) / LayoutEngine::getFixedPointScaleFloat();
		const float l_height = static_cast<float>(border_image_height) / LayoutEngine::getFixedPointScaleFloat();
		const float f = std::min(l_width / (widths_[1] + widths_[3]), l_height / (widths_[0] + widths_[2]));
		if(f < 1.0f) {
			for(int side = 0; side != 4; ++side) {
				widths_[side] *= f;
			}
		}

		if(styles_->getBorderImage() != nullptr) {
			image_ = styles_->getBorderImage()->getTexture(border_image_width / LayoutEngine::getFixedPointScale(), border_image_height / LayoutEngine::getFixedPointScale());
			if(image_) {
				image_->setFiltering(0, KRE::Texture::Filtering::LINEAR, KRE::Texture::Filtering::LINEAR, KRE::Texture::Filtering::POINT);
			}
		}
	}

	void BorderInfo::renderNormal(const KRE::SceneTreePtr& scene_tree, const Dimensions& dims, const point& offset) const
	{
		std::array<std::shared_ptr<SolidRenderable>, 4> border;

		FixedPoint bw[4];
		bw[0] = dims.border_.top; 
		bw[1] = dims.border_.left; 
		bw[2] = dims.border_.bottom;
		bw[3] = dims.border_.right; 

		int draw_border = 0;

		// this is the left/top edges of the appropriate side
		const float side_left    = static_cast<float>(offset.x - dims.padding_.left   - dims.border_.left) / LayoutEngine::getFixedPointScaleFloat();
		const float side_top     = static_cast<float>(offset.y - dims.padding_.top    - dims.border_.top) / LayoutEngine::getFixedPointScaleFloat();
		const float side_right   = static_cast<float>(offset.x + dims.content_.width  + dims.padding_.right) / LayoutEngine::getFixedPointScaleFloat();
		const float side_bottom  = static_cast<float>(offset.y + dims.content_.height + dims.padding_.bottom) / LayoutEngine::getFixedPointScaleFloat();
		const float left_width   = static_cast<float>(dims.border_.left) / LayoutEngine::getFixedPointScaleFloat();
		const float top_width    = static_cast<float>(dims.border_.top) / LayoutEngine::getFixedPointScaleFloat();
		const float right_width  = static_cast<float>(dims.border_.right) / LayoutEngine::getFixedPointScaleFloat();
		const float bottom_width = static_cast<float>(dims.border_.bottom) / LayoutEngine::getFixedPointScaleFloat();

		auto& border_color = styles_->getBorderColor();
		auto& border_style = styles_->getBorderStyle();
		for(int side = 0; side != 4; ++side) {
			border[side] = std::make_shared<SolidRenderable>();
			border[side]->setColorPointer(border_color[side]);
		}

		std::array<std::vector<KRE::vertex_color>, 4> vc;

		KRE::Color white;
		KRE::Color off_white(128, 128, 128);
		if(bw[0] > 0 && border_color[0]->ai() != 0 && (border_style[0] != BorderStyle::NONE || border_style[0] != BorderStyle::HIDDEN)) {
			draw_border |= 1;
			switch(border_style[0]) {
				case BorderStyle::SOLID:
					generate_solid_top_side(&vc[0], side_left, left_width, side_right, right_width, side_top, top_width, white.as_u8vec4()); 
					break;
				case BorderStyle::INSET:
					generate_solid_top_side(&vc[0], side_left, left_width, side_right, right_width, side_top, top_width, off_white.as_u8vec4());
					break;
				case BorderStyle::OUTSET:
					generate_solid_top_side(&vc[0], side_left, left_width, side_right, right_width, side_top, top_width, white.as_u8vec4());
					break;
				case BorderStyle::DOUBLE:
					generate_solid_top_side(&vc[0], side_left, left_width/3.0f, side_right+2.0f*right_width/3.0f, right_width/3.0f, side_top, top_width/3.0f, white.as_u8vec4()); 
					generate_solid_top_side(&vc[0], side_left+2.0f*left_width/3.0f, left_width/3.0f, side_right, right_width/3.0f, side_top+2.0f*top_width/3.0f, top_width/3.0f, white.as_u8vec4()); 
					break;
				case BorderStyle::GROOVE:
					generate_solid_top_side(&vc[0], side_left, left_width/2.0f, side_right+right_width/2.0f, right_width/2.0f, side_top, top_width/2.0f, off_white.as_u8vec4()); 
					generate_solid_top_side(&vc[0], side_left+left_width/2.0f, left_width/2.0f, side_right+right_width/2.0f, right_width/2.0f, side_top+top_width/2.0f, top_width/2.0f, white.as_u8vec4()); 
					break;
				case BorderStyle::RIDGE:
					generate_solid_top_side(&vc[0], side_left, left_width/2.0f, side_right+right_width/2.0f, right_width/2.0f, side_top, top_width/2.0f, white.as_u8vec4()); 
					generate_solid_top_side(&vc[0], side_left+left_width/2.0f, left_width/2.0f, side_right+right_width/2.0f, right_width/2.0f, side_top+top_width/2.0f, top_width/2.0f, off_white.as_u8vec4()); 
					break;
				case BorderStyle::DOTTED:
				case BorderStyle::DASHED:
					ASSERT_LOG(false, "No support for border style of: " << static_cast<int>(border_style[0]));
					break;
				case BorderStyle::HIDDEN:
				case BorderStyle::NONE:
				default:
					// these skip drawing.
					break;
			}
		}
		if(bw[1] > 0 && border_color[1]->ai() != 0 && (border_style[1] != BorderStyle::NONE || border_style[1] != BorderStyle::HIDDEN)) {
			draw_border |= 2;
			switch(border_style[1]) {
				case BorderStyle::SOLID:
					generate_solid_left_side(&vc[1], side_left, left_width, side_top, top_width, side_bottom, bottom_width, white.as_u8vec4());
					break;
				case BorderStyle::INSET:
					generate_solid_left_side(&vc[1], side_left, left_width, side_top, top_width, side_bottom, bottom_width, off_white.as_u8vec4());
					break;
				case BorderStyle::OUTSET:
					generate_solid_left_side(&vc[1], side_left, left_width, side_top, top_width, side_bottom, bottom_width, white.as_u8vec4());
					break;
				case BorderStyle::DOUBLE:
					generate_solid_left_side(&vc[1], side_left, left_width/3.0f, side_top, top_width/3.0f, side_bottom+2.0f*bottom_width/3.0f, bottom_width/3.0f, white.as_u8vec4()); 
					generate_solid_left_side(&vc[1], side_left+2.0f*left_width/3.0f, left_width/3.0f, side_top+2.0f*top_width/3.0f, top_width/3.0f, side_bottom, bottom_width/3.0f, white.as_u8vec4()); 
					break;
				case BorderStyle::GROOVE:
					generate_solid_left_side(&vc[1], side_left, left_width/2.0f, side_top, top_width/2.0f, side_bottom+bottom_width/2.0f, bottom_width/2.0f, off_white.as_u8vec4()); 
					generate_solid_left_side(&vc[1], side_left+left_width/2.0f, left_width/2.0f, side_top+top_width/2.0f, top_width/2.0f, side_bottom, bottom_width/2.0f, white.as_u8vec4()); 
					break;
				case BorderStyle::RIDGE:
					generate_solid_left_side(&vc[1], side_left, left_width/2.0f, side_top, top_width/2.0f, side_bottom+bottom_width/2.0f, bottom_width/2.0f, white.as_u8vec4()); 
					generate_solid_left_side(&vc[1], side_left+left_width/2.0f, left_width/2.0f, side_top+top_width/2.0f, top_width/2.0f, side_bottom, bottom_width/2.0f, off_white.as_u8vec4()); 
					break;
				case BorderStyle::DOTTED:
				case BorderStyle::DASHED:
					ASSERT_LOG(false, "No support for border style of: " << static_cast<int>(border_style[0]));
					break;
				case BorderStyle::HIDDEN:
				case BorderStyle::NONE:
				default:
					// these skip drawing.
					break;
			}
		}
		if(bw[2] > 0 && border_color[2]->ai() != 0 && (border_style[2] != BorderStyle::NONE || border_style[2] != BorderStyle::HIDDEN)) {
			draw_border |= 4;
			switch(border_style[2]) {
				case BorderStyle::SOLID:
					generate_solid_bottom_side(&vc[2], side_left, left_width, side_right, right_width, side_bottom, bottom_width, white.as_u8vec4());
					break;
				case BorderStyle::INSET:
					generate_solid_bottom_side(&vc[2], side_left, left_width, side_right, right_width, side_bottom, bottom_width, white.as_u8vec4());
					break;
				case BorderStyle::OUTSET:
					generate_solid_bottom_side(&vc[2], side_left, left_width, side_right, right_width, side_bottom, bottom_width, off_white.as_u8vec4());
					break;
				case BorderStyle::DOUBLE:
					generate_solid_bottom_side(&vc[2], side_left+2.0f*left_width/3.0f, left_width/3.0f, side_right, right_width/3.0f, side_bottom, bottom_width/3.0f, white.as_u8vec4()); 
					generate_solid_bottom_side(&vc[2], side_left, left_width/3.0f, side_right+2.0f*right_width/3.0f, right_width/3.0f, side_bottom+2.0f*bottom_width/3.0f, bottom_width/3.0f, white.as_u8vec4()); 
					break;
				case BorderStyle::GROOVE:
					generate_solid_bottom_side(&vc[2], side_left+left_width/2.0f, left_width/2.0f, side_right, right_width/2.0f, side_bottom, bottom_width/2.0f, off_white.as_u8vec4()); 
					generate_solid_bottom_side(&vc[2], side_left, left_width/2.0f, side_right+right_width/2.0f, right_width/2.0f, side_bottom+bottom_width/2.0f, bottom_width/2.0f, white.as_u8vec4());
					break;
				case BorderStyle::RIDGE:
					generate_solid_bottom_side(&vc[2], side_left+left_width/2.0f, left_width/2.0f, side_right, right_width/2.0f, side_bottom, bottom_width/2.0f, white.as_u8vec4()); 
					generate_solid_bottom_side(&vc[2], side_left, left_width/2.0f, side_right+right_width/2.0f, right_width/2.0f, side_bottom+bottom_width/2.0f, bottom_width/2.0f, off_white.as_u8vec4());
					break;
				case BorderStyle::DOTTED:
				case BorderStyle::DASHED:
					ASSERT_LOG(false, "No support for border style of: " << static_cast<int>(border_style[0]));
					break;
				case BorderStyle::HIDDEN:
				case BorderStyle::NONE:
				default:
					// these skip drawing.
					break;
			}
		}
		if(bw[3] > 0 && border_color[3]->ai() != 0 && (border_style[3] != BorderStyle::NONE || border_style[3] != BorderStyle::HIDDEN)) {
			draw_border |= 8;
			switch(border_style[3]) {
				case BorderStyle::SOLID:
					generate_solid_right_side(&vc[3], side_right, right_width, side_top, top_width, side_bottom, bottom_width, white.as_u8vec4()); 
					break;
				case BorderStyle::INSET:
					generate_solid_right_side(&vc[3], side_right, right_width, side_top, top_width, side_bottom, bottom_width, white.as_u8vec4());
					break;
				case BorderStyle::OUTSET:
					generate_solid_right_side(&vc[3], side_right, right_width, side_top, top_width, side_bottom, bottom_width, off_white.as_u8vec4());
					break;
				case BorderStyle::DOUBLE:
					generate_solid_right_side(&vc[3], side_right, right_width/3.0f, side_top+2.0f*top_width/3.0f, top_width/3.0f, side_bottom, bottom_width/3.0f, white.as_u8vec4()); 
					generate_solid_right_side(&vc[3], side_right+2.0f*right_width/3.0f, right_width/3.0f, side_top, top_width/3.0f, side_bottom+2.0f*bottom_width/3.0f, bottom_width/3.0f, white.as_u8vec4()); 
					break;
				case BorderStyle::GROOVE:
					generate_solid_right_side(&vc[3], side_right, right_width/2.0f, side_top+top_width/2.0f, top_width/2.0f, side_bottom, bottom_width/2.0f, off_white.as_u8vec4()); 
					generate_solid_right_side(&vc[3], side_right+right_width/2.0f, right_width/2.0f, side_top, top_width/2.0f, side_bottom+bottom_width/2.0f, bottom_width/2.0f, white.as_u8vec4()); 
					break;
				case BorderStyle::RIDGE:
					generate_solid_right_side(&vc[3], side_right, right_width/2.0f, side_top+top_width/2.0f, top_width/2.0f, side_bottom, bottom_width/2.0f, white.as_u8vec4()); 
					generate_solid_right_side(&vc[3], side_right+right_width/2.0f, right_width/2.0f, side_top, top_width/2.0f, side_bottom+bottom_width/2.0f, bottom_width/2.0f, off_white.as_u8vec4());
					break;
				case BorderStyle::DOTTED:
				case BorderStyle::DASHED:
					ASSERT_LOG(false, "No support for border style of: " << static_cast<int>(border_style[0]));
					break;
				case BorderStyle::HIDDEN:
				case BorderStyle::NONE:
				default:
					// these skip drawing.
					break;
			}
		}

		for(int side = 0; side != 4; ++side) {
			if(draw_border & (1 << side)) {
				border[side]->update(&vc[side]);
				scene_tree->addObject(border[side]);
			}
		}
	}

	bool BorderInfo::render(const KRE::SceneTreePtr& scene_tree, const Dimensions& dims, const point& offset) const
	{
		if(styles_ == nullptr) {
			return false;
		}

		if(image_ == nullptr) {
			// no image, indicate we should try fallback.
			renderNormal(scene_tree, dims, offset);
			return false;
		}
		bool no_fill = false;

		// Create a renderable object to store co-ordinates we will use.
		auto ptr = std::make_shared<TextureRenderable>(image_);
		std::vector<KRE::vertex_texcoord> coords;

		// These are the outside edges.
		const float y1 = static_cast<float>(offset.x - dims.padding_.top - dims.border_.top) / LayoutEngine::getFixedPointScaleFloat() - outset_[0];
		const float x1 = static_cast<float>(offset.y - dims.padding_.left - dims.border_.left) / LayoutEngine::getFixedPointScaleFloat() - outset_[1];
		const float y2 = static_cast<float>(offset.x + dims.content_.height + dims.padding_.bottom + dims.border_.bottom) / LayoutEngine::getFixedPointScaleFloat() + outset_[2];
		const float x2 = static_cast<float>(offset.y + dims.content_.width + dims.padding_.right + dims.border_.right) / LayoutEngine::getFixedPointScaleFloat() + outset_[3];

		auto uw1 = image_->getTextureCoordW(0, slice_[1]);
		auto vw1 = image_->getTextureCoordH(0, slice_[0]);
		auto uw2 = image_->getTextureCoordW(0, slice_[3]);
		auto vw2 = image_->getTextureCoordH(0, slice_[2]);

		// top-left corner
		coords.emplace_back(glm::vec2(x1, y1), glm::vec2(0.0f, 0.0f));
		coords.emplace_back(glm::vec2(x1, y1+widths_[0]), glm::vec2(0.0f, vw1));
		coords.emplace_back(glm::vec2(x1+widths_[1], y1+widths_[0]), glm::vec2(uw1, vw1));
		coords.emplace_back(glm::vec2(x1+widths_[1], y1+widths_[0]), glm::vec2(uw1, vw1));
		coords.emplace_back(glm::vec2(x1, y1), glm::vec2(0.0f, 0.0f));
		coords.emplace_back(glm::vec2(x1+widths_[1], y1), glm::vec2(uw1, 0.0f));

		// top-right corner
		coords.emplace_back(glm::vec2(x2-widths_[3], y1), glm::vec2(1.0f-uw2, 0.0f));
		coords.emplace_back(glm::vec2(x2-widths_[3], y1+widths_[0]), glm::vec2(1.0f-uw2, vw1));
		coords.emplace_back(glm::vec2(x2, y1+widths_[0]), glm::vec2(1.0f, vw1));
		coords.emplace_back(glm::vec2(x2, y1+widths_[0]), glm::vec2(1.0f, vw1));
		coords.emplace_back(glm::vec2(x2-widths_[3], y1), glm::vec2(1.0f-uw2, 0.0f));
		coords.emplace_back(glm::vec2(x2, y1), glm::vec2(1.0f, 0.0f));

		// bottom-left corner
		coords.emplace_back(glm::vec2(x1, y2-widths_[2]), glm::vec2(0.0f, 1.0f-vw2));
		coords.emplace_back(glm::vec2(x1, y2), glm::vec2(0.0f, 1.0f));
		coords.emplace_back(glm::vec2(x1+widths_[1], y2), glm::vec2(uw1, 1.0f));
		coords.emplace_back(glm::vec2(x1+widths_[1], y2), glm::vec2(uw1, 1.0f));
		coords.emplace_back(glm::vec2(x1, y2-widths_[2]), glm::vec2(0.0f, 1.0f-vw2));
		coords.emplace_back(glm::vec2(x1+widths_[1], y2-widths_[2]), glm::vec2(uw1, 1.0f-vw2));

		// bottom-right corner
		coords.emplace_back(glm::vec2(x2-widths_[3], y2-widths_[2]), glm::vec2(1.0f-uw2, 1.0f-vw2));
		coords.emplace_back(glm::vec2(x2-widths_[3], y2), glm::vec2(1.0f-uw2, 1.0f));
		coords.emplace_back(glm::vec2(x2, y2), glm::vec2(1.0f, 1.0f));
		coords.emplace_back(glm::vec2(x2, y2), glm::vec2(1.0f, 1.0f));
		coords.emplace_back(glm::vec2(x2-widths_[3], y2-widths_[2]), glm::vec2(1.0f-uw2, 1.0f-vw2));
		coords.emplace_back(glm::vec2(x2, y2-widths_[2]), glm::vec2(1.0f, 1.0f-vw2));

		const float side_height = x2 - x1 - (widths_[2] + widths_[0]);
		const float side_width = y2 - y1 - (widths_[3] + widths_[1]);

		// left and right sides being shown are contingent on the top/bottom slices
		// being less than the height of the image
		if(slice_[0] + slice_[2] < image_->surfaceHeight()) {
			const float l_u1 = 0.0f;
			const float l_v1 = vw1;
			const float l_u2 = uw2;
			const float l_v2 = 1.0f - vw2;

			const float r_u1 = 1.0f - uw2;
			const float r_v1 = vw1;
			const float r_u2 = 1.0f;
			const float r_v2 = 1.0f - vw2;
			switch(styles_->getBorderImageRepeatVert()) {
				case CssBorderImageRepeat::STRETCH: 
					// left-side
					coords.emplace_back(glm::vec2(x1, y1+widths_[0]), glm::vec2(l_u1, l_v1));
					coords.emplace_back(glm::vec2(x1, y2-widths_[2]), glm::vec2(l_u1, l_v2));
					coords.emplace_back(glm::vec2(x1+widths_[1], y2-widths_[2]), glm::vec2(l_u2, l_v2));
					coords.emplace_back(glm::vec2(x1+widths_[1], y2-widths_[2]), glm::vec2(l_u2, l_v2));
					coords.emplace_back(glm::vec2(x1, y1+widths_[0]), glm::vec2(l_u1, l_v1));
					coords.emplace_back(glm::vec2(x1+widths_[1], y1+widths_[0]), glm::vec2(l_u2, l_v1));

					// right-side
					coords.emplace_back(glm::vec2(x2-widths_[3], y1+widths_[0]), glm::vec2(r_u1, r_v1));
					coords.emplace_back(glm::vec2(x2-widths_[3], y2-widths_[2]), glm::vec2(r_u1, r_v2));
					coords.emplace_back(glm::vec2(x2, y2-widths_[2]), glm::vec2(r_u2, r_v2));
					coords.emplace_back(glm::vec2(x2, y2-widths_[2]), glm::vec2(r_u2, r_v2));
					coords.emplace_back(glm::vec2(x2-widths_[3], y1+widths_[0]), glm::vec2(r_u1, r_v1));
					coords.emplace_back(glm::vec2(x2, y1+widths_[0]), glm::vec2(r_u2, r_v1));
					break;
				case CssBorderImageRepeat::REPEAT:
					// left side
					render_repeat_vert(&coords, x1, y1+widths_[0], x1+widths_[1], y2-widths_[2], widths_[1], widths_[0], l_u1, l_v1, l_u2, l_v2);
					// right side
					render_repeat_vert(&coords, x2-widths_[3], y1+widths_[0], x2, y2-widths_[2], widths_[3], widths_[2], r_u1, r_v1, r_u2, r_v2);
					break;
				case CssBorderImageRepeat::ROUND:
					// left side
					render_round_vert(&coords, x1, y1+widths_[0], x1+widths_[1], y2-widths_[2], widths_[1], widths_[0], l_u1, l_v1, l_u2, l_v2);
					// right side
					render_round_vert(&coords, x2-widths_[3], y1+widths_[0], x2, y2-widths_[2], widths_[3], widths_[2], r_u1, r_v1, r_u2, r_v2);
					break;
				case CssBorderImageRepeat::SPACE:
					// left side
					render_round_vert(&coords, x1, y1+widths_[0], x1+widths_[1], y2-widths_[2], widths_[1], widths_[0], l_u1, l_v1, l_u2, l_v2, true);
					// right side
					render_round_vert(&coords, x2-widths_[3], y1+widths_[0], x2, y2-widths_[2], widths_[3], widths_[2], r_u1, r_v1, r_u2, r_v2, true);
					break;
			}
		} else {
			no_fill = true;
		}

		// top and bottom sides being shown are contingent on the left/right slices
		// being less than the width of the image
		if(slice_[1] + slice_[3] < image_->surfaceWidth()) {
			const float t_u1 = uw1;
			const float t_v1 = 0.0f;
			const float t_u2 = 1.0f - uw2;
			const float t_v2 = vw1;

			const float b_u1 = uw1;
			const float b_v1 = 1.0f - vw2;
			const float b_u2 = 1.0f - uw2;
			const float b_v2 = 1.0f;

			switch(styles_->getBorderImageRepeatHoriz()) {
				case CssBorderImageRepeat::STRETCH:
					// top-side
					coords.emplace_back(glm::vec2(x1+widths_[1], y1), glm::vec2(t_u1, t_v1));
					coords.emplace_back(glm::vec2(x1+widths_[1], y1+widths_[0]), glm::vec2(t_u1, t_v2));
					coords.emplace_back(glm::vec2(x2-widths_[3], y1+widths_[0]), glm::vec2(t_u2, t_v2));
					coords.emplace_back(glm::vec2(x2-widths_[3], y1+widths_[0]), glm::vec2(t_u2, t_v2));
					coords.emplace_back(glm::vec2(x1+widths_[1], y1), glm::vec2(t_u1, t_v1));
					coords.emplace_back(glm::vec2(x2-widths_[3], y1), glm::vec2(t_u2, t_v1));

					// bottom-side
					coords.emplace_back(glm::vec2(x1+widths_[1], y2-widths_[2]), glm::vec2(b_u1, b_v1));
					coords.emplace_back(glm::vec2(x1+widths_[1], y2), glm::vec2(b_u1, b_v2));
					coords.emplace_back(glm::vec2(x2-widths_[3], y2), glm::vec2(b_u2, b_v2));
					coords.emplace_back(glm::vec2(x2-widths_[3], y2), glm::vec2(b_u2, b_v2));
					coords.emplace_back(glm::vec2(x1+widths_[1], y2-widths_[2]), glm::vec2(b_u1, b_v1));
					coords.emplace_back(glm::vec2(x2-widths_[3], y2-widths_[2]), glm::vec2(b_u2, b_v1));
					break;
				case CssBorderImageRepeat::REPEAT:
					// top-side
					render_repeat_horiz(&coords, x1+widths_[1], y1, x2-widths_[3], y1+widths_[0], widths_[1], widths_[0], t_u1, t_v1, t_u2, t_v2);
					// bottom-side
					render_repeat_horiz(&coords, x1+widths_[1], y2-widths_[2], x2-widths_[3], y2, widths_[3], widths_[2], b_u1, b_v1, b_u2, b_v2);
					break;
				case CssBorderImageRepeat::ROUND:
					// top-side
					render_round_horiz(&coords, x1+widths_[1], y1, x2-widths_[3], y1+widths_[0], widths_[1], widths_[0], t_u1, t_v1, t_u2, t_v2);
					// bottom-side
					render_round_horiz(&coords, x1+widths_[1], y2-widths_[2], x2-widths_[3], y2, widths_[3], widths_[2], b_u1, b_v1, b_u2, b_v2);
					break;
				case CssBorderImageRepeat::SPACE:
					// top-side
					render_round_horiz(&coords, x1+widths_[1], y1, x2-widths_[3], y1+widths_[0], widths_[1], widths_[0], t_u1, t_v1, t_u2, t_v2, true);
					// bottom-side
					render_round_horiz(&coords, x1+widths_[1], y2-widths_[2], x2-widths_[3], y2, widths_[3], widths_[2], b_u1, b_v1, b_u2, b_v2, true);
					break;
			}
		} else {
			no_fill = true;
		}

		// fill
		if(styles_->isBorderImageFilled() && !no_fill) {
			if(styles_->getBorderImageRepeatHoriz() == CssBorderImageRepeat::STRETCH && styles_->getBorderImageRepeatVert() == CssBorderImageRepeat::STRETCH) {
				// handle this case seperately as it's the easiest, requiring no tiling.
				const float m_u1 = uw1;
				const float m_v1 = vw1;
				const float m_u2 = 1.0f - uw2;
				const float m_v2 = 1.0f - vw2;

				coords.emplace_back(glm::vec2(x1+widths_[1], y1+widths_[0]), glm::vec2(m_u1, m_v1));
				coords.emplace_back(glm::vec2(x1+widths_[1], y2-widths_[2]), glm::vec2(m_u1, m_v2));
				coords.emplace_back(glm::vec2(x2-widths_[3], y2-widths_[2]), glm::vec2(m_u2, m_v2));
				coords.emplace_back(glm::vec2(x2-widths_[3], y2-widths_[2]), glm::vec2(m_u2, m_v2));
				coords.emplace_back(glm::vec2(x1+widths_[1], y1+widths_[0]), glm::vec2(m_u1, m_v1));
				coords.emplace_back(glm::vec2(x2-widths_[3], y1+widths_[0]), glm::vec2(m_u2, m_v1));
			}
		}

		// pass co-ordinates to renderable object and add to display list ready for rendering.
		ptr->update(&coords);
		scene_tree->addObject(ptr);
		// returning true indicates we handled drawing the border
		return true;
	}
}
