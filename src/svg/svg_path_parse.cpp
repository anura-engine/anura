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

/*
	From www.w3.org/TR/SVG/implnote.html#PathElementImplementationNotes

The S/s commands indicate that the first control point of the given cubic Bezier segment
is calculated by reflecting the previous path segments second control point relative to 
the current point. The exact math is as follows. If the current point is (curx, cury) 
and the second control point of the previous path segment is (oldx2, oldy2), then the 
reflected point (i.e., (newx1, newy1), the first control point of the current path segment) is:

(newx1, newy1) = (curx - (oldx2 - curx), cury - (oldy2 - cury))
               = (2*curx - oldx2, 2*cury - oldy2)

*/

#include <cfloat>
#include <cmath>
#include <iostream>
#include <list>

#include "formatter.hpp"
#include "svg_fwd.hpp"
#include "svg_path_parse.hpp"

namespace KRE
{
	namespace SVG
	{
		namespace
		{

			// Compute the angle between two vectors
			double compute_angle(double ux, double uy, double vx, double vy)
			{
				const double sign = ux*vy - uy*vx < 0 ? -1 : 1;
				const double length_u = std::sqrt(ux*ux+uy*uy);
				const double length_v = std::sqrt(vx*vx+vy*vy);
				const double dot_uv = ux*vx + uy*vy;
				return sign * std::acos(dot_uv/(length_u*length_v));
			}
		}

		path_command::path_command(PathInstruction ins, bool absolute)
			: ins_(ins), absolute_(absolute)
		{
		}

		path_command::~path_command()
		{
		}

		void path_command::cairo_render(path_cmd_context& ctx)
		{
			handle_cairo_render(ctx);

			auto status = cairo_status(ctx.cairo_context());
			ASSERT_LOG(status == CAIRO_STATUS_SUCCESS, "Cairo error: " << cairo_status_to_string(status) << " : " << static_cast<int>(ins_));
		}

		class move_to_command : public path_command
		{
		public:
			move_to_command(bool absolute, double x, double y)
				: path_command(PathInstruction::MOVETO, absolute), 
				x_(x), 
				y_(y) 
			{
			}
			virtual ~move_to_command() {}
		private:
			void handle_cairo_render(path_cmd_context& ctx) override {
				if(is_absolute()) {
					cairo_move_to(ctx.cairo_context(), x_, y_); 
				} else {
					if(!cairo_has_current_point(ctx.cairo_context())) {
						cairo_move_to(ctx.cairo_context(), 0, 0);
					}
					cairo_rel_move_to(ctx.cairo_context(), x_, y_);
				}
				ctx.clear_control_points();
			}
			double x_;
			double y_;
		};

		class line_to_command : public path_command
		{
		public:
			line_to_command(bool absolute, double x, double y)
				: path_command(PathInstruction::LINETO, absolute),
				x_(x),
				y_(y)
			{
			}
			virtual ~line_to_command() {}
		private:
			void handle_cairo_render(path_cmd_context& ctx) override {
				if(is_absolute()) {
					cairo_line_to(ctx.cairo_context(), x_, y_);
				} else {
					cairo_rel_line_to(ctx.cairo_context(), x_, y_);
				}
				ctx.clear_control_points();
			}
			double x_;
			double y_;
		};

		class closepath_command : public path_command
		{
		public:
			closepath_command() : path_command(PathInstruction::CLOSEPATH, true)
			{
			}
			virtual ~closepath_command() {}
		private:
			void handle_cairo_render(path_cmd_context& ctx) override {
				cairo_close_path(ctx.cairo_context());
				ctx.clear_control_points();
			}
		};

		class line_to_h_command : public path_command
		{
		public:
			line_to_h_command(bool absolute, double x)
				: path_command(PathInstruction::LINETO_H, absolute),
				x_(x)
			{
			}
			virtual ~line_to_h_command() {}
		private:
			void handle_cairo_render(path_cmd_context& ctx) override {
				if(is_absolute()) {
					double cx, cy;
					cairo_get_current_point(ctx.cairo_context(), &cx, &cy);
					cairo_line_to(ctx.cairo_context(), x_, cy);
				} else {
					cairo_rel_line_to(ctx.cairo_context(), x_, 0.0);
				}
			}
			double x_;
		};

