// spline3d -- http://en.wikipedia.org/wiki/Cubic_Hermite_spline

#include <vector>
#include <glm/glm.hpp>

namespace geometry
{
    template<typename T>
    class spline3d
    {
    public:
        spline3d() : recalc_(true) {}
        spline3d(const std::vector<glm::tvec3<T>>& cps) 
            : points_(cps), 
              recalc_(false), 
			  coeffs_(T(2), T(-3), T(0), T(1), T(-2), T(3), T(0), T(0), T(1), T(-2), T(1), T(0), T(1), T(-1), T(0), T(0))
        {
            recalculate_tangents();
        }
        void add_point(const glm::tvec3<T>& pt) 
        {
            points_.emplace_back(pt);
            if(recalc_) {
                recalculate_tangents();
            }
        }
        void recalculate_tangents()
        {
            if(points_.size() < 2) {
                // need at least 3 points.
                return;
            }
            tangents_.resize(points_.size());
            
            for(size_t n = 0; n != points_.size(); ++n) {
                // Catmull-Rom approach
                if(n == 0) {
                    if(points_[0] == points_[points_.size()-1]) {
                        tangents_[n] = static_cast<T>(0.5) * points_[1] - points_[n-2];
                    } else {
                        tangents_[n] = static_cast<T>(0.5) * points_[1] - points_[0];
                    }
                } else if(n == points_.size() - 1) {
                    if(points_[0] == points_[n-1]) {
                        tangents_[n] = tangents_[0];
                    } else {
                        tangents_[n] = static_cast<T>(0.5) * points_[n] - points_[n-1];
                    }
                } else {
                    tangents_[n] = static_cast<T>(0.5) * (points_[n+1] - points_[n-1]);
                }
            }
        }
        glm::tvec3<T> interpolate(T x)
        {
			assert(!points_.empty());
            const T seg = x * (points_.size()-1);
            const unsigned useg = static_cast<unsigned>(seg);
            return interpolate(useg, seg - useg);
        }
        void clear()
        {
            points_.clear();
            tangents_.clear();
        }
        size_t size()
        {
            return points_.size();
        }
    private:
        glm::tvec3<T> interpolate(unsigned seg, T x)
        {
            if(x == T(0)) {
                return points_[seg];
            } else if(x == T(1)) {
                return points_[seg+1];
            }
            glm::tvec4<T> powers(x*x*x, x*x, x, T(1));
			glm::tmat4x4<T> m(points_[seg].x, points_[seg+1].x, tangents_[seg].x, tangents_[seg+1].x,
				points_[seg].y, points_[seg+1].y, tangents_[seg].y, tangents_[seg+1].y,
				points_[seg].z, points_[seg+1].z, tangents_[seg].z, tangents_[seg+1].z,
				T(1), T(1), T(1), T(1));
			auto ret = powers * coeffs_ * m;
            return glm::tvec3<T>(ret.x, ret.y, ret.z);
        }
        
        bool recalc_;
        const glm::tmat4x4<T> coeffs_;
        std::vector<glm::tvec3<T>> points_;
        std::vector<glm::tvec3<T>> tangents_;
    };
}
