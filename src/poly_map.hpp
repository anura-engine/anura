/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "geometry.hpp"

#include "widget.hpp"

namespace geometry
{
	typedef geometry::Point<double> fpoint;
	typedef std::vector<fpoint> fpoint_list;	

	struct edge
	{
		fpoint p1;
		fpoint p2;
		edge(const fpoint& a, const fpoint& b) : p1(a), p2(b) {}
	};

	class Polygon
	{
	public:
		Polygon(int id) : id_(id), height_(0) {}
		virtual ~Polygon() {}
		
		void addPoint(double x, double y) {
			pts_.push_back(fpoint(x,y));
		}
		void setHeight(int height) {
			height_ = height;
		}
		void init();
		void normalise() {
			pts_.erase(std::unique(pts_.begin(), pts_.end()), pts_.end());
		}
		void calculateCentroid(fpoint& centroid) {
			centroid.x = 0;
			centroid.y = 0;
			for(auto& p : pts_) {
				centroid.x += p.x;
				centroid.y += p.y;
			}
			centroid.x /= double(pts_.size());
			centroid.y /= double(pts_.size());
		}

		void setCentroid(const fpoint& ct) {
			centroid_ = ct;
		}
		void setColor(const KRE::Color& c) {
			color_ = c;
		}

		int getId() const { return id_; }
		int height() const { return height_; }
		const fpoint& getCentroid() const { return centroid_; }
		const std::vector<fpoint>& getPoints() const { return pts_; }
		const KRE::Color& getColor() const { return color_; }

		void draw(int xt=0, int yt=0, float rotate=0, float scale=0) const;
	private:
		std::vector<fpoint> pts_;
		int id_;

		// A somewhat nebulous parameter
		int height_;

		// Stuff for drawing.
		// Constructed triangle fan
		std::vector<glm::vec2> varray_;
		// Color of polygon
		KRE::Color color_;
		// edges for drawing black border.
		std::vector<glm::vec2> vedges_;

		fpoint centroid_;

		Polygon();
		Polygon(const Polygon&);
	};
	typedef std::shared_ptr<Polygon> PolygonPtr;

	std::ostream& operator<<(std::ostream& os, const Polygon& poly);

	namespace voronoi
	{
		class Wrapper
		{
		public:
			Wrapper(const fpoint_list& pts, int relaxations=1, double left=0, double top=0, double right=0, double bottom=0);
			~Wrapper();

			double left() const { return left_; }
			double right() const { return right_; }
			double top() const { return top_; }
			double bottom() const { return bottom_; }

			const std::vector<edge>& getEdges() const { return output_; }
			const std::vector<PolygonPtr>& getPolys() const { return polygons_; }
			const fpoint_list& getSites() const { return sites_; }
		private:
			// bounding box
			double left_;
			double top_; 
			double right_; 
			double bottom_;

			fpoint_list sites_;

			void calculateBoundingBox(const fpoint_list& pts);
			void generate(fpoint_list& pts);

			std::vector<edge> output_;
			std::vector<PolygonPtr> polygons_;

			Wrapper();
			Wrapper(const Wrapper&);
		};

		std::ostream& operator<<(std::ostream& os, const Wrapper& obj);
	}

	class PolyMap : public gui::Widget
	{
	public:
		explicit PolyMap(int n_pts, int relaxations_, int width, int height);
		explicit PolyMap(const variant& v, game_logic::FormulaCallable* e);
		virtual ~PolyMap();

		void init();

		gui::WidgetPtr clone() const override;
	protected:
		virtual void handleDraw() const override;
	private:
		DECLARE_CALLABLE(PolyMap);

		int npts_;
		int relaxations_;
		uint32_t noise_seed_;
		std::vector<fpoint> pts_;
		std::vector<glm::vec2> edges_;

		// Controls the island-ness of the terrain.
		float noise_multiplier_;

		std::vector<PolygonPtr> polygons_;

		PolyMap() = delete;
	};

	typedef ffl::IntrusivePtr<PolyMap> PolyMapPtr;
}
