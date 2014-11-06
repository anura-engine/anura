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
#include "data_blob.hpp"

BEGIN_DEFINE_CALLABLE_NOBASE(data_blob)
DEFINE_FIELD(string, "string")
	return variant(obj.to_string());
END_DEFINE_CALLABLE(data_blob)

data_blob::data_blob(const std::string& key, const std::vector<char>& in_data) 
	: data_(in_data), key_(key)
{
	rw_ops_ = boost::shared_ptr<SDL_RWops>(SDL_RWFromMem(&data_[0], data_.size()), deleter());
}
	
data_blob::~data_blob()
{
}

SDL_RWops* data_blob::get_rw_ops()
{
	return rw_ops_.get();
}

std::string data_blob::operator()()
{
	return key_;
}

std::string data_blob::to_string() const
{
	return std::string(data_.begin(), data_.end());
}
