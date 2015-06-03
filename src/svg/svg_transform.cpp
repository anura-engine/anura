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

#include <boost/tokenizer.hpp>
#include <sstream>

#include <cfloat>
#include <cmath>
#include "asserts.hpp"
#include "svg_transform.hpp"

namespace KRE
{
	namespace SVG
	{
		class matrix_transform : public transform
		{
		public:
			matrix_transform(const std::vector<double>& params) : transform(TransformType::MATRIX) {
				// Ordering of matrix parameters is as follows.
				// [ 0  2  4 ]
				// [ 1  3  5 ]
				// which neatly is the same order as cairo expects.
				ASSERT_LOG(params.size() == 6, 
					"Parsing transform:matrix found " 
					<< params.size() 
					<< " parameter(s), expected 6");
				cairo_matrix_init(&mat_, params[0], params[1], params[2], params[3], params[4], params[5]);
			}
			virtual ~matrix_transform() {}
			std::string as_string() const override {
				std::stringstream str;
				str << "matrix(" 
					<< mat_.xx << " " << mat_.yx << " " 
					<< mat_.xy << " " << mat_.yy << " " 
					<< mat_.x0 << " " << mat_.y0 
					<< ")";
				return str.str();
			}
		private:
			void handle_apply(render_context& ctx) override {
				cairo_transform(ctx.cairo(), &mat_);
			}
			void handle_apply_matrix(cairo_matrix_t* mtx) const override {
				cairo_matrix_multiply(mtx, mtx, &mat_);
			}
			cairo_matrix_t mat_;
		};

		class translate_transform : public transform
		{
		public:
			translate_transform(double x, double y) : transform(TransformType::TRANSLATE), x_(x), y_(y) {
			}
			virtual ~translate_transform() {}
			std::string as_string() const override {
				std::stringstream str;
				str << "translate(" << x_ << " " << y_ << ")";
				return str.str();
			}
		private:
			void handle_apply(render_context& ctx) override {
				cairo_translate(ctx.cairo(), x_, y_);
			}
			void handle_apply_matrix(cairo_matrix_t* mtx) const override {
				cairo_matrix_translate(mtx, x_, y_);
			}
			double x_;
			double y_;
		};

		class rotation_transform : public transform
		{
		public:
			rotation_transform(double angle, double cx=0, double cy=0) 
				: transform(TransformType::ROTATE), 
				angle_(angle),
				cx_(cx),
				cy_(cy) {
			}
			virtual ~rotation_transform() {}
			std::string as_string() const override {
				std::stringstream str;
				if(std::abs(cx_) < DBL_EPSILON && std::abs(cy_) < DBL_EPSILON) {
					str << "rotate(" << angle_ << ")";
				} else {
					str << "rotate(" << angle_ << " " << cx_ << " " << cy_ << ")";
				}
				return str.str();
			}
		private:
			void handle_apply(render_context& ctx) override {
				if(std::abs(cx_) < DBL_EPSILON && std::abs(cy_) < DBL_EPSILON) {
					cairo_rotate(ctx.cairo(), angle_);
				} else {
					cairo_translate(ctx.cairo(), cx_, cy_);
					cairo_rotate(ctx.cairo(), angle_);
					cairo_translate(ctx.cairo(), -cx_, -cy_);
				}
			}
			void handle_apply_matrix(cairo_matrix_t* mtx) const override {
				if(std::abs(cx_) < DBL_EPSILON && std::abs(cy_) < DBL_EPSILON) {
					cairo_matrix_rotate(mtx, angle_);
				} else {
					cairo_matrix_translate(mtx, cx_, cy_);
					cairo_matrix_rotate(mtx, angle_);
					cairo_matrix_translate(mtx, cx_, cy_);
				}
			}
			double angle_;
			double cx_;
			double cy_;
		};

