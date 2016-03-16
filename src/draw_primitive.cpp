/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include <cmath>
#include <vector>

#include <glm/gtc/type_precision.hpp>

#include "geometry.hpp"
#include "WindowManager.hpp"

#include "asserts.hpp"
#include "draw_primitive.hpp"
#include "level.hpp"
#include "variant_utils.hpp"

namespace graphics
{
	namespace
	{

		class RectPrimitive : public DrawPrimitive
		{
		public:
			explicit RectPrimitive(const variant& v);
		protected:
			void reInit(const KRE::WindowPtr& wm) override { init(); }
		private:
			DECLARE_CALLABLE(RectPrimitive)
			void init();

			rect area_;
			KRE::Color color_;
		};

		RectPrimitive::RectPrimitive(const variant& v)
			: DrawPrimitive(v), 
			  area_(v["area"]), 
			  color_(v["color"])
		{
			init();
		}

		void RectPrimitive::init()
		{
			if(getAnuraShader() == nullptr) {
				setShader(KRE::ShaderProgram::getProgram("simple"));
			}
			std::vector<glm::vec2> varray;
			varray.emplace_back(area_.x(), area_.y());
			varray.emplace_back(area_.x2(),area_.y());
			varray.emplace_back(area_.x(), area_.y2());
			varray.emplace_back(area_.x2(),area_.y2());

			using namespace KRE;

			auto ab = DisplayDevice::createAttributeSet(false, false, false);
			auto pos = new Attribute<glm::vec2>(AccessFreqHint::DYNAMIC, KRE::AccessTypeHint::DRAW);
			pos->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false));
			ab->addAttribute(AttributeBasePtr(pos));

			ab->setDrawMode(KRE::DrawMode::TRIANGLE_STRIP);
			addAttributeSet(ab);

			pos->update(&varray);

