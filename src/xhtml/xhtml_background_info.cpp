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

#include <cairo.h>

#include "AttributeSet.hpp"
#include "Blittable.hpp"
#include "CameraObject.hpp"
#include "StencilScope.hpp"
#include "DisplayDevice.hpp"
#include "RenderTarget.hpp"
#include "SceneObject.hpp"
#include "Shaders.hpp"
#include "WindowManager.hpp"

#include "profile_timer.hpp"
#include "solid_renderable.hpp"
#include "xhtml_background_info.hpp"
#include "xhtml_layout_engine.hpp"

namespace xhtml
{
	using namespace css;

	namespace
	{
		static const int kernel_size = 9;
		static const int half_kernel_size = kernel_size / 2;
		static unsigned kernel_acc = 0;
		std::vector<uint8_t>& get_gaussian_kernel()
		{
			static std::vector<uint8_t> res;
			if(res.empty()) {
				res.resize(kernel_size);
				kernel_acc = 0;
				for(int n = 0; n != kernel_size; ++n) {
					float f = static_cast<float>(n - half_kernel_size);
					res[n] = static_cast<uint8_t>(std::exp(-f * f / 30.0f) * 80.0f);
					kernel_acc += res[n];
				}
			}
			return res;
		}

		void gaussian_filter(int width, int height, uint8_t* src, int src_stride, int radius)
		{
			cairo_surface_t* tmp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

			uint8_t *dst = cairo_image_surface_get_data(tmp);
			const int dst_stride = cairo_image_surface_get_stride(tmp);

			profile::manager pman("convolution");
			auto& kernel = get_gaussian_kernel();

			for(int y = 0; y != height; ++y) {
				uint32_t* s = reinterpret_cast<uint32_t *>(src + y * src_stride);
				uint32_t* d = reinterpret_cast<uint32_t *>(dst + y * dst_stride);
				for(int x = 0; x != width; ++x) {
					if(radius < x && x < width - radius) {
						d[x] = s[x];
						continue;
					}

					int r = 0, g = 0, b = 0, a = 0;
					for(int n = 0; n != kernel_size; ++n) {
						if (x - half_kernel_size + n < 0 || x - half_kernel_size + n >= width) {
							continue;
						}
						uint32_t pix = s[x - half_kernel_size + n];
						r += ((pix >> 24) & 0xff) * kernel[n];
						g += ((pix >> 16) & 0xff) * kernel[n];
						b += ((pix >>  8) & 0xff) * kernel[n];
						a += ((pix >>  0) & 0xff) * kernel[n];
					}
					d[x] = (r / kernel_acc << 24) | (g / kernel_acc << 16) | (b / kernel_acc << 8) | (a / kernel_acc);
				}
			}

			for(int y = 0; y != height; ++y) {
				uint32_t* s = reinterpret_cast<uint32_t *>(dst + y * dst_stride);
				uint32_t* d = reinterpret_cast<uint32_t *>(src + y * src_stride);
				for(int x = 0; x != width; ++x) {
					if(radius <= y && y < height - radius) {
						d[x] = s[x];
						continue;
					}

					int r = 0, g = 0, b = 0, a = 0;
					for(int n = 0; n != kernel_size; ++n) {
						if (y - half_kernel_size + n < 0 || y - half_kernel_size + n >= height) {
							continue;
						}

						s = reinterpret_cast<uint32_t *>(dst + (y - half_kernel_size + n) * dst_stride);
						uint32_t pix = s[x];
						r += ((pix >> 24) & 0xff) * kernel[n];
						g += ((pix >> 16) & 0xff) * kernel[n];
						b += ((pix >>  8) & 0xff) * kernel[n];
						a += ((pix >>  0) & 0xff) * kernel[n];
					}
					d[x] = (r / kernel_acc << 24) | (g / kernel_acc << 16) | (b / kernel_acc << 8) | (a / kernel_acc);
				}
			}
			cairo_surface_destroy(tmp);
		}


