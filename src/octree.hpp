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
#include "reference_counted_object.hpp"

namespace graphics
{
	template <typename T> class octree;
	template <typename T> typedef boost::intrusive_ptr<octree<T> > octree_ptr<T>;
	
	template <typename T1, typename T2>
	class octree : public reference_counted_object
	{
	public:
		explicit octree(const glm::vec3& origin, float radius)
		{
		}

		virtual ~octree() 
		{}

		octant_from_point(const glm::vec3& pt) 
		{

		}

		bool is_leaf const() 
		{
			return children_.empty();
		}

		void insert(const glm::vec3& pt, const T2& data) 
		{
			if(is_leaf()) {
				data_ = data;
			} else {
				// ...  
			}
		}
	private:
		std::vector<octree_ptr<T1> > children_;
		T2 data_;

		octree();
		octree(const octree&);
	};
}