		class line_to_v_command : public path_command
		{
		public:
			line_to_v_command(bool absolute, double y)
				: path_command(PathInstruction::LINETO_V, absolute),
				y_(y)
			{
			}
			virtual ~line_to_v_command() {}
		private:
			void handle_cairo_render(path_cmd_context& ctx) override {
				if(is_absolute()) {
					double cx, cy;
					cairo_get_current_point(ctx.cairo_context(), &cx, &cy);
					cairo_line_to(ctx.cairo_context(), cx, y_);
				} else {
					cairo_rel_line_to(ctx.cairo_context(), 0.0, y_);
				}
				ctx.clear_control_points();
			}
			double y_;
		};

		class cubic_bezier_command : public path_command
		{
		public:
			cubic_bezier_command(bool absolute, bool smooth, double x, double y, double cp1x, double cp1y, double cp2x, double cp2y)
				: path_command(PathInstruction::CUBIC_BEZIER, absolute),
				smooth_(smooth),
				x_(x), y_(y), 
				cp1x_(cp1x), cp1y_(cp1y), 
				cp2x_(cp2x), cp2y_(cp2y)
			{
			}
			virtual ~cubic_bezier_command() {}
		private:
			void handle_cairo_render(path_cmd_context& ctx) override {
				double c0x, c0y;
				cairo_get_current_point(ctx.cairo_context(), &c0x, &c0y);
				if(smooth_) {
					ctx.get_control_points(&cp1x_, &cp1y_);
					cp1x_ = 2.0*c0x - cp1x_;
					cp1y_ = 2.0*c0y - cp1y_;
					if(!is_absolute()) {
						cp1x_ -= c0x;
						cp1y_ -= c0y;
					}
				}
				if(is_absolute()) {
					cairo_curve_to(ctx.cairo_context(), cp1x_, cp1y_, cp2x_, cp2y_, x_, y_);
				} else {
					cairo_rel_curve_to(ctx.cairo_context(), cp1x_, cp1y_, cp2x_, cp2y_, x_, y_);
				}
				// we always write control points in absolute co-ords
				ctx.set_control_points(is_absolute() ? cp2x_ : cp2x_ + c0x, is_absolute() ? cp2y_ : cp2y_ + c0y);
			}
			bool smooth_;
			double x_;
			double y_;
			// control point one for cubic bezier
			double cp1x_;
			double cp1y_;
			// control point two for cubic bezier
			double cp2x_;
			double cp2y_;
		};

		class quadratic_bezier_command : public path_command
		{
		public:
			quadratic_bezier_command(bool absolute, bool smooth, double x, double y, double cp1x, double cp1y)
				: path_command(PathInstruction::CUBIC_BEZIER, absolute),
				smooth_(smooth),
				x_(x), y_(y), 
				cp1x_(cp1x), cp1y_(cp1y)
			{
			}
			virtual ~quadratic_bezier_command() {}
		private:
			void handle_cairo_render(path_cmd_context& ctx) override {
				double c0x, c0y;
				cairo_get_current_point(ctx.cairo_context(), &c0x, &c0y);
				if(smooth_) {
					double cp1x, cp1y;
					ctx.get_control_points(&cp1x, &cp1y);
					cp1x_ = 2.0*c0x - cp1x;
					cp1y_ = 2.0*c0y - cp1y;
					if(!is_absolute()) {
						cp1x_ -= c0x;
						cp1y_ -= c0y;
					}
				}
				double dx, dy;
				double acp1x, acp1y;
				// Simple quadratic -> cubic conversion.
				dx = x_;
				dy = y_;
				acp1x = cp1x_;
				acp1y = cp1y_;
				if(!is_absolute()) {
					dx += c0x;
					dy += c0y;
					acp1x += c0x;
					acp1y += c0y;
				}
				const double cpx1 = c0x + 2.0/3.0 * (acp1x - c0x);
				const double cpy1 = c0y + 2.0/3.0 * (acp1y - c0y);
				const double cpx2 = dx + 2.0/3.0 * (acp1x - dx);
				const double cpy2 = dy + 2.0/3.0 * (acp1y - dy);

				cairo_curve_to(ctx.cairo_context(), cpx1, cpy1, cpx2, cpy2, x_, y_);

				// we always write control points in absolute co-ords
				ctx.set_control_points(is_absolute() ? cp1x_ : cp1x_ + c0x, is_absolute() ? cp1y_ : cp1y_ + c0y);
			}
			bool smooth_;
			double x_;
			double y_;
			// control point for quadratic bezier
			double cp1x_;
			double cp1y_;
		};

