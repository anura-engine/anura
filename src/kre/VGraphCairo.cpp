/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma comment(lib, "libcairo-2")

#include "asserts.hpp"
#include "AttributeSet.hpp"
#include "DisplayDevice.hpp"
#include "SceneFwd.hpp"
#include "Texture.hpp"
#include "VGraphCairo.hpp"

namespace KRE
{
	namespace Vector
	{
		enum class InstructionType {
			UNKNOWN,
			CLOSE_PATH,
			MOVE_TO,
			LINE_TO,
			CURVE_TO,
			QUAD_CURVE_TO,
			ARC,
			TEXT_PATH,
		};

		class PathInstruction
		{
		public:
			virtual ~PathInstruction() {}
			InstructionType GetType() const { return static_cast<InstructionType>(instruction_type_); }
			virtual void execute(cairo_t* context) = 0;
			virtual std::string asString() const = 0;
		protected:
			PathInstruction() {}
		private:
			enum {
				instruction_type_ = static_cast<unsigned>(InstructionType::UNKNOWN)
			};
			PathInstruction(const PathInstruction&);
		};
		typedef std::shared_ptr<PathInstruction> PathInstructionPtr;

		class ClosePathInstruction : public PathInstruction
		{
		public:
			ClosePathInstruction() {}
			void execute(cairo_t* context) override {
				cairo_close_path(context);
			}
			std::string asString() const override { return "close_path"; }
		private:
			enum {
				instruction_type_ = static_cast<unsigned>(InstructionType::CLOSE_PATH)
			};
		};

		class MoveToInstruction : public PathInstruction
		{
		public:
			MoveToInstruction(const double x, const double y, bool relative=false) 
				: x_(x), y_(y), relative_(relative) {
			}
			void execute(cairo_t* context) override {
				if(relative_) {
					cairo_rel_move_to(context, x_, y_);
				} else {
					cairo_move_to(context, x_, y_);
				}
			}
			std::string asString() const override { return "move_to"; }
		private:
			enum {
				instruction_type_ = static_cast<unsigned>(InstructionType::MOVE_TO)
			};
			double x_;
			double y_;
			bool relative_;
		};

		class LineToInstruction : public PathInstruction
		{
		public:
			LineToInstruction(const double x, const double y, bool relative=false) 
				: x_(x), y_(y), relative_(relative) {
			}
			void execute(cairo_t* context) override {
				if(relative_) {
					cairo_rel_line_to(context, x_, y_);
				} else {
					cairo_line_to(context, x_, y_);
				}
			}
			std::string asString() const override { return "line_to"; }
		private:
			enum {
				instruction_type_ = static_cast<unsigned>(InstructionType::LINE_TO)
			};
			double x_;
			double y_;
			bool relative_;
		};

		class ArcInstruction : public PathInstruction
		{
		public:
			ArcInstruction(const double x, const double y, const double radius, const double start_angle, const double end_angle, bool negative=false) 
				: x_(x), y_(y), radius_(radius), start_angle_(start_angle), end_angle_(end_angle), negative_(negative) {
			}
			void execute(cairo_t* context) override {
				if(negative_) {
					cairo_arc_negative(context, x_, y_, radius_, start_angle_, end_angle_);
				} else {
					cairo_arc(context, x_, y_, radius_, start_angle_, end_angle_);
				}
			}
			std::string asString() const override { return "arc"; }
		private:
			enum {
				instruction_type_ = static_cast<unsigned>(InstructionType::ARC)
			};
			double x_;
			double y_;
			double radius_;
			double start_angle_;
			double end_angle_;
			bool negative_;
		};

