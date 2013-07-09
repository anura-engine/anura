/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include <boost/intrusive_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include "reference_counted_object.hpp"

namespace graphics
{
	template <typename T1, typename T2> class octree;
	template <typename T1, typename T2> typedef boost::intrusive_ptr<octree<T1,T2> > octree_ptr<T1,T2>;
	
	template <typename T1, typename T2>
	class octree : public reference_counted_object
	{
	public:
		explicit octree(const glm::vec3& origin, float radius)
			: origin_(origin), radius_(radius)
		{
		}

		virtual ~octree() 
		{}

		int octant_from_point(const glm::vec3& pt) 
		{
			int octant = 0;
			if(pt.x >= origin_.x) {
				octant |= 4;
			}
			if(pt.y >= origin_.y) {
				octant |= 2;
			}
			if(pt.z >= origin_.z) {
				octant |= 1;
			}
			return octant;
		}

		bool is_leaf const() 
		{
			return children_.empty();
		}

		void insert(const glm::vec3& pt, const T2& data) 
		{
			if(is_leaf()) {
				is(data_ == NULL) {
					data_.reset(new std::make_pair(pt, data));
				} else {
					T2 old_pt = data_->first;
					T2 old_data = data_->second;
					data_.reset();
					for(int n = 0; n != 8; ++n) {
						glm::vec3 child_origin(origin_.x+radius/(i&4 ? 2.0f : -2.0f), 
							origin_.y+radius/(i&2 ? 2.0f : -2.0f),
							origin_.z+radius/(i&1 ? 2.0f : -2.0f));
						children_.push_back(new octree(child_origin, radius/2.0f));
					}
					children_[octant_from_point(old_pt)].insert(old_pt, old_data);
					children_[octant_from_point(pt)].insert(pt, data);
				}
			} else {
				children_[octant_from_point(pt)].insert(pt, data);
			}
		}

		void points_in_box(const glm::vec3& b1, const glm::vec3& b2, std::vector<T2>& results)
		{
			if(is_leaf()) {
				if(data_ != NULL) {
					if(data_->first.x > b1.x || data_->first.y > b1.y || data_->first.z > b1.z
						|| data_->first.x < b2.x || data_->first.y < b2.y || data_->first.z < b2.z) {
						return;
					}
					results.push_back(data_->second);
				}
			} else {
				for(auto child : children_) {
					glm::vec3 c1 = child.origin_ - child.radius_;
					glm::vec3 c2 = child.origin_ + child.radius_;

					if(c1.x < b1.x || c1.y < b1.y || c1.z < b1.z 
						|| c2.x > b2.x || c2.y > b2.y || c2.z > b2.z) {
						continue;
					}
					child.points_in_box(b1, b2, results);
				}
			}
		}

	private:
		glm::vec3 origin_;
		float radius_;

		std::vector<octree_ptr<T1> > children_;
		boost::shared_ptr<std::pair<glm::vec3, T2> > data_;

		octree();
		octree(const octree&);
	};
}