		class elliptical_arc_command : public path_command
		{
		public:
			elliptical_arc_command(bool absolute, double x, double y, double rx, double ry, double x_axis_rot, bool large_arc, bool sweep)
				: path_command(PathInstruction::CUBIC_BEZIER, absolute),
				  x_(x), 
                  y_(y), 
				  rx_(rx), 
                  ry_(ry), 
				  large_arc_flag_(large_arc), 
				  sweep_flag_(sweep) 
			{
				x_axis_rotation_ = x_axis_rot / 180.0 * M_PI;
			}
			virtual ~elliptical_arc_command() {}
		private:
			void handle_cairo_render(path_cmd_context& ctx) override {
				double x1, y1;
				cairo_get_current_point(ctx.cairo_context(), &x1, &y1);

				// calculate some ellipse stuff
				// a is the length of the major axis
				// b is the length of the minor axis
				double a = rx_;
				double b = ry_;
				const double x2 = is_absolute() ? x_ : x_ + x1;
				const double y2 = is_absolute() ? y_ : y_ + y1;

				// start and end points in the same location is equivalent to not drawing the arc.
				if(std::abs(x1-x2) < DBL_EPSILON && std::abs(y1-y2) < DBL_EPSILON) {
					return;
				}
			
				const double r1 = (x1-x2)/2.0;
				const double r2 = (y1-y2)/2.0;

				const double cosp = cos(x_axis_rotation_);
				const double sinp = sin(x_axis_rotation_);

				const double x1_prime = cosp*r1 + sinp*r2;
				const double y1_prime = -sinp * r1 + cosp*r2;

				double gamma = (x1_prime*x1_prime)/(a*a) + (y1_prime*y1_prime)/(b*b);
				if (gamma > 1) {
					a *= sqrt(gamma);
					b *= sqrt(gamma);
				}

				const double denom1 = a*a*y1_prime*y1_prime+b*b*x1_prime*x1_prime;
				if(std::abs(denom1) < DBL_EPSILON) {
					return;
				}
				const double root = std::sqrt(std::abs(a*a*b*b/denom1-1));
				double xc_prime = root * a * y1_prime / b;
				double yc_prime = -root * b * x1_prime / a;

				if((large_arc_flag_ && sweep_flag_ ) || (!large_arc_flag_ && !sweep_flag_ )) {
					xc_prime = -1 * xc_prime;
					yc_prime = -1 * yc_prime;
				}

				const double xc = cosp * xc_prime - sinp * yc_prime + (x1+x2)/2.0;
				const double yc = sinp * xc_prime + cosp * yc_prime + (y1+y2)/2.0;

				const double k1 = (x1_prime - xc_prime)/a;
				const double k2 = (y1_prime - yc_prime)/b;
				const double k3 = (-x1_prime - xc_prime)/a;
				const double k4 = (-y1_prime - yc_prime)/b;

				const double k5 = sqrt(fabs(k1*k1 + k2*k2));
				if(std::abs(k5) < DBL_EPSILON) { 
					return;
				}

				const double t1 = (k2 < 0 ? -1 : 1) * std::acos(clamp(k1/k5, -1.0, 1.0));	// theta_1

				const double k7 = std::sqrt(fabs((k1*k1 + k2*k2)*(k3*k3 + k4*k4)));
				if(std::abs(k7) < DBL_EPSILON) {
					return;
				}

				const double theta_delta = (k1*k4 - k3*k2 < 0 ? -1 : 1) * acos(clamp((k1*k3 + k2*k4)/k7, -1.0, 1.0));
				const double t2 = theta_delta > 0 && !sweep_flag_ ? theta_delta-2.0*M_PI : theta_delta < 0 && sweep_flag_ ? theta_delta+2.0*M_PI : theta_delta;

				const int n_segs = int(std::ceil(std::abs(t2/(M_PI*0.5+0.001))));
				for(int i = 0; i < n_segs; i++) {
					const double th0 = t1 + i * t2 / n_segs;
					const double th1 = t1 + (i + 1) * t2 / n_segs;
					const double th_half = 0.5 * (th1 - th0);
					const double t = (8.0 / 3.0) * std::sin(th_half * 0.5) * std::sin(th_half * 0.5) / std::sin(th_half);
					const double x1 = a*(std::cos(th0) - t * std::sin(th0));
					const double y1 = b*(std::sin(th0) + t * std::cos(th0));
					const double x3 = a*std::cos(th1);
					const double y3 = b*std::sin(th1);
					const double x2 = x3 + a*(t * std::sin(th1));
					const double y2 = y3 + b*(-t * std::cos(th1));
					cairo_curve_to(ctx.cairo_context(), 
						xc + cosp*x1 - sinp*y1, 
						yc + sinp*x1 + cosp*y1, 
						xc + cosp*x2 - sinp*y2, 
						yc + sinp*x2 + cosp*y2, 
						xc + cosp*x3 - sinp*y3, 
						yc + sinp*x3 + cosp*y3);
				}

				ctx.clear_control_points();
			}
			double x_;
			double y_;
			// elliptical arc radii
			double rx_;
			double ry_;
			// arc x axis rotation
			double x_axis_rotation_;
			bool large_arc_flag_;
			bool sweep_flag_;
		};


