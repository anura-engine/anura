/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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

#if defined(USE_ISOMAP)

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
	void handle_event(int nevent, const formula_callable* context=NULL);
	void handle_event(const std::string& event, const formula_callable* context=NULL);

	virtual void process(level& lvl);

	virtual bool execute_command(const variant& b);
private:
	variant get_value_by_slot(int slot) const;
	void set_value_by_slot(int slot, const variant& value);

	variant get_value(const std::string& key) const;
	void set_value(const std::string& key, const variant& value);

	const_voxel_object_type_ptr type_;
	std::vector<variant> data_;
	int data_target_;

	bool created_;
};

typedef boost::intrusive_ptr<user_voxel_object> user_voxel_object_ptr;

}

#endif
