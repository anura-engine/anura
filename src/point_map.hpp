/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>

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

#include <vector>

#include "geometry.hpp"

//A point_map is a data structure which can map (x,y) points as keys to
//values.
template<typename ValueType>
class point_map
{
public:
	const ValueType& get(const point& p) const
	{
		const ValueType* value = lookup(p);
		if(!value) {
			static ValueType EmptyValue;
			return EmptyValue;
		} else {
			return *value;
		}
	}

	void insert(const point& p, ValueType value)
	{
		Row* row;
		if(p.y < 0) {
			const int index = -p.y - 1;
			if(static_cast<std::size_t>(index) >= negative_rows_.size()) {
				negative_rows_.resize((index + 1)*2);
			}

			row = &negative_rows_[index];
		} else {
			const int index = p.y;
			if(static_cast<std::size_t>(index) >= positive_rows_.size()) {
				positive_rows_.resize((index + 1)*2);
			}

			row = &positive_rows_[index];
		}

		if(p.x < 0) {
			const int index = -p.x - 1;
			if(static_cast<std::size_t>(index) >= row->negative_cells.size()) {
				row->negative_cells.resize(index+1);
			}

			row->negative_cells[index] = value;
		} else {
			const int index = p.x;
			if(static_cast<std::size_t>(index) >= row->positive_cells.size()) {
				row->positive_cells.resize(index+1);
			}

			row->positive_cells[index] = value;
		}
	}

private:

	const ValueType* lookup(const point& p) const
	{
		const Row* row;
		if(p.y < 0) {
			const int index = -p.y - 1;
			if(static_cast<std::size_t>(index) >= negative_rows_.size()) {
				return nullptr;
			}

			row = &negative_rows_[index];
		} else {
			const unsigned index = static_cast<unsigned>(p.y);
			if(index >= positive_rows_.size()) {
				return nullptr;
			}

			row = &positive_rows_[index];
		}

		if(p.x < 0) {
			const int index = -p.x - 1;
			if(index < 0 || static_cast<unsigned>(index) >= row->negative_cells.size()) {
				return nullptr;
			}

			return &row->negative_cells[index];
		} else {
			const int index = p.x;
			if(index < 0 || static_cast<unsigned>(index) >= row->positive_cells.size()) {
				return nullptr;
			}

			return &row->positive_cells[index];
		}
	}

	struct Row {
		std::vector<ValueType> negative_cells, positive_cells;
	};

	std::vector<Row> negative_rows_, positive_rows_;
};
