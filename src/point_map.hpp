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
#ifndef POINT_MAP_HPP_INCLUDED
#define POINT_MAP_HPP_INCLUDED

#include <vector>

#include "geometry.hpp"

//A point_map is a data structure which can map (x,y) points as keys to
//values.
template<typename ValueType>
class point_map
{
public:

	const ValueType& get(const point& p) const {
		const ValueType* value = lookup(p);
		if(!value) {
			static ValueType EmptyValue;
			return EmptyValue;
		} else {
			return *value;
		}
	}

	void insert(const point& p, ValueType value) {
		Row* row;
		if(p.y < 0) {
			const int index = -p.y - 1;
			if(index >= negative_rows_.size()) {
				negative_rows_.resize((index + 1)*2);
			}

			row = &negative_rows_[index];
		} else {
			const int index = p.y;
			if(index >= positive_rows_.size()) {
				positive_rows_.resize((index + 1)*2);
			}

			row = &positive_rows_[index];
		}

		if(p.x < 0) {
			const int index = -p.x - 1;
			if(index >= row->negative_cells.size()) {
				row->negative_cells.resize(index+1);
			}

			row->negative_cells[index] = value;
		} else {
			const int index = p.x;
			if(index >= row->positive_cells.size()) {
				row->positive_cells.resize(index+1);
			}

			row->positive_cells[index] = value;
		}
	}

private:

	const ValueType* lookup(const point& p) const {

		const Row* row;
		if(p.y < 0) {
			const int index = -p.y - 1;
			if(index >= negative_rows_.size()) {
				return NULL;
			}

			row = &negative_rows_[index];
		} else {
			const int index = p.y;
			if(index >= positive_rows_.size()) {
				return NULL;
			}

			row = &positive_rows_[index];
		}

		if(p.x < 0) {
			const int index = -p.x - 1;
			if(index >= row->negative_cells.size()) {
				return NULL;
			}

			return &row->negative_cells[index];
		} else {
			const int index = p.x;
			if(index >= row->positive_cells.size()) {
				return NULL;
			}

			return &row->positive_cells[index];
		}
	}

	struct Row {
		std::vector<ValueType> negative_cells, positive_cells;
	};

	std::vector<Row> negative_rows_, positive_rows_;
};

#endif
