#pragma once

#include <vector>
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "widget.hpp"

namespace geom
{
	template<class T>
	struct point
	{
		point() : x(0), y(0) {}
		explicit point(T xx, T yy) : x(xx), y(yy) {}
		T x, y;

		bool operator==(const point<T>& o) const {
			return o.x == x && o.y == y;
		}
	};

	typedef point<double> fpoint;
	typedef std::vector<fpoint> fpoint_list;	

	struct edge
	{
		fpoint p1;
		fpoint p2;
		edge(const fpoint& a, const fpoint& b) : p1(a), p2(b) {}
	};

	class polygon
	{
	public:
		polygon(int id) : id_(id), height_(0) {}
		virtual ~polygon() {}
		
		void add_point(double x, double y) {
			pts_.push_back(fpoint(x,y));
		}
		void set_height(int height) {
			height_ = height;
		}

		void init() {
			if(pts_.size() > 0) {
				varray_.push_back(centroid().x); 
				varray_.push_back(centroid().y);
				for(auto& p : pts_) {
					varray_.push_back(p.x); 
					varray_.push_back(p.y);
				}
				// close the loop
				varray_.push_back(varray_[2]); 
				varray_.push_back(varray_[3]);

				for(int n = 1; n != pts_.size(); ++n) {
					vedges_.push_back(GLfloat(pts_[n-1].x));
					vedges_.push_back(GLfloat(pts_[n-1].y));
					vedges_.push_back(GLfloat(pts_[n].x));
					vedges_.push_back(GLfloat(pts_[n].y));
				}

			}
		}
		void normalise() {
			pts_.erase(std::unique(pts_.begin(), pts_.end()), pts_.end());
		}
		void calculate_centroid(fpoint& centroid) {
			centroid.x = 0;
			centroid.y = 0;
			for(auto& p : pts_) {
				centroid.x += p.x;
				centroid.y += p.y;
			}
			centroid.x /= double(pts_.size());
			centroid.y /= double(pts_.size());
		}

		void set_centroid(const fpoint& ct) {
			centroid_ = ct;
		}
		void set_color(const graphics::color& c) {
			color_ = c;
		}
		void set_color(const SDL_Color& c) {
			color_ = graphics::color(c);
		}

		int id() const { return id_; }
		int height() const { return height_; }
		const fpoint& centroid() const { return centroid_; }
		const std::vector<fpoint>& points() const { return pts_; }
		const graphics::color& color() const { return color_; }

		virtual void draw() const;
	private:
		std::vector<fpoint> pts_;
		int id_;

		// A somewhat nebulous parameter
		int height_;

		// Stuff for drawing.
		// Constructed triangle fan
		std::vector<GLfloat> varray_;
		// Color of polygon
		graphics::color color_;
		// edges for drawing black border.
		std::vector<GLfloat> vedges_;

		fpoint centroid_;

		polygon();
		polygon(const polygon&);
	};
	typedef std::shared_ptr<polygon> polygon_ptr;

	std::ostream& operator<<(std::ostream& os, const polygon& poly);

	namespace voronoi
	{
		class wrapper
		{
		public:
			wrapper(const fpoint_list& pts, int relaxations=1, double left=0, double top=0, double right=0, double bottom=0);
			~wrapper();

			double left() const { return left_; }
			double right() const { return right_; }
			double top() const { return top_; }
			double bottom() const { return bottom_; }

			const std::vector<edge>& get_edges() const { return output_; }
			const std::vector<polygon_ptr>& get_polys() const { return polygons_; }
			const fpoint_list& sites() const { return sites_; }
		private:
			// bounding box
			double left_;
			double top_; 
			double right_; 
			double bottom_;

			fpoint_list sites_;

			void calculate_bounding_box(const fpoint_list& pts);
			void generate(fpoint_list& pts);

			std::vector<edge> output_;
			std::vector<polygon_ptr> polygons_;

			wrapper();
			wrapper(const wrapper&);
		};

		std::ostream& operator<<(std::ostream& os, const wrapper& obj);
	}

	class poly_map : public gui::widget
	{
	public:
		explicit poly_map(int n_pts, int relaxations_, int width, int height);
		explicit poly_map(const variant& v, game_logic::formula_callable* e);
		virtual ~poly_map();

		void init();

	protected:
		virtual void handle_draw() const;
	private:
		DECLARE_CALLABLE(poly_map);

		int npts_;
		int relaxations_;
		uint32_t noise_seed_;
		std::vector<fpoint> pts_;
		std::vector<GLfloat> edges_;

		// Controls the island-ness of the terrain.
		float noise_multiplier_;

		std::vector<polygon_ptr> polygons_;

		poly_map();
		poly_map(const poly_map&);
	};

	typedef boost::intrusive_ptr<poly_map> poly_map_ptr;
}
