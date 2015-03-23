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

#include <cairo.h>

#include "AttributeSet.hpp"
#include "geometry.hpp"
#include "SceneUtil.hpp"
#include "VGraph.hpp"

namespace KRE
{
	namespace Vector
	{
		class CairoPath;

		class CairoMatrix : public Matrix
		{
		public:
			CairoMatrix();
			explicit CairoMatrix(const cairo_matrix_t& mat);
			virtual void init(double xx, double yx, double xy, double yy, double x0, double y0) override;
			virtual void initIdentity() override;
			virtual void initTranslate(double x0, double y0) override;
			virtual void initScale(double xs, double ys) override;
			virtual void initRotation(double rad) override;
			virtual void translate(double tx, double ty) override;
			virtual void scale(double sx, double sy) override;
			virtual void rotate(double rad) override;
			virtual void invert() override;
			virtual void multiply(const MatrixPtr& a) override;
			virtual geometry::Point<double> transformDistance(double x, double y) override;
			virtual geometry::Point<double> transformPoint(double x, double y) override;
			virtual MatrixPtr clone() override;
			void set(const cairo_matrix_t& mat) { matrix_ = mat; }
			const cairo_matrix_t* get() const { return &matrix_; }
		private:
			cairo_matrix_t matrix_;
		};

		class CairoContext : public Context
		{
		public:
			CairoContext(int width, int height);
			virtual ~CairoContext();

			virtual void Save() override;
			virtual void Restore() override;

			virtual void PushGroup() override;
			virtual void PopGroup(const bool to_source=false) override;

			virtual void SetSourceColor(const double r, const double g, const double b, const double a=1.0) override;
			virtual void SetSourceColor(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a=255) override;
			virtual void SetSourceColor(const Color& color) override;
			virtual void SetSource(const PatternPtr& p) override;
			virtual PatternPtr GetSource() const override;

			virtual void SetFillRule(const FillRule fr) override;
			virtual FillRule GetFillRule() const override;

			virtual void SetLineCap(const LineCap lc) override;
			virtual LineCap GetLineCap() const override;

			virtual void SetLineJoin(const LineJoin lj) override;
			virtual LineJoin GetLineJoin() const override;

			virtual void SetLineWidth(const double width) override;
			virtual double GetLineWidth() const override;

			virtual void SetMiterLimit(const double limit) override;
			virtual double GetMiterLimit() const override;

			virtual void SetDashStyle(const std::vector<double>& dashes, const double offset=0) override;
			virtual const std::vector<double> GetDashStyle() const override;
			virtual void SetDashOffset(double offset) override;
			virtual double GetDashOffset() const override;
				
			virtual void Paint(const double alpha=1.0) override;

			virtual void Fill(const bool preserve=false) override;
			virtual void FillExtents(double& x1, double& y1, double& x2, double& y2) override;
			virtual bool InFill(const double x, const double y) override;

			virtual void Stroke(const bool preserve=false) override;
			virtual void StrokeExtents(double& x1, double& y1, double& x2, double& y2) override;
			virtual bool InStroke(const double x, const double y) override;

			virtual void Clip(const bool preserve=false) override;
			virtual void ClipExtents(double& x1, double& y1, double& x2, double& y2) override;
			virtual bool InClip(const double x, const double y) override;
			virtual void ClipReset() override;

			//virtual void set_antialiasing(const AntiAliasing aa) override;
			//virtual AntiAliasing get_antialiasing() const override;

			virtual void GetCurrentPoint(double& x, double& y) override;
			virtual bool HasCurrentPoint() override;

			virtual void PathExtents(double& x1, double& y1, double& x2, double& y2) override;

			virtual PathPtr NewPath() override;
			virtual void AddPath(const PathPtr& path) override;
			virtual void AddSubPath(const PathPtr& path) override;
		
			virtual void preRender(const WindowPtr& wnd) override;

			void translate(double tx, double ty) override;
			void scale(double sx, double sy) override;
			void rotate(double rad) override;
			void setMatrix(const MatrixPtr& m) override;
			MatrixPtr getMatrix() const override;
			void transform(const MatrixPtr& m) override;
			void setIdentityMatrix() override;
			geometry::Point<double> userToDevice(double x, double y) override;
			geometry::Point<double> userToDeviceDistance(double x, double y) override;
			geometry::Point<double> deviceToUser(double x, double y) override;
			geometry::Point<double> deviceToUserDistance(double x, double y) override;

			MatrixPtr createMatrix() override;
		protected:
			cairo_t* GetContext() { return context_; }
			cairo_surface_t* GetSurface() { return surface_; }
		private:
			cairo_t* context_;
			cairo_surface_t* surface_;

			std::vector<PathPtr> path_list_;

			rectf draw_rect_;
			TexturePtr tex_;
			std::shared_ptr<Attribute<vertex_texcoord>> attribs_;

			friend class CairoPath;
		};
	}
}