		class CubicCurveInstruction : public PathInstruction
		{
		public:
			CubicCurveInstruction(const double x1, const double y1, const double x2, const double y2, const double ex, const double ey, bool relative=false) 
				: cp_x1_(x1), cp_y1_(y1), cp_x2_(x2), cp_y2_(y2), ex_(ex), ey_(ey), relative_(relative) {
			}
			void execute(cairo_t* context) override {
				if(relative_) {
					cairo_rel_curve_to(context, cp_x1_, cp_y1_, cp_x2_, cp_y2_, ex_, ey_);
				} else {
					cairo_curve_to(context, cp_x1_, cp_y1_, cp_x2_, cp_y2_, ex_, ey_);
				}
			}
			std::string asString() const override { return "cubic_bézier"; }
		private:
			enum {
				instruction_type_ = static_cast<unsigned>(InstructionType::CURVE_TO)
			};
			// control point 1
			double cp_x1_;
			double cp_y1_;
			// control point 2
			double cp_x2_;
			double cp_y2_;
			// end point
			double ex_;
			double ey_;
			bool relative_;
		};

		class QuadraticCurveInstruction : public PathInstruction
		{
		public:
			QuadraticCurveInstruction(const double x1, const double y1, const double ex, const double ey, bool relative=false) 
				: cp_x1_(x1), cp_y1_(y1), ex_(ex), ey_(ey), relative_(relative) {
			}
			void execute(cairo_t* context) override {
				ASSERT_LOG(cairo_has_current_point(context) != 0, "No current point defined.");
				double cx, cy;
				cairo_get_current_point(context, &cx, &cy);

				double nx1 = cp_x1_;
				double ny1 = cp_y1_;
				double nex = ex_;
				double ney = ey_;
				if(relative_) {
					nx1 += cx;
					ny1 += cy;
					nex += cx;
					ney += cy;
				}

				double cp1x = cx + (2.0*(nx1-cx))/3.0;
				double cp1y = cy + (2.0*(ny1-cy))/3.0;
				double cp2x = nex + (2.0*(nx1-nex))/3.0;
				double cp2y = ney + (2.0*(ny1-ney))/3.0;

				cairo_curve_to(context, cp1x, cp1y, cp2x, cp2y, nex, ney);
			}
			std::string asString() const override { return "quadratic_bézier"; }
		private:
			enum {
				instruction_type_ = static_cast<unsigned>(InstructionType::QUAD_CURVE_TO)
			};
			// control point 1
			double cp_x1_;
			double cp_y1_;
			// end point
			double ex_;
			double ey_;
			bool relative_;
		};

		class TextPathInstruction : public PathInstruction
		{
		public:
			TextPathInstruction(const std::string& text) : text_(text) {
			}
			void execute(cairo_t* context) override {
				cairo_text_path(context, text_.c_str());
			}
			std::string asString() const override { return "text_path"; }
		private:
			std::string text_;
		};

		// XXX change this to generate and store a list of commands
		class CairoPath : public Path
		{
		public:
			CairoPath(CairoContext* context) : context_(context) {
				ASSERT_LOG(context_ != nullptr, "Passed an null context");
			}
			void MoveTo(const double x, const double y, const bool relative=false) override {
				path_instructions_.emplace_back(new MoveToInstruction(x, y, relative));
			}
			void LineTo(const double x, const double y, const bool relative=false) override	{
				path_instructions_.emplace_back(new LineToInstruction(x, y, relative));
			}

			// Helper function equivalent to drawing an arc between 0.0 and 2*M_PI
			void Circle(const double x, const double y, const double r) override {
				path_instructions_.emplace_back(new ArcInstruction(x, y, r, 0.0, 2.0*M_PI));
			}
			void Line(const double x1, const double y1, const double x2, const double y2) override {
				path_instructions_.emplace_back(new MoveToInstruction(x1, y1));
				path_instructions_.emplace_back(new LineToInstruction(x2, y2));
				path_instructions_.emplace_back(new ClosePathInstruction());
			}
			void Rectangle(const double x, const double y, const double width, const double height) override {
				path_instructions_.emplace_back(new MoveToInstruction(x, y));
				path_instructions_.emplace_back(new LineToInstruction(width, 0));
				path_instructions_.emplace_back(new LineToInstruction(0, height));
				path_instructions_.emplace_back(new LineToInstruction(-width, 0));
				path_instructions_.emplace_back(new ClosePathInstruction());
			}

			void Arc(const double cx, const double cy, const double radius, const double start_angle, const double end_angle, bool negative=false) override {
				path_instructions_.emplace_back(new ArcInstruction(cx, cy, radius, start_angle, end_angle, negative));
			}
				
