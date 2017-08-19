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
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#ifndef _USE_MATH_DEFINES
#	define _USE_MATH_DEFINES	1
#endif 

#include <cmath>
#include <string>
#include <vector>

#include "Color.hpp"
#include "SceneObject.hpp"
#include "VGraphFwd.hpp"

#ifndef M_PI
#	define M_PI	3.14159265358979323846
#endif

namespace KRE
{
	namespace Vector
	{
		enum FillRule {
			FILL_RULE_WINDING,
			FILL_RULE_EVEN_ODD,
		};

		enum LineCap {
			LINE_CAP_BUTT,
			LINE_CAP_ROUND,
			LINE_CAP_SQUARE,
		};

		enum LineJoin {
			LINE_JOIN_MITER,
			LINE_JOIN_ROUND,
			LINE_JOIN_BEVEL,
		};

		class Path
		{
		public:
			Path();
			Path(const double x, const double y, bool relative=false);
			virtual ~Path();

			virtual void MoveTo(const double x, const double y, const bool relative=false) = 0;
			virtual void LineTo(const double x, const double y, const bool relative=false) = 0;

			// Helper function equivalent to drawing an arc between 0.0 and 2*M_PI
			virtual void Circle(const double x, const double y, const double r) = 0;
			virtual void Line(const double x1, const double y1, const double x2, const double y2) = 0;
			virtual void Rectangle(const double x, const double y, const double width, const double height) = 0;

			virtual void Arc(const double cx, const double cy, const double radius, const double start_angle, const double end_angle, bool negative=false) = 0;
				
			// Adds a Cubic Bezier curve to the current path from the current position to the end position
			// (ex,ey) using the control points (x1,y1) and (x2,y2)
			// If relative is true then the curve is drawn with all positions relative to the current point.
			virtual void CubicCurveTo(const double x1, const double y1, const double x2, const double y2, const double ex, const double ey, bool relative=false) = 0;
			// Adds a Quadratic Bezier curve to the current path from the current position to the end position
			// (ex,ey) using the control point (x1,y1)
			// If relative is true then the curve is drawn with all positions relative to the current point.
			virtual void QuadraticCurveTo(const double x1, const double y1, const double ex, const double ey, bool relative=false) = 0;

			//virtual void GlyphPath(const std::vector<Glyph>& g);
			virtual void TextPath(const std::string& s) = 0;

			virtual void ClosePath() = 0;
		private:
		};

		class Matrix
		{
		public:
			Matrix();
			virtual ~Matrix() {}
			virtual void init(double xx, double yx, double xy, double yy, double x0, double y0) = 0;
			virtual void initIdentity() = 0;
			virtual void initTranslate(double x0, double y0) = 0;
			virtual void initScale(double xs, double ys) = 0;
			virtual void initRotation(double rad) = 0;
			virtual void translate(double tx, double ty) = 0;
			virtual void scale(double sx, double sy) = 0;
			virtual void rotate(double rad) = 0;
			virtual void invert() = 0;
			virtual void multiply(const MatrixPtr& a) = 0;
			virtual geometry::Point<double> transformDistance(double x, double y) = 0;
			virtual geometry::Point<double> transformPoint(double x, double y) = 0;
			virtual MatrixPtr clone() = 0;
		};

		static MatrixPtr multiply(const MatrixPtr& a, const MatrixPtr& b);

		class Context : public SceneObject
		{
		public:
			Context(int width, int height);
			virtual ~Context();

			int width() const { return width_; }
			int height() const { return height_; }
				
			virtual void Save() = 0;
			virtual void Restore() = 0;

			virtual void PushGroup() = 0;
			virtual void PopGroup(const bool to_source=false) = 0;

			virtual void SetSourceColor(const double r, const double g, const double b, const double a=1.0) = 0;
			virtual void SetSourceColor(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a=255) = 0;
			virtual void SetSourceColor(const Color& color) = 0;
			virtual void SetSource(const PatternPtr& p) = 0;
			virtual PatternPtr GetSource() const = 0;

			virtual void SetFillRule(const FillRule fr) = 0;
			virtual FillRule GetFillRule() const = 0;

			virtual void SetLineCap(const LineCap lc) = 0;
			virtual LineCap GetLineCap() const = 0;

			virtual void SetLineJoin(const LineJoin lj) = 0;
			virtual LineJoin GetLineJoin() const = 0;

			virtual void SetLineWidth(const double width) = 0;
			virtual double GetLineWidth() const = 0;

			virtual void SetMiterLimit(const double limit) = 0;
			virtual double GetMiterLimit() const = 0;

			virtual void SetDashStyle(const std::vector<double>& dashes, const double offset=0) = 0;
			virtual const std::vector<double> GetDashStyle() const = 0;
			virtual void SetDashOffset(double offset) = 0;
			virtual double GetDashOffset() const = 0;
				
			virtual void Paint(const double alpha=1.0) = 0;

			virtual void Fill(const bool preserve=false) = 0;
			virtual void FillExtents(double& x1, double& y1, double& x2, double& y2) = 0;
			virtual bool InFill(const double x, const double y) = 0;

			virtual void Stroke(const bool preserve=false) = 0;
			virtual void StrokeExtents(double& x1, double& y1, double& x2, double& y2) = 0;
			virtual bool InStroke(const double x, const double y) = 0;

			virtual void Clip(const bool preserve=false) = 0;
			virtual void ClipExtents(double& x1, double& y1, double& x2, double& y2) = 0;
			virtual bool InClip(const double x, const double y) = 0;
			virtual void ClipReset() = 0;

			//virtual void set_antialiasing(const AntiAliasing aa);
			//virtual AntiAliasing get_antialiasing() const;

			virtual void GetCurrentPoint(double& x, double& y) = 0;
			virtual bool HasCurrentPoint() = 0;

			virtual PathPtr NewPath() = 0;
			virtual void AddPath(const PathPtr& path) = 0;
			virtual void AddSubPath(const PathPtr& path) = 0;

			virtual void PathExtents(double& x1, double& y1, double& x2, double& y2) = 0;

			// Sometimes we may wish to draw some vector graphics in the context of the 2D Canvas
			// We can do so through this member function.
			// XXX should maybe make this a little more abstracted so that it seperates concerns
			// between drawing as a SceneObject and Drawing to the Canvas.
			//virtual void CanvasDraw() const = 0;

			virtual void translate(double tx, double ty) = 0;
			virtual void scale(double sx, double sy) = 0;
			virtual void rotate(double rad) = 0;
			virtual void setMatrix(const MatrixPtr& m) = 0;
			virtual MatrixPtr getMatrix() const = 0;
			virtual void transform(const MatrixPtr& m) = 0;
			virtual void setIdentityMatrix() = 0;
			virtual geometry::Point<double> userToDevice(double x, double y) = 0;
			virtual geometry::Point<double> userToDeviceDistance(double x, double y) = 0;
			virtual geometry::Point<double> deviceToUser(double x, double y) = 0;
			virtual geometry::Point<double> deviceToUserDistance(double x, double y) = 0;

			static ContextPtr CreateInstance(const std::string& hint, int width, int height);

			virtual MatrixPtr createMatrix() = 0;
		protected:
			Context();
		private:
			int width_;
			int height_;

			Context(const Context&);
		};
	}
}