		class scale_transform : public transform
		{
		public:
			scale_transform(double sx, double sy) : transform(TransformType::SCALE), sx_(sx), sy_(sy) {
			}
			virtual ~scale_transform() {}
			std::string as_string() const override {
				std::stringstream str;
				str << "scale(" << sx_ << " " << sy_ << ")";
				return str.str();
			}
		private:
			void handle_apply(render_context& ctx) override {
				cairo_scale(ctx.cairo(), sx_, sy_);
			}
			void handle_apply_matrix(cairo_matrix_t* mtx) const override {
				cairo_matrix_scale(mtx, sx_, sy_);
			}
			double sx_;
			double sy_;
		};

		class skew_x_transform : public transform
		{
		public:
			skew_x_transform(double sx) : transform(TransformType::SKEW_X), sx_(sx) {
				cairo_matrix_init(&mat_, 1, 0, sx_, 1, 0, 0);
			}
			virtual ~skew_x_transform() {}
			std::string as_string() const override {
				std::stringstream str;
				str << "skewX(" << sx_ << ")";
				return str.str();
			}
		private:
			void handle_apply(render_context& ctx) override {
				cairo_transform(ctx.cairo(), &mat_);
			}
			void handle_apply_matrix(cairo_matrix_t* mtx) const override {
				cairo_matrix_multiply(mtx, mtx, &mat_);
			}
			double sx_;
			cairo_matrix_t mat_;
		};

		class skew_y_transform : public transform
		{
		public:
			skew_y_transform(double sy) : transform(TransformType::SKEW_Y), sy_(sy) {
				cairo_matrix_init(&mat_, 1, sy_, 0, 1, 0, 0);
			}
			virtual ~skew_y_transform() {}
			std::string as_string() const override {
				std::stringstream str;
				str << "skewY(" << sy_ << ")";
				return str.str();
			}
		private:
			void handle_apply(render_context& ctx) override {
				cairo_transform(ctx.cairo(), &mat_);
			}
			void handle_apply_matrix(cairo_matrix_t* mtx) const override {
				cairo_matrix_multiply(mtx, mtx, &mat_);
			}
			double sy_;
			cairo_matrix_t mat_;
		};

		transform::transform(TransformType tt)
			: type_(tt)
		{
		}

		void transform::apply(render_context& ctx)
		{
			handle_apply(ctx);
		}

		transform_ptr transform::factory(TransformType tt, const std::vector<double>& params)
		{
			switch(tt) {
			case TransformType::MATRIX: {
				ASSERT_LOG(params.size() == 6, "matrix requires six(6) parameters. Found: " << params.size());
				auto mat = new matrix_transform(params);
				return transform_ptr(mat);
			}
			case TransformType::TRANSLATE: {
				ASSERT_LOG(params.size() == 2, "translate requires two(2) parameters. Found: " << params.size());
				auto trans = new translate_transform(params[0], params[1]);
				return transform_ptr(trans);
			}
			case TransformType::SCALE: {
				ASSERT_LOG(params.size() == 2, "scale requires two parameters. Found: " << params.size());
				auto scale = new scale_transform(params[0], params[1]);
				return transform_ptr(scale);
			}
			case TransformType::ROTATE: {
				ASSERT_LOG(params.size() == 3, "rotate requires three(3) parameters. Found: " << params.size());
				auto rotate = new rotation_transform(params[0],params[1],params[2]);
				return transform_ptr(rotate);
			}
			case TransformType::SKEW_X: {
				ASSERT_LOG(params.size() == 1, "skew_x requires one(1) parameters. Found: " << params.size());
				auto skew_x = new skew_x_transform(params[0]);
				return transform_ptr(skew_x);
			}
			case TransformType::SKEW_Y: {
				ASSERT_LOG(params.size() == 1, "skew_y requires one(1) parameters. Found: " << params.size());
				auto skew_y = new skew_y_transform(params[0]);
				return transform_ptr(skew_y);
			}
			default:
				ASSERT_LOG(false, "Unknown transform type used.");
			}
			return transform_ptr();
		}