			setColor(color_);
		}

		BEGIN_DEFINE_CALLABLE(RectPrimitive, DrawPrimitive)
			DEFINE_FIELD(color, "[int,int,int,int]|string")
				return obj.color_.write();
			DEFINE_SET_FIELD
				obj.color_ = KRE::Color(value);
				obj.setColor(obj.color_);
		END_DEFINE_CALLABLE(RectPrimitive)


		class CirclePrimitive : public DrawPrimitive
		{
		public:
			explicit CirclePrimitive(const variant& v);
		protected:
			void reInit(const KRE::WindowPtr& wm) override { init(); }
		private:
			DECLARE_CALLABLE(CirclePrimitive);
			void init();

			glm::vec2 center_;
			float radius_;
			float y_radius_;
			float stroke_width_;

			KRE::Color color_;
			KRE::Color stroke_color_;
		};

		CirclePrimitive::CirclePrimitive(const variant& v)
			: DrawPrimitive(v),
				radius_(static_cast<float>(v["radius"].as_float())),
				y_radius_(static_cast<float>(v["y_radius"].as_decimal(decimal(radius_)).as_float())),
				stroke_width_(0.0f)
		{
			center_.x = v["x"].as_float();
			center_.y = v["y"].as_float();

			if(v.has_key("color")) {
				color_ = KRE::Color(v["color"]);
			} else {
				color_ = KRE::Color(200, 0, 0, 255);
			}

			if(v.has_key("stroke_color")) {
				stroke_color_ = KRE::Color(v["stroke_color"]);
				stroke_width_ = v["stroke_width"].as_float();
			}

			init();
		}

		void CirclePrimitive::init()
		{
			// XXX should replace this with a circle shader.
			std::vector<glm::vec2> varray;
			varray.emplace_back(center_);
			for(double angle = 0; angle < M_PI*2.0; angle += 0.1) {
				const float xpos = static_cast<float>(center_.x +   radius_*cos(angle));
				const float ypos = static_cast<float>(center_.y + y_radius_*sin(angle));
				varray.emplace_back(xpos, ypos);
			}

			//repeat the first coordinate to complete the circle.
			varray.emplace_back(varray[1]);

			using namespace KRE;
			if(color_.a() > 0) {
				auto ab = DisplayDevice::createAttributeSet(false, false, false);
				auto pos = new Attribute<glm::vec2>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
				pos->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false));
				ab->addAttribute(AttributeBasePtr(pos));
				ab->setDrawMode(DrawMode::TRIANGLE_FAN);
				ab->setColor(color_);
				addAttributeSet(ab);
				pos->update(varray);
			}

			if(stroke_color_.a() > 0) {
				auto ll = DisplayDevice::createAttributeSet(false, false, false);
				auto ll_pos = new Attribute<glm::vec2>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
				ll_pos->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false));
				ll->addAttribute(AttributeBasePtr(ll_pos));
				ll->setDrawMode(DrawMode::LINE_LOOP);
				ll->setColor(stroke_color_);
				addAttributeSet(ll);
				ll_pos->update(varray);
				ll->setCount(varray.size()-1);
			}
		}

		BEGIN_DEFINE_CALLABLE(CirclePrimitive, DrawPrimitive)
			DEFINE_FIELD(dummy, "any")
				return variant();
		END_DEFINE_CALLABLE(CirclePrimitive)


		class ArrowPrimitive : public DrawPrimitive
		{
		public:
			explicit ArrowPrimitive(const variant& v);
			void preRender(const KRE::WindowPtr& wnd) override;
		protected:
			void reInit(const KRE::WindowPtr& wm) override { init(); }
		private:
			DECLARE_CALLABLE(ArrowPrimitive);
			void init();
			void setPoints(const variant& points);
			void curve(const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, std::vector<glm::vec2>* out) const;

			std::vector<glm::vec2> points_;
			float granularity_;
			int arrow_head_length_;
			float arrow_head_width_;
			KRE::Color color_;
			int fade_in_length_;

			float width_base_, width_head_;

			std::vector<glm::vec2> uvarray_;
			std::vector<glm::vec2> varray_;
			std::vector<glm::u8vec4> carray_;

			std::shared_ptr<KRE::Attribute<glm::vec2>> pos_;
			std::shared_ptr<KRE::Attribute<glm::vec2>> tex_;
			std::shared_ptr<KRE::Attribute<glm::u8vec4>> col_;


			KRE::TexturePtr texture_;
			float texture_scale_;

			void calculate_draw_arrays();
		};

		ArrowPrimitive::ArrowPrimitive(const variant& v)
			: DrawPrimitive(v),
			granularity_(static_cast<float>(v["granularity"].as_decimal(decimal(0.005)).as_float())),
			arrow_head_length_(v["arrow_head_length"].as_int(10)),
			arrow_head_width_(static_cast<float>(v["arrow_head_width"].as_decimal(decimal(2.0)).as_float())),
			fade_in_length_(v["fade_in_length"].as_int(50)),
			width_base_(static_cast<float>(v["width_base"].as_decimal(decimal(12.0)).as_float())),
			width_head_(static_cast<float>(v["width_head"].as_decimal(decimal(5.0)).as_float()))
		{
			if(v.has_key("texture")) {
				texture_ = KRE::Texture::createTexture(v["texture"]);
				texture_scale_ = static_cast<float>(v["texture_scale"].as_decimal(decimal(1.0)).as_float());
			}

			if(v.has_key("color")) {
				color_ = KRE::Color(v["color"]);
			} else {
				color_ = KRE::Color(200, 0, 0, 255);
			}

			setPoints(v["points"]);
			init();
		}

		void ArrowPrimitive::init()
		{
			using namespace KRE;

			if(texture_) {
				setShader(KRE::ShaderProgram::getProgram("vtc_shader"));
			} else {
				setShader(KRE::ShaderProgram::getProgram("attr_color_shader"));
			}

			auto ab = DisplayDevice::createAttributeSet(true, false, false);
			pos_.reset(new Attribute<glm::vec2>(AccessFreqHint::DYNAMIC, KRE::AccessTypeHint::DRAW));
			pos_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false));
			ab->addAttribute(pos_);

			col_.reset(new Attribute<glm::u8vec4>(AccessFreqHint::DYNAMIC, KRE::AccessTypeHint::DRAW));
			col_->addAttributeDesc(AttributeDesc(AttrType::COLOR, 4, AttrFormat::UNSIGNED_BYTE, true));
			ab->addAttribute(col_);

			tex_.reset(new Attribute<glm::vec2>(AccessFreqHint::DYNAMIC, KRE::AccessTypeHint::DRAW));
			tex_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false));
			if(!texture_) {
				tex_->disable();
			}
			ab->addAttribute(tex_);

			ab->setDrawMode(DrawMode::TRIANGLE_STRIP);
			addAttributeSet(ab);	
		}

		void ArrowPrimitive::preRender(const KRE::WindowPtr& wnd)
		{
			if(varray_.empty()) {
				calculate_draw_arrays();

				pos_->update(varray_);
				col_->update(carray_);
				if(texture_) {
					tex_->update(uvarray_);
					tex_->enable();
				} else {
					tex_->enable(false);
				}
			}
		}

		void ArrowPrimitive::calculate_draw_arrays()
		{
			if(!varray_.empty()) {
				return;
			}

			std::vector<glm::vec2> path;

			for(unsigned n = 1; n < points_.size()-1; ++n) {
				std::vector<glm::vec2> new_path;
				curve(points_[n-1], points_[n], points_[n+1], &new_path);

				if(path.empty()) {
					path.swap(new_path);
				} else {
					ASSERT_LOG(path.size() >= new_path.size(), "path.size() < new_path.size() : " << path.size() << " < " << new_path.size());
					const auto overlap = path.size()/2;
					for(int n = 0; n != overlap; ++n) {
						const float ratio = static_cast<float>(n)/static_cast<float>(overlap);
						glm::vec2& value = path[(path.size() - overlap) + n];
						glm::vec2 new_value = new_path[n];
						value[0] = value[0]*(1.0f-ratio) + new_value[0]*ratio;
						value[1] = value[1]*(1.0f-ratio) + new_value[1]*ratio;
					}

					path.insert(path.end(), new_path.begin() + overlap, new_path.end());
				}
			}

			const float PathLength = static_cast<float>(path.size()-1);

			std::vector<std::pair<glm::vec2, glm::vec2>> lr_path;
			for(unsigned n = 0; n < path.size()-1; ++n) {
				const glm::vec2& p = path[n];

				const glm::vec2 unit_direction = glm::normalize(path[n+1] - p);		
				const glm::vec2 normal_direction_left(-unit_direction.y, unit_direction.x);
				const glm::vec2 normal_direction_right(unit_direction.y, -unit_direction.x);

				const float ratio = n / PathLength;

				float arrow_width = width_base_ - (width_base_ - width_head_) * ratio;

				const auto time_until_end = path.size() - 2 - n;
				if(time_until_end < arrow_head_length_) {
					arrow_width = arrow_head_width_ * time_until_end;
				}

				lr_path.emplace_back(p + normal_direction_left * arrow_width, p + normal_direction_right * arrow_width);
			}

			int n = 0;
			const int alpha = color_.a_int();
			glm::u8vec4 col = color_.as_u8vec4();
			for(auto& p : lr_path) {
				varray_.emplace_back(p.first);
				varray_.emplace_back(p.second);

				uvarray_.emplace_back(n * texture_scale_, 0.0f);
				uvarray_.emplace_back(n * texture_scale_, 1.0f);

				if(n < fade_in_length_) {
					col.a = static_cast<uint8_t>((alpha * n) / static_cast<float>(fade_in_length_));
				} else {
					col.a = alpha;
				}
				carray_.emplace_back(col);
				carray_.emplace_back(col);
				
				++n;
			}
		}

		BEGIN_DEFINE_CALLABLE(ArrowPrimitive, DrawPrimitive)
			DEFINE_FIELD(points, "[[int,int]]")
				std::vector<variant> result;
				for(const glm::vec2& p : obj.points_) {
					std::vector<variant> pos;
					pos.emplace_back(variant(static_cast<int>(p.x)));
					pos.emplace_back(variant(static_cast<int>(p.y)));
					result.push_back(variant(&pos));
				}
				return variant(&result);
			DEFINE_SET_FIELD
				obj.setPoints(value);
			DEFINE_FIELD(color, KRE::Color::getDefineFieldType())
				return obj.color_.write();
			DEFINE_SET_FIELD_TYPE(KRE::Color::getSetFieldType())
				obj.color_ = KRE::Color(value);
				obj.setDirty();
			DEFINE_FIELD(granularity, "decimal")
				return variant(obj.granularity_);
			DEFINE_SET_FIELD_TYPE("int|decimal")
				obj.granularity_ = value.as_float();
				obj.setDirty();
			DEFINE_FIELD(arrow_head_length, "int")
				return variant(obj.arrow_head_length_);
			DEFINE_SET_FIELD
				obj.arrow_head_length_ = value.as_int();
				obj.setDirty();
			DEFINE_FIELD(fade_in_length, "int")
				return variant(obj.fade_in_length_);
			DEFINE_SET_FIELD
				obj.fade_in_length_ = value.as_int();
				obj.setDirty();
			DEFINE_FIELD(width_base, "decimal")
				return variant(obj.width_base_);
			DEFINE_SET_FIELD_TYPE("int|decimal")
				obj.width_base_ = value.as_float();
				obj.setDirty();
			DEFINE_FIELD(width_head, "decimal")
				return variant(obj.width_head_);
			DEFINE_SET_FIELD_TYPE("int|decimal")
				obj.width_head_ = value.as_float();
				obj.setDirty();
		END_DEFINE_CALLABLE(ArrowPrimitive)


		void ArrowPrimitive::setPoints(const variant& points)
		{
			ASSERT_LOG(points.is_list(), "arrow points is not a list: " << points.debug_location());

			varray_.clear();
			points_.clear();

			for(int n = 0; n != points.num_elements(); ++n) {
				variant p = points[n];
				ASSERT_LOG(p.is_list() && p.num_elements() == 2, "arrow points in invalid format: " << points.debug_location() << " : " << p.write_json());
				glm::vec2 p1;
				p1.x = p[0].as_float();
				p1.y = p[1].as_float();
				points_.push_back(p1);
			}
		}

		void ArrowPrimitive::curve(const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, std::vector<glm::vec2>* out) const
		{
			for(float t = 0.0; t < 1.0 - granularity_; t += granularity_) {
				glm::vec2 p;
				for(int n = 0; n != 2; ++n) {
					//formula for a bezier curve.
					p[n] = (1-t)*(1-t)*p0[n] + 2*(1-t)*t*p1[n] + t*t*p2[n];
				}

				out->push_back(p);
			}
		}

		class WireframeBoxPrimitive : public DrawPrimitive
		{
		public:
			explicit WireframeBoxPrimitive(const variant& v);

		protected:
			void reInit(const KRE::WindowPtr& wm) override { init(); }
		private:
			DECLARE_CALLABLE(WireframeBoxPrimitive);

			void init();

			glm::vec3 b1_;
			glm::vec3 b2_;

			KRE::Color color_;

			std::vector<glm::vec3> varray_;
		};

		WireframeBoxPrimitive::WireframeBoxPrimitive(const variant& v)
			: DrawPrimitive(v)
		{
			if(v.has_key("points")) {
				ASSERT_LOG(v["points"].is_list() && v["points"].num_elements() == 2, "'points' must be a list of two elements.");
				b1_ = variant_to_vec3(v["points"][0]);
				b2_ = variant_to_vec3(v["points"][1]);
			} else {
				ASSERT_LOG(v.has_key("point1") && v.has_key("point2"), "Must specify 'points' or 'point1' and 'point2' attributes.");
				b1_ = variant_to_vec3(v["point1"]);
				b2_ = variant_to_vec3(v["point2"]);
			}
			if(v.has_key("color")) {
				color_ = KRE::Color(v["color"]);
			} else {
				color_ = KRE::Color(200, 0, 0, 255);
			}
			setColor(color_);

			init();
		}

		void WireframeBoxPrimitive::init()
		{
			setShader(KRE::ShaderProgram::getProgram("line_3d"));
			if(b1_.x > b2_.x) {
				std::swap(b1_.x, b2_.x);
			}
			if(b1_.y > b2_.y) {
				std::swap(b1_.y, b2_.y);
			}
			if(b1_.z > b2_.z) {
				std::swap(b1_.z, b2_.z);
			}

			// XXX need to adjust these to be centered.
			varray_.clear();
			varray_.emplace_back(b1_.x, b1_.y, b1_.z); varray_.emplace_back(b2_.x, b1_.y, b1_.z); 
			varray_.emplace_back(b1_.x, b1_.y, b1_.z); varray_.emplace_back(b1_.x, b2_.y, b1_.z); 
			varray_.emplace_back(b1_.x, b1_.y, b1_.z); varray_.emplace_back(b1_.x, b1_.y, b2_.z); 

			varray_.emplace_back(b2_.x, b2_.y, b2_.z); varray_.emplace_back(b2_.x, b2_.y, b1_.z); 
			varray_.emplace_back(b2_.x, b2_.y, b2_.z); varray_.emplace_back(b1_.x, b2_.y, b2_.z); 
			varray_.emplace_back(b2_.x, b2_.y, b2_.z); varray_.emplace_back(b2_.x, b1_.y, b2_.z); 

			varray_.emplace_back(b1_.x, b2_.y, b2_.z); varray_.emplace_back(b1_.x, b2_.y, b1_.z); 
			varray_.emplace_back(b1_.x, b2_.y, b2_.z); varray_.emplace_back(b1_.x, b1_.y, b2_.z); 

			varray_.emplace_back(b2_.x, b2_.y, b1_.z); varray_.emplace_back(b1_.x, b2_.y, b1_.z); 
			varray_.emplace_back(b2_.x, b2_.y, b1_.z); varray_.emplace_back(b2_.x, b1_.y, b1_.z); 

			varray_.emplace_back(b2_.x, b1_.y, b2_.z); varray_.emplace_back(b1_.x, b1_.y, b2_.z); 
			varray_.emplace_back(b2_.x, b1_.y, b2_.z); varray_.emplace_back(b2_.x, b1_.y, b1_.z); 

			using namespace KRE;
			auto ab = DisplayDevice::createAttributeSet(false, false, false);
			auto pos = new Attribute<glm::vec3>(AccessFreqHint::DYNAMIC, KRE::AccessTypeHint::DRAW);
			pos->addAttributeDesc(AttributeDesc(AttrType::POSITION, 3, AttrFormat::FLOAT, false));
			ab->addAttribute(AttributeBasePtr(pos));

			ab->setDrawMode(KRE::DrawMode::LINES);
			addAttributeSet(ab);

			// might be better doing this in pre-render?
			pos->update(&varray_);
		}

		BEGIN_DEFINE_CALLABLE(WireframeBoxPrimitive, DrawPrimitive)
			DEFINE_FIELD(color, KRE::Color::getDefineFieldType())
				return obj.color_.write();
			DEFINE_SET_FIELD_TYPE(KRE::Color::getSetFieldType())
				obj.color_ = KRE::Color(value);
				obj.setColor(obj.color_);
			DEFINE_FIELD(points, "[[decimal,decimal,decimal],[decimal,decimal,decimal]]")
				std::vector<variant> v;
				v.push_back(vec3_to_variant(obj.b1_));
				v.push_back(vec3_to_variant(obj.b2_));
				return variant(&v);
			DEFINE_SET_FIELD
				ASSERT_LOG(value.is_list() && value.num_elements() == 2, "'points' must be a list of two elements.");
				obj.b1_ = variant_to_vec3(value[0]);
				obj.b2_ = variant_to_vec3(value[1]);
				obj.setDirty();
			DEFINE_FIELD(point1, "[decimal,decimal,decimal]")
				return vec3_to_variant(obj.b1_);
			DEFINE_SET_FIELD
				obj.b1_ = variant_to_vec3(value);
				obj.setDirty();
			DEFINE_FIELD(point1, "[decimal,decimal,decimal]")
				return vec3_to_variant(obj.b2_);
			DEFINE_SET_FIELD
				obj.b2_ = variant_to_vec3(value);
				obj.setDirty();
		END_DEFINE_CALLABLE(WireframeBoxPrimitive)
		}

	class BoxPrimitive : public DrawPrimitive
	{
	public:
		explicit BoxPrimitive(const variant& v)
			: DrawPrimitive(v)
		{
			if(v.has_key("points")) {
				ASSERT_LOG(v["points"].is_list() && v["points"].num_elements() == 2, "'points' must be a list of two elements.");
				b1_ = variant_to_vec3(v["points"][0]);
				b2_ = variant_to_vec3(v["points"][1]);
			} else {
				ASSERT_LOG(v.has_key("point1") && v.has_key("point2"), "Must specify 'points' or 'point1' and 'point2' attributes.");
				b1_ = variant_to_vec3(v["point1"]);
				b2_ = variant_to_vec3(v["point2"]);
			}
			if(v.has_key("color")) {
				color_ = KRE::Color(v["color"]);
			} else {
				color_ = KRE::Color(200, 0, 0, 255);
			}
			setColor(color_);

			init();
		}
	protected:
		void reInit(const KRE::WindowPtr& wm) override { init(); }
	private:
		DECLARE_CALLABLE(BoxPrimitive);

		void init()
		{
			setShader(KRE::ShaderProgram::getProgram("line_3d"));
			if(b1_.x > b2_.x) {
				std::swap(b1_.x, b2_.x);
			}
			if(b1_.y > b2_.y) {
				std::swap(b1_.y, b2_.y);
			}
			if(b1_.z > b2_.z) {
				std::swap(b1_.z, b2_.z);
			}

			// XXX these need to be centered.
			varray_.clear();
			varray_.emplace_back(b1_.x, b1_.y, b2_.z);
			varray_.emplace_back(b2_.x, b1_.y, b2_.z);
			varray_.emplace_back(b2_.x, b2_.y, b2_.z);

			varray_.emplace_back(b2_.x, b2_.y, b2_.z);
			varray_.emplace_back(b1_.x, b2_.y, b2_.z);
			varray_.emplace_back(b1_.x, b1_.y, b2_.z);

			varray_.emplace_back(b2_.x, b2_.y, b2_.z);
			varray_.emplace_back(b2_.x, b1_.y, b2_.z);
			varray_.emplace_back(b2_.x, b2_.y, b1_.z);

			varray_.emplace_back(b2_.x, b2_.y, b1_.z);
			varray_.emplace_back(b2_.x, b1_.y, b2_.z);
			varray_.emplace_back(b2_.x, b1_.y, b1_.z);

			varray_.emplace_back(b2_.x, b2_.y, b2_.z);
			varray_.emplace_back(b2_.x, b2_.y, b1_.z);
			varray_.emplace_back(b1_.x, b2_.y, b2_.z);

			varray_.emplace_back(b1_.x, b2_.y, b2_.z);
			varray_.emplace_back(b2_.x, b2_.y, b1_.z);
			varray_.emplace_back(b1_.x, b2_.y, b1_.z);

			varray_.emplace_back(b2_.x, b1_.y, b1_.z);
			varray_.emplace_back(b1_.x, b1_.y, b1_.z);
			varray_.emplace_back(b1_.x, b2_.y, b1_.z);

			varray_.emplace_back(b1_.x, b2_.y, b1_.z);
			varray_.emplace_back(b2_.x, b2_.y, b1_.z);
			varray_.emplace_back(b2_.x, b1_.y, b1_.z);

			varray_.emplace_back(b1_.x, b2_.y, b2_.z);
			varray_.emplace_back(b1_.x, b2_.y, b1_.z);
			varray_.emplace_back(b1_.x, b1_.y, b2_.z);

			varray_.emplace_back(b1_.x, b1_.y, b2_.z);
			varray_.emplace_back(b1_.x, b2_.y, b1_.z);
			varray_.emplace_back(b1_.x, b1_.y, b1_.z);

			varray_.emplace_back(b2_.x, b1_.y, b2_.z);
			varray_.emplace_back(b1_.x, b1_.y, b2_.z);
			varray_.emplace_back(b2_.x, b1_.y, b1_.z);

			varray_.emplace_back(b2_.x, b1_.y, b1_.z);
			varray_.emplace_back(b1_.x, b1_.y, b2_.z);
			varray_.emplace_back(b1_.x, b1_.y, b1_.z);

			using namespace KRE;
			auto ab = DisplayDevice::createAttributeSet(false, false, false);
			auto pos = new Attribute<glm::vec3>(AccessFreqHint::DYNAMIC, KRE::AccessTypeHint::DRAW);
			pos->addAttributeDesc(AttributeDesc(AttrType::POSITION, 3, AttrFormat::FLOAT, false));
			ab->addAttribute(AttributeBasePtr(pos));

			ab->setDrawMode(KRE::DrawMode::TRIANGLES);
			addAttributeSet(ab);

			// might be better doing this in pre-render?
			pos->update(&varray_);
		}

		glm::vec3 b1_;
		glm::vec3 b2_;

		KRE::Color color_;

		std::vector<glm::vec3> varray_;
	
		BoxPrimitive();
		BoxPrimitive(const BoxPrimitive&);
	};

	BEGIN_DEFINE_CALLABLE(BoxPrimitive, DrawPrimitive)
		DEFINE_FIELD(color, KRE::Color::getDefineFieldType())
			return obj.color_.write();
		DEFINE_SET_FIELD_TYPE(KRE::Color::getSetFieldType())
			obj.color_ = KRE::Color(value);
			obj.setColor(obj.color_);
			obj.setDirty();
		DEFINE_FIELD(points, "[[decimal,decimal,decimal],[decimal,decimal,decimal]]")
			std::vector<variant> v;
			v.push_back(vec3_to_variant(obj.b1_));
			v.push_back(vec3_to_variant(obj.b2_));
			return variant(&v);
		DEFINE_SET_FIELD
			ASSERT_LOG(value.is_list() && value.num_elements() == 2, "'points' must be a list of two elements.");
			obj.b1_ = variant_to_vec3(value[0]);
			obj.b2_ = variant_to_vec3(value[1]);
			obj.setDirty();
		DEFINE_FIELD(point1, "[decimal,decimal,decimal]")
			return vec3_to_variant(obj.b1_);
		DEFINE_SET_FIELD
			obj.b1_ = variant_to_vec3(value);
			obj.setDirty();
		DEFINE_FIELD(point1, "[decimal,decimal,decimal]")
			return vec3_to_variant(obj.b2_);
		DEFINE_SET_FIELD
			obj.b2_ = variant_to_vec3(value);
			obj.setDirty();
	END_DEFINE_CALLABLE(BoxPrimitive)


	class LinePrimitive : public DrawPrimitive
	{
	public:
		explicit LinePrimitive(const variant& node);
	protected:
		void reInit(const KRE::WindowPtr& wm) override { init(); }
	private:
		DECLARE_CALLABLE(LinePrimitive);
		void init();
		int x1_;
		int y1_;
		int x2_;
		int y2_;
		float width_;
		KRE::Color color1_;
		KRE::Color color2_;
		KRE::Color stroke_color_;
		bool has_stroke_;
		std::vector<glm::vec2> v1array_;
		std::vector<glm::vec2> v2array_;
		std::vector<glm::u8vec4> carray_;
		std::shared_ptr<KRE::Attribute<glm::vec2>> pos_;
		std::shared_ptr<KRE::Attribute<glm::u8vec4>> col_;
		std::shared_ptr<KRE::Attribute<glm::vec2>> ll_pos_;
		KRE::AttributeSetPtr ll_;
		LinePrimitive();
		LinePrimitive(const LinePrimitive&);
		LinePrimitive& operator=(const LinePrimitive&);
	};

	LinePrimitive::LinePrimitive(const variant& node)
		: DrawPrimitive(node),
			color1_(node["color1"]),
			color2_(node["color2"]),
			width_(1.0f),
			has_stroke_(false)
	{
		if(node.has_key("p1") && node.has_key("p2")) {
			point p1(node["p1"]);
			x1_ = p1.x;
			y1_ = p1.y;
			point p2(node["p2"]);
			x2_ = p2.x;
			y2_ = p2.y;
		} else if(node.has_key("area")) {
			rect r(node["area"]);
			x1_ = r.x();
			y1_ = r.y();
			x2_ = r.x2();
			y2_ = r.y2();
		} else if(node.has_key("x1") && node.has_key("y1") && node.has_key("x2") && node.has_key("y2")) {
			x1_ = node["x1"].as_int();
			y1_ = node["y1"].as_int();
			x2_ = node["x2"].as_int();
			y2_ = node["y2"].as_int();
		} else {
			ASSERT_LOG(false, "Nothing containing points was found, either p1/p2, area or x1/y1/x2/y2 are required.");
		}
		if(node.has_key("width")) {
			width_ = static_cast<float>(node["width"].as_float());
		}
		if(node.has_key("stroke_color")) {
			has_stroke_ = true;
			stroke_color_ = KRE::Color(node["stroke_color"]);
		}

		using namespace KRE;

		auto ab = DisplayDevice::createAttributeSet(false, false, false);
		ab->setDrawMode(DrawMode::TRIANGLE_STRIP);

		pos_ = std::make_shared<Attribute<glm::vec2>>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
		pos_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false));
		ab->addAttribute(pos_);

		col_ = std::make_shared<Attribute<glm::u8vec4>>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
		col_->addAttributeDesc(AttributeDesc(AttrType::COLOR, 4, AttrFormat::UNSIGNED_BYTE, true));
		ab->addAttribute(col_);

		addAttributeSet(ab);

		ll_ = DisplayDevice::createAttributeSet(false, false, false);
		ll_pos_ = std::make_shared<Attribute<glm::vec2>>(AccessFreqHint::DYNAMIC, KRE::AccessTypeHint::DRAW);
		ll_pos_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false));
		ll_->addAttribute(AttributeBasePtr(ll_pos_));
		ll_->disable();
		ll_->setDrawMode(KRE::DrawMode::LINE_LOOP);
		addAttributeSet(ll_);

		init();
	}

	void LinePrimitive::init()
	{
		v1array_.clear();
		v2array_.clear();
		carray_.clear();

		const double theta = std::atan2(static_cast<double>(y2_-y1_),static_cast<double>(x2_-x1_));
		const double wx_half = width_/2.0 * std::sin(theta);
		const double wy_half = width_/2.0 * std::cos(theta);

		v1array_.emplace_back(static_cast<float>(x1_ - wx_half), static_cast<float>(y1_ + wy_half));
		v1array_.emplace_back(static_cast<float>(x2_ - wx_half), static_cast<float>(y2_ + wy_half));
		v1array_.emplace_back(static_cast<float>(x1_), static_cast<float>(y1_));
		v1array_.emplace_back(static_cast<float>(x2_), static_cast<float>(y2_));
		v1array_.emplace_back(static_cast<float>(x1_ + wx_half), static_cast<float>(y1_ - wy_half));
		v1array_.emplace_back(static_cast<float>(x2_ + wx_half), static_cast<float>(y2_ - wy_half));
		carray_.emplace_back(color1_.ri(), color1_.gi(), color1_.bi(), 0);
		carray_.emplace_back(color2_.ri(), color2_.gi(), color2_.bi(), 0);
		carray_.emplace_back(color1_.ri(), color1_.gi(), color1_.bi(), color1_.ai());
		carray_.emplace_back(color2_.ri(), color2_.gi(), color2_.bi(), color2_.ai());
		carray_.emplace_back(color1_.ri(), color1_.gi(), color1_.bi(), 0);
		carray_.emplace_back(color2_.ri(), color2_.gi(), color2_.bi(), 0);
		v2array_.emplace_back(static_cast<float>(x1_ - wx_half), static_cast<float>(y1_ + wy_half));
		v2array_.emplace_back(static_cast<float>(x2_ - wx_half), static_cast<float>(y2_ + wy_half));
		v2array_.emplace_back(static_cast<float>(x2_ + wx_half), static_cast<float>(y2_ - wy_half));
		v2array_.emplace_back(static_cast<float>(x1_ + wx_half), static_cast<float>(y1_ - wy_half));

		pos_->update(&v1array_);
		col_->update(&carray_);

		if(has_stroke_) {
			ll_->enable();
			ll_->setColor(stroke_color_);

			ll_pos_->update(&v2array_);
		} else {
			ll_->disable();
		}
	}


	BEGIN_DEFINE_CALLABLE(LinePrimitive, DrawPrimitive)
		DEFINE_FIELD(color1, KRE::Color::getDefineFieldType())
			return obj.color1_.write();
		DEFINE_SET_FIELD_TYPE(KRE::Color::getSetFieldType())
			obj.color1_ = KRE::Color(value);
			obj.setDirty();
		DEFINE_FIELD(color2, KRE::Color::getDefineFieldType())
			return obj.color2_.write();
		DEFINE_SET_FIELD_TYPE(KRE::Color::getSetFieldType())
			obj.color2_ = KRE::Color(value);
			obj.setDirty();
		DEFINE_FIELD(p1, "[int,int]")
			return point(obj.x1_, obj.y1_).write();
		DEFINE_SET_FIELD
			point p1(value);
			obj.x1_ = p1.x;
			obj.y1_ = p1.y;
			obj.setDirty();
		DEFINE_FIELD(p2, "[int,int]")
			return point(obj.x2_, obj.y2_).write();
		DEFINE_SET_FIELD
			point p2(value);
			obj.x2_ = p2.x;
			obj.y2_ = p2.y;
			obj.setDirty();
		DEFINE_FIELD(stroke_color, KRE::Color::getDefineFieldType())
			return obj.stroke_color_.write();
		DEFINE_SET_FIELD_TYPE(KRE::Color::getSetFieldType())
			obj.has_stroke_ = true;
			obj.stroke_color_ = KRE::Color(value);
			obj.setDirty();
		DEFINE_FIELD(width, "decimal")
			return variant(obj.width_);
		DEFINE_SET_FIELD_TYPE("decimal|int")
			obj.width_ = value.as_float();
			obj.setDirty();
	END_DEFINE_CALLABLE(LinePrimitive)


	DrawPrimitivePtr DrawPrimitive::create(const variant& v)
	{
		if(v.is_callable()) {
			DrawPrimitivePtr dp = v.try_convert<DrawPrimitive>();
			ASSERT_LOG(dp != nullptr, "Couldn't convert callable type to DrawPrimitive");
			return dp;
		}
		const std::string type = v["type"].as_string();
		if(type == "arrow") {
			return new ArrowPrimitive(v);
		} else if(type == "circle") {
			return new CirclePrimitive(v);
		} else if(type == "rect") {
			return new RectPrimitive(v);
		} else if(type == "line") {
			return new LinePrimitive(v);
		} else if(type == "box") {
			return new BoxPrimitive(v);
		} else if(type == "box_wireframe") {
			return new WireframeBoxPrimitive(v);
		}

		ASSERT_LOG(false, "UNKNOWN DRAW PRIMITIVE TYPE: " << v["type"].as_string());
		return DrawPrimitivePtr();
	}

	DrawPrimitive::DrawPrimitive(const variant& node)
		: SceneObject(node)
	{
		if(node.has_key("shader")) {
			if(node["shader"].is_string()) {
				shader_.reset(new AnuraShader(node["shader"].as_string()));
			} else {
				shader_.reset(new AnuraShader(node["shader"]["name"].as_string(), node["shader"]));
			}
			setShader(shader_->getShader());
		} else {
			setShader(KRE::ShaderProgram::getProgram("attr_color_shader"));
		}
	}

	void DrawPrimitive::preRender(const KRE::WindowPtr& wm) 
	{
		if(dirty_) {
			dirty_ = false;
			reInit(wm);
		}
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(DrawPrimitive)
		// XXX placeholder
		DEFINE_FIELD(blend, "string")
			return variant("normal");
	END_DEFINE_CALLABLE(DrawPrimitive)
}