		KRE::StencilSettings& get_stencil_mask_settings() 
		{
			static KRE::StencilSettings ss(true, 
				KRE::StencilFace::FRONT_AND_BACK, 
				KRE::StencilFunc::NOT_EQUAL, 
				0xff, 0x00, 0xff, 
				KRE::StencilOperation::INCREMENT, 
				KRE::StencilOperation::KEEP, 
				KRE::StencilOperation::KEEP);
			return ss;
		}

		
		void calculate_ellipse_quadrant(std::vector<glm::vec2>* res, int divisions, float rx, float ry, float x_start, float x_end, float x_offset, float y_offset)
		{
			// divisions is the number of increments along the x-axis to use.
			// rx (a) the x radius.
			// ry (b) the y radius.
			// x_start starting x co-ordinate.
			// x_end ending x co-ordinate.
			// x_offset X Offset added to result (i.e. x translation)
			// y_offset Y Offset added to result (i.e. y translation)

			ASSERT_LOG(divisions > 0, "Number of divisions can't be zero of negative.");

			const float rx_sqr = rx * rx;

			float x_incr = (x_end - x_start) / (divisions - 1);
			float x = x_start;
			for(int n = 0; n != divisions; ++n) {
				const float intermediate = 1 - (x * x / rx_sqr);
				ASSERT_LOG(intermediate >= 0, "Intermediate value was less than zero.");
				const float y = std::sqrt(intermediate) * ry;
				res->emplace_back(x + x_offset, y + y_offset);
				x += x_incr;
			}
		}

		void calculate_border_shape(std::vector<glm::vec2>* res, 
			const std::array<FixedPoint, 4>& horiz_radius, 
			const std::array<FixedPoint, 4>& vert_radius, 
			float left, float top, float right, float bottom)
		{
			// res The result, i.e. where vertices go.
			// horiz_radius The horizontal radius values. Ordered TL, TR, BR, BL.
			// vert_radius The vertical radius values. Ordered TL, TR, BR, BL.
			// left, top, right, bottom The border box bounds.

			std::array<glm::vec2, 4> corners;
			corners[0] = glm::vec2(left, top);
			corners[1] = glm::vec2(right, top);
			corners[2] = glm::vec2(right, bottom);
			corners[3] = glm::vec2(left, bottom);

			std::array<glm::vec2, 4> quadrant = {{ glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(-1.0f, 1.0f) }};

			const float width = right - left;
			const float height = bottom - top;

			const float center_x = (right - left) / 2.0f;
			const float center_y = (bottom - top) / 2.0f;

			std::array<glm::vec2, 4> radii;
			radii[0] = glm::vec2(horiz_radius[0]/LayoutEngine::getFixedPointScaleFloat(), vert_radius[0]/LayoutEngine::getFixedPointScaleFloat());
			radii[1] = glm::vec2(horiz_radius[1]/LayoutEngine::getFixedPointScaleFloat(), vert_radius[1]/LayoutEngine::getFixedPointScaleFloat());
			radii[2] = glm::vec2(horiz_radius[2]/LayoutEngine::getFixedPointScaleFloat(), vert_radius[2]/LayoutEngine::getFixedPointScaleFloat());
			radii[3] = glm::vec2(horiz_radius[3]/LayoutEngine::getFixedPointScaleFloat(), vert_radius[3]/LayoutEngine::getFixedPointScaleFloat());

			const float left_radius_height = radii[0].x + radii[1].x;
			const float right_radius_height = radii[2].x + radii[3].x;
			const float bottom_radius_width = radii[1].y + radii[2].y;
			const float top_radius_width = radii[0].y + radii[3].y;

			const float fx = std::min(top_radius_width == 0.0f ? 1.0f : width / top_radius_width, bottom_radius_width == 0.0f ? 1.0f : width / bottom_radius_width);
			const float fy = std::min(left_radius_height == 0.0f ? 1.0f : height / left_radius_height, right_radius_height == 0.0f ? 1.0f : height / right_radius_height);
			const float f = std::min(fx, fy);

			for(int corner = 0; corner != 4; ++corner) {
				if(f < 1.0f) {
					radii[corner].x *= f;
					radii[corner].y *= f;
				}
				// check for overlapping borders and proportionally reduce them.
				if(radii[corner].x > width/2.0f) {
					radii[corner].x = width/2.0f;
				}
				if(radii[corner].y > height/2.0f) {
					radii[corner].y = height/2.0f;
				}
				if(radii[corner].x == 0 && radii[corner].y == 0) {
					// Is just a square edge
					// XXX this may need adjust if the ellipse from an adjacent intersects
					res->emplace_back(corners[corner]);
				} else {
					const float rx = radii[corner].x;
					const float ry = radii[corner].y;

					//roughly estimate number of divisions
					const int ndivisions = std::max(10, std::max(static_cast<int>(rx)/2, static_cast<int>(ry)/2));

					calculate_ellipse_quadrant(res, ndivisions, 
						rx, ry * quadrant[corner].t, 
						(corner % 2) ? 0 : quadrant[corner].s * rx , (corner % 2) ? rx * quadrant[corner].s : 0, 
						corners[corner].x - rx * quadrant[corner].s, 
						corners[corner].y - ry * quadrant[corner].t);
				}
			}
		}