		std::vector<transform_ptr> transform::factory(const std::string& s)
		{
			std::vector<transform_ptr> results;
			enum {
				STATE_TYPE,
				STATE_NUMBER,
			} state = STATE_TYPE;
		
			std::vector<double> parameters;

			TransformType type = TransformType::ERROR;

			boost::char_separator<char> seperators(" \n\t\r,", "()");
			boost::tokenizer<boost::char_separator<char>> tok(s, seperators);
			for(auto it = tok.begin(); it != tok.end(); ++it) {
				if(state == STATE_TYPE) {
					if(*it == "matrix") {
						type = TransformType::MATRIX;
					} else if(*it == "translate") {
						type = TransformType::TRANSLATE;
					} else if(*it == "scale") {
						type = TransformType::SCALE;
					} else if(*it == "rotate") {
						type = TransformType::ROTATE;
					} else if(*it == "skewX") {
						type = TransformType::SKEW_X;
					} else if(*it == "skewY") {
						type = TransformType::SKEW_Y;
					} else if(*it == "(") {
						parameters.clear();
						state = STATE_NUMBER;
					} else {
						ASSERT_LOG(false, "Unexpected token while looking for a type: " << *it << " : " << s);
					}
				} else if(state == STATE_NUMBER) {
					if(*it == ")") {
						ASSERT_LOG(type != TransformType::ERROR, "svg transform type was not initialized");
						switch(type) {
							case TransformType::MATRIX: {
								matrix_transform* mtrf = new matrix_transform(parameters);
								results.emplace_back(mtrf);
								break;
							}
							case TransformType::TRANSLATE: {
								ASSERT_LOG(parameters.size() == 1 || parameters.size() == 2, "Parsing transform:translate found " << parameters.size() << " parameter(s), expected 1 or 2");
								double tx = parameters[0];
								double ty = parameters.size() == 2 ? parameters[1] : 0.0f;
								translate_transform * ttrf = new translate_transform(tx, ty);
								results.emplace_back(ttrf);
								break;
							}
							case TransformType::SCALE: {
								ASSERT_LOG(parameters.size() == 1 || parameters.size() == 2, "Parsing transform:scale found " << parameters.size() << " parameter(s), expected 1 or 2");
								double sx = parameters[0];
								double sy = parameters.size() == 2 ? parameters[1] : sx;
								scale_transform * strf = new scale_transform(sx, sy);
								results.emplace_back(strf);
								break;
							}
							case TransformType::ROTATE: {
								ASSERT_LOG(parameters.size() == 1 || parameters.size() == 3, "Parsing transform:rotate found " << parameters.size() << " parameter(s), expected 1 or 3");
								double angle = parameters[0] / 180.0 * M_PI;
								double cx = parameters.size() == 3 ? parameters[1] : 0;
								double cy = parameters.size() == 3 ? parameters[2] : 0;
								rotation_transform* rtrf = new rotation_transform(angle, cx, cy);
								results.emplace_back(rtrf);
								break;
							}
							case TransformType::SKEW_X: {
								ASSERT_LOG(parameters.size() == 1, "Parsing transform:skewX found " << parameters.size() << " parameter(s), expected 1");
								double sa = tan(parameters[0]);
								skew_x_transform* sxtrf = new skew_x_transform(sa);
								results.emplace_back(sxtrf);
								break;
							}
							case TransformType::SKEW_Y: {
								ASSERT_LOG(parameters.size() == 1, "Parsing transform:skewY found " << parameters.size() << " parameter(s), expected 1");
								double sa = tan(parameters[0]);
								skew_y_transform* sxtrf = new skew_y_transform(sa);
								results.emplace_back(sxtrf);
								break;
							}

							case TransformType::ERROR:
								assert(false);
								break;
						}
						state = STATE_TYPE;					
					} else {
						char* end = nullptr;
						double value = strtod(it->c_str(), &end);
						if(value == 0 && it->c_str() == end) {
							ASSERT_LOG(false, "Invalid number value: " << *it);
						}
						ASSERT_LOG(errno != ERANGE, "parsed numeric value out-of-range: " << *it);					
						parameters.push_back(value);
					}
				}
			}

			return results;
		}

		transform::~transform()
		{
		}

		void transform::apply_matrix(cairo_matrix_t* mtx) const
		{
			handle_apply_matrix(mtx);
		}
	}
}