		class path_parser
		{
		public:
			path_parser(const std::string& s) : path_(s.begin(), s.end()) {
				do {
					if(path_.empty()) {
						throw parsing_exception("Found empty string");
					}
				} while(match_wsp_opt());
				match_moveto_drawto_command_groups();
				while(match_wsp_opt()) {
				}
				if(path_.size() > 0) {
					throw parsing_exception(formatter() << "Input data left after parsing: " << std::string(path_.begin(), path_.end()));
				}
			}
			bool match_wsp_opt()
			{
				if(path_.empty()) {
					return false;
				}
				char c = path_.front();
				if(c == ' ' || c == '\t' || c == '\r' || c == '\n') {
					path_.pop_front();
					return true;
				}
				return false;
			}
			void match_wsp_star()
			{
				while(match_wsp_opt()) {
				}
			}
			void match_wsp_star_or_die()
			{
				do {
					if(path_.empty()) {
						throw parsing_exception("Found empty string");
					}
				} while(match_wsp_opt());
			}
			bool match(char c)
			{
				if(path_.empty()) {
					return false;
				}
				if(path_.front() == c) {
					path_.pop_front();
					return true;
				}
				return false;
			}
			bool match_moveto_drawto_command_groups()
			{
				if(path_.empty()) {
					return false;
				}
				match_moveto_drawto_command_group();
				match_wsp_star();
				return match_moveto_drawto_command_groups();
			}
			bool match_moveto_drawto_command_group()
			{
				if(!match_moveto()) {
					return false;
				}
				match_wsp_star();
				return match_drawto_commands();
			}
			bool match_moveto()
			{
				if(path_.empty()) {
					return false;
				}
				// ( "M" | "m" ) wsp* moveto-argument-sequence
				char c = path_.front();
				if(c == 'M' || c == 'm') {
					path_.pop_front();
					match_wsp_star_or_die();
					match_moveto_argument_sequence(c == 'M' ? true : false);
				} else {
					throw parsing_exception("Expected 'M' or 'm'");
				}
				return true;
			}
			bool match_moveto_argument_sequence(bool absolute)
			{
				double x, y;
				match_coordinate_pair(x, y);
				// emit
				cmds_.emplace_back(new move_to_command(absolute, x, y));
				match_comma_wsp_opt();
				return match_lineto_argument_sequence(absolute);
			}
			bool match_lineto_argument_sequence(bool absolute)
			{
				double x, y;
				if(match_coordinate_pair(x, y)) {
					// emit
					cmds_.emplace_back(new line_to_command(absolute, x, y));
					match_comma_wsp_opt();
					match_lineto_argument_sequence(absolute);
				}
				return true;
			}
			bool match_coordinate_pair(double& x, double& y)
			{
				if(!match_coordinate(x)) {
					return false;
				}
				match_comma_wsp_opt();
				if(!match_coordinate(y)) {
					throw parsing_exception(formatter() << "Expected a second co-ordinate while parsing value: " << std::string(path_.begin(), path_.end()));
				}
				return true;
			}
			bool match_coordinate(double& v)
			{
				return match_number(v);
			}
			bool match_number(double& d)
			{
				std::string s(path_.begin(), path_.end());
				char* end;
				d = strtod(s.c_str(), &end);
				if(errno == ERANGE) {
					throw parsing_exception(formatter() << "Decode of numeric value out of range. " << s);
				}
				if(d == 0 && end == s.c_str()) {
					// No number to convert.
					return false;
				}
				auto it = path_.begin();
				std::advance(it, end - s.c_str());
				path_.erase(path_.begin(), it);
				return true;
			}
			bool match_comma_wsp_opt()
			{
				if(path_.empty()) {
					return false;
				}
				char c = path_.front();
				if(c == ',') {
					path_.pop_front();
					match_wsp_star();
				} else {
					if(!match_wsp_opt()) {
						return true;
					}
					match_wsp_star();
					c = path_.front();
					if(c != ',') {
						//throw parsing_exception("Expected COMMA");
						return true;
					}
					path_.pop_front();
					match_wsp_star();
				}
				return true;
			}
			void match_comma_wsp_or_die() {
				if(!match_comma_wsp_opt()) {
					throw parsing_exception("End of string found");
				}
			}
			bool match_drawto_commands()
			{
				if(!match_drawto_command()) {
					return false;
				}
				match_wsp_star();
				return match_drawto_commands();
			}
			bool match_drawto_command()
			{
				if(path_.empty()) {
					return false;
				}
				char c = path_.front();
				if(c == 'M' || c == 'm') {
					return false;
				}
				path_.pop_front();
				switch(c) {
					case 'Z': case 'z': 
						cmds_.emplace_back(new closepath_command()); 
						break;
					case 'L':  case 'l': 
						match_wsp_star();
						match_lineto_argument_sequence(c == 'L' ? true : false);
						break;
					case 'H': case 'h':
						match_wsp_star();
						match_single_coordinate_argument_sequence(PathInstruction::LINETO_H, c == 'H' ? true : false);
						break;
					case 'V': case 'v':
						match_wsp_star();
						match_single_coordinate_argument_sequence(PathInstruction::LINETO_V, c == 'V' ? true : false);
						break;
					case 'C': case 'c': case 'S': case 's':
						match_wsp_star();
						match_curveto_argument_sequence(c=='C'||c=='S'?true:false, c=='S'||c=='s'?true:false);
						break;
					case 'Q': case 'q': case 'T': case 't':
						match_wsp_star();
						match_bezierto_argument_sequence(c=='Q'||c=='T'?true:false, c=='T'||c=='t'?true:false);
						break;
					case 'A': case 'a':
						match_arcto_argument_sequence(c == 'A' ? true : false);
						break;
					default:
						throw parsing_exception(formatter() << "Unrecognised draw-to symbol: " << c);
				}			
				return true;
			}
			bool match_single_coordinate_argument_sequence(PathInstruction ins, bool absolute)
			{
				double v;
				if(!match_coordinate(v)) {
					return false;
				}
				// emit
				if(ins == PathInstruction::LINETO_H) {
					cmds_.emplace_back(new line_to_h_command(absolute, v));
				} else if(ins == PathInstruction::LINETO_V) {
					cmds_.emplace_back(new line_to_v_command(absolute, v));
				} else {
					ASSERT_LOG(false, "Unexpected command given.");
				}
				match_wsp_star();
				return match_single_coordinate_argument_sequence(ins, absolute);
			}
			bool match_curveto_argument_sequence(bool absolute, bool smooth)
			{
				double x, y;
				double cp1x, cp1y;
				double cp2x, cp2y;
				if(!match_curveto_argument(smooth, x, y, cp1x, cp1y, cp2x, cp2y)) {
					return false;
				}
				// emit
				cmds_.emplace_back(new cubic_bezier_command(absolute, smooth, x, y, cp1x, cp1y, cp2x, cp2y));
				match_wsp_star();
				return match_curveto_argument_sequence(absolute, smooth);
			}
			bool match_curveto_argument(bool smooth, double& x, double& y, double& cp1x, double& cp1y, double& cp2x, double& cp2y) 
			{
				if(!smooth) {
					if(!match_coordinate_pair(cp1x, cp1y)) {
						return false;
					}
					if(!match_comma_wsp_opt()) {
						throw parsing_exception("End of string found");
					}
				} else {
					cp1x = cp1y = 0;
				}
				if(!match_coordinate_pair(cp2x, cp2y)) {
					if(smooth) {
						return false;
					} else {
						throw parsing_exception(formatter() << "Expected first pair of control points in curve: " << std::string(path_.begin(), path_.end()));
					}
				}
				if(!match_comma_wsp_opt()) {
					throw parsing_exception("End of string found");
				}
				if(!match_coordinate_pair(x, y)) {
					throw parsing_exception(formatter() << "Expected second pair of control points in curve: " << std::string(path_.begin(), path_.end()));
				}
				return true;
			}
			bool match_bezierto_argument_sequence(bool absolute, bool smooth)
			{
				double x, y;
				double cp1x, cp1y;
				if(!match_bezierto_argument(smooth, x, y, cp1x, cp1y)) {
					return false;
				}
				// emit
				cmds_.emplace_back(new quadratic_bezier_command(absolute, smooth, x, y, cp1x, cp1y));
				match_wsp_star();
				return match_bezierto_argument_sequence(absolute, smooth);
			}
			bool match_bezierto_argument(bool smooth, double& x, double& y, double& cp1x, double& cp1y) 
			{
				if(smooth) {
					cp1x = cp1y = 0;
				} else {
					if(!match_coordinate_pair(cp1x, cp1y)) {
						return false;
					}
					if(!match_comma_wsp_opt()) {
						throw parsing_exception("End of string found");
					}
				}
				if(!match_coordinate_pair(x, y)) {
					if(smooth) {
						return false;
					} else {
						throw parsing_exception(formatter() << "Expected first pair of control points in curve: " << std::string(path_.begin(), path_.end()));
					}
				}
				return true;
			}
			bool match_arcto_argument_sequence(bool absolute)
			{
				double x, y;
				double rx, ry;
				double x_axis_rot;
				bool large_arc;
				bool sweep;
				if(!match_arcto_argument(x, y, rx, ry, x_axis_rot, large_arc, sweep)) {
					return false;
				}
				// emit
				rx = std::abs(rx);
				ry = std::abs(ry);
				if(rx < DBL_EPSILON) {
					cmds_.emplace_back(new line_to_v_command(absolute, ry));
				} else if(ry < DBL_EPSILON) {
					cmds_.emplace_back(new line_to_h_command(absolute, rx));
				} else {
					cmds_.emplace_back(new elliptical_arc_command(absolute, x, y, rx, ry, x_axis_rot, large_arc, sweep));
				}
				match_wsp_star();
				return match_arcto_argument_sequence(absolute);
			}
			bool match_arcto_argument(double& x, double& y, double& rx, double& ry, double& x_axis_rot, bool& large_arc, bool& sweep) 
			{
				if(!match_coordinate(rx)) {
					return false;
				}
				if(rx < 0) {
					throw parsing_exception(formatter() << "While parsing elliptic arc command found negative RX value: " << rx);
				}
				match_comma_wsp_or_die();
				if(!match_coordinate(ry)) {
					throw parsing_exception("Unmatched RY value while parsing elliptic arc command");
				}
				if(ry < 0) {
					throw parsing_exception(formatter() << "While parsing elliptic arc command found negative RY value: " << y);
				}
				match_comma_wsp_or_die();
				if(!match_coordinate(x_axis_rot)) {
					throw parsing_exception("Unmatched x_axis_rotation value while parsing elliptic arc command");
				}
				match_comma_wsp_or_die();
				double large_arc_flag;
				if(!match_number(large_arc_flag)) {
					throw parsing_exception("Unmatched large_arc_flag value while parsing elliptic arc command");
				}
				large_arc = large_arc_flag > 0 ? true : false;
				match_comma_wsp_or_die();
				double sweep_flag;
				if(!match_number(sweep_flag)) {
					throw parsing_exception("Unmatched sweep_flag value while parsing elliptic arc command");
				}
				sweep = sweep_flag > 0 ? true : false;
				match_comma_wsp_or_die();
				if(!match_coordinate_pair(x, y)) {
					throw parsing_exception(formatter() << "Expected X,Y points in curve: " << std::string(path_.begin(), path_.end()));
				}
				return true;
			}
			const std::vector<path_commandPtr>& get_command_list() const { return cmds_; }
		private:
			std::list<char> path_;
			std::vector<path_commandPtr> cmds_;
		};

		std::vector<path_commandPtr> parse_path(const std::string& s)
		{
			path_parser pp(s);
			return pp.get_command_list();
		}
	}
}