			// Adds a Cubic Bezier curve to the current path from the current position to the end position
			// (ex,ey) using the control points (x1,y1) and (x2,y2)
			// If relative is true then the curve is drawn with all positions relative to the current point.
			void CubicCurveTo(const double x1, const double y1, const double x2, const double y2, const double ex, const double ey, bool relative=false) override {
				path_instructions_.emplace_back(new CubicCurveInstruction(x1, y1, x2, y2, ex, ey, relative));
			}
			// Adds a Quadratic Bezier curve to the current path from the current position to the end position
			// (ex,ey) using the control point (x1,y1)
			// If relative is true then the curve is drawn with all positions relative to the current point.
			void QuadraticCurveTo(const double x1, const double y1, const double ex, const double ey, bool relative=false) override {
				path_instructions_.emplace_back(new QuadraticCurveInstruction(x1, y1, ex, ey, relative));
			}

			//virtual void GlyphPath(const std::vector<Glyph>& g);
			void TextPath(const std::string& s) override {
				path_instructions_.emplace_back(new TextPathInstruction(s));
			}

			void ClosePath() override {
				path_instructions_.emplace_back(new ClosePathInstruction());
			}

			void execute(cairo_t* context) {
				std::ostringstream ss;
				ss << "Executing path:";
				for(auto ins : path_instructions_) {
					ss << " " << ins->asString();
					ins->execute(context);
				}
				LOG_DEBUG(ss.str());
			}
		private:
			CairoContext* context_;
			std::vector<PathInstructionPtr> path_instructions_;
		};