		KRE::RenderablePtr create_border_mask(const std::array<FixedPoint, 4>& horiz_radius, 
			const std::array<FixedPoint, 4>& vert_radius, 
			int left, int top, int right, int bottom)
		{
			std::shared_ptr<SimpleRenderable> sr = std::make_shared<SimpleRenderable>(KRE::DrawMode::TRIANGLE_FAN);

			const float l = static_cast<float>(left);
			const float t = static_cast<float>(top);
			const float r = static_cast<float>(right);
			const float b = static_cast<float>(bottom);

			const float cx = (r-l)/2.0f;
			const float cy = (b-t)/2.0f;

			std::vector<glm::vec2> vertices;
			vertices.reserve(20*4);
			
			// Place center as first vertex
			vertices.emplace_back(cx, cy);

			calculate_border_shape(&vertices, horiz_radius, vert_radius, l, t, r, b);
			// repeat second point.
			vertices.push_back(vertices[1]);

			sr->update(&vertices);

			return sr;
		}
	}

	BgBoxShadow::BgBoxShadow() 
		: x_offset(0), 
		  y_offset(0), 
		  blur_radius(0), 
		  spread_radius(0), 
		  inset(false), 
		  color(std::make_shared<KRE::Color>(KRE::Color::colorBlack()))
	{
	}

	BgBoxShadow::BgBoxShadow(FixedPoint x, FixedPoint y, FixedPoint blur, FixedPoint spread, bool ins, const KRE::ColorPtr& col) 
		: x_offset(x), 
		  y_offset(y), 
		  blur_radius(blur), 
		  spread_radius(spread), 
		  inset(ins), 
		  color(col) 
	{
	}

	BackgroundInfo::BackgroundInfo(const StyleNodePtr& styles)
		: styles_(styles),
		  texture_(),
		  box_shadows_(),
		  border_radius_horiz_{},
		  border_radius_vert_{},
		  has_border_radius_(false)
	{
		if(styles == nullptr) {
			return;
		}
		if(styles_->getBoxShadow() != nullptr) {
			auto& shadows = styles_->getBoxShadow()->getShadows();
			for(auto it = shadows.crbegin(); it != shadows.crend(); ++it) {
				auto& shadow = *it;
				box_shadows_.emplace_back(shadow.getX().compute(), 
					shadow.getY().compute(), 
					shadow.getBlur().compute(), 
					shadow.getSpread().compute(), 
					shadow.inset(), 
					shadow.getColor().compute());
			}
		}
	}

