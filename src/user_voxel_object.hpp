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

#include <vector>

#include "variant.hpp"
#include "voxel_object.hpp"
#include "voxel_object_type.hpp"

namespace voxel
{
class user_voxel_object : public voxel_object
{
public:
	explicit user_voxel_object(const variant& node);
	user_voxel_object(const std::string& type, float x, float y, float z);
	void handle_event(int nevent, const formula_callable* context=NULL);
	void handle_event(const std::string& event, const formula_callable* context=NULL);

	virtual void process(level& lvl);
private:
	variant get_value_by_slot(int slot) const;
	void set_value_by_slot(int slot, const variant& value);

	variant get_value(const std::string& key) const;
	void set_value(const std::string& key, const variant& value);

	const_voxel_object_type_ptr type_;
	std::vector<variant> data_;
	int data_target_;
};

typedef boost::intrusive_ptr<user_voxel_object> user_voxel_object_ptr;

}
