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

#include <cstdint>
#include "Color.hpp"
#include "VGraphFwd.hpp"

namespace Graphics
{
	namespace Vector
	{
		enum PatternType {
			PATTERN_TYPE_SOLID,
			PATTERN_TYPE_SURFACE,
			PATTERN_TYPE_LINEAR,
			PATTERN_TYPE_RADIAL,
			PATTERN_TYPE_MESH,
		};

		class Pattern
		{
		public:
			virtual ~Pattern();
			PatternType type() const { return type_; }
		protected:
			pattern(PatternType type);		
		private:
			PatternType type_;
			Pattern();
			Pattern(const Pattern&);
		};

		class SolidPattern : public Pattern
		{
		public:
			SolidPattern();
			SolidPattern(const double r, const double g, const double b, const double a=1.0);
			SolidPattern(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a=255);
			virtual ~SolidPattern();
			double red() const { return color_.red(); }
			double green() const { return color_.green(); }
			double blue() const { return color_.blue(); }
			double alpha() const { return color_.alpha(); }
			const Color& color() const { return color_; }
		private:
			Color color_;
		};

		/*class SurfacePattern : public Pattern
		{
		public:
			SurfacePattern(const SurfacePtr& surface);
			virtual ~SurfacePattern();
		private:
		};*/

		typedef std::pair<double,Color> ColorStop;

		class LinearPattern : public Pattern
		{
		public:
			LinearPattern(const double x1, const double y1, const double x2, const double y2);
			virtual ~LinearPattern();
			void AddColorStop(double offset, const Color& color);
			void AddColorStop(double offset, const double r, const double g, const double b, const double a=1.0);
			void AddColorStop(double offset, const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a=255);
			const std::vector<ColorStop>& GetColorStops() const { return color_stops_; }
		private:
			double x1_;
			double y1_;
			double x2_;
			double y2_;
			std::vector<ColorStop> color_stops_;
		};

		class RadialPattern : public Pattern
		{
		public:
			RadialPattern(const double cx1, const double cy1, const double r1, const double cx2, const double cy2, const double r2);
			virtual ~RadialPattern();
			void AddColorStop(double offset, const Color& color);
			void AddColorStop(double offset, const double r, const double g, const double b, const double a=1.0);
			void AddColorStop(double offset, const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a=255);
			const std::vector<ColorStop>& GetColorStops() const { return color_stops_; }
		private:
			std::vector<std::pair<double,Color>> stops_;
		};

		class MeshPatch
		{
		public:
			MeshPatch();
			virtual ~MeshPatch();
			void MoveTo(const double x, const double y);
			void LineTo(const double x, const double y);
			void CurveTo(const double x1, const double y1, const double x2, const double y2, const double ex, const double ey);
			void SetControlPoint(const size_t n, const double x, const double y);
			void SetCornerColor(const size_t corner, const double r, const double g, const double b, const double a=1.0);
			void SetCornerColor(const size_t corner, const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a=255);
			void SetCornerColor(const size_t corner, const Color& color);
		private:
			MeshPatch(const MeshPatch&);
		};
		typedef std::shared_ptr<MeshPatch> MeshPatchPtr;

		class MeshPattern : public Pattern
		{
		public:
			MeshPattern();
			virtual ~MeshPattern();
			void AddPatch(const MeshPatchPtr& patch)
		private:
			std::vector<MeshPatchPtr> patches_;
		};
	}
}
