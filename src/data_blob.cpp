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