	void BackgroundInfo::init(const Dimensions& dims)
	{
		if(styles_ == nullptr) {
			return;
		}
		// Height and width of border box
		FixedPoint bbox_width  = dims.content_.width + dims.padding_.left + dims.padding_.right + dims.border_.left + dims.border_.right;
		FixedPoint bbox_height = dims.content_.height + dims.padding_.top + dims.padding_.bottom + dims.border_.top + dims.border_.bottom;

		Property props[4]  = { Property::BORDER_TOP_LEFT_RADIUS, Property::BORDER_TOP_RIGHT_RADIUS, Property::BORDER_BOTTOM_LEFT_RADIUS, Property::BORDER_BOTTOM_RIGHT_RADIUS };
		for(int n = 0; n != 4; ++n) {
			auto& br = styles_->getBorderRadius();
			border_radius_horiz_[n] = br[n]->getHoriz().compute(bbox_width);
			border_radius_vert_[n]  = br[n]->getVert().compute(bbox_height);
			if(border_radius_horiz_[n] != 0 || border_radius_vert_[n] != 0) {
				has_border_radius_ = true;
			}
		}

		if(styles_->getBackgroundImage() != nullptr) {
			texture_ = styles_->getBackgroundImage()->getTexture(bbox_width, bbox_height);
			if(texture_) {
				texture_->setFiltering(0, KRE::Texture::Filtering::LINEAR, KRE::Texture::Filtering::LINEAR, KRE::Texture::Filtering::POINT);
				switch(styles_->getBackgroundRepeat()) {
					case BackgroundRepeat::REPEAT:
						texture_->setAddressModes(0, KRE::Texture::AddressMode::WRAP, KRE::Texture::AddressMode::WRAP, KRE::Texture::AddressMode::WRAP);
						break;
					case BackgroundRepeat::REPEAT_X:
						texture_->setAddressModes(0, KRE::Texture::AddressMode::WRAP, KRE::Texture::AddressMode::BORDER, KRE::Texture::AddressMode::BORDER, KRE::Color(0,0,0,0));
						break;
					case BackgroundRepeat::REPEAT_Y:
						texture_->setAddressModes(0, KRE::Texture::AddressMode::BORDER, KRE::Texture::AddressMode::WRAP, KRE::Texture::AddressMode::BORDER, KRE::Color(0,0,0,0));
						break;
					case BackgroundRepeat::NO_REPEAT:
						texture_->setAddressModes(0, KRE::Texture::AddressMode::BORDER, KRE::Texture::AddressMode::BORDER, KRE::Texture::AddressMode::BORDER, KRE::Color(0,0,0,0));
						break;
				}
			}
		}
	}