		CairoContext::CairoContext(int width, int height)
			: Context(width, height),
			  draw_rect_(0.0f, 0.0f, float(width), float(height))
		{
			surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
			auto status = cairo_surface_status(surface_);
			ASSERT_LOG(status == CAIRO_STATUS_SUCCESS, "Unable to create cairo surface: " << cairo_status_to_string(status));

			context_ = cairo_create(surface_);
			status = cairo_status(context_);
			ASSERT_LOG(status == CAIRO_STATUS_SUCCESS, "Unable to create cairo instance: " << cairo_status_to_string(status));

			int w = cairo_image_surface_get_width(surface_);
			int h = cairo_image_surface_get_height(surface_);
			auto fmt = cairo_image_surface_get_format(surface_);
			//int stride = cairo_image_surface_get_stride(surface_);

			PixelFormat::PF pffmt;
			switch(fmt) {
				case CAIRO_FORMAT_A8:
					ASSERT_LOG(false, "CAIRO_FORMAT_A8 unsupported at this time");
					break;
				case CAIRO_FORMAT_A1:
					ASSERT_LOG(false, "CAIRO_FORMAT_A1 unsupported at this time");
					break;
				case CAIRO_FORMAT_ARGB32:	pffmt = PixelFormat::PF::PIXELFORMAT_ARGB8888;	break;
				case CAIRO_FORMAT_RGB24:	pffmt = PixelFormat::PF::PIXELFORMAT_RGB888;	break;
				case CAIRO_FORMAT_RGB16_565:pffmt = PixelFormat::PF::PIXELFORMAT_RGB565;	break;
#if CAIRO_VERSION_MAJOR >= 1 && CAIRO_VERSION_MINOR >= 12
				case CAIRO_FORMAT_RGB30:	pffmt = PixelFormat::PF::PIXELFORMAT_RGB101010;	break;
#endif
				default:
					ASSERT_LOG(false, "Unrecognised cairo surface format: " << fmt);
			}
			tex_ = Texture::createTexture2D(w, h, pffmt);
			tex_->setAddressModes(-1, Texture::AddressMode::CLAMP, Texture::AddressMode::CLAMP);
			setTexture(tex_);

			auto as = DisplayDevice::createAttributeSet();
			attribs_.reset(new Attribute<vertex_texcoord>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW));
			attribs_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, vtx)));
			attribs_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE,  2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, tc)));
			as->addAttribute(AttributeBasePtr(attribs_));
			as->setDrawMode(DrawMode::TRIANGLE_STRIP);
			addAttributeSet(as);

			float offs_x = -draw_rect_.w()/2.0f;
			float offs_y = -draw_rect_.h()/2.0f;
			// XXX we should only do this if things changed.
			const float vx1 = draw_rect_.x() + offs_x;
			const float vy1 = draw_rect_.y() + offs_y;
			const float vx2 = draw_rect_.x2() + offs_x;
			const float vy2 = draw_rect_.y2() + offs_y;

			rectf r = tex_->getSourceRectNormalised();

			std::vector<vertex_texcoord> vertices;
			vertices.emplace_back(glm::vec2(vx1,vy1), glm::vec2(r.x(),r.y()));
			vertices.emplace_back(glm::vec2(vx2,vy1), glm::vec2(r.x2(),r.y()));
			vertices.emplace_back(glm::vec2(vx1,vy2), glm::vec2(r.x(),r.y2()));
			vertices.emplace_back(glm::vec2(vx2,vy2), glm::vec2(r.x2(),r.y2()));
			getAttributeSet().back()->setCount(vertices.size());
			attribs_->update(&vertices);
		}

		CairoContext::~CairoContext()
		{
			cairo_destroy(context_);
			cairo_surface_destroy(surface_);
		}

		void CairoContext::Save()
		{
			cairo_save(context_);
		}

		void CairoContext::Restore()
		{
			cairo_restore(context_);
		}


		void CairoContext::PushGroup()
		{
			cairo_push_group(context_);
		}

		void CairoContext::PopGroup(const bool to_source)
		{
			if(to_source) {
				cairo_pop_group_to_source(context_);
			} else {
				cairo_pop_group(context_);
			}
		}


		void CairoContext::SetSourceColor(const double r, const double g, const double b, const double a)
		{
			cairo_set_source_rgba(context_, r, g, b, a);
		}

		void CairoContext::SetSourceColor(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a)
		{
			cairo_set_source_rgba(context_, r/255.0, g/255.0, b/255.0, a/255.0);
		}

		void CairoContext::SetSourceColor(const Color& color)
		{
			cairo_set_source_rgba(context_, color.r(), color.g(), color.b(), color.a());
		}

		void CairoContext::SetSource(const PatternPtr& p)
		{
			// XXX
			//auto pattern = std::dynamic_pointer_cast<CairoPattern>(p);
			//pattern = 
		}

		PatternPtr CairoContext::GetSource() const
		{
			// XXX 
			return PatternPtr();
		}


		void CairoContext::SetFillRule(const FillRule fr)
		{
			cairo_set_fill_rule(context_, cairo_fill_rule_t(fr));
		}

		FillRule CairoContext::GetFillRule() const
		{
			auto fr = cairo_get_fill_rule(context_);
			return FillRule(fr);
		}


		void CairoContext::SetLineCap(const LineCap lc)
		{
			cairo_set_line_cap(context_, cairo_line_cap_t(lc));
		}

		LineCap CairoContext::GetLineCap() const
		{
			auto lc = cairo_get_line_cap(context_);
			return LineCap(lc);
		}


		void CairoContext::SetLineJoin(const LineJoin lj)
		{
			cairo_set_line_join(context_, cairo_line_join_t(lj));
		}

		LineJoin CairoContext::GetLineJoin() const
		{
			return LineJoin(cairo_get_line_join(context_));
		}


		void CairoContext::SetLineWidth(const double width)
		{
			cairo_set_line_width(context_, width);
		}

		double CairoContext::GetLineWidth() const
		{
			return cairo_get_line_width(context_);
		}


		void CairoContext::SetMiterLimit(const double limit)
		{
			cairo_set_miter_limit(context_, limit);
		}

		double CairoContext::GetMiterLimit() const
		{
			return cairo_get_miter_limit(context_);
		}


		void CairoContext::SetDashStyle(const std::vector<double>& dashes, const double offset)
		{
			cairo_set_dash(context_, &dashes[0], static_cast<int>(dashes.size()), offset);
		}

		const std::vector<double> CairoContext::GetDashStyle() const
		{
			int cnt = cairo_get_dash_count(context_);
			std::vector<double> dashes(cnt);
			double offset;
			cairo_get_dash(context_, &dashes[0], &offset);
			return dashes;
		}

		void CairoContext::SetDashOffset(double offset)
		{
			// XXX
		}

		double CairoContext::GetDashOffset() const
		{
			int cnt = cairo_get_dash_count(context_);
			std::vector<double> dashes(cnt);
			double offset;
			cairo_get_dash(context_, &dashes[0], &offset);
			return offset;
		}

				
		void CairoContext::Paint(const double alpha)
		{
			cairo_paint_with_alpha(context_, alpha);
		}


		void CairoContext::Fill(const bool preserve)
		{
			if(preserve) {
				cairo_fill_preserve(context_);
			} else {
				cairo_fill(context_);
			}
		}

		void CairoContext::FillExtents(double& x1, double& y1, double& x2, double& y2)
		{
			cairo_fill_extents(context_, &x1, &y1, &x2, &y2);
		}

		bool CairoContext::InFill(const double x, const double y)
		{
			return cairo_in_fill(context_, x, y) ? true : false;
		}


		void CairoContext::Stroke(const bool preserve)
		{
			if(preserve) {
				cairo_stroke_preserve(context_);
			} else {
				cairo_stroke(context_);
			}
		}

		void CairoContext::StrokeExtents(double& x1, double& y1, double& x2, double& y2)
		{
			cairo_stroke_extents(context_, &x1, &y1, &x2, &y2);
		}

		bool CairoContext::InStroke(const double x, const double y)
		{
			return cairo_in_stroke(context_, x, y) ? true : false;
		}


		void CairoContext::Clip(const bool preserve)
		{
			if(preserve) {
				cairo_clip_preserve(context_);
			} else {
				cairo_clip(context_);
			}
		}

		void CairoContext::ClipExtents(double& x1, double& y1, double& x2, double& y2)
		{
			cairo_clip_extents(context_, &x1, &y1, &x2, &y2);
		}

		bool CairoContext::InClip(const double x, const double y)
		{
			return cairo_in_clip(context_, x, y) ? true : false;
		}

		void CairoContext::ClipReset()
		{
			cairo_reset_clip(context_);
		}


		//void set_antialiasing(const AntiAliasing aa)
		//AntiAliasing get_antialiasing() const


		void CairoContext::GetCurrentPoint(double& x, double& y)
		{
			cairo_get_current_point(context_, &x, &y);
		}

		bool CairoContext::HasCurrentPoint()
		{
			return cairo_has_current_point(context_) ? true : false;
		}

		PathPtr CairoContext::NewPath()
		{
			return PathPtr(new CairoPath(this));
		}

		void CairoContext::AddPath(const PathPtr& path)
		{
			auto cpath = std::dynamic_pointer_cast<CairoPath>(path);
			ASSERT_LOG(cpath != nullptr, "Couldn't convert path to appropriate type CairoPath");
			cpath->execute(context_);
		}

		void CairoContext::AddSubPath(const PathPtr& path)
		{
			auto cpath = std::dynamic_pointer_cast<CairoPath>(path);
			ASSERT_LOG(cpath != nullptr, "Couldn't convert path to appropriate type CairoPath");
			cairo_new_sub_path(context_);
			cpath->execute(context_);
		}

		void CairoContext::preRender(const WindowPtr& wnd) 
		{
			tex_->update2D(0, 0, 0, width(), height(), cairo_image_surface_get_width(surface_), cairo_image_surface_get_data(surface_));
		}

		void CairoContext::PathExtents(double& x1, double& y1, double& x2, double& y2) 
		{
			cairo_path_extents(context_, &x1, &y1, &x2, &y2);
		}

		void CairoContext::translate(double tx, double ty) 
		{
			cairo_translate(context_, tx, ty);
		}

		void CairoContext::scale(double sx, double sy) 
		{
			cairo_scale(context_, sx, sy);
		}

		void CairoContext::rotate(double rad) 
		{
			cairo_rotate(context_, rad);
		}

		void CairoContext::setMatrix(const MatrixPtr& m) 
		{
			auto mat = std::dynamic_pointer_cast<CairoMatrix>(m);
			ASSERT_LOG(mat != nullptr, "Matrix given to setMatrix is not an instance of CairoMatrix.");
			cairo_set_matrix(context_, mat->get());
		}

		MatrixPtr CairoContext::getMatrix() const 
		{
			cairo_matrix_t mat;
			cairo_get_matrix(context_, &mat);
			return std::make_shared<CairoMatrix>(mat);
		}

		void CairoContext::transform(const MatrixPtr& m) 
		{
			auto mat = std::dynamic_pointer_cast<CairoMatrix>(m);
			ASSERT_LOG(mat != nullptr, "Matrix given to transform is not an instance of CairoMatrix.");
			cairo_transform(context_, mat->get());
		}

		void CairoContext::setIdentityMatrix() 
		{
			cairo_identity_matrix(context_);
		}

		geometry::Point<double> CairoContext::userToDevice(double x, double y) 
		{
			cairo_user_to_device(context_, &x, &y);
			return geometry::Point<double>(x, y);
		}

		geometry::Point<double> CairoContext::userToDeviceDistance(double x, double y) 
		{
			cairo_user_to_device_distance(context_, &x, &y);
			return geometry::Point<double>(x, y);
		}

		geometry::Point<double> CairoContext::deviceToUser(double x, double y) 
		{
			cairo_device_to_user(context_, &x, &y);
			return geometry::Point<double>(x, y);
		}

		geometry::Point<double> CairoContext::deviceToUserDistance(double x, double y) 
		{
			cairo_device_to_user_distance(context_, &x, &y);
			return geometry::Point<double>(x, y);
		}

		MatrixPtr CairoContext::createMatrix()
		{
			return std::make_shared<CairoMatrix>();
		}

		CairoMatrix::CairoMatrix()
		{
			initIdentity();
		}

		CairoMatrix::CairoMatrix(const cairo_matrix_t& mat)
			: matrix_(mat)
		{
		}

		void CairoMatrix::init(double xx, double yx, double xy, double yy, double x0, double y0) 
		{
			cairo_matrix_init(&matrix_, xx, yy, xy, yy, x0, y0);
		}

		void CairoMatrix::initIdentity() 
		{
			cairo_matrix_init_identity(&matrix_);
		}

		void CairoMatrix::initTranslate(double x0, double y0) 
		{
			cairo_matrix_init_translate(&matrix_, x0, y0);
		}

		void CairoMatrix::initScale(double xs, double ys) 
		{
			cairo_matrix_init_scale(&matrix_, xs, ys);
		}

		void CairoMatrix::initRotation(double rad) 
		{
			cairo_matrix_init_rotate(&matrix_, rad);
		}

		void CairoMatrix::translate(double tx, double ty) 
		{
			cairo_matrix_translate(&matrix_, tx, ty);
		}

		void CairoMatrix::scale(double sx, double sy) 
		{
			cairo_matrix_scale(&matrix_, sx, sy);
		}

		void CairoMatrix::rotate(double rad) 
		{
			cairo_matrix_rotate(&matrix_, rad);
		}

		void CairoMatrix::invert() 
		{
			cairo_matrix_invert(&matrix_);
		}

		void CairoMatrix::multiply(const MatrixPtr& a) 
		{
			cairo_matrix_t new_mat;
			cairo_matrix_multiply(&new_mat, &matrix_, &std::dynamic_pointer_cast<CairoMatrix>(a)->matrix_);
			matrix_ = new_mat;
		}

		geometry::Point<double> CairoMatrix::transformDistance(double x, double y) 
		{
			cairo_matrix_transform_distance(&matrix_, &x, &y);
			return geometry::Point<double>(x, y);
		}

		geometry::Point<double> CairoMatrix::transformPoint(double x, double y) 
		{
			cairo_matrix_transform_point(&matrix_, &x, &y);
			return geometry::Point<double>(x, y);
		}

		MatrixPtr CairoMatrix::clone() 
		{
			return std::make_shared<CairoMatrix>(*this);
		}
	}
}