	void BackgroundInfo::renderBoxShadow(const KRE::SceneTreePtr& scene_tree, const Dimensions& dims, KRE::RenderablePtr clip_shape) const
	{
		using namespace KRE;

		// XXX We should be using the shape generated via clipping.
		const int box_width = (dims.content_.width 
			+ dims.border_.right + dims.padding_.right 
			+ dims.border_.left + dims.padding_.left) / LayoutEngine::getFixedPointScale();
		const int box_height = (dims.content_.height 
			+ dims.border_.top + dims.padding_.top 
			+ dims.border_.bottom + dims.padding_.bottom ) / LayoutEngine::getFixedPointScale();

		RenderablePtr new_clip_shape = nullptr;
		if(clip_shape != nullptr) {
			new_clip_shape = std::shared_ptr<Renderable>(new Renderable(*clip_shape));
		}

		for(auto& shadow : box_shadows_) {
			if(shadow.inset) {
				// XXX
			} else {
				// This needs to be clipped by the background clip shape.
				const float ssr = shadow.spread_radius / LayoutEngine::getFixedPointScaleFloat();

				const float spread_width = static_cast<float>(box_width) + 2 * ssr;
				const float spread_height = static_cast<float>(box_height) + 2 * ssr;

				if(std::abs(shadow.blur_radius) < FLT_EPSILON 
					|| !KRE::DisplayDevice::checkForFeature(KRE::DisplayDeviceCapabilties::RENDER_TO_TEXTURE)) {
					rectf box_size(0, 0, spread_width, spread_height);
					SolidRenderablePtr box = std::make_shared<SolidRenderable>(box_size, shadow.color);
					if(clip_shape != nullptr) {
						RenderablePtr new_clip_shape = std::shared_ptr<Renderable>(new Renderable(*clip_shape));
						const float scalew = spread_width / static_cast<float>(box_width);
						const float scaleh = spread_height / static_cast<float>(box_height);
						new_clip_shape->setScale(scalew, scaleh);
						box->setClipSettings(get_stencil_mask_settings(), new_clip_shape);
					}
					box->setPosition((shadow.x_offset) / LayoutEngine::getFixedPointScaleFloat() - ssr, 
						(shadow.y_offset) / LayoutEngine::getFixedPointScaleFloat() - ssr);
					scene_tree->addObject(box);
				} else {
					const int gaussian_radius = 7;
				
					const int width = static_cast<int>(spread_width + gaussian_radius * 4); 
					const int height = static_cast<int>(spread_height + gaussian_radius * 4);

					auto shader_blur = ShaderProgram::createGaussianShader(gaussian_radius)->clone();
					const int blur_two = shader_blur->getUniform("texel_width_offset");
					const int blur_tho = shader_blur->getUniform("texel_height_offset");
					const int u_gaussian = shader_blur->getUniform("gaussian");
					std::vector<float> gaussian = generate_gaussian(ssr/2.0f, gaussian_radius);

					CameraPtr rt_cam = std::make_shared<Camera>("ortho_blur", 0, width, 0, height);

					rect box_size(0, 0, static_cast<int>(spread_width), static_cast<int>(spread_height));
					SolidRenderablePtr box = std::make_shared<SolidRenderable>(box_size, shadow.color);
					if(clip_shape != nullptr) {
						const float scalew = spread_width / static_cast<float>(box_width);
						const float scaleh = spread_height / static_cast<float>(box_height);
						new_clip_shape->setScale(scalew, scaleh);
						box->setClipSettings(get_stencil_mask_settings(), new_clip_shape);
					}
					box->setPosition(gaussian_radius * 2, gaussian_radius * 2);
					box->setCamera(rt_cam);

					WindowPtr wnd = WindowManager::getMainWindow();
					// We need to create the "rt_blur_h" render target with a minimum of a stencil buffer.
					RenderTargetPtr rt_blur_h = RenderTarget::create(width, height, 1, false, true/*, true, 4*/);
					rt_blur_h->getTexture()->setFiltering(-1, Texture::Filtering::LINEAR, Texture::Filtering::LINEAR, Texture::Filtering::POINT);
					rt_blur_h->getTexture()->setAddressModes(-1, Texture::AddressMode::CLAMP, Texture::AddressMode::CLAMP);
					rt_blur_h->setCentre(Blittable::Centre::TOP_LEFT);
					rt_blur_h->setClearColor(Color(0,0,0,0));
					{
						RenderTarget::RenderScope rs(rt_blur_h, rect(0, 0, width, height));
						box->preRender(wnd);
						wnd->render(box.get());
					}
					rt_blur_h->setCamera(rt_cam);
					rt_blur_h->setShader(shader_blur);
					shader_blur->setUniformDrawFunction([blur_two, blur_tho, width, gaussian, u_gaussian](ShaderProgramPtr shader){ 
						shader->setUniformValue(u_gaussian, &gaussian[0]);
						shader->setUniformValue(blur_two, 1.0f / (width - 1.0f));
						shader->setUniformValue(blur_tho, 0.0f);
					});

					RenderTargetPtr rt_blur_v = RenderTarget::create(width, height);
					rt_blur_v->getTexture()->setFiltering(-1, Texture::Filtering::LINEAR, Texture::Filtering::LINEAR, Texture::Filtering::POINT);
					rt_blur_v->getTexture()->setAddressModes(-1, Texture::AddressMode::CLAMP, Texture::AddressMode::CLAMP);
					rt_blur_v->setCentre(Blittable::Centre::TOP_LEFT);
					rt_blur_v->setClearColor(Color(0,0,0,0));
					{
						RenderTarget::RenderScope rs(rt_blur_v, rect(0, 0, width, height));
						rt_blur_h->preRender(wnd);
						wnd->render(rt_blur_h.get());
					}
					rt_blur_v->setShader(shader_blur);
					shader_blur->setUniformDrawFunction([blur_two, blur_tho, height, gaussian, u_gaussian](ShaderProgramPtr shader){ 
						shader->setUniformValue(u_gaussian, &gaussian[0]);
						shader->setUniformValue(blur_two, 0.0f);
						shader->setUniformValue(blur_tho, 1.0f / (height - 1.0f));
					});

					rt_blur_v->setPosition((shadow.x_offset) / LayoutEngine::getFixedPointScaleFloat() - ssr - gaussian_radius * 2, 
						(shadow.y_offset) / LayoutEngine::getFixedPointScaleFloat() - ssr - gaussian_radius * 2);
					scene_tree->addObject(rt_blur_v);
				}
			}
		}
	}

	void BackgroundInfo::render(const KRE::SceneTreePtr& scene_tree, const Dimensions& dims, const point& offset) const
	{
		if(styles_ == nullptr) {
			return;
		}

		// XXX if we're rendering the body element then it takes the entire canvas :-/
		// technically the rule is that if no background styles are applied to the html element then
		// we apply the body styles.
		const int rx = (offset.x - dims.padding_.left - dims.border_.left) / LayoutEngine::getFixedPointScale();
		const int ry = (offset.y - dims.padding_.top - dims.border_.top) / LayoutEngine::getFixedPointScale();
		const int rw = (dims.content_.width + dims.padding_.left + dims.padding_.right + dims.border_.left + dims.border_.right) / LayoutEngine::getFixedPointScale();
		const int rh = (dims.content_.height + dims.padding_.top + dims.padding_.bottom + dims.border_.top + dims.border_.bottom) / LayoutEngine::getFixedPointScale();


		KRE::RenderablePtr clip_shape = nullptr;
		switch(styles_->getBackgroundClip()) {
			case BackgroundClip::BORDER_BOX:
				// don't do anything unless border-radius is specified
				if(has_border_radius_) {
					clip_shape = create_border_mask(border_radius_horiz_, border_radius_vert_, 0, 0, rw, rh);					
				}
				break;
			case BackgroundClip::PADDING_BOX:
				clip_shape = std::make_shared<SolidRenderable>(rect(
					0,
					0,
					(dims.content_.width + dims.padding_.left + dims.padding_.right) / LayoutEngine::getFixedPointScale(),
					(dims.content_.height + dims.padding_.top + dims.padding_.bottom) / LayoutEngine::getFixedPointScale()));
				clip_shape->setPosition(dims.border_.left/LayoutEngine::getFixedPointScale(), dims.border_.top/LayoutEngine::getFixedPointScale());
				break;
			case BackgroundClip::CONTENT_BOX:
				clip_shape = std::make_shared<SolidRenderable>(rect(
					0,
					0,
					dims.content_.width / LayoutEngine::getFixedPointScale(),
					dims.content_.height / LayoutEngine::getFixedPointScale()));
				clip_shape->setPosition((dims.padding_.left + dims.border_.left)/LayoutEngine::getFixedPointScale(), 
					(dims.padding_.top + dims.border_.top)/LayoutEngine::getFixedPointScale());
				break;
		}

		renderBoxShadow(scene_tree, dims, clip_shape);

		if(styles_->getBackgroundColor()->ai() != 0) {
			auto solid = std::make_shared<SolidRenderable>(rect(0, 0, rw, rh), styles_->getBackgroundColor());
			solid->setPosition(rx, ry);
			if(clip_shape != nullptr) {
				solid->setClipSettings(get_stencil_mask_settings(), clip_shape);
			}
			scene_tree->addObject(solid);
		}
		// XXX if texture is set then use background position and repeat as appropriate.
		if(texture_ != nullptr) {
			// With a value pair of '14% 84%', the point 14% across and 84% down the image is to be placed at the point 14% across and 84% down the padding box.
			const int sw = texture_->surfaceWidth();
			const int sh = texture_->surfaceHeight();

			const FixedPoint rx = (offset.x - dims.padding_.left - dims.border_.left);
			const FixedPoint ry = (offset.y - dims.padding_.top - dims.border_.top);
			const FixedPoint rw = (dims.content_.width + dims.padding_.left + dims.padding_.right + dims.border_.left + dims.border_.right);
			const FixedPoint rh = (dims.content_.height + dims.padding_.top + dims.padding_.bottom + dims.border_.top + dims.border_.bottom);

			int sw_offs = 0;
			int sh_offs = 0;
			
			auto& pos_top = styles_->getBackgroundPosition()[0];
			auto& pos_left = styles_->getBackgroundPosition()[1];
			if(pos_left.isPercent()) {
				sw_offs = pos_left.compute(sw * LayoutEngine::getFixedPointScale());
			}
			if(pos_top.isPercent()) {
				sh_offs = pos_top.compute(sh * LayoutEngine::getFixedPointScale());
			}

			const int rw_offs = pos_left.compute(rw);
			const int rh_offs = pos_top.compute(rh);
			
			const float rxf = static_cast<float>(rx) / LayoutEngine::getFixedPointScaleFloat();
			const float ryf = static_cast<float>(ry) / LayoutEngine::getFixedPointScaleFloat();

			const float left = static_cast<float>(rw_offs - sw_offs /*+ rx*/) / LayoutEngine::getFixedPointScaleFloat();
			const float top = static_cast<float>(rh_offs - sh_offs /*+ ry*/) / LayoutEngine::getFixedPointScaleFloat();
			const float width = static_cast<float>(rw) / LayoutEngine::getFixedPointScaleFloat();
			const float height = static_cast<float>(rh) / LayoutEngine::getFixedPointScaleFloat();

			auto tex = texture_->clone();
			auto ptr = std::make_shared<KRE::Blittable>(tex);
			ptr->setCentre(KRE::Blittable::Centre::TOP_LEFT);
			ptr->setPosition(rxf, ryf);
			switch(styles_->getBackgroundRepeat()) {
				case BackgroundRepeat::REPEAT:
					tex->setSourceRect(0, rect(-static_cast<int>(left), -static_cast<int>(top), static_cast<int>(width), static_cast<int>(height)));
					ptr->setDrawRect(rectf(0.0f, 0.0f, width, height));
					break;
				case BackgroundRepeat::REPEAT_X:
					tex->setSourceRect(0, rect(-static_cast<int>(left), 0, static_cast<int>(width), sh));
					ptr->setDrawRect(rectf(0.0f, top, width, static_cast<float>(sh)));
					break;
				case BackgroundRepeat::REPEAT_Y:
					tex->setSourceRect(0, rect(0, -static_cast<int>(top), sw, static_cast<int>(height)));
					ptr->setDrawRect(rectf(left, 0.0f, static_cast<float>(sw), height));
					break;
				case BackgroundRepeat::NO_REPEAT:
					tex->setSourceRect(0, rect(0, 0, sw, sh));
					ptr->setDrawRect(rectf(left, top, static_cast<float>(sw), static_cast<float>(sh)));
					break;
			}

			if(clip_shape != nullptr) {
				ptr->setClipSettings(get_stencil_mask_settings(), clip_shape);
			}
			scene_tree->addObject(ptr);
		}
	}
}
